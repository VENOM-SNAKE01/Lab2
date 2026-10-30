# Lab2 BombLab 实验报告

姓名 / 学号：（可填，也可留空）

## 一、实验环境与方法

- 环境：WSL2 Ubuntu，`Linux x86_64`，`gcc`、`gdb`、`objdump`（binutils）。
- 每关统一流程：
  1. `objdump -d --no-show-raw-insn --disassemble=phase_0N ./bomb` 拿到本关汇编；
  2. 从 `valid_u32_fields` 的第二个参数和 `sscanf` 的格式串确定**整数个数**；
  3. 顺着调用链把 `lea / xor / shr / add` 逐条翻译成表达式，恢复出「challenge → 口令」的关系；
  4. 用 `./bomb --seed S --phase N < 口令` 单关验证，再写进 `defuser.c`；
  5. `make` 后跑 `python3 public_test.py --phase N`。
- 关键前提：每关的 `challenge` 由 `(seed, phase)` 独立生成，**同一 seed 下第 1 关和第 2 关的 challenge 不同**，所以公式必须对任意 `challenge` 成立，不能记录某次运行的具体答案。

最终结果：`Public tests: 60/60`（六关各 10/10）。

## 二、Phase 1：函数调用与返回值

### 关键汇编

```
phase_01:
40209d: mov    $0x1,%esi
4020a2: call   valid_u32_fields       ; 第二个参数 1 → 只允许 1 个整数
4020c6: mov    $0x403189,%esi          ; 格式串 "%u"
4020db: call   p1_compute
4020e0: cmp    %eax,-0x14(%rbp)        ; 输入值 == 返回值

p1_compute:
402055: shr    $0x20,%rax              ; 高 32 位
402064: shr    $0x10,%rdx              ; ch >> 16
40206d: call   p1_step2                ; (高位, 低32)
402072: mov    %rdx,%rsi               ; 注意：step2 没写 rdx
402078: mov    %eax,%ecx
40207a: call   p1_step0                ; (低32, ch>>16, step2 结果)
40207f: mov    %ecx,%esi ; mov %eax,%edi
402083: call   p1_step1                ; (step0 结果, step2 结果)
```

三个小函数：

```
p1_step2: eax = 5*a + b ; eax ^= 0x3902
p1_step0: eax = 5*a ; esi += 0xcd4e ; eax ^= esi
p1_step1: eax = b + 0xb86 ; eax ^= a
```

### 恢复出的计算关系

令 `lo = challenge & 0xffffffff`、`hi = challenge >> 32`、`mid = (uint32_t)(challenge >> 16)`：

```
t2  = (5*hi + lo) ^ 0x3902
s0  = (5*lo) ^ (mid + 0xcd4e)
ans = (t2 + 0xb86) ^ s0
```

### 推理过程

`p1_step2` 只写了 `%eax`，没有碰 `%rdx`，所以 `402072: mov %rdx,%rsi` 是**借用这个未被破坏的寄存器**把 `ch>>16` 传给 `step0`；同理 `p1_step0` 没碰 `%ecx`，`%ecx` 里仍是 step2 的结果，于是它成了 `step1` 的第二个参数。另外 `lea (...),%eax` 和 `add $0xcd4e,%esi` 都是 32 位运算，必须按模 2³² 回绕。

### 验证记录（非公开 seed）

seed `20261030`，challenge `0x30348194e1f8df4d`，输入 `985045630` → `Phase 1 defused.`

## 三、Phase 2：数组、循环与递推

### 关键汇编

```
4020fd: mov    $0x6,%esi ; call valid_u32_fields    ; 恰好 6 个整数
40213c: lea    -0x40(%rbp),%rdx   ← v[0]
40212e: lea    -0x3c(%rbp),%rcx   ← v[1]
402148: lea    -0x38(%rbp),%r8    ← v[2]
402144: lea    -0x34(%rbp),%r9    ← v[3]
402143: push   %rax(-0x30)        ← v[4]（栈参数）
402137: push   %rax(-0x2c)        ← v[5]（栈参数）
402155: cmp    $0x6,%eax
```

六个数在 `rbp-0x40 ... rbp-0x2c` 上连续排列，步长 4，即 `int v[6]`，基址 `-0x40(%rbp)`；数组下标寻址写作 `mov -0x40(%rbp,%rdx,4),%eax`。

