# CSAPP Cache Lab

本实验将帮助您了解缓存存储器对 C 语言性能的影响程式。实验室由两部分组成。 在第一部分中，您将编写一个小的 C 程序（大约 200-300 行）模拟高速缓存的行为。 在第二部分中，您将优化一个小型矩阵转置函数，目标是最小化高速缓存未命中的次数。

## **Part A: Writing a Cache Simulator**

在 A 部分中，您将在 `csim.c` 中编写一个缓存模拟器，它将 `valgrind` 内存跟踪作为输入，在此跟踪上模拟高速缓存的命中/未命中行为，并输出命中，未命中和驱逐总数。实验为我们提供了参考缓存模拟器的二进制可执行文件，称为 `csim-ref`，它在 `valgrind` 跟踪文件上模拟具有任意大小和关联性的缓存行为。 它使用选择要逐出的缓存行时的 `LRU（最近最少使用）`替换策略。关于`csim-ref`的详细信息可以查看官方文档，或自动测试。

我们在这部分的工作是填写 `csim.c` 文件，以便它采用相同的命令行参数和产生与参考模拟器`csim-ref`相同的输出。

1.  查看官方实验文档，得知`csim-ref`最多有6个命令行参数，所以首先需要处理命令行参数的读入，可以使用`getopt`方便的处理。完整代码中这部分模块如下，先了解框架即可。

    ```c
    // 使用getopt函数，每次取出一个命令行参数
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
            case 'h':
                printf("%s", help_info);
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                debug = true;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                trace_file = optarg;
                break;
    
            default:  // error input
                fprintf(stderr, "Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
                exit(EXIT_FAILURE);
                break;
        }
    }
    ```

2.  读入的命令行参数中有高速缓存参数，`s E b` 分别为组索引位数量（2^s为组数），每组行数，块偏移位数量（2^b为块大小单位字节），和书上的内容一致。同时我们需要设置每一行的结构，如标记，组索引，块偏移（实际用不到），以及一些其他变量，如`hit_cnt, miss_cnt, eviction_cnt`  命中，未命中，驱逐个数，等等。一些设置如下。

    ```c
    extern char* optarg;  // getopt设置的全局变量
    
    bool debug = false;                   // 命令行参数v的标记，显示跟踪信息的表示，默认关闭
    int s, E, b;                          // 缓存参数：组索引位数量（2^s为组数），每组行数，块偏移位数量（2^b为块大小单位字节）
    char* trace_file;                     // 输入来源文件
    int hit_cnt, miss_cnt, eviction_cnt;  // 命中，未命中，驱逐 个数
    int cur_time = 0;                     // 时间计数(用于LRU算法)
    
    char opera = 0;        // 运算符
    uint64_t address = 0;  // 地址
    int size = 0;          // 大小
    
    // 缓存行结构
    typedef struct Cacheline {
        bool vaild;           // 有效位
        int tag, time_stamp;  // 标记位，时间戳（用于LRU算法）
    } Cache;
    Cache* cache;
    // 便于cache的访问
    #define cache(x, y) cache[x * E + y]
    
    // const int kMaxSize = 64; c语言不能用const常量定义数组大小
    #define kMaxSize 64
    char input_str[kMaxSize];  // 存储读入的一行数据
    const char* debug_info[3] = {"miss", "miss hit", "miss eviction"};  // 跟踪信息
    ```

