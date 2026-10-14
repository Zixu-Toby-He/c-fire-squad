#include "bot_ctrl.h"

// 机器人礼枪队同步开火（Firing Squad Synchronization）实现
//
// 机制（与参考视频所述一致）：
//   1. 每个分队的“将军”先发令，同时向外发出两路信号：
//        · 手掌信号：每拍走 1 格  —— 队内最快的“光速”信号
//          （EMIT_x 表示发令那一拍，HAND_R / HAND_L 表示行进，HAND_xM / MIRR_xM 表示刚被镜面反射）；
//        · 腿部信号：3 拍走 1 格 —— 速度为 1/3 的慢信号
//          （将军自己走 GEN_x1→GEN_x2→GEN_x3 三拍，随后前锋由 LEG_x1→x2→x3 接力前进）；
//   2. 手掌信号在分队另一端（镜面：队列端点机器人，或上一级的“将军”）反射回来，
//      与继续外行的腿部信号在自己分队的中点迎面相遇；
//   3. 相遇处产生新的将军：
//        · 偶数长度分队（腿部信号第 1 拍相遇）：相撞的两台机器人各成为一个半段的将军；
//        · 奇数长度分队（腿部信号第 3 拍相遇）：只有正中间那台机器人成为将军，
//          并向左右两边同时发令（它同时属于两个子分队，扮演双重角色）；
//   4. 两半分队长度相同，并由镜像同步地递归重复（上一级端点成为新的镜面），
//      于是每一层的耗时对每台机器人都一致；
//   5. 递归到“分队只剩两台机器人”时，腿部信号第 1 拍就撞上刚从镜面反射回来的
//      手掌信号：两台机器人同时进入“预备开火”；此时全队列同时处于暂备开火，
//      下一拍全体一起开火。

// ------------------------------------------------------------ 小工具

// 是否处于“将军”的静默状态（发令完毕，充当镜面）
static int is_idle_general(bot_status_t status)
{
	return (status == BOT_STATUS_GEN);
}

// 是否处于将军的“腿部信号三拍”
static int is_general_phase(bot_status_t status)
{
	switch (status)
	{
		case BOT_STATUS_GEN_R1:
		case BOT_STATUS_GEN_R2:
		case BOT_STATUS_GEN_R3:
		case BOT_STATUS_GEN_L1:
		case BOT_STATUS_GEN_L2:
		case BOT_STATUS_GEN_L3:
		case BOT_STATUS_GEN_B1:
		case BOT_STATUS_GEN_B2:
		case BOT_STATUS_GEN_B3:
			return 1;
		default:
			return 0;
	}
}

// 是否起镜面作用（发令中的将军与静默将军都可以当镜子）
static int is_mirror(bot_status_t status)
{
	return (is_idle_general(status) || is_general_phase(status));
}

// 是否处于“发令/发令后三拍”的忙碌状态（需要逐拍推进）
static int is_general_busy(bot_status_t status)
{
	return (is_general_phase(status) || status == BOT_STATUS_EMIT_R ||
		status == BOT_STATUS_EMIT_L || status == BOT_STATUS_EMIT_B);
}

// 是否是“刚被镜面反射”的手掌信号（只在镜面那一格停留一拍）
static int is_fresh_reflected_hand(bot_status_t status)
{
	return (status == BOT_STATUS_HAND_RM || status == BOT_STATUS_HAND_LM ||
		status == BOT_STATUS_MIRR_RM || status == BOT_STATUS_MIRR_LM ||
		status == BOT_STATUS_MIRR_BM);
}

// 是否是“从左边打过来”的手掌信号（含刚发令的那一拍）
static int is_hand_from_left(bot_status_t status)
{
	return (status == BOT_STATUS_HAND_R || status == BOT_STATUS_EMIT_R ||
		status == BOT_STATUS_EMIT_B);
}

// 是否是“从右边打过来”的手掌信号（含刚发令的那一拍）
static int is_hand_from_right(bot_status_t status)
{
	return (status == BOT_STATUS_HAND_L || status == BOT_STATUS_EMIT_L ||
		status == BOT_STATUS_EMIT_B);
}

