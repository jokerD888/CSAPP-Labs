# CSAPP Architecture Lab

此lab涉及Y86-64的实现，具体Y86的内容可查看CSAPP第四章,做完本实验可以提高你对处理器设计以及软件与硬件的理解。

从[CMU官网](http://csapp.cs.cmu.edu/3e/labs.html)下载完所需实验包后，参考实验所给的官方文档`simguide.pdf`，首先建立实验环境，解压`sim`包，你可能会在make时遇到错误，这里对于错误的处理，我参照了[这里](https://zhuanlan.zhihu.com/p/454779772)的解决方法，简单来说，直接执行如下脚本即可（本人当时亲测可行）

```bash
wget https://gitee.com/lin-xi-269/csapplab/raw/master/lab4archlab/archlab-handout/installTclTk.sh && bash installTclTk.sh
```

实验分为三个部分，每个部分都有自己的处理方法。 在 A 部分，您将编写一些简单的 Y86-64程序并熟悉 Y86-64 工具。 在 B 部分，您将使用一个扩展 SEQ 模拟器新指令。 这两部分将为您准备 C 部分，这是实验室的核心，您将在其中进行优化Y86-64基准程序和处理器设计。

## **Part A**

这一部分有三个题需要我们解决。

#### sum.ys

第一题，要求你根据所给的C语言代码编写对应的Y86-64汇编代码，关于Y86-64汇编代码的编写，可参考`simguide.pdf`中的Figure1示例或书中示例。本题所给C代码`sum_list`是一个链表求和的函数，直接模仿示例编写即可。

```assembly
# Execution begins at address 0
        .pos 0
        irmovq stack,%rsp        # Set up stack pointer
        call main                   # Execute main program
        halt                        # Terminate program

# Sample linked list
.align 8
ele1:
        .quad 0x00a
        .quad ele2
ele2:
        .quad 0x0b0
        .quad ele3
ele3:
        .quad 0xc00
        .quad 0

main:
        irmovq ele1,%rdi
        call sumlist
        ret

sumlist:
        xorq %rax,%rax                  # sum = 0
loop:
        andq %rdi,%rdi                  # Set CC , 设置条件码
        je end                          # 为零，跳转
        mrmovq (%rdi),%rsi               # 解地址
        addq %rsi,%rax                   # val += ls->val
        irmovq $8,%rdx  
        addq %rdx,%rdi                  # rdi移动到ls->next
        mrmovq (%rdi),%rdi              # ls=ls->next
        jmp loop
end:
        ret
# Stack starts here and grows to lower addresses
        .pos 0x200
stack: 
```

经模拟器运行测试，测试结果如下：

![](CSAPP%20Architecture%20Lab.assets/Snipaste_2022-12-06_11-25-30.png)

返回值`rax`的值如我们所预期的`0x00a+0x0b0+0xc00=0Xcba`，说明所编写的程序正确。

#### rsum.ys

第二题，要求我们在第一题的基础上改为递归形式。

代码框架与第一题相同，需要改变的仅有`rsum_list`函数部分，需要注意的是调用函数时要保存相关的寄存器内容，即要把`调用者保存`的那些寄存器保存起来。

```assembly
# Execution begins at address 0
        .pos 0
        irmovq stack,%rsp        # Set up stack pointer
        call main                   # Execute main program
        halt                        # Terminate program

# Sample linked list
.align 8
ele1:
        .quad 0x00a
        .quad ele2
ele2:
        .quad 0x0b0
        .quad ele3
ele3:
        .quad 0xc00
        .quad 0

main:
        irmovq ele1,%rdi
        xorq %rax,%rax                  # rax = 0
        irmovq $8,%rdx                  # 在main中设置偏移量，避免递归调用中重复设置
        call rsum_list
        ret

rsum_list:
        pushq %rbx                      # 被调用者保存，此时，此函数为被调用函数
        andq %rdi,%rdi                  # Set CC
        je end                          # 为零，跳转
        mrmovq  (%rdi),%rbx             # 解地址，ls->val，将val保存到被调用者保存寄存器rbx中
        addq %rdx,%rdi                   # ls->next
        mrmovq (%rdi),%rdi              # ls=ls->next
        call rsum_list                  
        addq %rbx,%rax
end:
        popq %rbx                      # 恢复
        ret

# Stack starts here and grows to lower addresses
        .pos 0x200
stack: 
```

测试结果如下，验证正确。

![](CSAPP%20Architecture%20Lab.assets/ar11.png)

#### **copy.ys**

第三题，是数值拷贝，跟第一题差不多。

```assembly
# Execution begins at address 0
        .pos 0
        irmovq stack,%rsp        # Set up stack pointer
        call main                   # Execute main program
        halt                        # Terminate program

# Sample linked list
.align 8
# Source block
src:
        .quad 0x00a
        .quad 0x0b0
        .quad 0xc00
# Destination block
dest:
        .quad 0x111
        .quad 0x222
        .quad 0x333

main:
        irmovq src,%rdi
        irmovq dest,%rsi
        irmovq $3,%rdx
        call copy_block
        ret

copy_block:
        xorq %rax,%rax
        irmovq $8,%rcx
        irmovq $1,%r8
loop:
        andq %rdx,%rdx                  # Set CC
        je end                          # 为零，跳转
        mrmovq  (%rdi),%rbx             # 解地址    val=*src
        addq %rcx,%rdi                  # src++
        rmmovq %rbx,(%rsi)              # *dest=val
        addq %rcx,%rsi                  # dest++
        xorq %rbx,%rax                  # result^=val
        subq %r8,%rdx                   # len--
        jmp loop
end:
        ret

# Stack starts here and grows to lower addresses
        .pos 0x200
stack: 
```

测试结果如下，验证正确。

![](CSAPP%20Architecture%20Lab.assets/ar12.png)

##  **Part B**

在这部分中，在目录 sim/seq 中工作。任务是扩展 SEQ 处理器以支持 iaddq，如作业问题中所述4.51 和 4.52。 要添加此指令，我们将修改文件 seq-full.hcl，该文件实现了CS:APP3e 教科书中描述的 SEQ 版本。

先参考书上图4.18内容，完成`iaddq`的实现阶段。如下

```assembly
#取值
    icode:ifun <- M1[PC]
    rA:rB <- M1[PC+1]
    valC <- M8[PC+2]
    valP <- PC+10
#译码
	valB <- R[rB]
#执行
	valE <- valB + valC
#访存

#写回
	R[rB] <- valE
#更新PC
	PC <- valP
```

随后根据上面的内容，完成对 seq-full.hcl 的修改，其实就是将`IIADDQ`添加都几个地方。这部分可以参考书上4.3.4节SEQ阶段的实现。

```
# ！！增加一个IIADDQ常量值表示 iaddq指令
bool instr_valid = icode in 
	{ INOP, IHALT, IRRMOVQ, IIRMOVQ, IRMMOVQ, IMRMOVQ,
	       IOPQ, IJXX, ICALL, IRET, IPUSHQ, IPOPQ ,IIADDQ};

# Does fetched instruction require a regid byte?
# ！！ iaddq需要寄存器，加上
bool need_regids =
	icode in { IRRMOVQ, IOPQ, IPUSHQ, IPOPQ, 
		     IIRMOVQ, IRMMOVQ, IMRMOVQ ,IIADDQ};

# Does fetched instruction require a constant word?
# ！！ 同样也需要常数字
bool need_valC =
	icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ, IJXX, ICALL,IIADDQ };

## What register should be used as the B source?
# ！！ B source源 需要寄存器
word srcB = [
	icode in { IOPQ, IRMMOVQ, IMRMOVQ ,IIADDQ } : rB;
	icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
	1 : RNONE;  # Don't need register
];

## What register should be used as the E destination?
# ！！ E destination 目的需要寄存器
word dstE = [
	icode in { IRRMOVQ } && Cnd : rB;
	icode in { IIRMOVQ, IOPQ,IIADDQ} : rB;
	icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
	1 : RNONE;  # Don't write any register
];

## Select input A to ALU
# ！！valC + valB
word aluA = [
	icode in { IRRMOVQ, IOPQ } : valA;
	icode in { IIRMOVQ, IRMMOVQ, IMRMOVQ ,IIADDQ} : valC;
	icode in { ICALL, IPUSHQ } : -8;
	icode in { IRET, IPOPQ } : 8;
	# Other instructions don't need ALU
];

## Select input B to ALU
# ！！ valC + valB
word aluB = [
	icode in { IRMMOVQ, IMRMOVQ, IOPQ, ICALL, 
		      IPUSHQ, IRET, IPOPQ ,IIADDQ} : valB;
	icode in { IRRMOVQ, IIRMOVQ } : 0;
	# Other instructions don't need ALU
];

## Should the condition codes be updated?
# ！！ 条件码应该被更新
bool set_cc = icode in { IOPQ ,IIADDQ};
```

通过一下命令测试，测试指令来自官方文档。当执行`make VERSION=full`出现`undefined reference to 'matherr'`错误时，可以注释掉`ssim.c`里的两条相关代码。

```bash
make VERSION=full					#根据seq-full.hcl文件构建新的仿真器
./ssim -t ../y86-code/asumi.yo		#在小的Y86-64程序中测试你的方法
cd ../y86-code; make testssim		#在小的Y86-64程序中测试你的方法
cd ../ptest; make SIM=../seq/ssim	#测试除了iaddq以外的所有指令
cd ../ptest; make SIM=../seq/ssim TFLAGS=-i		#测试我们实现的iaddq指令
```

测试结果如下

![](CSAPP%20Architecture%20Lab.assets/ar22.png)

![](CSAPP%20Architecture%20Lab.assets/ar21.png)

## **Part C**

在这部分中，在目录 sim/seq 中工作。任务是通过修改 ncopy.ys 和 pipe-full.hcl，目的是使 ncopy.ys尽可能快地跑。

我们先测试一下什么也不做能跑多快。参考官方文档进行测试。

```bash
make VERSION=full				# 构建模拟器
./psim -t sdriver.yo			# 模拟运行，4长度数组 
./psim -t ldriver.yo			# 模拟运行,63长度数组
./correctness.pl				# 测试ncopy.ys代码的正确性（数组长度0~65)
./benchmark.pl				# 测CPE

```

原始版本测得`CPI: 897 cycles/765 instructions = 1.17 (ldriver.yo)`和`Average CPE 15.18`，不同机器可能略有差异。

1.首先可以想到得第一个点是使用Part B部分实现的`iaddq`，替换后如下。

```assembly
# Version 1.0 ！！ iaddq
# You can modify this portion
	# Loop header
	xorq %rax,%rax		# count = 0;
	andq %rdx,%rdx		# len <= 0?
	jle Done		# if so, goto Done:

Loop:	mrmovq (%rdi), %r10	# read val from src...
	rmmovq %r10, (%rsi)	# ...and store it to dst
	andq %r10, %r10		# val <= 0?
	jle Npos		# if so, goto Npos:
	iaddq $1, %rax		# count++
Npos:	
	iaddq $8, %rdi		# src++
	iaddq $8, %rsi		# dst++
	iaddq $-1,%rdx		# len--,同时设置状态码
	jg Loop			# if so, goto Loop:
```

测试得`CPI: 677 cycles/545 instructions = 1.24(ldriver.yo)`和`Average CPE 11.70`

2.进一步我们考虑循环展开，我们考虑`8*1展开`

```assembly
# Version 2.0 ！！ iaddq+循环展开
# You can modify this portion
	# Loop header
	xorq %rax,%rax		# count = 0;
	rrmovq %rdx,%rcx	# limit=len
	iaddq $-7,%rcx		# limit-=7	！！！ 注意这里要步长减1，否则某些情况会少展开一次，甚至速度变慢(主要跟判断流程有关)
	jle Rest		# if so, goto Rest:

Loop:
# read val from src
	mrmovq (%rdi),%r8
	mrmovq 8(%rdi), %r9
	mrmovq 16(%rdi), %r10
	mrmovq 24(%rdi), %r11
	mrmovq 32(%rdi), %r12
	mrmovq 40(%rdi), %r13
	mrmovq 48(%rdi), %r14
	mrmovq 56(%rdi), %rbx
# store val to dst
	rmmovq %r8, (%rsi)
	rmmovq %r9, 8(%rsi)
	rmmovq %r10, 16(%rsi)
	rmmovq %r11, 24(%rsi)
	rmmovq %r12, 32(%rsi)
	rmmovq %r13, 40(%rsi)
	rmmovq %r14, 48(%rsi)
	rmmovq %rbx, 56(%rsi)

ele1:
	andq %r8,%r8		# val<=0?
	jle ele2			# if so,goto ele2
	iaddq $1,%rax		# count++ 
ele2:   
	andq %r9, %r9          
	jle ele3
	iaddq $1, %rax
ele3:   
	andq %r10, %r10
	jle ele4
	iaddq $1, %rax
ele4:   
	andq %r11, %r11
	jle ele5
	iaddq $1, %rax
ele5:   
	andq %r12, %r12
	jle ele6
	iaddq $1, %rax
ele6:   
	andq %r13, %r13
	jle ele7
	iaddq $1, %rax
ele7:   
	andq %r14, %r14
	jle ele8
	iaddq $1, %rax
ele8:   
	andq %rbx, %rbx
	jle End1
	iaddq $1, %rax
End1:
	iaddq $64,%rdi	# src+=8
	iaddq $64,%rsi	# dst+=8
	iaddq $-8,%rdx # len-=8
	iaddq $-8,%rcx	# limit-=8
	jg Loop

Rest:	# 不够8个展开，剩余的进行循环
	andq %rdx,%rdx		# len<=0
	jle Done
LoopRest:
	mrmovq (%rdi),%rbx	
	rmmovq %rbx,(%rsi)
	andq %rbx,%rbx
	jle End2
	iaddq $1,%rax
End2:
	iaddq $8,%rdi
	iaddq $8,%rsi
	iaddq $-1,%rdx
	jg LoopRest:
```

测试得`CPI: 439 cycles/359 instructions = 1.22(ldriver.yo)`和`Average CPE 8.61`

3.尽管上面的`CPE达到了8.61`,但得分却只有`Score 37.9/60.0`。不妨进一步将剩余不够K个展开的，不再使用循环，展开成一个一个的。而且我们用到的寄存器也只有13个，还有一个`rbp`没用到（`Y86-64`共15个寄存器，除开`rsp`)。

