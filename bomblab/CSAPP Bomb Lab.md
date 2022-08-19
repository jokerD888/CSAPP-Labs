# CSAPP Bomb Lab

bomb lab给了我们一个bomb的可执行文件，以及一个bomb.c的源文件，不过这个文件只是程序的逻辑逻辑框架，无法编译。进入bomb.c可以看到程序的流程是有6个phase，先读取一行输入，再进入phase判断输入是否正确，正确就可以进入下一关，否则炸弹就会爆炸。
		我们需要用反汇编，将可执行文件反汇编为汇编文件，使用<code>objdump -d bomb > bomb.s</code>得到bomb.s文件。

对于我们的输入，我们将标准输入重定向到文件中去，这样我们就不用每次重复同样的工作，这里使用answer.txt作为输入文件。

我们需要分析汇编代码，可能还需要gdb的调试，打断点，查看内存地址中的值，还要能清楚一些主要寄存器的作用，还有各种常见指令的用法及作用。

本lab中调试用到的gdb指令有：

-   <code>gdb bomb</code>使用gdb调试bomb
-   <code>r < answer.txt</code>重定向标准输入到文件，或用<code>r answer.txt</code>，这是由于main可以指定输入文件。
-   <code>b *0x4015e0</code>在0x4015e0设置断点
-   <code>x/s 0x402400 </code>以字符串形式打印0x402400地址处的值
-   <code>x 0x6032d0</code> 默认以十六进制打印0x6032d0地址处的值，更多gdb之x命令用法，[点这里](https://blog.csdn.net/allenlinrui/article/details/5964046)

## phase_1

phase_1汇编代码如下

```assembly
0000000000400ee0 <phase_1>:
  400ee0:	48 83 ec 08          	sub    $0x8,%rsp    ;栈指针减小，分配栈帧
  400ee4:	be 00 24 40 00       	mov    $0x402400,%esi   ;将0x402400放到寄存器esi中
  400ee9:	e8 4a 04 00 00       	callq  401338 <strings_not_equal>   ;调用函数strigns_not_equal,其中第一个参数为phase_1的参数input,第二个参数为0x402400(%esi)
  400eee:	85 c0                	test   %eax,%eax              ; test用来检查%eax是正数负数还是零
  400ef0:	74 05                	je     400ef7 <phase_1+0x17>   ; 是0就跳转到0x4003f7
  400ef2:	e8 43 05 00 00       	callq  40143a <explode_bomb>    ; 否则，调用expolde_bomb,通过函数名,可以得知要使炸弹爆炸
  400ef7:	48 83 c4 08          	add    $0x8,%rsp          ; 栈指针增加，释放栈帧
  400efb:	c3                   	retq   
```

代码中分号后面的内容即为该行注释，仅通过phase_1函数我们大概可以猜出，是要比较我们的input和地址0x402400处的字符串是否相等，所以不妨打印一下0x402400处的地址，通过gdb x/s 0x402400 来打印该字符串。

![](E:/csapp/phase_1.png)

看来结果显然易见了，我们只需要输入Border relations with Canada have never been better. 这句话，第一关就可以过了。

如果仅凭调用pahse_1不能说服你的话，我们不妨进入strings_not_equal看一看是否如我们所想。

```assembly
0000000000401338 <strings_not_equal>:           ; string_not_equal(input,0x402400) 参一：rdi  参二：rsi 
  401338:	41 54                	push   %r12
  40133a:	55                   	push   %rbp
  40133b:	53                   	push   %rbx
  40133c:	48 89 fb             	mov    %rdi,%rbx        ; rdi -> rbx
  40133f:	48 89 f5             	mov    %rsi,%rbp        ; rsi -> rbp
  401342:	e8 d4 ff ff ff       	callq  40131b <string_length>     ; 计算input字符串的长度
  401347:	41 89 c4             	mov    %eax,%r12d
  40134a:	48 89 ef             	mov    %rbp,%rdi        ; rbp -> rdi
  40134d:	e8 c9 ff ff ff       	callq  40131b <string_length>     ; 计算M[0x402400]字符串的长度
  401352:	ba 01 00 00 00       	mov    $0x1,%edx
  401357:	41 39 c4             	cmp    %eax,%r12d
  40135a:	75 3f                	jne    40139b <strings_not_equal+0x63>    
  40135c:	0f b6 03             	movzbl (%rbx),%eax      ; M[rbx] -> eax    参一
  40135f:	84 c0                	test   %al,%al
  401361:	74 25                	je     401388 <strings_not_equal+0x50>
  401363:	3a 45 00             	cmp    0x0(%rbp),%al    ; 比较 M[rbp] 和 al 的值，也就是比较0x402400处的值和input的值
  401366:	74 0a                	je     401372 <strings_not_equal+0x3a>
  401368:	eb 25                	jmp    40138f <strings_not_equal+0x57>
  40136a:	3a 45 00             	cmp    0x0(%rbp),%al    ; 比较 M[rbp] 和 al 的值，也就是比较0x402400处的值和input的值
  40136d:	0f 1f 00             	nopl   (%rax)
  401370:	75 24                	jne    401396 <strings_not_equal+0x5e>
  401372:	48 83 c3 01          	add    $0x1,%rbx		; 准备比较下一个
  401376:	48 83 c5 01          	add    $0x1,%rbp		; 准备比较下一个
  40137a:	0f b6 03             	movzbl (%rbx),%eax
  40137d:	84 c0                	test   %al,%al			; al所表示字符不为’\0'
  40137f:	75 e9                	jne    40136a <strings_not_equal+0x32>	; 跳上去，再比较一番
  401381:	ba 00 00 00 00       	mov    $0x0,%edx
  401386:	eb 13                	jmp    40139b <strings_not_equal+0x63>
  401388:	ba 00 00 00 00       	mov    $0x0,%edx
  40138d:	eb 0c                	jmp    40139b <strings_not_equal+0x63>
  40138f:	ba 01 00 00 00       	mov    $0x1,%edx
  401394:	eb 05                	jmp    40139b <strings_not_equal+0x63>
  401396:	ba 01 00 00 00       	mov    $0x1,%edx
  40139b:	89 d0                	mov    %edx,%eax
  40139d:	5b                   	pop    %rbx
  40139e:	5d                   	pop    %rbp
  40139f:	41 5c                	pop    %r12
  4013a1:	c3                   	retq   
```

对于上strings_not_equal的汇编代码我们关系参一参二，即寄存器rdi,rsi这两种寄存器的相关操作，可以发现正如我们所想，观察各种跳转指令的的目的地址，可以发现strings_not_equal此函数的大致流程是逐字符比较，若相同就再比较下一个字符。也有不少代码是比较长度的。

### answer

```c
Border relations with Canada have never been better.
```

## phase_2

phase_2汇编代码如下

```assembly
0000000000400efc <phase_2>:         
  400efc:	55                   	push   %rbp
  400efd:	53                   	push   %rbx
  400efe:	48 83 ec 28          	sub    $0x28,%rsp                   ; rsp-40     栈中开辟了40字节内存，可以认为开了个数组，设此时栈指针的值为a，通过后续指令cmpl,eax等可以发现，数组的类型是int
  400f02:	48 89 e6             	mov    %rsp,%rsi                    ; rsp-> rsi  第二参数寄存器指向栈指针
  400f05:	e8 52 05 00 00       	callq  40145c <read_six_numbers>    ; 调用函数read_six_numbers(input,rsi)，此函数作用为读取6个数字到数组中
  400f0a:	83 3c 24 01          	cmpl   $0x1,(%rsp)                  ; a[0]是否为1
  400f0e:	74 20                	je     400f30 <phase_2+0x34>        ; 为1
  400f10:	e8 25 05 00 00       	callq  40143a <explode_bomb>        ; 否则爆炸
  400f15:	eb 19                	jmp    400f30 <phase_2+0x34>
  400f17:	8b 43 fc             	mov    -0x4(%rbx),%eax              ; [rbx-4] -> eax   即eax=a[0]
  400f1a:	01 c0                	add    %eax,%eax                    ; eax+=eax          
  400f1c:	39 03                	cmp    %eax,(%rbx)                  ; 比较eax和[rbx]   即 2*a[0]==a[1]
  400f1e:	74 05                	je     400f25 <phase_2+0x29>        ; 相等
  400f20:	e8 15 05 00 00       	callq  40143a <explode_bomb>        ; 否则爆炸
  400f25:	48 83 c3 04          	add    $0x4,%rbx                    ; rbx+4            
  400f29:	48 39 eb             	cmp    %rbp,%rbx                    ; 比较rbp和rbx    即比较当前位置是否到了rbp
  400f2c:	75 e9                	jne    400f17 <phase_2+0x1b>        ; 不等，跳上去，再迭代，注释中的变量即为第一次迭代
  400f2e:	eb 0c                	jmp    400f3c <phase_2+0x40>        ; 相等，直接跳到结尾
  400f30:	48 8d 5c 24 04       	lea    0x4(%rsp),%rbx               ; rsp+4 -> rbx
  400f35:	48 8d 6c 24 18       	lea    0x18(%rsp),%rbp              ; rsp+24 -> rbp
  400f3a:	eb db                	jmp    400f17 <phase_2+0x1b>
  400f3c:	48 83 c4 28          	add    $0x28,%rsp
  400f40:	5b                   	pop    %rbx
  400f41:	5d                   	pop    %rbp
  400f42:	c3                   	retq   
```

我们发现，刚进入函数开了40字节内存，且第二参数指向栈指针后，就调用了read_six_numbers，通过函数名字，我们大概可以猜到是读6个数字到数组中。这个read_six_numbers函数放到后面将。然后一步一步从上往下模拟过程可以发现，最开始要求a[0]=1,随后的过程中要求每个元素是前一个的2倍，直到“指针”指向a[6]。也就是说我们只要按次要求输入：1 2 4 8 16 32 64即可。ok,运行程序，输入答案，Keep going！值得一提的是，最少需要6个数字且这前6个数字是固定的，之后你想输入几个输入几个，想输入几输入几，因为它只读前6个。

汇编代码流程C语言形式大致如下：

```c
void phase_2(char* input){			// 不关心它返回的啥，认为void就行
   	int a[8];						// 40字节
	read_six_numbers(input,a);		// 读6个数字
    if(a[0]!=1) explode_bomb();		// a[0]!=1就爆炸
    int* l=a+1,*r=a+6;				// 循环判断，每个元素是不是前一个的2倍，不是就爆炸
    while(l!=r){
        int num=l-1;
        num+=num;
        if(num!=a[l]) explode_bomb();
        ++l;
    }
}

```

现在来看read_six_numbers汇编代码的具体内容：

```assembly
000000000040145c <read_six_numbers>:                          ; 调用函数read_six_numbers(char* input,rsi)
  40145c:	48 83 ec 18          	sub    $0x18,%rsp              ; rep-=24     开24字节空间,大小形如long long[3]; 
  401460:	48 89 f2             	mov    %rsi,%rdx               ; rsi -> rdx     a[0]        rdx=&a[0]; 
  401463:	48 8d 4e 04          	lea    0x4(%rsi),%rcx          ; rsi+4 -> rcx   a[1]        rcx=&a[1];
  401467:	48 8d 46 14          	lea    0x14(%rsi),%rax         ; rsi+20 -> rax  a[5]        rax=&a[5];
  40146b:	48 89 44 24 08       	mov    %rax,0x8(%rsp)          ; rax -> rsp+8   b[2]        b[1]=rax;  
  401470:	48 8d 46 10          	lea    0x10(%rsi),%rax         ; rsi+16 -> rax  a[4]        rax=&a[4];
  401474:	48 89 04 24          	mov    %rax,(%rsp)             ; rax -> rsp     b[0]        b[0]=rax;
  401478:	4c 8d 4e 0c          	lea    0xc(%rsi),%r9           ; rsi+12 -> r9   a[3]        r9=&a[3];
  40147c:	4c 8d 46 08          	lea    0x8(%rsi),%r8           ; rsi+8 -> r8    a[2]        r8=&a[2];
  401480:	be c3 25 40 00       	mov    $0x4025c3,%esi          ; 0x4025c3 ->esi
  401485:	b8 00 00 00 00       	mov    $0x0,%eax               ; 0 -> eax
  40148a:	e8 61 f7 ff ff       	callq  400bf0 <__isoc99_sscanf@plt>
  40148f:	83 f8 05             	cmp    $0x5,%eax               ; 比较sscanf返回值和5,sscanf的返回值是读取到的元素个数
  401492:	7f 05                	jg     401499 <read_six_numbers+0x3d>   ; 若读到的个数>5
  401494:	e8 a1 ff ff ff       	callq  40143a <explode_bomb>            ; 否则爆炸
  401499:	48 83 c4 18          	add    $0x18,%rsp
  40149d:	c3                   	retq   
```

我们发现，a数组前6个地址，每个4字节，通过寄存器和栈内容传到了sscanf中去，我们知道sscanf的作用是读取格式化的字符串中的数据，且返回值是读取元素的个数。通过cmp    $0x5,%eax可以发现，若返回个数<=5就会引爆炸弹。所以我们至少要输入6个元素。

###   answer

```c
1 2 4 8 16 32 64
```

## phase_3

phase_3汇编代码如下

```assembly
0000000000400f43 <phase_3>:
  400f43:	48 83 ec 18          	sub    $0x18,%rsp                   ; rsp-=24   ,
  400f47:	48 8d 4c 24 0c       	lea    0xc(%rsp),%rcx               ; rcx = rsp+12
  400f4c:	48 8d 54 24 08       	lea    0x8(%rsp),%rdx               ; rdx = rsp+8
  400f51:	be cf 25 40 00       	mov    $0x4025cf,%esi               ; esi = 0x4025cf
  400f56:	b8 00 00 00 00       	mov    $0x0,%eax                    ; eax = 0
  400f5b:	e8 90 fc ff ff       	callq  400bf0 <__isoc99_sscanf@plt> ; 读第一个数到rep+8,读第二个数到rsp+12
  400f60:	83 f8 01             	cmp    $0x1,%eax                    ; comp(1,eax)
  400f63:	7f 05                	jg     400f6a <phase_3+0x27>        ; 若eax>1   即输入个数需要大于1
  400f65:	e8 d0 04 00 00       	callq  40143a <explode_bomb>        ; 否则爆炸
  400f6a:	83 7c 24 08 07       	cmpl   $0x7,0x8(%rsp)               ; comp(7,rsp+8)     
  400f6f:	77 3c                	ja     400fad <phase_3+0x6a>        ; 若大于（无符号），跳过去爆炸， 说明第一个数要小于等于7
  400f71:	8b 44 24 08          	mov    0x8(%rsp),%eax               ; eax = [rsp+8]
  400f75:	ff 24 c5 70 24 40 00 	jmpq   *0x402470(,%rax,8)           ; 跳到[0x402470+[rax]*8],这里应该是个地址，不妨打印出来看看，用rax等于1带入，结果为400fb9,果然是下面的某个指令的地址
  400f7c:	b8 cf 00 00 00       	mov    $0xcf,%eax                   ; eax=0
  400f81:	eb 3b                	jmp    400fbe <phase_3+0x7b>
  400f83:	b8 c3 02 00 00       	mov    $0x2c3,%eax                  ; eax=2
  400f88:	eb 34                	jmp    400fbe <phase_3+0x7b>
  400f8a:	b8 00 01 00 00       	mov    $0x100,%eax                  ; eax=3
  400f8f:	eb 2d                	jmp    400fbe <phase_3+0x7b>
  400f91:	b8 85 01 00 00       	mov    $0x185,%eax                  ; eax=4
  400f96:	eb 26                	jmp    400fbe <phase_3+0x7b>
  400f98:	b8 ce 00 00 00       	mov    $0xce,%eax                   ; eax=5
  400f9d:	eb 1f                	jmp    400fbe <phase_3+0x7b>
  400f9f:	b8 aa 02 00 00       	mov    $0x2aa,%eax                  ; eax=6
  400fa4:	eb 18                	jmp    400fbe <phase_3+0x7b>
  400fa6:	b8 47 01 00 00       	mov    $0x147,%eax                  ; eax=7
  400fab:	eb 11                	jmp    400fbe <phase_3+0x7b>
  400fad:	e8 88 04 00 00       	callq  40143a <explode_bomb>
  400fb2:	b8 00 00 00 00       	mov    $0x0,%eax
  400fb7:	eb 05                	jmp    400fbe <phase_3+0x7b>
  400fb9:	b8 37 01 00 00       	mov    $0x137,%eax                  ; eax = 1
  400fbe:	3b 44 24 0c          	cmp    0xc(%rsp),%eax               ; cmp([rsp+12],eax)
  400fc2:	74 05                	je     400fc9 <phase_3+0x86>        ; 相等的话，跳过去结束。说明第二个数要等于eax
  400fc4:	e8 71 04 00 00       	callq  40143a <explode_bomb>
  400fc9:	48 83 c4 18          	add    $0x18,%rsp
  400fcd:	c3                   	retq   
```

老样子从上往下一步一步模拟，我们可以发现sscanf读入了两个数，第一个数存到了[rsp+8]，第而个数存到了[rsp+12]，而后我们发现第一个数不能大于7，也就是说第一个输入的数需要在[0,7]之间。又通过jmpq   *0x402470(,%rax,8)我们可以发现，是要根据第一个数的值跳到某处去，我们发现0x402470这个值汇编代码中根本没有，所以我们大概可以猜测需要用gdb将其打印出来，再做进一步打算。在gdb中使用x/8xb 0x402470+8（我们这里设rax=1)，结果为400fb9，恰好对应mov    $0x137,%eax 这行代码，所以我们可以认为通过jmpq   *0x402470(,%rax,8) 跳过去后会根据rax的值再跳回来到某个位置。接着在看中间有很多步相同的操作，都是对eax赋了某些值，然后跳过cmp    0xc(%rsp),%eax进行比较，之后就一目了然了，通过对比判断炸弹爆炸与否。

