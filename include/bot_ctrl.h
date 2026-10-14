#ifndef BOT_CTRL_H
#define BOT_CTRL_H

// 机器人礼枪队（同步开火问题 / Firing Squad Synchronization Problem）
//
// 约定：
//   * 每个机器人只能看见自己与左右邻居（端点只能看见唯一的邻居）；
//   * 全体机器人同步（同一步）更新状态；
//   * 初始时刻只有 0 号机器人处于“指令”状态，其余为“休止”；
//   * 通信规则：休止的机器人，若左右邻居都休止，则必须保持休止
//     （否则“全体一起变化”这种无意义的解就成立了）；
//   * 目标：所有机器人在同一时刻第一次进入“开火”状态。
//
// 本实现采用经典的“双速波＋折半递归”方案（McCarthy/Minsky 思路）：
//   1. 每个“段”的将军同时发出两列向右的波：
//        快波（1 格/步）与慢波（3 步/格，即速度 1/3）；
//   2. 快波到达段的右墙后反弹成向左的快波，与向右的慢波在段的
//      正中相撞（速度比 1:1/3 使相遇点恰好是中点）；
//   3. 相撞处产生新的将军：向左右两个半段各发一对波，递归下去；
//   4. 递归深度和每层耗时对各机器人相同，因此最后所有机器人
//      同时进入“预备开火”，再同时开火。
typedef enum
{
	BOT_STATUS_REST    = 0,  // 待命：未参与同步，也是分队中的普通机器人
	BOT_STATUS_1       = 1,  // 动作1，即第一个机器人收到指令后即刻的动作
	BOT_STATUS_EMIT_R  = 2,  // 将军发令：向右发出手掌信号（1 拍）
	BOT_STATUS_EMIT_L  = 3,  // 将军发令：向左发出手掌信号（1 拍）
	BOT_STATUS_EMIT_B  = 4,  // 将军发令：左右同时发出手掌信号（1 拍）
	BOT_STATUS_GEN_R1  = 5,  // 将军（向右带队）腿部信号第 1 拍
	BOT_STATUS_GEN_R2  = 6,  // 将军（向右带队）腿部信号第 2 拍
	BOT_STATUS_GEN_R3  = 7,  // 将军（向右带队）腿部信号第 3 拍
	BOT_STATUS_GEN_L1  = 8,  // 将军（向左带队）腿部信号第 1 拍
	BOT_STATUS_GEN_L2  = 9,  // 将军（向左带队）腿部信号第 2 拍
	BOT_STATUS_GEN_L3  = 10, // 将军（向左带队）腿部信号第 3 拍
	BOT_STATUS_GEN_B1  = 11, // 将军（左右同时带队）腿部信号第 1 拍
	BOT_STATUS_GEN_B2  = 12, // 将军（左右同时带队）腿部信号第 2 拍
	BOT_STATUS_GEN_B3  = 13, // 将军（左右同时带队）腿部信号第 3 拍
	BOT_STATUS_GEN     = 14, // 将军：发令完毕，静默，充当镜面
	BOT_STATUS_HAND_R  = 15, // 手掌信号：向右传播（1 格/拍）
	BOT_STATUS_HAND_L  = 16, // 手掌信号：向左传播（1 格/拍）
	BOT_STATUS_HAND_RM = 17, // 手掌信号：由镜面反射出来的向右信号
	BOT_STATUS_HAND_LM = 18, // 手掌信号：由镜面反射出来的向左信号
	BOT_STATUS_MIRR_RM = 19, // 中间“将军”镜面反射出来的向右信号（随后回到静默将军）
	BOT_STATUS_MIRR_LM = 20, // 中间“将军”镜面反射出来的向左信号
	BOT_STATUS_MIRR_BM = 21, // 同一面“将军”镜面左右两侧同时来信号：两侧各反射一份
	BOT_STATUS_LEG_R1  = 22, // 腿部信号：向右传播第 1 拍
	BOT_STATUS_LEG_R2  = 23, // 腿部信号：向右传播第 2 拍
	BOT_STATUS_LEG_R3  = 24, // 腿部信号：向右传播第 3 拍
	BOT_STATUS_LEG_L1  = 25, // 腿部信号：向左传播第 1 拍
	BOT_STATUS_LEG_L2  = 26, // 腿部信号：向左传播第 2 拍
	BOT_STATUS_LEG_L3  = 27, // 腿部信号：向左传播第 3 拍
	BOT_STATUS_PRE     = 28, // 预备开火：最深层所有机器人同步进入
	BOT_STATUS_SHOOT   = 29, // 开火（所有动作的最大值）
}
bot_status_t;


// 中间段机器人状态转移
bot_status_t get_next_status_mid(bot_status_t self_status, bot_status_t left_status, bot_status_t right_status);

// 左端点机器人状态转移
bot_status_t get_next_status_leftest(bot_status_t self_status, bot_status_t right_status);

// 右端点机器人状态转移
bot_status_t get_next_status_rightest(bot_status_t self_status, bot_status_t left_status);

// 只有一个机器人的状态转移
bot_status_t get_next_status_single(bot_status_t self_status);

char* convert_status_to_name(bot_status_t);

#endif