// 是否是普通行进中的手掌信号
static int is_moving_hand(bot_status_t status)
{
	return (status == BOT_STATUS_HAND_R || status == BOT_STATUS_HAND_L);
}

// 将军处于第 1 拍（腿部信号前锋就在将军自己身上）
static int is_general_phase1(bot_status_t status)
{
	return (status == BOT_STATUS_GEN_R1 || status == BOT_STATUS_GEN_L1 ||
		status == BOT_STATUS_GEN_B1);
}

// 将军处于第 3 拍
static int is_general_phase3(bot_status_t status)
{
	return (status == BOT_STATUS_GEN_R3 || status == BOT_STATUS_GEN_L3 ||
		status == BOT_STATUS_GEN_B3);
}

// 将军处于第 1 拍并且是“向右带队”（镜面在右侧）
static int is_general_phase1_right(bot_status_t status)
{
	return (status == BOT_STATUS_GEN_R1 || status == BOT_STATUS_GEN_B1);
}

// 将军处于第 1 拍并且是“向左带队”（镜面在左侧）
static int is_general_phase1_left(bot_status_t status)
{
	return (status == BOT_STATUS_GEN_L1 || status == BOT_STATUS_GEN_B1);
}

// 将军处于第 3 拍并且是“向右带队”
static int is_general_phase3_right(bot_status_t status)
{
	return (status == BOT_STATUS_GEN_R3 || status == BOT_STATUS_GEN_B3);
}

// 将军处于第 3 拍并且是“向左带队”
static int is_general_phase3_left(bot_status_t status)
{
	return (status == BOT_STATUS_GEN_L3 || status == BOT_STATUS_GEN_B3);
}

// 腿部信号前锋（向右 / 向左）
static int is_leg_right(bot_status_t status)
{
	return (status == BOT_STATUS_LEG_R1 || status == BOT_STATUS_LEG_R2 ||
		status == BOT_STATUS_LEG_R3);
}

static int is_leg_left(bot_status_t status)
{
	return (status == BOT_STATUS_LEG_L1 || status == BOT_STATUS_LEG_L2 ||
		status == BOT_STATUS_LEG_L3);
}

// 腿部信号前锋处于第 1 拍 / 第 3 拍
static int is_leg_phase1(bot_status_t status)
{
	return (status == BOT_STATUS_LEG_R1 || status == BOT_STATUS_LEG_L1);
}

static int is_leg_phase3(bot_status_t status)
{
	return (status == BOT_STATUS_LEG_R3 || status == BOT_STATUS_LEG_L3);
}

// 将军第 1 拍 -> 第 2 拍；第 2 拍 -> 第 3 拍；第 3 拍 -> 静默
static bot_status_t general_advance(bot_status_t status)
{
	switch (status)
	{
		case BOT_STATUS_EMIT_R:
			return BOT_STATUS_GEN_R1;
		case BOT_STATUS_EMIT_L:
			return BOT_STATUS_GEN_L1;
		case BOT_STATUS_EMIT_B:
			return BOT_STATUS_GEN_B1;
		case BOT_STATUS_GEN_R1:
			return BOT_STATUS_GEN_R2;
		case BOT_STATUS_GEN_L1:
			return BOT_STATUS_GEN_L2;
		case BOT_STATUS_GEN_B1:
			return BOT_STATUS_GEN_B2;
		case BOT_STATUS_GEN_R2:
			return BOT_STATUS_GEN_R3;
		case BOT_STATUS_GEN_L2:
			return BOT_STATUS_GEN_L3;
		case BOT_STATUS_GEN_B2:
			return BOT_STATUS_GEN_B3;
		default:
			return BOT_STATUS_GEN;
	}
}

// ---------------------------------------------------- 普通（中间）机器人