rax取值0到7打印结果如下：

![](CSAPP%20Bomb%20Lab.assets/phase_3.png)

综上，总流程为，读取两个数，然后根据第一个数，跳到不同的位置比较第二个数，若相同，Halfway there!，否则爆炸。

### answer

答案的话，共有8组，对应的组合已经标到代码注释中了，这里就以第一个数为1为例。

```c
1 311
```

## phase_4

phase_4汇编代码如下

```assembly
000000000040100c <phase_4>:
  40100c:	48 83 ec 18          	sub    $0x18,%rsp                     ; rsp-=24
  401010:	48 8d 4c 24 0c       	lea    0xc(%rsp),%rcx                 ; rcx = rsp+12   
  401015:	48 8d 54 24 08       	lea    0x8(%rsp),%rdx                 ; rdx = rsp+8
  40101a:	be cf 25 40 00       	mov    $0x4025cf,%esi                 ; esi = 0x4025cf 
  40101f:	b8 00 00 00 00       	mov    $0x0,%eax                      ; eax = 0
  401024:	e8 c7 fb ff ff       	callq  400bf0 <__isoc99_sscanf@plt>   ; 第一个数据读到rdx=rsp+8,第二个参数读到rcx =rsp+12
  401029:	83 f8 02             	cmp    $0x2,%eax                      ; comp(2,eax)
  40102c:	75 07                	jne    401035 <phase_4+0x29>          ; 若不等，跳过去爆炸，说明只能输入两个数据
  40102e:	83 7c 24 08 0e       	cmpl   $0xe,0x8(%rsp)                 ; comp(14,[rsp+8])
  401033:	76 05                	jbe    40103a <phase_4+0x2e>          ; 若[rsp+8]<=14,跳过去  ，第一个输入数据需要<=14
  401035:	e8 00 04 00 00       	callq  40143a <explode_bomb>          ; 否则爆炸
  40103a:	ba 0e 00 00 00       	mov    $0xe,%edx                      ; edx = 14      参三
  40103f:	be 00 00 00 00       	mov    $0x0,%esi                      ; esi = 0       参二
  401044:	8b 7c 24 08          	mov    0x8(%rsp),%edi                 ; edi = [rsp+8] 参一
  401048:	e8 81 ff ff ff       	callq  400fce <func4>                 ; 调用func4，参数如上所示
  40104d:	85 c0                	test   %eax,%eax                      ; eax==0
  40104f:	75 07                	jne    401058 <phase_4+0x4c>          ; 不等，跳过去爆炸，说明返回值需要是0
  401051:	83 7c 24 0c 00       	cmpl   $0x0,0xc(%rsp)                 ; comp(0,[rsp+12])
  401056:	74 05                	je     40105d <phase_4+0x51>          ; 相等，跳过去，结束，否则爆炸，第二个输入数据经过func4为需要0
  401058:	e8 dd 03 00 00       	callq  40143a <explode_bomb>          
  40105d:	48 83 c4 18          	add    $0x18,%rsp
  401061:	c3                   	retq   
```