3.  接下来需要根据命令行参数获取的`trace_file`进行操作的读取。首先根据文档我们不用处理`I`操作，进一步实际上我们只是需要模拟高速缓存的行匹配机制和不匹配时加载即可，并不用真的进行数据的读取和存储，所以`L`操作和`S`操作是一样的。而且文档说`M`操作只是先数据加载，再数据存储，所以`M`操作也可以变为`L`操作和`S`操作。最终我们要做的只有一个操作。下面是操作的读取以及行匹配以及不匹配时加载的操作。

    ```c
    //行匹配以及不匹配时加载操作
    void load(uint64_t address, bool display) {
        ++cur_time;  // 每次load时间都会++
    
        int tag = address >> (b + s);                 // 标记
        int index = (address >> b) & ((1 << s) - 1);  // 组索引
    
        // 遍历第index组，查看是否命中
        for (int i = 0; i < E; ++i) {
            if (cache(index, i).vaild && cache(index, i).tag == tag) {
                ++hit_cnt;
                cache(index, i).time_stamp = cur_time;
                if (display) printDebug(0);
                return;
            }
        }
    
        // 不命中--
        ++miss_cnt;
    
        // 判断是否有“空行”
        for (int i = 0; i < E; ++i) {
            if (!cache(index, i).vaild) {
                // cache(index, i) = {true, tag, cur_time};     C 语言不允许这样整体赋值
                cache(index, i).vaild = true;
                cache(index, i).tag = tag;
                cache(index, i).time_stamp = cur_time;
                if (display) printDebug(1);
                return;
            }
        }
        // 行替换（LRU)
        int last_stamp = cur_time, pos = -1;
        ++eviction_cnt;
        for (int i = 0; i < E; ++i) {
            if (cache(index, i).time_stamp < last_stamp) {
                last_stamp = cache(index, i).time_stamp;
                pos = i;
            }
        }
    
        // cache(index, pos) = {true, tag, 0};
        cache(index, pos).vaild = true;
        cache(index, pos).tag = tag;
        cache(index, pos).time_stamp = cur_time;
        if (display) printDebug(2);
    }
    // 工作函数，读所给文件，进行处理
    void work() {
        FILE* fp = fopen(trace_file, "r");  // 读形式打开文件
    
        while (fgets(input_str, kMaxSize, fp) != NULL) {
            sscanf(input_str, " %c %lx,%d", &opera, &address, &size);  // 即使I运算符前面有空格也可以正常读入
            switch (opera) {
                case 'I':
                    continue;  // 指令加载 忽略不在我们的处理范围
                    break;
                case 'L':                 // 数据加载，即读
                case 'S':                 // 数据存储，即写
                    load(address, true);  // 我们模拟的高速缓存的读和写实际上是相同的操作
                    break;
                case 'M':                 // 数据修改，即先数据加载再数据存储
                    load(address, true);  // 执行两次即可
                    load(address, false);
                    break;
                default:
                    break;
            }
        }
        fclose(fp);
        free(cache);
    }
    ```