// 中间机器人的状态转移：只看 left/self/right 三个状态
bot_status_t get_next_status_mid(bot_status_t self_status, bot_status_t left_status, bot_status_t right_status)
{
	// ---- 1. 将军发令后的三拍（腿部信号从将军身上出发） ----
	if (is_general_phase1_right(self_status) && is_fresh_reflected_hand(right_status))
	{
		// 分队只剩两台机器人：腿部信号第 1 拍就撞上镜面反射的手掌信号
		return BOT_STATUS_PRE;
	}
	if (is_general_phase1_left(self_status) && is_fresh_reflected_hand(left_status))
	{
		return BOT_STATUS_PRE;
	}
	if (is_general_phase3_right(self_status) && right_status == BOT_STATUS_HAND_L)
	{
		// 奇数长度分队：正中间的是手掌信号那一格，自己回到静默将军（当镜面）
		return BOT_STATUS_GEN;
	}
	if (is_general_phase3_left(self_status) && left_status == BOT_STATUS_HAND_R)
	{
		return BOT_STATUS_GEN;
	}
	if (is_general_busy(self_status))
	{
		return general_advance(self_status);
	}
	if (is_idle_general(self_status))
	{
		// 静默将军充当镜面：把打过来的手掌信号反射回去。
		// 奇数长度分队留下的中间将军，左右两侧的子分队会同时打到它身上，
		// 这时两侧都要反射，否则有一侧的手掌信号会消失。
		if (is_hand_from_left(left_status) && is_hand_from_right(right_status))
		{
			return BOT_STATUS_MIRR_BM;
		}
		if (is_hand_from_left(left_status))
		{
			return BOT_STATUS_MIRR_LM;
		}
		if (is_hand_from_right(right_status))
		{
			return BOT_STATUS_MIRR_RM;
		}
		// 相邻将军的三拍结束：腿部信号从自己这里接力继续前进
		if (left_status == BOT_STATUS_LEG_R3 || is_general_phase3_right(left_status))
		{
			return BOT_STATUS_LEG_R1;
		}
		if (right_status == BOT_STATUS_LEG_L3 || is_general_phase3_left(right_status))
		{
			return BOT_STATUS_LEG_L1;
		}
		return BOT_STATUS_GEN;
	}

	// ---- 2. 手掌信号的传播 ----
	if (self_status == BOT_STATUS_HAND_L)
	{
		if (is_general_phase3_right(left_status))
		{
			return BOT_STATUS_EMIT_B; // 奇数长度分队：自己就是正中间，成为双重将军
		}
		if (left_status == BOT_STATUS_LEG_R1)
		{
			return BOT_STATUS_EMIT_R; // 偶数长度分队：自己成为右半段将军
		}
		if (left_status == BOT_STATUS_LEG_R3)
		{
			return BOT_STATUS_EMIT_B; // 奇数长度分队：自己成为双重将军
		}
		return BOT_STATUS_REST;
	}
	if (self_status == BOT_STATUS_HAND_R)
	{
		if (is_general_phase3_left(right_status))
		{
			return BOT_STATUS_EMIT_B;
		}
		if (right_status == BOT_STATUS_LEG_L1)
		{
			return BOT_STATUS_EMIT_L;
		}
		if (right_status == BOT_STATUS_LEG_L3)
		{
			return BOT_STATUS_EMIT_B;
		}
		return BOT_STATUS_REST;
	}
	if (is_fresh_reflected_hand(self_status))
	{
		// 镜面上的反射信号：若身边正是第 1 拍的腿部信号前锋，则双方同时预备开火
		if (is_general_phase1_right(left_status) || is_general_phase1_left(right_status))
		{
			return BOT_STATUS_PRE;
		}
		if (self_status == BOT_STATUS_MIRR_RM || self_status == BOT_STATUS_MIRR_LM ||
			self_status == BOT_STATUS_MIRR_BM)
		{
			return BOT_STATUS_GEN; // 中间将军当镜子：反射完继续当将军
		}
		return BOT_STATUS_REST; // 端点当镜子：反射完回到待命
	}

	// ---- 3. 腿部信号前锋的三拍推进与相遇 ----
	if (self_status == BOT_STATUS_LEG_R1)
	{
		if (right_status == BOT_STATUS_HAND_L)
		{
			return BOT_STATUS_EMIT_L; // 偶数长度分队：自己成为左半段将军
		}
		return BOT_STATUS_LEG_R2;
	}
	if (self_status == BOT_STATUS_LEG_R2)
	{
		return BOT_STATUS_LEG_R3;
	}
	if (self_status == BOT_STATUS_LEG_R3)
	{
		return BOT_STATUS_REST; // 正中间的机器人被手掌信号选为双重将军，自己回到待命
	}
	if (self_status == BOT_STATUS_LEG_L1)
	{
		if (left_status == BOT_STATUS_HAND_R)
		{
			return BOT_STATUS_EMIT_R; // 偶数长度分队：自己成为右半段将军
		}
		return BOT_STATUS_LEG_L2;
	}
	if (self_status == BOT_STATUS_LEG_L2)
	{
		return BOT_STATUS_LEG_L3;
	}
	if (self_status == BOT_STATUS_LEG_L3)
	{
		return BOT_STATUS_REST;
	}

	// ---- 4. 待命机器人：接收发令、手掌信号、腿部信号 ----
	if (self_status == BOT_STATUS_REST)
	{
		// 将军在身旁发令：手掌信号起步
		if (left_status == BOT_STATUS_EMIT_R || left_status == BOT_STATUS_EMIT_B)
		{
			return BOT_STATUS_HAND_R;
		}
		if (right_status == BOT_STATUS_EMIT_L || right_status == BOT_STATUS_EMIT_B)
		{
			return BOT_STATUS_HAND_L;
		}
		// 手掌信号经过自己
		if (left_status == BOT_STATUS_HAND_R || left_status == BOT_STATUS_MIRR_RM ||
			left_status == BOT_STATUS_MIRR_BM)
		{
			return BOT_STATUS_HAND_R;
		}
		if (right_status == BOT_STATUS_HAND_L || right_status == BOT_STATUS_MIRR_LM ||
			right_status == BOT_STATUS_MIRR_BM)
		{
			return BOT_STATUS_HAND_L;
		}
		// 镜面反射出来的信号开始行进：反射信号退化为普通手掌信号
		if (right_status == BOT_STATUS_HAND_LM)
		{
			return BOT_STATUS_HAND_L;
		}
		if (left_status == BOT_STATUS_HAND_RM)
		{
			return BOT_STATUS_HAND_R;
		}
		// 腿部信号越过自己继续前进
		if (left_status == BOT_STATUS_LEG_R3 || is_general_phase3_right(left_status))
		{
			return BOT_STATUS_LEG_R1;
		}
		if (right_status == BOT_STATUS_LEG_L3 || is_general_phase3_left(right_status))
		{
			return BOT_STATUS_LEG_L1;
		}
		return BOT_STATUS_REST;
	}

	// ---- 5. 全体预备开火 -> 全体开火 ----
	if (self_status == BOT_STATUS_PRE)
	{
		if (left_status == BOT_STATUS_PRE && right_status == BOT_STATUS_PRE)
		{
			return BOT_STATUS_SHOOT;
		}
		return BOT_STATUS_PRE;
	}
	if (self_status == BOT_STATUS_SHOOT)
	{
		return BOT_STATUS_SHOOT;
	}
	return self_status;
}

