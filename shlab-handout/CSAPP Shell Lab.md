# CSAPP Shell Lab

本实验的目的是更加熟悉过程控制和信号的概念。 您将通过编写一个支持作业控制的简单 Unix shell 程序来完成此操作。

我们要做的是填写`tsh.c`文件的空函数，完善其内容，使其达到我们预期的目的。详细的需要实现的功能请查看实验文档。根据文档的提示，我们可以根据测试文件`tracei.txt`来从易到难一步一步实现相应功能，通过`make testi`和`make rtesti`对比输出内容判断相应功能是否正确实现。做之前先看一遍已经给实现好的辅助函数一遍后续解题正确使用，并且要仔细看实验文档，里面有很多重要提示，最好仔细阅读过第八章内容。

## 关键点

以下是程序中的一些要点，一些甚至关乎着程序的正确性：

-   实现信号处理程序时，务必将信号发送至整个进程组，即在kill函数中使用`-pid`而不是`pid`。
-   使用`sigprocmask`来避免`deletejob`和`addjob`出现竞争情况。做法是父进程必须在派生子进程之前使用 `sigprocmask` 来阻塞` SIGCHLD `信号，在将子进程通过`addjob`添加到作业列表后再次使用 `sigprocmask`以取消阻塞这些信号。 由于子进程继承了父进程的阻塞向量，因此子进程必须在执行新程序之前取消阻塞 `SIGCHLD` 信号。
-   使用`setpgid(0,0)`将`fork`的子进程创建新的进程组。确保在前台进程中只有一个进程，即我们的`shell`即`tsh`进程。这样做是为避免我们所执行的作业影响我们的`shell`，如果我们的`shell`创建了一个子进程，默认情况该子进程也是前台进程组，若此时键入`ctrl-c`将会向前台的进程组发送`SIGINT`，将导致`shell`退出，显然不对。
-   `waitpid`的`WUNTRACED`和`WNOHANG`选项，可避免等待任何运行中的子进程，同时可以获知子进程的终止和停止状态。
-   在`waitfg`中使用一个围绕睡眠函数的繁忙循环。
-   只要子进程的状态发生改变就会向父进程发送`SIGCHLD`信号。
-   在使用`printf`等有输出缓冲区的函数往标准输出上输出后，及时使用`fflush`刷新缓冲区，避免出现令人意想不到的结果。
-   阻塞所有的信号，保护对共享全局数据结构的访问。
-   保护和恢复`errno`。

## 准备工作