看上方代码，前面依然是从输入读取数据，通过cmp    $0x2,%eax 可以发现，我们只能输入两个整数，然后cmpl   $0xe,0x8(%rsp)则要求第一个数要<=14，然后是三个参数的设置，随后是函数func4的调用，我们先不进去看，看紧接着的返回值判断test   %eax,%eax，发现fun4的返回值必须要等于0才可以通过，否则爆炸。所以此时我们得出的信息有：输入的一个数要<=14，第二个数要为0，且调用的fun4d的返回值要为0。ok，那么接下来让我们进入fun4来一探究竟。

```assembly
0000000000400fce <func4>:                                ; func4(a=[rsp+8],b=0,c=014) 设输入的第一个数据为14，即a=14
  400fce:	48 83 ec 08          	sub    $0x8,%rsp                    ; rsp-=8 右边的等式是假设第一个数为14后的结果
  400fd2:	89 d0                	mov    %edx,%eax                    ; eax = edx(14)         eax=14
  400fd4:	29 f0                	sub    %esi,%eax                    ; eax -= esi(0)         eax=14  
  400fd6:	89 c1                	mov    %eax,%ecx                    ; ecx = eax             ecx=14  
  400fd8:	c1 e9 1f             	shr    $0x1f,%ecx                   ; ecx >>= 31 （逻辑右移） ecx=0  
  400fdb:	01 c8                	add    %ecx,%eax                    ; eax += ecx            eax=14  
  400fdd:	d1 f8                	sar    %eax                         ; 只有一个操作数是省略了1, 即eax>>=1  eax=7 
  400fdf:	8d 0c 30             	lea    (%rax,%rsi,1),%ecx           ; ecx = [rax]+[rsi]                 ecx=7 
  400fe2:	39 f9                	cmp    %edi,%ecx                    ; comp(edi,ecx)                comp(14,7)
  400fe4:	7e 0c                	jle    400ff2 <func4+0x24>          ; 若 ecx <= edi,即 ecx<=a     成立。跳过去
  400fe6:	8d 51 ff             	lea    -0x1(%rcx),%edx
  400fe9:	e8 e0 ff ff ff       	callq  400fce <func4>               ; edi esi edx
  400fee:	01 c0                	add    %eax,%eax
  400ff0:	eb 15                	jmp    401007 <func4+0x39>
  400ff2:	b8 00 00 00 00       	mov    $0x0,%eax                    ; eax = 0
  400ff7:	39 f9                	cmp    %edi,%ecx                    ; cmp(edi,ecx)    cmp(14,7)
  400ff9:	7d 0c                	jge    401007 <func4+0x39>          ; 若ecx >= edi ,跳过去结束。但我们假设的14不成立，应该假设为7才能成立跳过去，结束，且返回0,说明第一个输入数据需要为7
  400ffb:	8d 71 01             	lea    0x1(%rcx),%esi               ; esi = rcx+1
  400ffe:	e8 cb ff ff ff       	callq  400fce <func4>               ; fun4(a=[rsp+8],8,14)
  401003:	8d 44 00 01          	lea    0x1(%rax,%rax,1),%eax
  401007:	48 83 c4 08          	add    $0x8,%rsp
  40100b:	c3                   	retq   
```