循环部分：

```
40216e: mov    $0x1b,%ecx        ; cl = 27，移位量
402173: mov    $0x1,%edx         ; i = 1
402178: add    %edx,%eax         ; v[i-1] + i
40217d: shr    %cl,%rdi          ; challenge >> (27 + 5*(i-1))
402183: and    $0x1f,%edi        ; 取低 5 位
402180: lea    (%rax,%rax,2),%eax ; ×3
402186: sub    %edi,%eax
402192: add    $0x1,%rdx ; 402196: add $0x5,%ecx ; 402199: cmp $0x6,%rdx
```

首项由 `shr $0x27`（39）、`movzbl %dl`（取 8 位）、`lea ×3` 得到。

### 恢复出的计算关系

```
v[0] = 3 * ((challenge >> 39) & 0xff)
v[i] = 3 * (v[i-1] + i) - ((challenge >> (27 + 5*(i-1))) & 0x1f)     i = 1..5
```

`ecx` 每次加 5，所以六项的移位量依次是 **39、27、32、37、42、47**。

### 验证记录（非公开 seed）

seed `20261030`，challenge `0x0499d9767b99a822`，输入
`534 1590 4754 14260 42770 128306` → `Phase 2 defused.`

## 四、Phase 3：switch 与跳转表

### 关键汇编

```
4022fc: mov %r12d,%edi ; and $0x7,%edi    ; v[0] = challenge & 7
402304: cmp %edi,-0x18(%rbp)
40230f: rax = ch >> 3 ; rsi = ch >> 32
402317: xor %eax,%esi                      ; x = (ch>>32) ^ (ch>>3)
402319: call p3_dispatch

p3_dispatch:
402258: cmp  $0x7,%eax
40225b: ja   4022a2                        ; 越界检查
40225d: notrack jmp *0x405820(,%rax,8)     ; 跳转表，8 项，每项 8 字节
```

表内容（`.rodata`）：

```
405820: 0x402278  0x402280  0x402288  0x402290
405840: 0x402298  0x4022a0  0x402268  0x402270
```

这些表项并不直接指向处理分支，而是指向一层 `jmp` 桩：

```
0x402268: jmp p3_case_0   0x402270: jmp p3_case_2
0x402278: jmp p3_case_4   0x402280: jmp p3_case_1
0x402288: jmp p3_case_5   0x402290: jmp p3_case_6
0x402298: jmp p3_case_3   0x4022a0: jmp p3_case_7
```

### 恢复出的计算关系

令 `sel = challenge & 7`、`x = (uint32_t)((challenge >> 32) ^ (challenge >> 3))`：

| sel | 实际分支 | 计算式 |
|---|---|---|
| 0 | case_4 | `(x ^ 0x602f) + (x >> 2)` |
| 1 | case_1 | `(2x) ^ 0xc1f4` |
| 2 | case_5 | `(4x) ^ 0xdb50` |
| 3 | case_6 | `5x + 0x12e4e` |
| 4 | case_3 | `3x + 0x5eb3` |
| 5 | case_7 | `3x + 0x7775` |
| 6 | case_0 | `(x ^ 0x9a14) + (x >> 1)` |
| 7 | case_2 | `5x + 0x544c` |

口令为 `sel` 和该计算式的值。

### 推理过程

`0x405820(,%rax,8)` 是「基址 + 8 × 下标」的间接跳转，所以下标 `sel` 直接决定表项地址；读出表项后要再跟一层 `jmp` 桩，才能落到真正的 `p3_case_N`。这套映射**不是恒等映射**（`sel=0` 对应 `case_4`），说明分支顺序被刻意打乱，必须按表逐项核对，不能凭下标猜。

### 验证记录（非公开 seed）

seed `20261030`，challenge `0x60db877a835f9c30`，输入 `0 1021063698` → `Phase 3 defused.`

## 五、Phase 4：递归

### 关键汇编