4.  最后，根据模块的功能不同，封装了两个函数，`init`函数处理命令行参数的解析，`work`函数处理操作的读入以及操作的执行。最终完整代码如下。160行拿下，实际上不用对命令行参数`h`和`v`进行相应也是可以通过测试的。代码的整体设计还是比较满意的，模块的功能分明，逻辑也比较清晰。

    ```c
    #include <getopt.h>
    #include <stdbool.h>
    #include <stdint.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>
    
    #include "cachelab.h"
    
    extern char* optarg;  // getopt设置的全局变量
    
    bool debug = false;                   // 命令行参数v的标记，显示跟踪信息的表示，默认关闭
    int s, E, b;                          // 缓存参数：组索引位数量（2^s为组数），每组行数，块偏移位数量（2^b为块大小单位字节）
    char* trace_file;                     // 输入来源文件
    int hit_cnt, miss_cnt, eviction_cnt;  // 命中，未命中，驱逐 个数
    int cur_time = 0;                     // 时间计数
    
    char opera = 0;        // 运算符
    uint64_t address = 0;  // 地址
    int size = 0;          // 大小
    
    // 缓存行结构
    typedef struct Cacheline {
        bool vaild;           // 有效位
        int tag, time_stamp;  // 标记位，时间戳（用于LRU算法）
    } Cache;
    Cache* cache;
    // 便于cache的访问
    #define cache(x, y) cache[x * E + y]
    
    // const int kMaxSize = 64; c语言不能用const常量定义数组大小
    #define kMaxSize 64
    char input_str[kMaxSize];  // 存储读入的一行数据
    const char* debug_info[3] = {"miss", "miss hit", "miss eviction"};  // 跟踪信息
    
    // 初始化，用于参数的读入，以及高速缓存结构的建立
    void init(int argc, char* argv[]) {
        const char* help_info =
            "Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t "
            "<file>\nOptions:\n  -h Print this help message.\n  -v "
            "Optional verbose flag.\n  -s <num> Number of set index "
            "bits.\n  -E <num> Number of lines per set.\n  -b <num> "
            "Number of block offset bits.\n  -t <file> Trace file.\n\n"
            "Examples :\n linux> ./csim -s 4 -E 1 -b 4 -t "
            "traces/yi.trace\n linux> ./csim -v -s 8 -E 2 "
            "-b 4 -t traces/yi.trace\n ";
    
        int opt = 0;
        // 使用getopt函数，每次取出一个命令行参数
        while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
            switch (opt) {
                case 'h':
                    printf("%s", help_info);
                    exit(EXIT_SUCCESS);
                    break;
                case 'v':
                    debug = true;
                    break;
                case 's':
                    s = atoi(optarg);
                    break;
                case 'E':
                    E = atoi(optarg);
                    break;
                case 'b':
                    b = atoi(optarg);
                    break;
                case 't':
                    trace_file = optarg;
                    break;
    
                default:  // error input
                    fprintf(stderr, "Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
                    exit(EXIT_FAILURE);
                    break;
            }
        }
    
        cache = (Cache*)malloc(sizeof(Cache) * (1 << s) * E);
        memset(cache, 0, sizeof(Cache) * (1 << s) * E);
    }
    
    void printDebug(int status) {
        // 0 命中   1 未命中    2 驱逐
        printf("%c %lx,%d %s\n", opera, address, size, debug_info[status]);
    }
    //行匹配以及不匹配时加载操作
    void load(uint64_t address, bool display) {
        ++cur_time;  // 每次load时间都会++
    
        int tag = address >> (b + s);                 // 标记
        int index = (address >> b) & ((1 << s) - 1);  // 组索引
    
        // 遍历第index组，查看是否命中
        for (int i = 0; i < E; ++i) {
            if (cache(index, i).vaild && cache(index, i).tag == tag) {
                ++hit_cnt;
                cache(index, i).time_stamp = cur_time;
                if (display) printDebug(0);
                return;
            }
        }
    
        // 不命中--
        ++miss_cnt;
    
        // 判断是否有“空行”
        for (int i = 0; i < E; ++i) {
            if (!cache(index, i).vaild) {
                // cache(index, i) = {true, tag, cur_time};     C 语言不允许这样整体赋值
                cache(index, i).vaild = true;
                cache(index, i).tag = tag;
                cache(index, i).time_stamp = cur_time;
                if (display) printDebug(1);
                return;
            }
        }
        // 行替换（LRU)
        int last_stamp = cur_time, pos = -1;
        ++eviction_cnt;
        for (int i = 0; i < E; ++i) {
            if (cache(index, i).time_stamp < last_stamp) {
                last_stamp = cache(index, i).time_stamp;
                pos = i;
            }
        }
    
        // cache(index, pos) = {true, tag, 0};
        cache(index, pos).vaild = true;
        cache(index, pos).tag = tag;
        cache(index, pos).time_stamp = cur_time;
        if (display) printDebug(2);
    }
    
    // 工作函数，读所给文件，进行处理
    void work() {
        FILE* fp = fopen(trace_file, "r");  // 读形式打开文件
    
        while (fgets(input_str, kMaxSize, fp) != NULL) {
            sscanf(input_str, " %c %lx,%d", &opera, &address, &size);  // 即使I运算符前面有空格也可以正常读入
            switch (opera) {
                case 'I':
                    continue;  // 指令加载 忽略不在我们的处理范围
                    break;
                case 'L':                 // 数据加载，即读
                case 'S':                 // 数据存储，即写
                    load(address, true);  // 我们模拟的高速缓存的读和写实际上是相同的操作
                    break;
                case 'M':                 // 数据修改，即先数据加载再数据存储
                    load(address, true);  // 执行两次即可
                    load(address, false);
                    break;
                default:
                    break;
            }
        }
        fclose(fp);
        free(cache);
    }
    int main(int argc, char* argv[]) {
        init(argc, argv);
        work();
        printSummary(hit_cnt, miss_cnt, eviction_cnt);
        return 0;
    }
    ```