经过一行一行注释，不难将其“翻译”为C语言代码，翻译大概的形式在下方。func4函数主要return 0的流程要经过如下4条汇编代码，我们发现edi既要<=ecx又要>=ecx，所以只能是edi=ecx，而ecx经过计算为7，也就是edi我们的第一个参数，即我们要输入的第一个数要为7，至此答案浮出水面：7 0  ,7和0就是我们要输入的两个数。但是别急，总感觉这样做不能说服自己，虽然也对，那么通过进一步研究，发现还有其他的答案。先看下方“翻译”简化后的C语言代码，可以发现果然还有其他答案，详细分析见注释。

```assembly
...
cmp    %edi,%ecx
jle    400ff2 <func4+0x24>
...
cmp    %edi,%ecx
jge    401007 <func4+0x39>
...
```

“翻译”如下：

```c
int func4(int a, int esi = 0, int edx = 14) {
    int eax = edx;
    eax -= esi;
    int ecx = eax;
    ecx >>= 31;
    eax += ecx;
    eax >>= 1;
    ecx = eax + esi;
    if (ecx <= a) {
        eax = 0;
        if (ecx >= a)
            return eax;			// 很明显，这里会返回0
        else {
            esi = ecx + 1;
            eax = func4(a, esi, edx);
            eax = 1 + eax + eax;
            return eax;
        }
    } else {
        edx = ecx - 1;
        eax = func4(a, esi, edx);
        eax += eax;
        return eax;
    }
}
// 进一步重命名和简化后
int func4(int a, int first = 0, int second = 14) {
    int b = second - first;     
    int c = b >> 31;        
    b += c;             
    b >>= 1;            
    c = b + first;      
    if (c <= a) {
        b = 0;
        if (c >= a)
            return b;			// 进入这里，明显的返回0
        else {
            first = c + 1;      
            b = func4(a, first, second);
            b = 1 + b + b;		// 可以看出若函数进入这里绝对返回的不是0
            return b;
        }
    } else {
        second = c - 1;
        b = func4(a, first, second);		// fun4(a,0,6)
       // 那么进入这里呢，只要上一步的函数调用返回0，这里也会返回0。若要进入此步，要求c>a,即答案第一个数要小于7，此时上方函数调用为func4(a,0,6)，模拟进入函数我们发现，若通过if (c >= a) return b;进行返回0，那么a就要为3。所以又一个答案为（3，0）。若还通过此位置返回0，那么还需要上方再调用一个fun4，那么又一个答案为（1，0），以及更深的（0，0），共计4个答案，（7，0），（3，0），（1，0），（0，0）
        b += b;
        return b;
    }
}
```

综上，代码流程为：读取两个数字，若读到的数字个数不为2（但实际输入个数大于2也正确），explode_bomb。进入函数func4，func4主要逻辑为取first和second的中间值c，判断c和a的大小，进入不同分支，但只要求最终结果返回0即可。

### answer

共四组答案（7，0），（3，0），（1，0），（0，0），我们任选一组即可

```c
7 0 
```

## phase_5

还有两关，离胜利不远了。phase_5汇编代码如下：

```assembly
0000000000401062 <phase_5>:
  401062:	53                   	push   %rbx          
  401063:	48 83 ec 20          	sub    $0x20,%rsp                     ; rsp-=32
  401067:	48 89 fb             	mov    %rdi,%rbx                      ; rbx = rdi     rdi为input的字符串的首地址
  40106a:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax                  ; 存储金丝雀值（fs:40是用段寻址机制从内存读入）
  401071:	00 00 
  401073:	48 89 44 24 18       	mov    %rax,0x18(%rsp)                ; [rsp+24]=rax  从内存中读到的金丝雀值存到了rsp+24处
  401078:	31 c0                	xor    %eax,%eax                      ; eax置为0
  40107a:	e8 9c 02 00 00       	callq  40131b <string_length>         ; 调用string_length
  40107f:	83 f8 06             	cmp    $0x6,%eax                      ; 返回值和6进行比较
  401082:	74 4e                	je     4010d2 <phase_5+0x70>          ; 相等，跳过去
  401084:	e8 b1 03 00 00       	callq  40143a <explode_bomb>          ; 否则，爆炸，说明我们要输入的字符长度为6
  401089:	eb 47                	jmp    4010d2 <phase_5+0x70>
  40108b:	0f b6 0c 03          	movzbl (%rbx,%rax,1),%ecx             ; ecx = [rbx + rax]
  40108f:	88 0c 24             	mov    %cl,(%rsp)                     ; [rsp]=cl    
  401092:	48 8b 14 24          	mov    (%rsp),%rdx                    ; rdx = [rsp]
  401096:	83 e2 0f             	and    $0xf,%edx                      ; rdx &= 15
  401099:	0f b6 92 b0 24 40 00 	movzbl 0x4024b0(%rdx),%edx            ; rdx = 0x4024b0+rdx
  4010a0:	88 54 04 10          	mov    %dl,0x10(%rsp,%rax,1)          ; [16+rsp+rax]=dl
  4010a4:	48 83 c0 01          	add    $0x1,%rax                      ; rax += 1
  4010a8:	48 83 f8 06          	cmp    $0x6,%rax                      ; comp(6,rax);
  4010ac:	75 dd                	jne    40108b <phase_5+0x29>          ; 不等，跳过去
  4010ae:	c6 44 24 16 00       	movb   $0x0,0x16(%rsp)                ; [rsp+22]=0
  4010b3:	be 5e 24 40 00       	mov    $0x40245e,%esi                 ; esi = 0x40245e
  4010b8:	48 8d 7c 24 10       	lea    0x10(%rsp),%rdi                ; rdi = [rsp+16] 
  4010bd:	e8 76 02 00 00       	callq  401338 <strings_not_equal>     ; 调strings_not_equal
  4010c2:	85 c0                	test   %eax,%eax                      
  4010c4:	74 13                	je     4010d9 <phase_5+0x77>          ; 若返回0，跳过去
  4010c6:	e8 6f 03 00 00       	callq  40143a <explode_bomb>          ; 否则，爆炸
  4010cb:	0f 1f 44 00 00       	nopl   0x0(%rax,%rax,1)               ; 空指令，啥也不干
  4010d0:	eb 07                	jmp    4010d9 <phase_5+0x77>
  4010d2:	b8 00 00 00 00       	mov    $0x0,%eax                      ; eax = 0
  4010d7:	eb b2                	jmp    40108b <phase_5+0x29>
  4010d9:	48 8b 44 24 18       	mov    0x18(%rsp),%rax                ; rax = [rsp+24]
  4010de:	64 48 33 04 25 28 00 	xor    %fs:0x28,%rax                  ; 判断金丝雀值
  4010e5:	00 00 
  4010e7:	74 05                	je     4010ee <phase_5+0x8c>
  4010e9:	e8 42 fa ff ff       	callq  400b30 <__stack_chk_fail@plt>
  4010ee:	48 83 c4 20          	add    $0x20,%rsp
  4010f2:	5b                   	pop    %rbx
  4010f3:	c3                   	retq   
```