```assembly
# Version 2.1 ！！ iaddq + 9*1展开 
# You can modify this portion
	# Loop header
	xorq %rax,%rax		# count = 0;
	rrmovq %rdx,%rcx	# limit=len
	iaddq $-8,%rcx		# limit-=8
	jle Pos1		# if so, goto ele1:

Loop:
# read val from src
	mrmovq (%rdi),%r8
	mrmovq 8(%rdi), %r9
	mrmovq 16(%rdi), %r10
	mrmovq 24(%rdi), %r11
	mrmovq 32(%rdi), %r12
	mrmovq 40(%rdi), %r13
	mrmovq 48(%rdi), %r14
	mrmovq 56(%rdi), %rbx
	mrmovq 64(%rdi), %rbp

# store val to dst
	rmmovq %r8, (%rsi)
	rmmovq %r9, 8(%rsi)
	rmmovq %r10, 16(%rsi)
	rmmovq %r11, 24(%rsi)
	rmmovq %r12, 32(%rsi)
	rmmovq %r13, 40(%rsi)
	rmmovq %r14, 48(%rsi)
	rmmovq %rbx, 56(%rsi)
	rmmovq %rbp, 64(%rsi)
ele1:
	andq %r8,%r8		# val<=0?
	jle ele2			# if so,goto ele2
	iaddq $1,%rax		# count++ 
ele2:   
	andq %r9, %r9          
	jle ele3
	iaddq $1, %rax
ele3:   
	andq %r10, %r10
	jle ele4
	iaddq $1, %rax
ele4:   
	andq %r11, %r11
	jle ele5
	iaddq $1, %rax
ele5:   
	andq %r12, %r12
	jle ele6
	iaddq $1, %rax
ele6:   
	andq %r13, %r13
	jle ele7
	iaddq $1, %rax
ele7:   
	andq %r14, %r14
	jle ele8
	iaddq $1, %rax
ele8:   
	andq %rbx, %rbx
	jle ele9
	iaddq $1, %rax
ele9:
	andq %rbp,%rbp
	jle End1
	iaddq $1,%rax
End1:
	iaddq $72,%rdi	# src+=9
	iaddq $72,%rsi	# dst+=9
	iaddq $-9,%rdx # len-=9
	iaddq $-9,%rcx	# limit-=9
	jg Loop

# 不够9个展开，剩余的进行循环,剩下的最多只有8个
Pos1:
	iaddq $-1,%rdx
	jl Done
	mrmovq (%rdi),%r8
	rmmovq %r8,(%rsi)
	andq %r8,%r8
	jle Pos2
	iaddq 1,%rax
Pos2:
	iaddq $-1,%rdx
	jl Done
	mrmovq 8(%rdi),%r8
	rmmovq %r8,8(%rsi)
	andq %r8,%r8
	jle Pos3
	iaddq 1,%rax
Pos3:
	iaddq $-1,%rdx
	jl Done
	mrmovq 16(%rdi),%r8
	rmmovq %r8,16(%rsi)
	andq %r8,%r8
	jle Pos4
	iaddq 1,%rax
Pos4:
	iaddq $-1,%rdx
	jl Done
	mrmovq 24(%rdi),%r8
	rmmovq %r8,24(%rsi)
	andq %r8,%r8
	jle Pos5
	iaddq 1,%rax
Pos5:
	iaddq $-1,%rdx
	jl Done
	mrmovq 32(%rdi),%r8
	rmmovq %r8,32(%rsi)
	andq %r8,%r8
	jle Pos6
	iaddq 1,%rax
Pos6:
	iaddq $-1,%rdx
	jl Done
	mrmovq 40(%rdi),%r8
	rmmovq %r8,40(%rsi)
	andq %r8,%r8
	jle Pos7
	iaddq 1,%rax
Pos7:
	iaddq $-1,%rdx
	jl Done
	mrmovq 48(%rdi),%r8
	rmmovq %r8,48(%rsi)
	andq %r8,%r8
	jle Pos8
	iaddq 1,%rax
Pos8:
	iaddq $-1,%rdx
	jl Done
	mrmovq 56(%rdi),%r8
	rmmovq %r8,56(%rsi)
	andq %r8,%r8
	jle Done
	iaddq 1,%rax
```