使用`unix> make && ./test-csim` 或 `unix> make && ./driver.py` 测试得到 Part A 27分满分，通过全部测试。这部分比较简单。

<img src="CSAPP%20Cache%20Lab.assets/cache1.png" style="zoom:80%;" />

##  **Part B: Optimizing Matrix Transpose**

做这部分时需要确保你已经安装了`valgrind`，可以`valgrind --version`查看是否安装，否则安装`sudo apt  install valgrind`(Ubuntu 20.04下)。

在 B 部分中，我们将在 `trans.c` 中编写一个转置函数，它会导致尽可能少的缓存未命中。我们在 `trans.c` 中为您提供了一个示例转置函数`trans`，用于计算N × M 矩阵 A 的转置并将结果存储在 M × N 矩阵 B 中。你在 B 部分的工作是编写一个类似的函数，称为 `transpose_submit`，它最小化数字不同大小矩阵的缓存未命中数。

注意，其中有一些限制规则需要我们遵守，详见实验文档。

实验所给的cache参数为`s = 5, E = 1, b = 5`，即共有32组，每组一行，每行32字节（存8个int）。共有32 × 32 (M = 32, N = 32)   	64 × 64 (M = 64, N = 64) 	 61  67 (M = 61, N = 67) 三类举证大小。

高速缓存cache大小为32×32=1024字节，对于一个32×32的矩阵A来说的话，能存下前8行，但是另外还有一个矩阵B，我们需要思考对于`A[x][y]`和`B[x][y]`是否会是同一个组吗。通过`test-trans`程序生成的文件`trace.fi`可以发现问题的答案是肯定的，会映射在同一个组。下面是我们观察例子函数`trans`对应的`trace.f1`,我们查看第一条`B[i][j]=A[i][j]`，即可发现`A[0][0]`和`B[0][0]`分别为`0010d080`和`0014d080`，根据组索引即可判断。（查看实验文档获得相应提示研究可知）

![](CSAPP%20Cache%20Lab.assets/cache2.png)

根据文档提示，可以使用书上提到的分块来提高时间局部性。

### 32×32

我们先考虑32×32的矩阵，由于每行可以存储8个int，所以我们使用8×8的分块大小进行处理。设计如下。miss次数为287小于300，拿下满分。

```c
for (int i = 0; i < N; i += 8)             		// 块起点x轴
        for (int j = 0; j < M; j += 8)         // 块起点y轴
            for (int k = i; k < i + 8; ++k) {  // 每次处理一行
                // 下面8个读入，只有第一个会miss,其他七个都会hit
                int tmp1 = A[k][j];
                int tmp2 = A[k][j + 1];
                int tmp3 = A[k][j + 2];
                int tmp4 = A[k][j + 3];
                int tmp5 = A[k][j + 4];
                int tmp6 = A[k][j + 5];
                int tmp7 = A[k][j + 6];
                int tmp8 = A[k][j + 7];
                B[j][k] = tmp1;
                B[j + 1][k] = tmp2;
                B[j + 2][k] = tmp3;
                B[j + 3][k] = tmp4;
                B[j + 4][k] = tmp5;
                B[j + 5][k] = tmp6;
                B[j + 6][k] = tmp7;
                B[j + 7][k] = tmp8;
            }
```

现在来分析下上面这个程序的miss数，对于对角线上的块，A数组读入第一行8个int，存储到B数组8个列，这里我们以左上角的块来距离，首先会在根据地址`0010d080`组索引8来查看第8组是否命中，由于是首次读入，miss，随后的7个都会hit。之后将读入的8个数存入数组B的首列，由于首次访问，8个都会miss（注意，B数组第一行的加载会驱逐A数组第一行）。随后开始A数组第二行的读入，（注意，这会驱逐B数组的第二行）写入B数组的第二列，随后的列的写入都会hit（除开第二行会miss)。所以对于对角线的8×8的块A和块B，一共会有23次miss，非对角线上的矩阵由于不会映射到同一组中，所以两个矩阵共有16次miss，`总miss= 23×4 + 16×12 = 284`。可是测试得出的却是287呀！

