# 机器人同步开枪算法程序实现

## 问题

有一支机器人礼枪队，我们可以给第一个机器人下达开火指令，它需要把命令传递给整支队伍，并让所有机器人同时开火。
每个机器人只能通过一系列动作进行交流，每个机器人只能看见其左右两个邻居（对于端点处则只能看见一个邻居）。

我们需要定义一组动作状态与动作传递规则，使得其能完成“给第一个机器人下达开火指令，一段时间后所有机器人同时开火”

## 解决方案：双速波 + 折半递归（Firing Squad Synchronization）

每个分队的“将军”发令后，同时向外发出两路信号：

* 手掌信号：每拍走 1 格，即队内的“光速”信号（`HAND_R` / `HAND_L`）；
* 腿部信号：3 拍走 1 格，即速度 1/3 的慢信号（将军自己走 `GEN_x1 → GEN_x2 → GEN_x3` 三拍，随后由 `LEG_x1 → LEG_x2 → LEG_x3` 逐格接力）。

手掌信号在分队另一端反射回来（镜面 = 队列端点机器人，或上一级留下的将军），
与继续外行的腿部信号**恰好在自己分队的中点迎面相遇**；

* 偶数长度分队（腿部信号第 1 拍相遇）：相撞的两台机器人各成为一个半段的将军；
* 奇数长度分队（腿部信号第 3 拍相遇）：只有正中间那台机器人成为将军，并向左右**同时**发令
  （它同时属于两个子分队，扮演双重角色）。

两个子分队把上一级的端点当成新的镜面，镜像同步地递归重复上述过程。
因为每次划分的左右两半长度相同，各层耗时对每台机器人都一致，
所以递归到“分队只剩两台机器人”时，腿部信号第 1 拍就撞上刚从镜面反射回来的手掌信号，
两台机器人同时进入预备开火（`PRE`）；此时全队列同时处于暂备开火，下一拍全体一起开火（`SHOOT`）。

奇数长度分队留下的“中间将军”会同时被左右两个子分队的手掌信号打到，
所以它必须**两侧各反射一份**（实现里用 `MIRR_BM` 表示“左右同时反射”，
只反射一侧会让另一侧子分队的手掌信号消失，那一支就永远等不到中点相撞）。

> 该机制与参考视频《轨道力学的反直觉真相》（B 站 BV1htgr6hEF5，1:48 起）所述的
> “手掌信号 / 腿部信号三拍 / 中点相遇产生新将军 / 分队当镜面 / 奇数分队中间机器人双重角色”一致。

## 目录结构

```
include/bot_ctrl.h    状态表与 4 个状态转移函数声明
src/bot_ctrl.c        状态转移规则（leftest / mid / rightest / single）与状态名
src/main.c            礼枪队模拟：双缓冲同步迭代，逐拍打印整队状态
test/test_sync.c      自检程序：逐个规模检查“是否全体在同一拍开火”
Makefile              编译规则（build 放中间文件，run 放可执行文件）
.vscode/              VS Code 跳转、编译任务与调试配置
```

## 编译与运行

默认编译条件：已经将 mingw32-make.exe 和 gcc.exe 的目录添加到环境变量中。
在工程根目录执行：

```
mingw32-make           # 编译全部源码，生成 run/bot_team.exe，中间文件在 build/
mingw32-make run       # 编译并运行 run/bot_team.exe
mingw32-make check     # 编译并运行自检程序 run/test_sync.exe
mingw32-make clean     # 删除 build/*.o 与 run/*.exe
```

不想用 make 时，等价的手工命令是：

```
gcc -std=c99 -Wall -Wextra -Wno-implicit-fallthrough -Iinclude -c src/bot_ctrl.c -o build/bot_ctrl.o
gcc -std=c99 -Wall -Wextra -Wno-implicit-fallthrough -Iinclude -c src/main.c     -o build/main.o
gcc build/bot_ctrl.o build/main.o -o run/bot_team.exe
```

## VS Code 配置说明

| 文件 | 作用 |
| --- | --- |
| `.vscode/c_cpp_properties.json` | 指定 `-Iinclude` 与 gcc 路径，`#include "bot_ctrl.h"`、`get_next_status_mid` 等符号可以 Ctrl+Click 跳转 |
| `.vscode/tasks.json` | `Ctrl+Shift+B` 执行 `mingw32-make`（默认任务），另有 check / clean 任务，报错可直接点击跳转 |
| `.vscode/launch.json` | `F5` 用 gdb 调试 `run/bot_team.exe`，会先自动执行编译任务 |
| `.vscode/settings.json` | 文件编码固定 UTF-8、缩进用 tab（与工程代码风格一致）、隐藏 build/run 生成物 |
| `.vscode/extensions.json` | 推荐安装 C/C++ 扩展（跳转与调试依赖它） |

## 自检结果

`mingw32-make check` 会逐个规模模拟并检查“全体是否在同一拍第一次开火”：

* n = 1~64 全部通过（0 个失败），例如 n=11 于第 32 拍、n=33 于第 102 拍全体同时开火；
* 开火时刻约为 3n 拍，属于经典的“双速波 + 折半递归”解（3n 量级），不是理论最快的 2n-2。

过程中曾卡住的是 n = 9、11、13、15… 这类含奇数长度分队的规模，原因是中间将军只反射了一侧的手掌信号；
补上 `MIRR_BM`（两侧同时反射）后全部通过。

## 已知问题

1. `src/main.c` 里为保证能编译运行，改过 4 处（现在仍在文件里）：
   * `bot_team_init` 的形参改为 `bot_status_t* buffers[2]`（原来与实参类型不匹配，编译报 incompatible pointer type）；
   * `bot_team_init` 补上 `bot_team->n_bots = n_bots;`（原来没保存机器人数量，导致判断与打印都看不到状态）；
   * `bot_team_judge_shot` 改为判断“当前这拍是否**全体**开火”（原来拿指针与状态值比较），并新增 `bot_team_current`；
   * `bot_team_sprint` 改为按当前缓冲打印（原来把指针数组当状态数组用），并把 `sprintf` 目标改成 `buffer + idx`（原来整行被 `"]"` 覆盖）。
2. `main` 的循环加了 `iter_count < 200` 上限，规则异常时不会无限循环。
3. 编译时还有一条警告来自 `bot_team_iter` 里 `i < (bot_team->n_bots - 1)`（`size_t` 与 `int` 比较），
   改成 `i + 1 < bot_team->n_bots` 即可消除，我没有擅自改动这一行。