先从上往下看，在调用string_length前，有两步是做金丝雀值的存储，不过无关紧要。string_length调用后cmp    $0x6,%eax以及其后两行，我们发现，输入的字符串需要长度为6，好，这是获取的第一个重要信息。再看movzbl (%rbx,%rax,1),%ecx 这句是把input[rax]的值赋给ecx，第一次执行这句的时候rax=0，随后的mov    %cl,(%rsp)以及mov    (%rsp),%rdx则是吧ecx的值赋给了rdx，然后是and    $0xf,%edx，即 edx &= 15（这句刚开始将and看成了add，迷了好久😑），这说明与15与后，eax的值在0~15。随后movzbl 0x4024b0(%rdx),%edx ，即edx = [rdx + 0x4024b0]。然后mov    %dl,0x10(%rsp,%rax,1) 则是将dl即刚才的edx赋给了rsp[16+rax]。所以以上几步主要是从input[rax]的值与上15后，再加上0x4024b0得到新地址，随后取出地址中的值放到rsp[16+rax]中去。接下来的add    $0x1,%rax，即rax++，然后cmp    $0x6,%rax，判断rax是否等于6，不然再跳上去，否则，movb  $0x0,0x16(%rsp)，即rsp[22]=0，然后进行strings_not_equal（rsp+16，(char*)0x40245e) 进行字符串比较，若相等，返回，退出phase_5，否则爆炸。

好，既让要rdx+0x4024b0地址处的字符串与0x40245e地址处的字符串进行比较，那么打印出来看看呗

![](CSAPP%20Bomb%20Lab.assets/phase_5-16606604234602.png)

仔细观察So前面的无序字符串，唉，恰好有16个字符，而刚好eax的值在0~15。所以我们看看flyers这6个字符在无序字符串中的位置，分别是9，15，14，5，6，7。所以我们只要输入的前6个字符的二进制低4位构成的值分别是这6个数字即可。所以答案不唯一，我们直接简单将这6个数字加上64，方便构成字母进行输入。即答案可以为IONEFG，或加上96，得ionefg。

总体流程：首先判断字符串input长度是否为6，若不是explode_bomb，再逐个遍历字符串input即为input[i]，然后将input[i]与上15再加上0x4024b0得到地址处的值放到rsp[16+i]中，遍历完6个字母后，rsp[22]=0即将字符串封尾，最后将rsp+16处的字符串和0x40245e地址处的字符串进行比较，若相同，Good work!  On to the next，否则，爆炸。

最后然我们来看下大概相对应的C语言形式代码

```c
void phase_5(char* input) {
    char* rbx = input;
    char rsp[32];
    int ret = string_lenhth(input);
    if (ret == 6) {
        ret = 0;
        L:
       char ecx = input[ret];
        rsp[0] = ecx;
        char rdx = rsp[0];
        rdx &= 15;
        rdx = *(char*)(0x4024b0 + rdx);
        rsp[16 + ret] = rdx;
        ret += 1;
        if (ret == 6) {
            *(rsp + 22) = 0;
            char* esi = (char*)0x40245e;
            input=rsp[16];
            ret=strings_not_equal(input,esi);
            if(ret==0){
                ret=rsp[24];
                return;
            }else{
                explode_bomb();
            }
        } else {
            goto L;
        }
    } else {
        explode_bomb();
    }
}
```

### answer

```c
IONEFG
```

## p_hase_6

<img src="CSAPP%20Bomb%20Lab.assets/1421baa30f1dd80ed360eb33d136975b.gif" alt="img" style="zoom: 25%;" />

最后一关了，但你想就这，看看关卡前的注释，没错，这关确实有些难，主要是繁琐，需要仔细耐心。

```c
This phase will never be used, since no one will get past the earlier ones.  But just in case, make this one extra hard.
```

上代码

```assembly
00000000004010f4 <phase_6>:
  4010f4:	41 56                	push   %r14
  4010f6:	41 55                	push   %r13
  4010f8:	41 54                	push   %r12
  4010fa:	55                   	push   %rbp              
  4010fb:	53                   	push   %rbx
  4010fc:	48 83 ec 50          	sub    $0x50,%rsp                   ; rsp -= 80
  401100:	49 89 e5             	mov    %rsp,%r13                    ; r13 = rsp
  401103:	48 89 e6             	mov    %rsp,%rsi                    ; rsi = rsp
  401106:	e8 51 03 00 00       	callq  40145c <read_six_numbers>    ; read_six_numbers，这个函数我们之前已经讨论过了，6个数分别读到数组中
  40110b:	49 89 e6             	mov    %rsp,%r14                    ; r14 = rsp
  40110e:	41 bc 00 00 00 00    	mov    $0x0,%r12d                   ; r12d = 0
  401114:	4c 89 ed             	mov    %r13,%rbp                    ; rbp = r13
  401117:	41 8b 45 00          	mov    0x0(%r13),%eax               ; rax = [r13]
  40111b:	83 e8 01             	sub    $0x1,%eax                    ; rax -= 1
  40111e:	83 f8 05             	cmp    $0x5,%eax                    ; comp(5,rax)
  401121:	76 05                	jbe    401128 <phase_6+0x34>        ; 若小于等于5,跳过去,无符号比较，即说明我们输入的6个数的范围为1~6
  401123:	e8 12 03 00 00       	callq  40143a <explode_bomb>        ; 否则爆炸
  401128:	41 83 c4 01          	add    $0x1,%r12d                   ; r12d+=1
  40112c:	41 83 fc 06          	cmp    $0x6,%r12d                   ; comp(6,r12d)
  401130:	74 21                	je     401153 <phase_6+0x5f>        ; 相等跳过去
  401132:	44 89 e3             	mov    %r12d,%ebx                   ;  ebx = r12d
  401135:	48 63 c3             	movslq %ebx,%rax                    ; rax = ebx
  401138:	8b 04 84             	mov    (%rsp,%rax,4),%eax           ; rax =[rsp+rax*4]
  40113b:	39 45 00             	cmp    %eax,0x0(%rbp)               ; comp(rax,[0+rbp])
  40113e:	75 05                	jne    401145 <phase_6+0x51>        ; 如果不等，跳
  401140:	e8 f5 02 00 00       	callq  40143a <explode_bomb>        ; 否则，爆炸
  401145:	83 c3 01             	add    $0x1,%ebx                    ; ebx += 1
  401148:	83 fb 05             	cmp    $0x5,%ebx                    ; comp(5,ebx)
  40114b:	7e e8                	jle    401135 <phase_6+0x41>        ; <= ,跳
  40114d:	49 83 c5 04          	add    $0x4,%r13                    ; r13 += 4
  401151:	eb c1                	jmp    401114 <phase_6+0x20>        ; 跳
  401153:	48 8d 74 24 18       	lea    0x18(%rsp),%rsi              ; rsi = rsp + 24
  401158:	4c 89 f0             	mov    %r14,%rax                    ; rax = r14
  40115b:	b9 07 00 00 00       	mov    $0x7,%ecx                    ; ecx = 7
  401160:	89 ca                	mov    %ecx,%edx                    ; edx = ecx
  401162:	2b 10                	sub    (%rax),%edx                  ; edx -= [rax]
  401164:	89 10                	mov    %edx,(%rax)                  ; [rax] = edx;
  401166:	48 83 c0 04          	add    $0x4,%rax                    ; rax += 4;
  40116a:	48 39 f0             	cmp    %rsi,%rax                    ; comp(rsi,rax)
  40116d:	75 f1                	jne    401160 <phase_6+0x6c>        ; 不等，跳过去
  40116f:	be 00 00 00 00       	mov    $0x0,%esi                    ; esi = 0
  401174:	eb 21                	jmp    401197 <phase_6+0xa3>
  401176:	48 8b 52 08          	mov    0x8(%rdx),%rdx               ; rdx = [8+rdx]
  40117a:	83 c0 01             	add    $0x1,%eax                    ; rax += 1
  40117d:	39 c8                	cmp    %ecx,%eax                    ; comp(ecx,rax)
  40117f:	75 f5                	jne    401176 <phase_6+0x82>        ; 不等，跳
  401181:	eb 05                	jmp    401188 <phase_6+0x94>        ; 跳
  401183:	ba d0 32 60 00       	mov    $0x6032d0,%edx             ; edx = 0x6032d0
  401188:	48 89 54 74 20       	mov    %rdx,0x20(%rsp,%rsi,2)     ; [32+rsp+rsi*2] = rdx
  40118d:	48 83 c6 04          	add    $0x4,%rsi                  ; rsi += 4
  401191:	48 83 fe 18          	cmp    $0x18,%rsi                 ; comp(24,rsi)
  401195:	74 14                	je     4011ab <phase_6+0xb7>      ; 相等，跳过去
  401197:	8b 0c 34             	mov    (%rsp,%rsi,1),%ecx         ; ecx =[rsp+rsi]      
  40119a:	83 f9 01             	cmp    $0x1,%ecx                  ; comp(1,ecx)
  40119d:	7e e4                	jle    401183 <phase_6+0x8f>      ; <= ,跳
  40119f:	b8 01 00 00 00       	mov    $0x1,%eax                  ; rax = 1 
  4011a4:	ba d0 32 60 00       	mov    $0x6032d0,%edx             ; edx = 0x6032d0
  4011a9:	eb cb                	jmp    401176 <phase_6+0x82>      ; 跳
  4011ab:	48 8b 5c 24 20       	mov    0x20(%rsp),%rbx            ; rbx = [32+rsp]
  4011b0:	48 8d 44 24 28       	lea    0x28(%rsp),%rax            ; rax = 40+rsp
  4011b5:	48 8d 74 24 50       	lea    0x50(%rsp),%rsi            ; rsi = rsp + 80
  4011ba:	48 89 d9             	mov    %rbx,%rcx                  ; rcx = rbx
  4011bd:	48 8b 10             	mov    (%rax),%rdx                ; rdx = [rax]
  4011c0:	48 89 51 08          	mov    %rdx,0x8(%rcx)             ; [8+rcx] = rdx
  4011c4:	48 83 c0 08          	add    $0x8,%rax                  ; rax += 8
  4011c8:	48 39 f0             	cmp    %rsi,%rax                  ; comp(rsi,rax)
  4011cb:	74 05                	je     4011d2 <phase_6+0xde>      ; 相等，跳过去
  4011cd:	48 89 d1             	mov    %rdx,%rcx                  ; rcx = rdx
  4011d0:	eb eb                	jmp    4011bd <phase_6+0xc9>      ; 跳
  4011d2:	48 c7 42 08 00 00 00 	movq   $0x0,0x8(%rdx)             ; [rdx+8] = 0
  4011d9:	00 
  4011da:	bd 05 00 00 00       	mov    $0x5,%ebp                  ; rbp = 5
  4011df:	48 8b 43 08          	mov    0x8(%rbx),%rax             ; rax = [8+rbx]
  4011e3:	8b 00                	mov    (%rax),%eax                ; eax = [rax]
  4011e5:	39 03                	cmp    %eax,(%rbx)                ; comp(eax,[rbx])
  4011e7:	7d 05                	jge    4011ee <phase_6+0xfa>      ; 大于等于，跳
  4011e9:	e8 4c 02 00 00       	callq  40143a <explode_bomb>      ; 否则爆炸
  4011ee:	48 8b 5b 08          	mov    0x8(%rbx),%rbx             ; rbx = [8+rbx]
  4011f2:	83 ed 01             	sub    $0x1,%ebp                  ; rbp -= 1
  4011f5:	75 e8                	jne    4011df <phase_6+0xeb>      ; 非零，跳
  4011f7:	48 83 c4 50          	add    $0x50,%rsp                 ; 结束
  4011fb:	5b                   	pop    %rbx
  4011fc:	5d                   	pop    %rbp
  4011fd:	41 5c                	pop    %r12
  4011ff:	41 5d                	pop    %r13
  401201:	41 5e                	pop    %r14
  401203:	c3                   	retq   
```