我是预先添加了错误处理函数，简化程序结构，如下。参考[csapp官网](http://csapp.cs.cmu.edu/3e/ics3/code/src/csapp.c)。

```c
// 额外增加错误处理包装函数--begin
pid_t Fork(void) {
    pid_t pid;

    if ((pid = fork()) < 0) unix_error("Fork error");
    return pid;
}

pid_t Waitpid(pid_t pid, int *iptr, int options) {
    pid_t retpid;

    if ((retpid = waitpid(pid, iptr, options)) < 0) unix_error("Waitpid error");
    return (retpid);
}
void Kill(pid_t pid, int signum) {
    int rc;

    if ((rc = kill(pid, signum)) < 0) unix_error("Kill error");
}

void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    if (sigprocmask(how, set, oldset) < 0) unix_error("Sigprocmask error");
    return;
}

void Sigemptyset(sigset_t *set) {
    if (sigemptyset(set) < 0) unix_error("Sigemptyset error");
    return;
}

void Sigfillset(sigset_t *set) {
    if (sigfillset(set) < 0) unix_error("Sigfillset error");
    return;
}

void Sigaddset(sigset_t *set, int signum) {
    if (sigaddset(set, signum) < 0) unix_error("Sigaddset error");
    return;
}

int Sigsuspend(const sigset_t *set) {
    int rc = sigsuspend(set); /* always returns -1 */
    if (errno != EINTR) unix_error("Sigsuspend error");
    return rc;
}
void Setpgid(pid_t pid, pid_t pgid) {
    int rc;

    if ((rc = setpgid(pid, pgid)) < 0) unix_error("Setpgid error");
    return;
}
// 额外增加错误处理包装函数--end
```

## builtin_cmd

最简单是先完成`builtin_cmd`,判断是否是内置命令，若是执行。

```c
int builtin_cmd(char **argv) {
    if (!strcmp(argv[0], "quit")) exit(EXIT_SUCCESS);
    if (!strcmp(argv[0], "jobs")) {
        listjobs(jobs);
        return 1;
    }
    if (!strcmp(argv[0], "bg") || !strcmp(argv[0], "fg")) {
        do_bgfg(argv);
        return 1;
    }
    return 0; /* not a builtin command */
}
```

## eval

再仿照书上完成`eval`的内容，需要额外注意的是，如果是运行在前台的，要调用`waitfg`函数等待其不再执行。

```c
void eval(char *cmdline) {
    char *agrv[MAXARGS];
    char buf[MAXLINE];
    int bg;
    pid_t pid;

    strcpy(buf, cmdline);
    bg = parseline(buf, agrv);
    if (agrv[0] == NULL) return;  // Ignore empty lines

    if (!builtin_cmd(agrv)) {
        sigset_t mask_all, mask_one, prev_one;
        Sigfillset(&mask_all);
        Sigemptyset(&mask_one);
        Sigaddset(&mask_one, SIGCHLD);

        // 阻塞SIGCHLD信号，消除竞争，避免父进程运行到addjob之前子进程就退出
        // 在调用fork之前，阻塞SIGCHLD信号，然后再调用addjob之后取消阻塞这些信号，保证了在子进程被添加到作业列表之后回收该子进程
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);  // 阻塞SIGCHLD
        if ((pid = Fork()) == 0) {                     // Child runs user job
            // 创建一个新的进程组,和tsh进程组分离，避免后续操作影响到tsh进程
            Setpgid(0, 0);
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);  // 子进程继承父进程状态，需要恢复上面阻塞SIGCHLD信号之前的状态
            if (execve(agrv[0], agrv, environ) < 0) {
                printf("%s: Command not found\n", agrv[0]);
                exit(EXIT_SUCCESS);
            }
        }
        int status = bg ? BG : FG;
        Sigprocmask(SIG_BLOCK, &mask_all, NULL);  // 阻塞全部信号
        addjob(jobs, pid, status, cmdline);
        // bg需要打印信息，也会使用全局数据，为保证安全，也需要在Sigprocmask之间，屏蔽所有信号
        if (bg) printf("[%d] (%d) %s", getjobpid(jobs, pid)->jid, pid, cmdline);
        Sigprocmask(SIG_SETMASK, &prev_one, NULL);  // 恢复阻塞SIGCHLD信号之前的状态

        if (!bg) {  // No background, waiting for child process to end
            waitfg(pid);
        }
    }
}
```

## waitfg

阻塞直到`pid`不再是前台进程。这里使用`sigsuspend`作为睡眠函数，得益于其本身的原子性，避免潜在的竞争，详细原因见`8.5.7`节。

```c
void waitfg(pid_t pid) {
    sigset_t mask;
    Sigemptyset(&mask);
    while (pid == fgpid(jobs)) Sigsuspend(&mask);
}
```

## sigtstp_handler

`SIGTSTP`信号的处理程序，捕获它并通过发送`SIGTSTP`挂起前台作业。

```c
void sigtstp_handler(int sig) {
    int olderrno = errno;
    pid_t pid = fgpid(jobs);
    if (pid) Kill(-pid, sig);
    errno = olderrno;
}
```

## sigint_handler

`SIGINT`信号的处理程序，捕获它并将其发送到前台作业。

```c
void sigint_handler(int sig) {
    int olderrno = errno;
    pid_t pid = fgpid(jobs);
    if (pid) Kill(-pid, sig);
    errno = olderrno;
}
```

## sigchld_handler

只要子进程的状态发生改变就会向父进程发送`SIGCHLD`信号。如终止，停止，继续，只要导致这三种状态的出现，都会触发`SIGCHLD`信号。下面的处理程序，只处理终止和停止的状态，继续执行的这种状态专由`do_bgfg`处理。检查子进程的状态由`waitpid`函数的`statusp`记录，详见`8.4.3`节。

```c
void sigchld_handler(int sig) {
    pid_t pid;
    int status;

    sigset_t mask_all, prev_all;
    Sigfillset(&mask_all);  // 设置全阻塞
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    // 这里要判断>0,因为WNOHANG | WUNTRACED表示若等待集合中的子进程都没有停止或终止，则返回0，
    // 若有任一个终止或停止，返回该子进程的pid
    // 由于我们的SIGCONT信号的处理是放在do_bgfg()中的，所以有可能是SIGCONT信号导致的SIGCHLD信号而返回0
    while ((pid = Waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if (WIFEXITED(status)) {  // 正常终止
            deletejob(jobs, pid);
        } else if (WIFSIGNALED(status)) {  // 信号导致终止
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, WTERMSIG(status));
            fflush(stdout);
            deletejob(jobs, pid);
        } else if (WIFSTOPPED(status)) {  // 信号导致停止
            struct job_t *job = getjobpid(jobs, pid);
            job->state = ST;
            printf("Job [%d] (%d) stopped by signal %d\n", job->jid, job->pid, WSTOPSIG(status));
            fflush(stdout);
        }
    }

    Sigprocmask(SIG_SETMASK, &prev_all, NULL);
}
```

## do_bgfg

此函数则是用于处理内置命令`bg fg`的，程序相当一部分是关于参数判断和错误的处理。需要注意的是进程状态的改变也要相应的在`jobs`中改变，如果是调到前台运行的，要调用相应的`waitfg`函数。

```c
void do_bgfg(char **argv) {
    // argv错误处理
    if (argv[1] == NULL) {
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        fflush(stdout);
        return;
    }

    sigset_t mask_all, prev_all;
    Sigfillset(&mask_all);

    struct job_t *job;

    if (argv[1][0] == '%') {                                             // jid
        if (strspn(argv[1] + 1, "0123456789") != strlen(argv[1] + 1)) {  // 如果%后面的不全是数字，错误
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            fflush(stdout);
            return;
        }
        int jid = atoi(argv[1] + 1);
        job = getjobjid(jobs, jid);
        if (job == NULL) {
            printf("%s: No such job\n", argv[1]);
            fflush(stdout);
            return;
        }
    } else {                                                     // pid
        if (strspn(argv[1], "0123456789") != strlen(argv[1])) {  // 如果不全是数字，错误
            printf("%s: argument must be a PID or %%jobid\n", argv[0]);
            fflush(stdout);
            return;
        }
        pid_t pid = atoi(argv[1]);
        job = getjobpid(jobs, pid);
        if (job == NULL) {
            printf("(%s): No such process\n", argv[1]);
            fflush(stdout);
            return;
        }
    }
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);  // 需要访问jobs全局变量，先屏蔽全部信号
    if (!strcmp(argv[0], "fg")) {                  // fg,将一个停止或运行的后台作业改变为在前台运行
        if (job->state == ST) {                    // 如果是停止的，发送SIGCONT信号重新启动
            Kill(-(job->pid), SIGCONT);
        }
        job->state = FG;
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        waitfg(job->pid);  // 等待前台作业运行
    } else {               // bg,将一个后台停止作业改变为后台运行
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
        Kill(-(job->pid), SIGCONT);
        job->state = BG;
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
}
```

## 总结

可以通过以下命令将测试的输出内容重定向到新建的文件`tsh.out`，随后通过文本比较和`tshref.out`进行比较，检查是否除了`pid`和`ps a`不同，其他都和`tshref.out`相同，经测试，以上所编写的代码和`tshref.out`比较无误：

```bash
./sdriver.pl -t trace01.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace02.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace03.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace04.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace05.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace06.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace07.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace08.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace09.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace10.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace11.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace12.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace13.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace14.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace15.txt -s ./tsh -a "-p" >> tsh.out 
./sdriver.pl -t trace16.txt -s ./tsh -a "-p" >> tsh.out 
```

本实验难度适中，代码量也不大，若是认真阅读过第八章内容以及实验文档，编写起来还是比较顺利的。通过此实验，较清晰的了解了`shell`的工作原理，以及信号的处理，如何避免出现竞争，并发的编程规范，但以上程序中，对于`getjobpid`此类从job获取信息但不对其进行修改我并没有使用`sigprocmask`屏蔽全部信号，因为我认为，及时指令序列被处理程序中断，也不会影响其后续的结果，关于这一问题，暂存疑，若有缺陷的地方，后续将会修改。