```
4023ee: mov %r12d,%edi ; and $0x7f,%edi   ; a = challenge & 0x7f
4023f1: xor %esi,%esi                     ; lo = 0
4023fe: mov $0x7f,%edx                    ; hi = 127
4023f7: shr $0x7,%rcx ; 402403: xor %eax,%ecx   ; d = (ch>>32) ^ (ch>>7)
402405: call p4_walk

p4_walk:                                  ; (a:edi, lo:esi, hi:edx, d:ecx)
402338: sub %esi,%edi ; shr $1,%edi ; add %esi,%edi   ; mid = lo + (hi-lo)/2
40233e: cmp %eax,%edi ; je 402390                     ; mid == a → 终止
402347: lea (%rdi,%rcx,1),%ebx                        ; e = mid + d
40234e: cmp %edi,%eax ; jb 402370                      ; a < mid → 左
402390: lea 0xfa(%rdi),%eax ; xor %ecx,%eax            ; (a + 0xfa) ^ d
```

### 恢复出的计算关系

```
walk(a, lo, hi, d):
    mid = lo + (hi - lo) / 2
    if mid == a:  return (mid + 0xfa) ^ d
    e = mid + d
    if a < mid:   return 3 * walk(a, lo, mid-1, d) ^ e ^ 0x1d
    else:         return 3 * walk(a, mid+1, hi, d) ^ e ^ 1
```

入口参数 `a = challenge & 0x7f`、`d = (ch>>32) ^ (ch>>7)`，初始区间 `[0, 127]`。

### 推理过程

结构是**二分查找式递归**：`mid = lo + (hi-lo)/2` 折半，与 `a` 比较后把区间收窄到 `[lo, mid-1]` 或 `[mid+1, hi]`，深度最多 7 层。返回路径上先 `lea (%rax,%rax,2)` 即 `3*r`，再异或 `e`，最后按方向异或 `0x1d`（左）或 `1`（右）——左右两个常量不同，等于把「每一步走了哪边」编码进了结果。

### 验证记录（非公开 seed）

seed `20261030`，challenge `0xd1e0cdee6a36cdda`，输入 `1916705825` → `Phase 4 defused.`

## 六、Phase 5：结构体、指针与链表

### 关键汇编

```
4024cc: lea -0x14(%rbp),%rsi     ; 第二个参数是输出指针
4024d3: call p5_traverse
4024dc: cmp %edx,-0x1c(%rbp)     ; v[0] == 返回值
4024e4: cmp %eax,-0x18(%rbp)     ; v[1] == *(&-0x14)

p5_traverse:
40242d: and $0x7,%eax
402434: mov 0x405b60(,%rax,8),%rdx   ; 按 ch & 7 选链表起点
40243f: shr $0x20,%rdi ; 402443: shr $0x6,%rax ; 402447: xor %edi,%eax
40244e: xor $0x15d4,%eax
402453: lea (%rax,%rax,2),%eax       ; 3*v
402456: add (%rdx),%eax              ; + node->value   （偏移 0）
40245b: xor %esi,%eax                ; ^ node->field4  （偏移 4）
402467: mov 0x8(%rdx),%rdx            ; node = node->next（偏移 8）
```

三个固定偏移说明节点布局：

```c
struct node { uint32_t value; uint32_t field4; struct node *next; };
```

入口表在 `0x405b60`（8 个指针），8 条链实际交汇在同一串 12 个节点上（`0x405c30` 的 `next` 为 0）。例如：

```
405c40: 4c720000 7f020000 505c4000  → value=0x724c, field4=0x27f, next=0x405c50
405c50: d7750000 3b0a0000 c05b4000  → value=0x75d7, field4=0xa3b, next=0x405bc0
```

循环变量 `ecx` 从 1 开始、每轮 `+1`，条件 `cmp %ecx,%edi ; jae`（`steps >= ecx`），所以共访问 `steps + 1` 个节点，`steps = (ch >> 3) & 3 ≤ 3`，最多 4 个。

### 恢复出的计算关系

```
v      = ((ch >> 6) ^ (ch >> 32)) ^ 0x15d4
start  = ch & 7 ,  steps = (ch >> 3) & 3
对 i = 0..steps:  v = 3*v + node[i].value ; v ^= node[i].field4
口令 = [v, node[steps].field4]
```

### 验证记录（非公开 seed）

seed `20261030`，challenge `0xa295dc5687270f3a`，输入 `2743599389 849` → `Phase 5 defused.`

## 七、Phase 6：函数指针表与间接调用

### 关键汇编