这次光看汇编代码就想要清楚逻辑流程，可能就比较困难了，主要是各种跳转指定跳来跳去，不能清楚知道跳转流程。但我们还是可以从前面一些部分得知我们需要输入6个数字，且6个数字范围在[1,6]。但后续流程就比较混乱了。 所以这次我们先大概“翻译”为C形式代码，便于梳理逻辑，“翻译”如下：

```c
void phase_6(char* input) {
    int rsp[20];
    int* r13 = rsp;
    read_six_numbers(input, rsp);
    int* r14 = rsp;
    int r12 = 0;
L4:
    int* rbp = r13;
    int rax = *r13;  // rsp[0]~rsp[5]，读入的第一个数字
    rax -= 1;        // 此行及下行，说明输入的6个数字需要在[1,6]范围上
    if (rax <= 5) {		// 无符号比较
   // r12从0开始递增1，即我们输入的前5个数全会进入else,而else语句中则进行判重，判断当前的数和后面的数是否相同，若相同，爆炸
        r12 += 1;
        if (r12 == 6) {
            int* rsi = rsp + 24;  // rsi=rsp[6]
            rax = (int)r14;       // rax=rsp[0]
            int ecx = 7;
        L:
            int edx = ecx;  // 以下4行主要是将我们输入的6个数全部被7减掉，然后再存在原位置
            edx -= *(int*)rax;
            *(int*)rax = edx;
            rax += 4;
            if (rsi == (int*)rax) {  // 6个数被7减完后
                rsi = 0;
            L7:
                ecx = *(rsp + (int)rsi);  // ecx =*(rsp+rsi)
                if (ecx <= 1) {  // 当原始输入的数字为6时，才可以直接进去，但6只能有一个，其余要做else处理
                    edx = 0x6032d0;
                L6:
                    *(32 + rsp + (int)rsi * 2) = edx;  // rsp[8+2*rsi]=edx
                    rsi += 4;  
                    // 最开始rsi是0，每次加4，得循环6次才能进入下面的if，否则入else，else主要是根据ecx的值对edx进行赋值
                    if (24 == (int)rsi) {
                        // 假设我们输入的数为 6 5 4 3 2 1，被7处理后为 1 2 3 4 5 6
                        // 对应的 rsp[8] = 0x6032d0  rsp[10] = 0x6032e0  rsp[12] = 0x6032f0
                        //       rsp[14] = 0x603300  rsp[16] = 0x603310  rsp[18] = 0x603320
                        int rbx = *(32 + rsp);  // rsp[8]
                        rax = 40 + (int)rsp;    // &rsp[10]
                        rsi = 80 + rsp;         // &rsp[20]
                        int rcx = rbx;          // rsp[8]
                    L1:
                        edx = *((int*)rax);      // rsp[10]    初始为0x6032e0
                        *(8 + (int*)rcx) = edx;  // *(rsp[8]+8) = edx  初始为*（0x6032d0+8）=0x6032e0
                        rax += 8;      // 此句要执行5次，才可进if,要通过else跳上来迭代4次，else语句为rcx = edx;
                        // 可以发现，通过上面3句的5遍执行，*（i+8）=i+1 即在6个rsp[i]加8后的新地址上存rsp[i+1]
                        if ((int)rsi == rax) {
                            *(8 + (int*)edx) = 0;
                            rbp = (int*)5;
                        L2:
                            // *rsp[8] = 0x104c *rsp[10] = 0xa8 *rsp[12] = 0x039c
                            // *rsp[14] = 0x02b3 *rsp[16] = 0x01dd *rsp[18] = 0x01bb
                            rax = *(8 + (int*)rbx);
                            rax = *(int*)rax;  // rax的值依次为 0xa8 0x039c 0x02b3 0x01dd   0x01bb
                            if (*(int*)rbx >=rax) {  
                                // if中*rbx的值为014c, 0xa8  0x039c 02b3  0x01dd  ,明显比较均不成立，
                                // 这说明我们假设的6 5 4 3 2 1方案不可行
                                // 我们发现，其实两者是相邻错位进行比较，且比较后两者同时后移动，两两相邻进行比较
                                // 根据我们假设的值得出的结果，进行顺序调整，保证解地址出来的值大小递减即可
                                // 所以结果为 4 3 2 1 6 5  ,6个值均不同，所以结果唯一
                                rbx = *(8 + (int*)rbx);
                                rbp -= 1;
                                if (rbp != 0) {		// 5次比较完，结束
                                    goto L2;
                                } else {
                                    return;				// !!!!!! 唯一的程序出口
                                }
                            } else {
                                explode_bomb();
                            }
                        } else {
                            rcx = edx;
                            goto L1;
                        }
                    } else {
                        goto L6;
                    }
                } else {
                    rax = 1;
                    edx = 0x6032d0;
                L5:
                    // 根据ecx的值进行循环ecx-1次，最多循环5次，不同的循环次数，最终的edx的值也不同
                    // 下句中edx 的更新依次为0x6032e0 0x6032f0 0x603300 0x603310 0x603320
                    // 关于*(8 + (int*)edx)的值可以通过gdb进行打印，可以用指令 x 地址
                    edx = *(8 + (int*)edx);
                    rax += 1;
                    if (ecx == rax) {
                        goto L6;
                    } else {
                        goto L5;
                    }
                }
            } else {
                goto L;
            }
        } else {  // 这个else主要是判重，即要求我们输入的6个数各不相同
            int ebx = r12;
        L3:
            rax = ebx;
            rax = *(rsp + rax * 4);
            if (rax != *(rbp)) {	// rbp不动，rax往后走，逐步进行判重
                ebx += 1;
                if (ebx <= 5) {
                    goto L3;
                } else {
                    r13 += 4;
                    goto L4;
                }
            } else {
                explode_bomb();
            }
        }
    } else {
        explode_bomb();
    }
}
```