<img src="CSAPP%20Cache%20Lab.assets/cache5.png" style="zoom:50%;" />

实际上我们使用`./csim-ref -v -s 5 -E 1 -b 5 -t trace.f0`追踪内存的操作，可以发现开始和结束时恰好有三个和矩阵转置无关的操作。如下图。观察这五个操作的地址，我认为应该是从栈中内存操作有关，前4个是函数的参数吗，但又不像，因为第一个操作是写入，不像其他三个。跟踪的最后一个我猜跟函数的返回有关，不确定是否跟栈指针有关。总之，我们的分析是正确的，手动计算284次miss加上矩阵转置之外的三次miss，巧合等于287。

<img src="CSAPP%20Cache%20Lab.assets/cache3.png" style="zoom: 67%;" />

<img src="CSAPP%20Cache%20Lab.assets/cache4.png" style="zoom:67%;" />

### 64×64

对于这个规模的单个矩阵的话，cache只能存下前4行了。那么就不能使用8×8的分块了，因为对于存储数组B的话，需要存8行，那么后4行就会和前4行映射到同一组中导致“抖动”，若直接用上面的8×8的分块计算的话，miss次数为4611，满分要求为小于1300，那么我们只能考虑4×4分块来避免“抖动”，但4×4分块也有缺点，因为cache一行可以存8个int，但4×4的话，只用到了4个，后面4个就会浪费。直接使用4×4分块，测试结果miss次数为1699，不够满分。那么我们进一步发现8×8分块的“抖动”只发生在B数组上，A数组运行良好，那么可以考虑改善8×8分块的算法，下面分为三步来分析。若对于以下的算法难以理解，建议画图辅助理解。

1.  首先，我们先处理A块的前4行，不同的是存入B中的位置，对于A块前4行的前4列就放入B中该放的位置，但后4列就放在B块的前4行的后4列。算法流程如下。

    ```c
    // 先处理A块的前4行
    for (int k = i; k < i + 4; ++k) {  // 每次处理一行
        tmp1 = A[k][j];
        tmp2 = A[k][j + 1];
        tmp3 = A[k][j + 2];
        tmp4 = A[k][j + 3];
        tmp5 = A[k][j + 4];
        tmp6 = A[k][j + 5];
        tmp7 = A[k][j + 6];
        tmp8 = A[k][j + 7];
        B[j][k] = tmp1;
        B[j + 1][k] = tmp2;
        B[j + 2][k] = tmp3;
        B[j + 3][k] = tmp4;
        B[j][k + 4] = tmp5;
        B[j + 1][k + 4] = tmp6;
        B[j + 2][k + 4] = tmp7;
        B[j + 3][k + 4] = tmp8;
    }
    ```

2.  之后，我们取出A块第一列的后4个数，再取出B块第一行的后4个数，然后将前者的4个数放入B块第一行的后4个位置（即后者4个数的原位置），再将后者的4个数写入B块第5行的前4个位置。余下的3部分依次下去。算法流程如下。

    ```c
    for (int k = j; k < j + 4; ++k) {
        tmp1 = A[i + 4][k], tmp2 = A[i + 5][k], tmp3 = A[i + 6][k], tmp4 = A[i + 7][k];
        tmp5 = B[k][i + 4], tmp6 = B[k][i + 5], tmp7 = B[k][i + 6], tmp8 = B[k][i + 7];
    
        B[k][i + 4] = tmp1, B[k][i + 5] = tmp2, B[k][i + 6] = tmp3, B[k][i + 7] = tmp4;
        B[k + 4][i] = tmp5, B[k + 4][i + 1] = tmp6, B[k + 4][i + 2] = tmp7, B[k + 4][i + 3] = tmp8;
    }
    ```