// ------------------------------------------------------------ 左端机器人

// 左端机器人：初始将军 + 左侧镜面，只看 self/right
bot_status_t get_next_status_leftest(bot_status_t self_status, bot_status_t right_status)
{
	// 初始指令：向右发令
	if (self_status == BOT_STATUS_1)
	{
		return BOT_STATUS_EMIT_R;
	}
	// 分队只剩两台机器人：腿部信号第 1 拍撞上镜面反射的手掌信号
	if ((is_general_phase1_right(self_status) || is_general_phase1_left(self_status)) &&
		is_fresh_reflected_hand(right_status))
	{
		return BOT_STATUS_PRE;
	}
	if (is_general_phase3_left(self_status) && right_status == BOT_STATUS_HAND_R)
	{
		return BOT_STATUS_GEN;
	}
	if (is_general_busy(self_status))
	{
		return general_advance(self_status);
	}
	// 左侧镜面：把从右边打来的手掌信号反射回右边
	if (right_status == BOT_STATUS_HAND_L || right_status == BOT_STATUS_EMIT_L ||
		right_status == BOT_STATUS_EMIT_B)
	{
		return BOT_STATUS_HAND_RM;
	}
	if (self_status == BOT_STATUS_HAND_RM)
	{
		if (is_general_phase1_left(right_status) || is_general_phase1_right(right_status))
		{
			return BOT_STATUS_PRE;
		}
		return BOT_STATUS_GEN;
	}
	if (self_status == BOT_STATUS_MIRR_RM)
	{
		return BOT_STATUS_GEN;
	}
	if (self_status == BOT_STATUS_HAND_R)
	{
		return BOT_STATUS_REST;
	}
	if (self_status == BOT_STATUS_LEG_R1)
	{
		if (right_status == BOT_STATUS_HAND_L)
		{
			return BOT_STATUS_EMIT_L;
		}
		return BOT_STATUS_LEG_R2;
	}
	if (self_status == BOT_STATUS_LEG_R2)
	{
		return BOT_STATUS_LEG_R3;
	}
	if (self_status == BOT_STATUS_LEG_R3)
	{
		return BOT_STATUS_REST;
	}
	if (self_status == BOT_STATUS_HAND_LM)
	{
		return BOT_STATUS_REST;
	}
	if (self_status == BOT_STATUS_REST)
	{
		if (right_status == BOT_STATUS_EMIT_L || right_status == BOT_STATUS_EMIT_B)
		{
			return BOT_STATUS_HAND_L;
		}
		if (right_status == BOT_STATUS_HAND_L || right_status == BOT_STATUS_MIRR_LM ||
			right_status == BOT_STATUS_HAND_LM)
		{
			return BOT_STATUS_HAND_L;
		}
		if (right_status == BOT_STATUS_LEG_L3)
		{
			return BOT_STATUS_LEG_L1;
		}
		return BOT_STATUS_REST;
	}
	if (self_status == BOT_STATUS_GEN)
	{
		return BOT_STATUS_GEN;
	}
	if (self_status == BOT_STATUS_PRE)
	{
		if (right_status == BOT_STATUS_PRE)
		{
			return BOT_STATUS_SHOOT;
		}
		return BOT_STATUS_PRE;
	}
	if (self_status == BOT_STATUS_SHOOT)
	{
		return BOT_STATUS_SHOOT;
	}
	return BOT_STATUS_REST;
}