通过上方代码中的注释基本就可以明白整个代码流程了，下图是各地址上的值

<img src="CSAPP%20Bomb%20Lab.assets/p_hase6.2.png" style="zoom:80%;" />

```c
// 代码处理完后，给地址值如下，是不是很像链表，结尾甚至有个0。
*(0x6032d0 + 8) = 0x6032e0
*(0x6032e0 + 8) = 0x6032f0
*(0x6032f0 + 8) = 0x603300
*(0x603300 + 8) = 0x603310
*(0x603310 + 8) = 0x603320
*(0x603320 + 8) = 0			// 对应 *(8 + (int*)edx) = 0;
```

![](CSAPP%20Bomb%20Lab.assets/p_hase6.3.png)

总体流程：读入6个数，判断6个数的范围是否在[1,6]之间，然后让7减去这6个数，再存在原位置。之后根据被7处理后的值，对edx选择不同的值，再将edx赋给rsp[8]，rsp[10]，rsp[12]，rsp[14]，rsp[16]。随后”连接节点“，如下，节点连接好后，错位进行比较，前一个要>=后一个，呈递减关系。

```c
*(rsp[8]+8)=rsp[10]
*(rsp[10]+8)=rsp[12]
*(rsp[12]+8)=rsp[14]
*(rsp[14]+8)=rsp[16]
*(rsp[16]+8)=rsp[18]
*(rsp[18]+8)=0
```

### answer

```c
4 3 2 1 6 5
```



## secret_phase

细心的你可能还发现了隐藏阶段，因为汇编代码和源文件中出现了这样的代码和注释

```c
0000000000401204 <fun7>:
0000000000401242 <secret_phase>:
/* Wow, they got it!  But isn't something... missing?  Perhaps
* something they overlooked?  Mua ha ha ha ha! */
```

那么怎么开启这个隐藏关卡呢，或者说入口在哪里，让我们来寻找一番。发现调用secret_phase的只有一个地方，它处于phase_defused函数内，让我们来看看。

```assembly
00000000004015c4 <phase_defused>:
  4015c4:	48 83 ec 78          	sub    $0x78,%rsp
  4015c8:	64 48 8b 04 25 28 00 	mov    %fs:0x28,%rax
  4015cf:	00 00 
  4015d1:	48 89 44 24 68       	mov    %rax,0x68(%rsp)
  4015d6:	31 c0                	xor    %eax,%eax
  4015d8:	83 3d 81 21 20 00 06 	cmpl   $0x6,0x202181(%rip)        # 603760 <num_input_strings>  
  4015df:	75 5e                	jne    40163f <phase_defused+0x7b>  ; 若已近输入的字符串个数等于6才走下面，不然，直接跳过最后
  4015e1:	4c 8d 44 24 10       	lea    0x10(%rsp),%r8
  4015e6:	48 8d 4c 24 0c       	lea    0xc(%rsp),%rcx
  4015eb:	48 8d 54 24 08       	lea    0x8(%rsp),%rdx
  4015f0:	be 19 26 40 00       	mov    $0x402619,%esi               ; 通过打印0x402619得，"%d %d %s"，看来是要输入两个数字加一个字符串
  4015f5:	bf 70 38 60 00       	mov    $0x603870,%edi               ; "" 直接打印发现是空串，同时发现它处于input_sting的内存上，所以需要程序运行起来，有了输入，就会显示相应内容
  4015fa:	e8 f1 f5 ff ff       	callq  400bf0 <__isoc99_sscanf@plt>
  4015ff:	83 f8 03             	cmp    $0x3,%eax                    ; 读到的个数和3比较
  401602:	75 31                	jne    401635 <phase_defused+0x71>  ; 不等于3的话，跳到后面
  401604:	be 22 26 40 00       	mov    $0x402622,%esi               ; esi = 0x402622    "DrEvil"
  401609:	48 8d 7c 24 10       	lea    0x10(%rsp),%rdi              ; edi = 10+rsp   10+rsp就是我们输入的哪个字符串
  40160e:	e8 25 fd ff ff       	callq  401338 <strings_not_equal>   ; 比较字符串是否相等
  401613:	85 c0                	test   %eax,%eax                    
  401615:	75 1e                	jne    401635 <phase_defused+0x71>  ; 若不为0，说明不等，跳后面
  401617:	bf f8 24 40 00       	mov    $0x4024f8,%edi               ; edi = 0x4024f8   "Curses, you've found the secret phase!"
  40161c:	e8 ef f4 ff ff       	callq  400b10 <puts@plt>
  401621:	bf 20 25 40 00       	mov    $0x402520,%edi               ; edi =0x402520    "But finding it and solving it are quite different..
  401626:	e8 e5 f4 ff ff       	callq  400b10 <puts@plt>
  40162b:	b8 00 00 00 00       	mov    $0x0,%eax                    ; eax = 0  
  401630:	e8 0d fc ff ff       	callq  401242 <secret_phase>        ; 终于，调用secret_phase !!!!!!!
  401635:	bf 58 25 40 00       	mov    $0x402558,%edi
  40163a:	e8 d1 f4 ff ff       	callq  400b10 <puts@plt>
  40163f:	48 8b 44 24 68       	mov    0x68(%rsp),%rax
  401644:	64 48 33 04 25 28 00 	xor    %fs:0x28,%rax
  40164b:	00 00 
  40164d:	74 05                	je     401654 <phase_defused+0x90>
  40164f:	e8 dc f4 ff ff       	callq  400b30 <__stack_chk_fail@plt>
  401654:	48 83 c4 78          	add    $0x78,%rsp
  401658:	c3                   	retq   
```

可以看到secret_phase的调用在callq  401242 <secret_phase> 行，那么接下来的任务就是怎样才能让汇编代码执行到这一行呢？通过cmpl   $0x6,0x202181(%rip)        # 603760 <num_input_strings>  我们大概可以猜到，只有通过了p_hase6才有可能进去，这个条件我们满足了。可以证实一番，我们在jne    40163f <phase_defused+0x7b>的下一句打上断点，进行调试。