3.  最后将，A块的右下角转置到B块的右下角。

    ```c
    for (int k = i + 4; k < i + 8; ++k) {
        tmp1 = A[k][j + 4];
        tmp2 = A[k][j + 5];
        tmp3 = A[k][j + 6];
        tmp4 = A[k][j + 7];
        B[j + 4][k] = tmp1;
        B[j + 5][k] = tmp2;
        B[j + 6][k] = tmp3;
        B[j + 7][k] = tmp4;
    }
    ```

     对于上述流程建议画图理解，尤其是第二步。

最终测试结果如下，满分通过：

![](CSAPP%20Cache%20Lab.assets/cache6.png)

现在来分析一下上述算法的miss次数，分析思路与32×32类似。下面的次数，都是块A和相对应的块B一起来说的。

对于非对角线8×8分块，在第一步时，miss次数易知共为8次，A块4次，B块4次；第二步时，miss次数共8次，A块4次，B块4次；第三步时共0次miss，整体16次miss。非对角线块共56对，总miss次数为`56×16=896`。

对于处于对角线上的块，由于会映射到同一组中，会略微麻烦些，对于第一步，共11次miss，一开始A块读一行，1次miss，B块存4行，4次miss，对于A块的剩下三行，A和B会出现“抖动”，导致每次的读和写都会miss，共6次，总计11次；第二步，共15次miss，读A块的左下角那一列时，4次miss，随后读右上角那一行时，1次miss，然后写左下角那一行时，1次miss，随后的读最下角一列时，读右上角一行时，已经1写最下角一行时都会导致一次miss，连续三次相同操作，共9次，总计15次；第三步时，写右下角一列时，3次miss（因为上一步的最后缓存了一行），余下的连续三次都会出现“抖动”，读写各1次miss，总计9次miss，整体35次miss。8各对角线上的块，总miss次数为`35×8=280`。

所以全部miss次数为`896+280=1176`加上我们之前发现的3次非矩阵转置的miss，总计1176，与测试结果相同，说明我们分析正确。

### 61×67

先使用4×4分块，测得miss次数为2425，能拿5.8分，再试试8×8分块，测定miss次数为2118，能拿8.8分，需要miss次数达到2000一下方可满分。观察61×67，由于长宽不是8的倍数，那么每行的首元素映射到cache的组索引就不像32×32每8行映射到一个组或64×64每4行映射到一组。自然可以猜想到61×67的B数组的映射冲突非常少。不妨写一个程序测试一下。

```cpp
#include <iostream>
#include <unordered_set>
using namespace std;
typedef unsigned long long ULL;
int main()
{
    ULL p = 0x14D080;	// B[0][0]地址
    unsigned int offset = 61* 4;
    unordered_set<ULL> check;
    for (int i = 0; i < 61; ++i) {
        ULL new_p = p + i * offset;
        // 取出组索引
        ULL idx = (new_p >> 5) &((1 << 5) - 1);
        printf("B[%d][0] = %llu\n", i,idx);
        if (check.count(idx)) {
            printf("clash\n");
        } else {
            check.insert(idx);
        }

    }
    return 0;
}
```

运行结果如下，可以看到知道读第21行才会和之前的一行映射到同一组中，导致冲突。
![](CSAPP%20Cache%20Lab.assets/cache7-16726505873012.png)

再顺便看看A数组的情况，和上面类似。实际A数组不用看，因为读一行，一行8个都会连续读走。
![](CSAPP%20Cache%20Lab.assets/cache8.png)

所以为了充分利用cache（cache能放32组，每组一行），所以我们可以选择大一些的分块，再测试过16×16以及24×24分块后， 使用16×16分块可以拿下满分，测试miss次数为1992。这次就不分析miss次数了，~~因为长宽不是8的倍数，分析起来太麻烦~~😪。

最后贴一个全部测试结果图，拿下满分。

![](CSAPP%20Cache%20Lab.assets/cache9.png)

## 总结

Part A部分比较简单，直接模拟即可，要说难的话，可能是参数的读入，以及整体框架的设计。Part B部分有些难度，但解题过程还算顺利，而且也十分有趣，特别是对miss次数的准确分析，对cache高速缓存的运行方式有了更深的理解，收获较大（~~网上没找到准确分析miss次数以及为什么要选16×16分块的文章~~）。
