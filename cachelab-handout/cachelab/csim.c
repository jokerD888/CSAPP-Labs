#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cachelab.h"

extern char* optarg;  // getopt设置的全局变量

bool debug = false;  // 命令行参数v的标记，显示跟踪信息的表示，默认关闭
int s, E, b;  // 缓存参数：组索引位数量（2^s为组数），每组行数，块偏移位数量（2^b为块大小单位字节）
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
char input_str[kMaxSize];                                           // 存储读入的一行数据
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
// 行匹配以及不匹配时加载操作
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