测试得`CPI: 400 cycles/331 instructions = 1.21(ldriver.yo)`和 `Average CPE	8.36`  ,`Score 42.8/60.0`

4.回想本题要求，通过修改 ncopy.ys 和 pipe-full.hcl，目的是使 ncopy.ys尽可能快地跑。目前为止，处去PartB部分我们还未曾修改`pipe-full.hcl`，那么接下来就要对`pipe-full.hcl`动手了。

我们观察上方的汇编代码，可以发现形同`mrmovq (%rdi),%r8 rmmovq %r8,(%rsi)`这两组紧挨的指令可能会发生数据冒险，事实上也确实会发生，这会导致产生一个暂停。（因为只要一条指令执行了 load操作，从内存中读一个值到寄存器，并且下一条指令要用这个寄存器作为源操作数，就会产生一个暂停。如果要在执行阶段中使用这个源操作数，暂停是避免冒险的唯一方法。对于第二条指令将源操作数存储到内存的情况，例如 rmmovg或 push 指令，是不需要这样的暂停的。摘自课后家庭作业4.57。）所以我们根据作业4.57加以实现`加载转发`。关于此作业的相关解答，参考自[这里](https://dreamanddead.github.io/CSAPP-3e-Solutions/chapter4/4.57/)。参考4.57作业添加`加载转发`如下。

```assembly
## Generate valA in execute stage
word e_valA = [
	E_icode in { IRMMOVQ, IPUSHQ } && E_srcA == M_dstM : m_valM;
	1:E_valA;    # Pass valA through stage
];
################ Pipeline Register Control #########################

# Should I stall or inject a bubble into Pipeline Register F?
# At most one of these can be true.
bool F_bubble = 0;
bool F_stall =
	# Conditions for a load/use hazard
	E_icode in { IMRMOVQ, IPOPQ } &&
	( 
		E_dstM==d_srcB || 	#!! 只有E_dstM==d_srcA才可能使用加载转发,否则只有暂停
		(
			E_dstM==d_srcA && !(D_icode in {IRMMOVQ,IPUSHQ})	# 进一步只有rmmovq和pushq对valA的使用不会在phase E
		) 
	) ||
	# Stalling at fetch while ret passes through pipeline
	IRET in { D_icode, E_icode, M_icode };

# Should I stall or inject a bubble into Pipeline Register D?
# At most one of these can be true.
bool D_stall = 
	# Conditions for a load/use hazard
	E_icode in { IMRMOVQ, IPOPQ } && 
	(
		E_dstM==d_srcB || 	#!! 只有E_dstM==d_srcA才可能使用加载转发,否则只有暂停
		(
			E_dstM==d_srcA && !(D_icode in {IRMMOVQ,IPUSHQ})	# 进一步只有rmmovq和pushq对valA的使用不会在phase E
		) 
	);

bool D_bubble =
	# Mispredicted branch
	(E_icode == IJXX && !e_Cnd) ||
	# Stalling at fetch while ret passes through pipeline
	# but not condition for a load/use hazard
	!(				# ！！ 这里也涉及到了装载/使用冒险，所以也需要做修改
		E_icode in { IMRMOVQ, IPOPQ } && 					
		(
			E_dstM==d_srcB || 	#!! 只有E_dstM==d_srcA才可能使用加载转发,否则只有暂停
			(
				E_dstM==d_srcA && !(D_icode in {IRMMOVQ,IPUSHQ})	# 进一步只有rmmovq和pushq对valA的使用不会在phase E
			)
		) 
	) &&
	  IRET in { D_icode, E_icode, M_icode };

# Should I stall or inject a bubble into Pipeline Register E?
# At most one of these can be true.
bool E_stall = 0;
bool E_bubble =
	# Mispredicted branch
	(E_icode == IJXX && !e_Cnd) ||
	# Conditions for a load/use hazard
	(						# ！！ 这里也涉及到了装载/使用冒险，所以也需要做修改
		E_icode in { IMRMOVQ, IPOPQ } && 					
		(
			E_dstM==d_srcB || 	#!! 只有E_dstM==d_srcA才可能使用加载转发,否则只有暂停
			(
				E_dstM==d_srcA && !(D_icode in {IRMMOVQ,IPUSHQ})	# 进一步只有rmmovq和pushq对valA的使用不会在phase E
			)
		) 
	);

# Should I stall or inject a bubble into Pipeline Register M?
# At most one of these can be true.
bool M_stall = 0;
# Start injecting bubbles as soon as exception passes through memory stage
bool M_bubble = m_stat in { SADR, SINS, SHLT } || W_stat in { SADR, SINS, SHLT };

# Should I stall or inject a bubble into Pipeline Register W?
bool W_stall = W_stat in { SADR, SINS, SHLT };
bool W_bubble = 0;
```

同时对`ncopy.ys`算法流程进行优化

-   len先减(step-1)循环结束之后再加上(step-1)即可避免在循环内对limit变量的修改
-   以及初始时不再对`rax`进行置零，默认是0。
-   由于对于跳转指令的默认预测策略是选择分支进行跳转，所以我们利用这点，使得程序不要直接默认跳转到Done，因为大多数情况下不会跳到Done
-   对不足K个一组的入口处进入优化

进行以上优化即可达到`Average CPE 7.62 `得分`Score	57.7/60.0`。

```assembly
# Version 2.1 ！！ iaddq + 9*1展开 + 算法流程优化		Score	57.7/60.0
# You can modify this portion
	# Loop header
	#xorq %rax,%rax		# count = 0;
	iaddq $-8,%rdx		# limit-=8
	jle Pos1		# if so, goto ele1:

Loop:
# read val from src
	mrmovq (%rdi),%r8
	mrmovq 8(%rdi), %r9
	mrmovq 16(%rdi), %r10
	mrmovq 24(%rdi), %r11
	mrmovq 32(%rdi), %r12
	mrmovq 40(%rdi), %r13
	mrmovq 48(%rdi), %r14
	mrmovq 56(%rdi), %rbx
	mrmovq 64(%rdi), %rbp

# store val to dst
	rmmovq %r8, (%rsi)
	rmmovq %r9, 8(%rsi)
	rmmovq %r10, 16(%rsi)
	rmmovq %r11, 24(%rsi)
	rmmovq %r12, 32(%rsi)
	rmmovq %r13, 40(%rsi)
	rmmovq %r14, 48(%rsi)
	rmmovq %rbx, 56(%rsi)
	rmmovq %rbp, 64(%rsi)
ele1:
	andq %r8,%r8		# val<=0?
	jle ele2			# if so,goto ele2
	iaddq $1,%rax		# count++ 
ele2:   
	andq %r9, %r9          
	jle ele3
	iaddq $1, %rax
ele3:   
	andq %r10, %r10
	jle ele4
	iaddq $1, %rax
ele4:   
	andq %r11, %r11
	jle ele5
	iaddq $1, %rax
ele5:   
	andq %r12, %r12
	jle ele6
	iaddq $1, %rax
ele6:   
	andq %r13, %r13
	jle ele7
	iaddq $1, %rax
ele7:   
	andq %r14, %r14
	jle ele8
	iaddq $1, %rax
ele8:   
	andq %rbx, %rbx
	jle ele9
	iaddq $1, %rax
ele9:
	andq %rbp,%rbp
	jle End1
	iaddq $1,%rax
End1:
	iaddq $72,%rdi	# src+=9
	iaddq $72,%rsi	# dst+=9
	iaddq $-9,%rdx # len-=9
	jg Loop

# 不够9个展开，剩余的进行循环,剩下的最多只有8个
Pos1:
	#iaddq $8,%rdx
	#iaddq $-1,%rdx
    iaddq $7,%rdx
	jge P1
	jmp Done
P1:
	mrmovq (%rdi),%r8
	rmmovq %r8,(%rsi)
	andq %r8,%r8
	jle Pos2
	iaddq 1,%rax
Pos2:
	iaddq $-1,%rdx
	jge P2
	jmp Done
P2:
	mrmovq 8(%rdi),%r8
	rmmovq %r8,8(%rsi)
	andq %r8,%r8
	jle Pos3
	iaddq 1,%rax
Pos3:
	iaddq $-1,%rdx
	jge P3
	jmp Done
P3:
	mrmovq 16(%rdi),%r8
	rmmovq %r8,16(%rsi)
	andq %r8,%r8
	jle Pos4
	iaddq 1,%rax
Pos4:
	iaddq $-1,%rdx
	jge P4
	jmp Done
P4:
	mrmovq 24(%rdi),%r8
	rmmovq %r8,24(%rsi)
	andq %r8,%r8
	jle Pos5
	iaddq 1,%rax
Pos5:
	iaddq $-1,%rdx
	jge P5
	jmp Done
P5:
	mrmovq 32(%rdi),%r8
	rmmovq %r8,32(%rsi)
	andq %r8,%r8
	jle Pos6
	iaddq 1,%rax
Pos6:
	iaddq $-1,%rdx
	jge P6
	jmp Done
P6:
	mrmovq 40(%rdi),%r8
	rmmovq %r8,40(%rsi)
	andq %r8,%r8
	jle Pos7
	iaddq 1,%rax
Pos7:
	iaddq $-1,%rdx
	jge P7
	jmp Done
P7:
	mrmovq 48(%rdi),%r8
	rmmovq %r8,48(%rsi)
	andq %r8,%r8
	jle Pos8
	iaddq 1,%rax
Pos8:
	iaddq $-1,%rdx
	jge P8
	jmp Done
P8:
	mrmovq 56(%rdi),%r8
	rmmovq %r8,56(%rsi)
	andq %r8,%r8
	jle Done
	iaddq 1,%rax
```

-   最终版，经过一顿搜索，经过参考改进，有了如下满分版本。`Average CPE	5.14`，`Score	60.0/60.0`。下面最主要的改变是直接修改`pipe-full.hcl`使其预测策略为不跳转，以及改为4*1展开，主要是为了减小较大的元素个数的CPE，依此来拉低整体的平均CPE。

额外对`pipe-full.hcl`做如下修改

```
# Predict next value of PC
word f_predPC = [
	
	f_icode in { ICALL } : f_valC;	# 删除IJXX，使其默认策略是不跳转
	1 : f_valP;
];
```

```assembly
# Version 3 ！！ iaddq + 4*1展开 + 算法流程优化		Score	60.0/60.0 
# You can modify this portion
	# Loop header
	#xorq %rax,%rax		# count = 0;
	iaddq $-3,%rdx		# limit-=3
	jle Pos1		# if so, goto ele1:

Loop:
# read val from src
	mrmovq (%rdi),%r8
	mrmovq 8(%rdi), %r9
	mrmovq 16(%rdi), %r10
	mrmovq 24(%rdi), %r11


# store val to dst
	rmmovq %r8, (%rsi)
	rmmovq %r9, 8(%rsi)
	rmmovq %r10, 16(%rsi)
	rmmovq %r11, 24(%rsi)
	
ele1:
	andq %r8,%r8		# val<=0?
	jle ele2			# if so,goto ele2
	iaddq $1,%rax		# count++ 
ele2:   
	andq %r9, %r9          
	jle ele3
	iaddq $1, %rax
ele3:   
	andq %r10, %r10
	jle ele4
	iaddq $1, %rax
ele4:   
	andq %r11, %r11
	jle End1
	iaddq $1, %rax
End1:
	iaddq $32,%rdi	# src+=9
	iaddq $32,%rsi	# dst+=9
	iaddq $-4,%rdx # len-=9
	jg Loop

# 不够4个展开，剩余的进行循环,剩下的最多只有3个
Pos1:
	iaddq $3,%rdx

	jle Done
	mrmovq (%rdi),%r8
	rmmovq %r8,(%rsi)
	andq %r8,%r8
	jle Pos2
	iaddq 1,%rax
Pos2:
	iaddq $-1,%rdx
	jle Done
	mrmovq 8(%rdi),%r8
	rmmovq %r8,8(%rsi)
	andq %r8,%r8
	jle Pos3
	iaddq 1,%rax
Pos3:
	iaddq $-1,%rdx
	jle Done
	mrmovq 16(%rdi),%r8
	rmmovq %r8,16(%rsi)
	andq %r8,%r8
	jle Done
	iaddq 1,%rax
```

[参考链接](https://zhuanlan.zhihu.com/p/440564789)

[课后习题参考链接](https://dreamanddead.github.io/CSAPP-3e-Solutions/chapter4/4.57/)

## 总结

Part A 和 Part B部分都非常容易，但Part C想要拿满分就比较困难，可能需要各种各样的优化。原本对于本章的内容自我感觉学的也是一知半解，但经过此实验，解题过程遇到不解的需要经常翻书，以及对家庭作业部分进行学习，加深了对本章的理解。明白了流水线化如何让不同的阶段并行操作，如何尽可能提高系统的吞吐量。