// ------------------------------------------------------------ 右端机器人

// 右端机器人：右侧镜面，只看 self/left
bot_status_t get_next_status_rightest(bot_status_t self_status, bot_status_t left_status)
{
	// 分队只剩两台机器人：腿部信号第 1 拍撞上镜面反射的手掌信号
	if ((is_general_phase1_right(self_status) || is_general_phase1_left(self_status)) &&
		is_fresh_reflected_hand(left_status))
	{
		return BOT_STATUS_PRE;
	}
	if (is_general_phase3_right(self_status) && left_status == BOT_STATUS_HAND_L)
	{
		return BOT_STATUS_GEN;
	}
	if (is_general_busy(self_status))
	{
		return general_advance(self_status);
	}
	// 右侧镜面：把从左边打来的手掌信号反射回左边
	if (left_status == BOT_STATUS_HAND_R || left_status == BOT_STATUS_EMIT_R ||
		left_status == BOT_STATUS_EMIT_B)
	{
		return BOT_STATUS_HAND_LM;
	}
	if (self_status == BOT_STATUS_HAND_LM)
	{
		if (is_general_phase1_right(left_status) || is_general_phase1_left(left_status))
		{
			return BOT_STATUS_PRE;
		}
		return BOT_STATUS_GEN;
	}
	if (self_status == BOT_STATUS_MIRR_LM)
	{
		return BOT_STATUS_GEN;
	}
	if (self_status == BOT_STATUS_HAND_L)
	{
		return BOT_STATUS_REST;
	}
	if (self_status == BOT_STATUS_LEG_L1)
	{
		if (left_status == BOT_STATUS_HAND_R)
		{
			return BOT_STATUS_EMIT_R;
		}
		return BOT_STATUS_LEG_L2;
	}
	if (self_status == BOT_STATUS_LEG_L2)
	{
		return BOT_STATUS_LEG_L3;
	}
	if (self_status == BOT_STATUS_LEG_L3)
	{
		return BOT_STATUS_REST;
	}
	if (self_status == BOT_STATUS_HAND_RM)
	{
		return BOT_STATUS_REST;
	}
	if (self_status == BOT_STATUS_REST)
	{
		if (left_status == BOT_STATUS_EMIT_R || left_status == BOT_STATUS_EMIT_B)
		{
			return BOT_STATUS_HAND_R;
		}
		if (left_status == BOT_STATUS_HAND_R || left_status == BOT_STATUS_MIRR_RM ||
			left_status == BOT_STATUS_HAND_RM)
		{
			return BOT_STATUS_HAND_R;
		}
		if (left_status == BOT_STATUS_LEG_R3)
		{
			return BOT_STATUS_LEG_R1;
		}
		return BOT_STATUS_REST;
	}
	if (self_status == BOT_STATUS_GEN)
	{
		return BOT_STATUS_GEN;
	}
	if (self_status == BOT_STATUS_PRE)
	{
		if (left_status == BOT_STATUS_PRE)
		{
			return BOT_STATUS_SHOOT;
		}
		return BOT_STATUS_PRE;
	}
	if (self_status == BOT_STATUS_SHOOT)
	{
		return BOT_STATUS_SHOOT;
	}
	return BOT_STATUS_REST;
}

