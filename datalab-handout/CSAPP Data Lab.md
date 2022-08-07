# CS:APP Data Lab

## 前言

data lab主要考察位运算，补码，浮点数等相关内容，此lab能检验对整型，浮点数存储方式的理解，以及位运算的各种妙用，能够对整数浮点数的实际使用的过程中，对可能出现的“怪异”结果知其所以然。

以下13个函数即是本lab需要完成的内容，根据函数上方的注释及可用操作符限制补充完函数即可，部分函数不允许任何超过8bit的常量，详细内容查看相关注释。
![](CSAPP%20Data%20Lab.assets/Snipaste_2022-08-07_17-01-22.png)

lab资源下载[点这里](http://csapp.cs.cmu.edu/public/labs.html)。

对于一些可能用到或了解的知识在最下方列出。

## lab

1.  bitXor

    ```c
    /* 
     * bitXor - x^y using only ~ and & 
     *   Example: bitXor(4, 5) = 1
     *   Legal ops: ~ &
     *   Max ops: 14
     *   Rating: 1
     */
    // 求x^y
    // 思路，先按位与求出x和y同为1的位，然后x,y分别和~c按位与，求出各自不同时为1的位a,b
    // 最后分别对a,b取反，使得各自位上同时为0或同时为1的位设为1，再将两者按位与后再取反，即得出不同时为1的位
    int bitXor(int x, int y) {  // x:4  y:5
        int c = x & y;          // 0100
        int a = x & ~c;         // 0000
        int b = y & ~c;         // 0001
        int ret = ~a & ~b;      // 1110
        return ~ret;            // 0001
        
        // 或,不是同时为0和不是同时为1的两种情况进行按位与
        // return ~(~x & ~y) & ~(x & y);
    }
    ```

    

2.  tmin

    ```c
    /* 
     * tmin - return minimum two's complement integer 
     *   Legal ops: ! ~ & ^ | + << >>
     *   Max ops: 4
     *   Rating: 1
     */
    // 求Tmin
    // 将1左移31位，这样除了最高符号位其余全为0,即可取得最小值
    int tmin(void) {
        return 1 << 31;
    }
    ```

    

3.  isTmax

    ```c
    /*
     * isTmax - returns 1 if x is the maximum, two's complement number,
     *     and 0 otherwise 
     *   Legal ops: ! ~ & ^ | +
     *   Max ops: 10
     *   Rating: 1
     */
    // 判断x是否是Tmax
    // 若x是Tmax,那么x+1=Tmin,而~Tmin=Tmax,所以可以以此验证，此外当x=-1时，由于溢出也满足这个等式，还需要特判一下
    // 利用异或和逻辑非来判断等式是否成立，!!(x+1)来判断x是否为-1，!!是将数字转化为true或false
    int isTmax(int x) {
        return !(~(x + 1) ^ x) & !!(x + 1);
    }
    ```

    

4.  allOddBits

    ```c
    /* 
     * allOddBits - return 1 if all odd-numbered bits in word set to 1
     *   where bits are numbered from 0 (least significant) to 31 (most significant)
     *   Examples allOddBits(0xFFFFFFFD) = 0, allOddBits(0xAAAAAAAA) = 1
     *   Legal ops: ! ~ & ^ | + << >>
     *   Max ops: 12
     *   Rating: 2
     */
    // 求x的二进制奇数位上全为1
    // 如果奇数位上全为1，返回true
    int allOddBits(int x) {
        int a, b;
        a = 0xAA + (0xAA << 8);  // 注意运算符优先级
        a= a + (a << 16);  //  a的奇数位全为1，偶数位为0
        b = a & x;  //  如果x的奇数位上也全位1，等式的结果为a
        return !(a ^ b);    // 用异或和逻辑非判断两者是否相等
    }
    ```

    

5.  negate

    ```c
    /* 
     * negate - return -x 
     *   Example: negate(1) = -1.
     *   Legal ops: ! ~ & ^ | + << >>
     *   Max ops: 5
     *   Rating: 2
     */
    // 求x的相反数
    // 补码的非=补码取反+1 , -x=~x+1
    int negate(int x) {
        return ~x + 1;
    }
    ```

    

6.  isAsciiDigit

    ```c
    /* 
     * isAsciiDigit - return 1 if 0x30 <= x <= 0x39 (ASCII codes for characters '0' to '9')
     *   Example: isAsciiDigit(0x35) = 1.
     *            isAsciiDigit(0x3a) = 0.
     *            isAsciiDigit(0x05) = 0.
     *   Legal ops: ! ~ & ^ | + << >>
     *   Max ops: 15
     *   Rating: 3
     */
    // 求x是否在0x30~0x39之间
    // 即 00110000～00111001之间，发现4～5位固定是1，之后全0,
    // 而0~3位则从0000递增变化到1001,所以我们先判断高位的24个bit是否为0x3（a)
    // 再判断第3位是否为1(b)，若为1，则判断1，2位上是否有1(c),利用b和c的按位与即可判断
    int isAsciiDigit(int x) {
        int a = !(x >> 4 ^ 3);	// 高位的24个bit是否为0x3
        int b = !!(x & 8);		// 第3位是否为1
        int c = !!(x & 6);		// 则判断1，2位上是否有1
        return a & !(b & c);	// 若高24bit为0x3，且低4位不超过0x9,return 1
    }
    ```

    

7.  conditional

    ```c
    /* 
     * conditional - same as x ? y : z 
     *   Example: conditional(2,4,5) = 4
     *   Legal ops: ! ~ & ^ | + << >>
     *   Max ops: 16
     *   Rating: 3
     */
    // 模拟三元运算符
    // 使用!x-1,即可将x的真假翻转，-1用~0来实现
    int conditional(int x, int y, int z) {
        int a = !x + ~0;    // 若x是0,a=0,否则a是全1
        return (a & y) | (~a & z);
    }
    ```

    

8.  isLessOrEqual

    ```c
    /* 
     * isLessOrEqual - if x <= y  then return 1, else return 0 
     *   Example: isLessOrEqual(4,5) = 1.
     *   Legal ops: ! ~ & ^ | + << >>
     *   Max ops: 24
     *   Rating: 3
     */
    // 模拟<=
    // if x<=y ,返回1，否则返回0
    // 通过y-x的正负即可判断,但作差可能导致溢出，需要特殊考虑一下
    int isLessOrEqual(int x, int y) {
        int sign_x = x >> 31 & 1;   // x的符号位
        int sign_y = y >> 31 & 1;   // y的符号位
        int a = y + (~x + 1);          // y - x
        int sign_a = a >> 31 & 1;   // a的符号位
        // 若a最高位是0，即y-x>=0,应该返回1，但若x>0,y<0,那此时是负溢出造成的结果，应当返回0
        // 若a最高位是1，即y-x<0,应该返回0，但若x<0,y>0,那此时是正溢出造成的结果,应当返回1
        // 总上，返回1有两种可能，x,y符号相同且a最高位是0，以及x<0,y>0(符号位无所谓，x负y正，返回1即可）
        int nosame_xy = sign_x ^ sign_y;  // 若x，y符号位不同为则为1
        return (!sign_a & !nosame_xy) | (nosame_xy & sign_x);
    }
    ```

    

9.  logicalNeg

    ```c
    /* 
     * logicalNeg - implement the ! operator, using all of 
     *              the legal operators except !
     *   Examples: logicalNeg(3) = 0, logicalNeg(0) = 1
     *   Legal ops: ~ & ^ | + << >>
     *   Max ops: 12
     *   Rating: 4 
     */
    // 实现逻辑非，0返回1，非0返回0
    // 只有0的相反数是其本身，所以x^(-x) 只有当x为0时，结果为0，否则，结果是负数，最高位为1，
    // 同时需要注意，在补码中，-Tmin=Tmin,需要特判
    // 所以将最高位提取出来与1异或即可
    // 如果最高位是1，返回0，如果最高位是0，且x^(-x)的最高位是1，返回0
    int logicalNeg(int x) {
        int sign_x = (x >> 31 & 1) ^ 1;   // 如果符号位是1,等式结果为0，否则为1
        int sign_xor = ((~x + 1) ^ x) >> 31 & 1;    //提出 x^(-x)的最高位
        return sign_x & (sign_xor ^ 1);     // ^1 是为了翻转最低位
    
        // 或，上面的用了10个ops,显然冗余了一些，简化如下
        // 对于(~x+1)>>31,若x是正数，结果为-1,若x是负数数或0，结果是0，也就不用x^(-x)来判断了，但也需要特判
        // 用 x|(~x+1>>31) 来表示，若结果为-1，则x不为0，若结果为0，则x为0。最后加上1返回即可
        // return ((x | (~x + 1)) >> 31) + 1;
    }
    ```

    

10.  howManyBits

     ```c
     /* howManyBits - return the minimum number of bits required to represent x in
      *             two's complement
      *  Examples: howManyBits(12) = 5
      *            howManyBits(298) = 10
      *            howManyBits(-5) = 4
      *            howManyBits(0)  = 1
      *            howManyBits(-1) = 1
      *            howManyBits(0x80000000) = 32
      *  Legal ops: ! ~ & ^ | + << >>
      *  Max ops: 90
      *  Rating: 4
      */
     // 求x用二进制补码表示最少需要几位
     // 若x为正数，显然求出其最左边的1的处于第几位，再加1表示符号位即可
     // 若x为负数，则求出其最左边的0的位置，再加1表示符号位即可
     //      为啥负数是找最左边的0呢，因为最高位符号位的权值，如11011（-5）而，1011（-5）
     //      可以发现，假设有w+1位，若符号位的紧邻右边的位也是1的话，先不管其余位，那么值为-(2^w)+(2^(w-1))=-(2^(w-1))
     //      也就是说，与其1100...00,不如100...00这样表示，能使得位数更小，符号位紧邻右边有连续的1同理
     //      我们不妨对x去反，这样就可以和正数一样找最左边的1
     // 那么怎么找最左边的1呢，二分思想，一开始我们先看原x的高16位是否有1，若有则将x>>16,相当于砍掉x的低16位
     // 若没有，则说明高16位全位0,不动，相当于砍掉x的高16位，以此8,4,2进行下去
     int howManyBits(int x) {
         int b16,b8,b4,b2,b1,b0;
         int sign= x >> 31;  
         // 若x是正数不变，若x是负数，按位取反，| 左边是为了应对负数，|右边是为了应对正数，各自的另一半则为0
         x = (sign & ~x) | (~sign & x);  
     
         // 二分思想
         b16 = !!(x >> 16) << 4; //若高16位有1，二进制位的4位置为1，权值2^4=16
         x = x >> b16;           // 若有，此表示式将x右移16位，否则不变
         b8 = !!(x >> 8) << 3;   // 同理，高8位是否有1
         x = x >> b8;
         b4 = !!(x >> 4) << 2;   // 高4位是否有1
         x = x >> b4;
         b2 = !!(x >> 2) << 1;   // 高2位是否有1
         x = x >> b2;
         b1 = !!(x >> 1);        // 最1位是否有1
         x = x >> b1;            
         // 上面所谓的高几位是原x不断砍半后的二进制位，b0为了检验移位到最后剩下的两个bit位的最低位是否为1
         b0 = x;             
         return b16 + b8 + b4 + b2 + b1 + b0 + 1;    //记得加上符号位
       
     }
     ```

     

11.  floatScale2

     ```c
     /* 
      * floatScale2 - Return bit-level equivalent of expression 2*f for
      *   floating point argument f.
      *   Both the argument and result are passed as unsigned int's, but
      *   they are to be interpreted as the bit-level representation of
      *   single-precision floating point values.
      *   When argument is NaN, return argument
      *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
      *   Max ops: 30
      *   Rating: 4
      */
     // 计算2*f,f是浮点数，不过参数和返回值都是无符号整形表示
     // 首先，对于NaN，不用计算，直接返回，NaN的阶码部分全位1，尾数部分不为0，
     // 先求阶码exp=(uf0x7f800000)>>23  ,再求尾数 frac=uf&0x7fffff
     //   如果exp=255,并且尾数非0,那就是NaN,return uf即可，如果frac全0，表示无穷大，*2还是无穷大，也return uf即可
     //   如果exp=0,非规格化形式,若frac=0,表示值为0，若frac!=0,表最接近0的那些数,可以统一处理为尾数*2，即frac<<=1，不用担心
     //   如果exp!=0 && exp!=255,规格化形式，只需要把exp+1,即可
     // 曾想过，为什么非规格化且不为0的时候为什么不能是exp+1,毕竟uf*2，不就是阶码+1，但非规格化和规格化一样处理的话
     // exp+1后，就从非规格化变为了规格化，但float最小的非规格化为1.4*e-45，最小的规格化为1.2*e-38,
     // 显然比较小的非规格化按这种方式处理后，一跃变为了规格化，变大了不止两倍。
     unsigned floatScale2(unsigned uf) {
         unsigned exp = (uf & 0x7f800000) >> 23;
         unsigned frac = uf & 0x7fffff;
         unsigned sign = uf >> 31 & 1;
         unsigned res;
         if(exp == 0xff) {
             return uf;
         }else if(exp == 0){
             frac <<= 1;
             res = sign << 31 | frac;    // 本应还要与上exp<<23，但此时exp全0就每必要了
         }else{
             ++exp;
             res = sign << 31 | exp << 23 | frac;
         }
         return res;
     }
     ```

     

12.  floatFloat2Int

     ```c
     /* 
      * floatFloat2Int - Return bit-level equivalent of expression (int) f
      *   for floating point argument f.
      *   Argument is passed as unsigned int, but
      *   it is to be interpreted as the bit-level representation of a
      *   single-precision floating point value.
      *   Anything out of range (including NaN and infinity) should return
      *   0x80000000u.
      *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
      *   Max ops: 30
      *   Rating: 4
      */
     // 将所给浮点数（无符号形式）强转为int型
     // 如果原浮点数值为0，return 0,
     // 若指数大于31，溢出，返回0x80000000
     // 若指数小于0，返回0
     // 否则，把小数部分看为整数(即相当于小数部分整体左移了23位），再根据exp的大小，对小数部分进行最终调整
     //    若exp>23,还得左移exp-23,若exp<23，说明左移多了，还得右移23-exp位
     //    最后根据符号位进行正负调整，返回即可
     //
     int floatFloat2Int(unsigned uf) {
         int sign = uf >> 31 & 1;    // 取出符号位
         int exp = ((uf & 0x7f800000) >> 23) - 127;  // 指数
         int frac = (uf & 0x007fffff) | 0x00800000;  // 小数，且加上了1
         // 若uf为0或指数小于0,返回0
         if(!(uf & 0x7fffffff) || exp<0) return 0;
         // 若指数大于31，溢出
         if(exp >= 31) 
             return 0x80000000;
         else if(exp > 23)  // 右移少了
             frac <<= exp - 23;
         else            // 右移多了
             frac >>= 23-exp;
         
         if(sign) frac = -frac;
     
         return frac;
     }
     ```

     

13.  floatPower2

     ```c
     /* 
      * floatPower2 - Return bit-level equivalent of the expression 2.0^x
      *   (2.0 raised to the power x) for any 32-bit integer x.
      *
      *   The unsigned value that is returned should have the identical bit
      *   representation as the single-precision floating-point number 2.0^x.
      *   If the result is too small to be represented as a denorm, return
      *   0. If too large, return +INF.
      * 
      *   Legal ops: Any integer/unsigned operations incl. ||, &&. Also if, while 
      *   Max ops: 30 
      *   Rating: 4
      */
     // 计算2.0^x
     // 浮点数标准用V=（-1)^s * M * 2^E
     // 根据浮点数表示，x就是exp部分
     // 根据int的范围进行讨论即可
     // 单精度exp占8位，阶码E=e-Bias(2^7-1) ,即范围是[-126,127]
     // 若 x>=128，超过所能表示的最大值了，+INF
     // 若 >=-127, 在指数表示范围内，直接放入exp部分
     // 若 x>=-150，exp部分能放127，frac有23位，根据标点数frac的含义，也可以用来表示，所以只要不小于150,都可以表示出来
     // 若 x<150, 超出最小的表示范围了,return 0
     unsigned floatPower2(int x) {
         if(x >= 128) return 0x7f800000;
         if(x >= -126) return (x + 127) << 23;
         if(x >= -150) return 1 << (x+150);
         return 0;
     }
     
     ```

     上方所有函数经测试结果如下

     ![](CSAPP%20Data%20Lab.assets/Snipaste_2022-08-06_23-09-47.png)

## note

![](CSAPP%20Data%20Lab.assets/tow-complement.png)

![](CSAPP%20Data%20Lab.assets/tow-complement-add.png)

![](CSAPP%20Data%20Lab.assets/tow-complement-non.png)

![](CSAPP%20Data%20Lab.assets/float.png)

![](CSAPP%20Data%20Lab.assets/float1.png)