<img src="CSAPP%20Bomb%20Lab.assets/s_hase2.png" style="zoom: 67%;" />

可以发现，果然是运行到了p_hase6才停下，所以只有当phase_6(input);之后的 phase_defused();才会把secret_phase调用起来。接着往下看，callq  400bf0 <__isoc99_sscanf> 调用了sscanf进行读取，且通过打印0x402619我们可以知道读取的形式为"%d %d %s"，但从哪里读入呢，mov    $0x603870,%edi，将0x603870作为了参一，也就是要读取字符串的地址，但我们直接打印出来为“”空串。

<img src="CSAPP%20Bomb%20Lab.assets/s_hase1-16608286551549.png" style="zoom:80%;" />

但我们注意到尖括号里面的内存，“input_strings+240"，这应该是读取字符串后存储的内存。而我们只是通过gdb直接打印，没有输入任何东西，所以只有运行通过前6关后，再打印才可能有内容，运行后打印如下。

<img src="CSAPP%20Bomb%20Lab.assets/s_hase3.png" style="zoom: 67%;" />

对吧，运行起来后在打印就有内容了。7 0 这不是我们p_hase4的答案吗，哦，开启秘密的方法揭晓了，我们需要在p_hase4的答案后再添上一个字符串，那添什么呢？通过sscanf之后的几句发现，只有当加上的字符串和0x402622地址处的字符串相同才能接着往下走，且0x402622打印出来为"DrEvil"，所以我们只要在p_hase4答案后添加上DrEvil，且随后输出几条语句后就会调用secret_phase。

好，终于正题来了

```assembly
0000000000401242 <secret_phase>:
  401242:	53                   	push   %rbx
  401243:	e8 56 02 00 00       	callq  40149e <read_line>         ; 读一行，是要我们输入字符串
  401248:	ba 0a 00 00 00       	mov    $0xa,%edx                  ; edx = 10
  40124d:	be 00 00 00 00       	mov    $0x0,%esi                  ; esi = 0
  401252:	48 89 c7             	mov    %rax,%rdi                  ; rdi = rax
  401255:	e8 76 f9 ff ff       	callq  400bd0 <strtol@plt>        ; c库函数，将字符串根据给定的base转为长整数，这里的base是10，详细请百度
  40125a:	48 89 c3             	mov    %rax,%rbx                  ; rbx = rax
  40125d:	8d 40 ff             	lea    -0x1(%rax),%eax            ; rax = rax - 1
  401260:	3d e8 03 00 00       	cmp    $0x3e8,%eax                ; comp(0x3e8,eax)
  401265:	76 05                	jbe    40126c <secret_phase+0x2a> ; 低于等于（无符号），跳
  401267:	e8 ce 01 00 00       	callq  40143a <explode_bomb>      ; 否则爆炸，说明，输入的字符串的合法数字应小于等于1000
  40126c:	89 de                	mov    %ebx,%esi                  ; esi = ebx
  40126e:	bf f0 30 60 00       	mov    $0x6030f0,%edi             ; edi = 0x6030f0
  401273:	e8 8c ff ff ff       	callq  401204 <fun7>              ; 调fun7，第二参数的值是strtol的返回值
  401278:	83 f8 02             	cmp    $0x2,%eax                  ; comp(2,eax)
  40127b:	74 05                	je     401282 <secret_phase+0x40> ; 相等，跳，说明我们上方调用的fun7需要返回2
  40127d:	e8 b8 01 00 00       	callq  40143a <explode_bomb>      ; 否则，爆炸
  401282:	bf 38 24 40 00       	mov    $0x402438,%edi             ; edi = 0x402438  "Wow! You've defused the secret stage!"
  401287:	e8 84 f8 ff ff       	callq  400b10 <puts@plt>
  40128c:	e8 33 03 00 00       	callq  4015c4 <phase_defused>
  401291:	5b                   	pop    %rbx
  401292:	c3                   	retq   
```

以上代码告知，我们输入的能被转化成的数字不能超过0x3e8，且随后调fun7（）的返回值必须为2，至此满足以上条件，即可完整通关。

```assembly
0000000000401204 <fun7>:                                    ; 假设fun7(0x6030f0,1000)  我们的secret_phase要求返回2
  401204:	48 83 ec 08          	sub    $0x8,%rsp            ; rsp -= 8
  401208:	48 85 ff             	test   %rdi,%rdi            ; test rdi
  40120b:	74 2b                	je     401238 <fun7+0x34>   ; 为0，跳，返回全f,即-1
  40120d:	8b 17                	mov    (%rdi),%edx          ; edx = [rdi] 36
  40120f:	39 f2                	cmp    %esi,%edx            ; comp(esi,edx)
  401211:	7e 0d                	jle    401220 <fun7+0x1c>   ; 若edx<=esi,跳
  401213:	48 8b 7f 08          	mov    0x8(%rdi),%rdi       ; rdi = [8+rdi] 0x00603110 *0x00603110=8
  401217:	e8 e8 ff ff ff       	callq  401204 <fun7>        ; 调fun7        3. 返回1    调用1
  40121c:	01 c0                	add    %eax,%eax            ; eax += eax    4. 1+1=2
  40121e:	eb 1d                	jmp    40123d <fun7+0x39>   ; 跳过去，返回   5. 返回
  401220:	b8 00 00 00 00       	mov    $0x0,%eax            ; eax = 0
  401225:	39 f2                	cmp    %esi,%edx            ; comp(esi,edx)
  401227:	74 14                	je     40123d <fun7+0x39>   ; 相等，跳,跳过去，返回0      调用3
  401229:	48 8b 7f 10          	mov    0x10(%rdi),%rdi      ; rdi = [16+rdi] *（0x00603130+16）=  0x00603150
  40122d:	e8 d2 ff ff ff       	callq  401204 <fun7>        ; 调fun7            1.返回0    调用2
  401232:	8d 44 00 01          	lea    0x1(%rax,%rax,1),%eax  ; eax = 1+rax+rax 2. eax=1
  401236:	eb 05                	jmp    40123d <fun7+0x39>     ; 跳过去，返回
  401238:	b8 ff ff ff ff       	mov    $0xffffffff,%eax       ; 按标号得顺序进行返回就可以得到2，调用顺序则应相反
  40123d:	48 83 c4 08          	add    $0x8,%rsp              ; 刚进来时，参二应该大于36，
  401241:	c3                   	retq   
  ; 所以，fun7的返回出口有：return -1，return 0，return 1+(fun7)*2，return (fun7)*2 共4个返回方式，设返回方案为1,2,3,4
```

所以我们要通过fun7的四种返回方式来组合返回2，可以通过方案2->3->4进行返回，实际调用顺序则相反，通过3次不同的调用，每次都会对我们的参数二num进行约数，依次要求 num<36 ，num>=8 ,num==22，所以结果揭晓，22。实际输入字符串22后面可以跟任意其他内容，只要第一个不要是数字即可。

![](CSAPP%20Bomb%20Lab.assets/overall.png)

至此6个p_hase加上secret_phase全部通过。太好了哈哈哈。<img src="CSAPP%20Bomb%20Lab.assets/70e55b8d5163833dceb9ec9267092762.gif" alt="img" style="zoom:50%;" />

### answer

最终全部答案如下。

```c
Border relations with Canada have never been better.
1 2 4 8 16 32 64
1 311 
7 0 DrEvil
IONEFG
4 3 2 1 6 5 
22 
```

## 总结

至此6个阶段全部完成，除开第一个刚接触不懂得流程做法，需要看看别人是怎么做得，其余的全部个人独立完成，这个bomb lab难度不算大，不过需要细心和耐心，每通过一个关卡心情都是非常愉悦的，总体收获很大，极大的提高了汇编代码的阅读能力以及发现了一些遗漏的知识点，也学习一些gdb的基础调试。