// -------------------------------------------------------- 只有一台机器人

// 只有一台机器人时直接开火
bot_status_t get_next_status_single(bot_status_t self_status)
{
	return BOT_STATUS_SHOOT;
}

// ------------------------------------------------------------ 状态名字

char* convert_status_to_name(bot_status_t status)
{
	switch (status)
	{
		case BOT_STATUS_REST:
			return "rest";
		case BOT_STATUS_1:
			return "cmd";
		case BOT_STATUS_EMIT_R:
			return "emitR";
		case BOT_STATUS_EMIT_L:
			return "emitL";
		case BOT_STATUS_EMIT_B:
			return "emitB";
		case BOT_STATUS_GEN_R1:
			return "genR1";
		case BOT_STATUS_GEN_R2:
			return "genR2";
		case BOT_STATUS_GEN_R3:
			return "genR3";
		case BOT_STATUS_GEN_L1:
			return "genL1";
		case BOT_STATUS_GEN_L2:
			return "genL2";
		case BOT_STATUS_GEN_L3:
			return "genL3";
		case BOT_STATUS_GEN_B1:
			return "genB1";
		case BOT_STATUS_GEN_B2:
			return "genB2";
		case BOT_STATUS_GEN_B3:
			return "genB3";
		case BOT_STATUS_GEN:
			return "gen";
		case BOT_STATUS_HAND_R:
			return "handR";
		case BOT_STATUS_HAND_L:
			return "handL";
		case BOT_STATUS_HAND_RM:
			return "handRm";
		case BOT_STATUS_HAND_LM:
			return "handLm";
		case BOT_STATUS_MIRR_RM:
			return "mirrRm";
		case BOT_STATUS_MIRR_LM:
			return "mirrLm";
		case BOT_STATUS_MIRR_BM:
			return "mirrBm";
		case BOT_STATUS_LEG_R1:
			return "legR1";
		case BOT_STATUS_LEG_R2:
			return "legR2";
		case BOT_STATUS_LEG_R3:
			return "legR3";
		case BOT_STATUS_LEG_L1:
			return "legL1";
		case BOT_STATUS_LEG_L2:
			return "legL2";
		case BOT_STATUS_LEG_L3:
			return "legL3";
		case BOT_STATUS_PRE:
			return "PRE";
		case BOT_STATUS_SHOOT:
			return "SHOOT";
		default:
			return "?";
	}
}