```
p6_dispatch:
40256c: shr $0x6,%rdi ; 402573: shr $0x20,%rax ; 402577: xor %eax,%edi   ; x0 = (ch>>6)^(ch>>32)
402583: and $0x7,%eax                       ; 第一次下标 = ch & 7
402580: and $0x38,%ebx                      ; 第二次下标 = (ch>>3) & 7（已乘 8）
402586: call *0x405e00(,%rax,8)             ; 第一次间接调用
40258d: xor $0xb312,%eax
402594: call *0x405e00(%rbx)                ; 第二次间接调用
40259f: add $0xfd35,%eax
```

表内容：

```
405e00: 0x402540  0x402550  0x402550  0x4024f0
405e20: 0x402550  0x402530  0x402520  0x402500
```

对应的小函数：

| 函数 | 计算式 |
|---|---|
| p6_op_0 | `(x + 0x2d2e) ^ (x >> 1)` |
| p6_op_1 | `(8x + 0xc9c2) ^ 7x` |
| p6_op_2 | `rol(x, 1) ^ 0x39b5` |
| p6_op_3 | `ror(x, 7)` |
| p6_op_4 | `(x ^ 0x8adb) + 4x` |
| p6_op_5 | `((x >> 1) ^ x) + 0x1346` |

### 恢复出的计算关系

表下标与函数的对应：`0 → op_4`、`1 → op_5`、`2 → op_5`、`3 → op_0`、`4 → op_5`、`5 → op_3`、`6 → op_2`、`7 → op_1`。

```
x0  = (ch >> 6) ^ (ch >> 32)
y   = T[ch & 7](x0) ^ 0xb312
ans = T[(ch >> 3) & 7](y) + 0xfd35
```

### 推理过程

两次间接调用的寻址写法不同：第一次 `(,%rax,8)` 用真实下标 × 8，第二次 `(%rbx)` 的 `ebx` 已被 `and $0x38` 预乘 8，两者都以 `0x405e00` 为基址。表里 8 个槽只对应 6 个函数（下标 1、2、4 都是 `op_5`），所以不能假设「8 个表项 = 8 种不同操作」。另外 `rol / ror` 是 32 位循环移位，必须用 C 的移位加或运算模拟。

### 验证记录（非公开 seed）

seed `20261030`，challenge `0x8156aef15aa39d4b`，输入 `1429514766` → `Phase 6 defused.`
整轮输出以 `Congratulations! You've defused the bomb.` 结束。

## 八、运行结果

- 公开测试：`Public tests: 60/60`（六关各 10/10）。
- 非公开 seed `20261030` 的全流程输出：

```
Phase 1 challenge: 0x30348194e1f8df4d
Phase 1 defused.
Phase 2 challenge: 0x0499d9767b99a822
Phase 2 defused.
Phase 3 challenge: 0x60db877a835f9c30
Phase 3 defused.
Phase 4 challenge: 0xd1e0cdee6a36cdda
Phase 4 defused.
Phase 5 challenge: 0xa295dc5687270f3a
Phase 5 defused.
Phase 6 challenge: 0x8156aef15aa39d4b
Phase 6 defused.
Congratulations! You've defused the bomb.
```

![公开测试 60/60](images/public-tests.png)

![非公开 seed 20261030 运行结果](images/seed-20261030.png)

截图文件放在仓库的 `images/` 目录下，命名与上面两行一致，随代码一起提交。

## 九、Honor（secret）部分

当前状态：**尚未完成**。`solve_secret` 仍是模板状态，公开测试中 `Secret public tests: 0/10`（`solver_error`）。计划通过 `secret_gate`、`secret_copy`、`secret_phase` 分析入口触发条件与 `context` 的来源，再补入实现；若截止前未能完成，此处如实标注为未完成。

## 十、参考与工具

- 课程提供的 `teaching/` 目录（`01_abi.c` 到 `07_stack_copy.c`）与 `tutor` 参考实现，用于对照各关卡考察点。
- 工具：`objdump`（反汇编与 `.rodata` 转储）、`gdb`（断点、`x/` 查看内存、`ni/si` 单步）、`python3`（在本地复算公式，与 `bomb` 单关交叉验证）。
- 使用 AI 助手辅助阅读汇编、核对运算宽度与整理本报告结构；所有公式均在本地用 `bomb` 对非公开 seed 实际验证通过后才写入 `defuser.c`。
