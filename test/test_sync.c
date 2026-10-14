#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "bot_ctrl.h"

// 同步开火自检程序：
//   对 n = 1..MAX_BOTS 逐个模拟，检查“所有机器人是否在同一拍第一次进入开火状态”，
//   并验证在开火之前没有任何机器人提前开火。

#define MAX_BOTS 64
#define MAX_STEPS 800

static bot_status_t 缓冲[2][MAX_BOTS];

// 单步推进：读上一拍，写这一拍
static void 推进一步(bot_status_t* 旧, bot_status_t* 新, uint8_t 个数)
{
	if (个数 == 1)
	{
		新[0] = get_next_status_single(旧[0]);
		return;
	}
	新[0] = get_next_status_leftest(旧[0], 旧[1]);
	for (uint8_t i = 1; i < 个数 - 1; i++)
	{
		新[i] = get_next_status_mid(旧[i], 旧[i - 1], 旧[i + 1]);
	}
	新[个数 - 1] = get_next_status_rightest(旧[个数 - 1], 旧[个数 - 2]);
}

// 打印一行状态
static void 打印一行(bot_status_t* 状态, uint8_t 个数)
{
	for (uint8_t i = 0; i < 个数; i++)
	{
		printf("%s ", convert_status_to_name(状态[i]));
	}
	puts("");
}

// 检查是否全体开火
static bool 全体开火(bot_status_t* 状态, uint8_t 个数)
{
	for (uint8_t i = 0; i < 个数; i++)
	{
		if (状态[i] != BOT_STATUS_SHOOT)
		{
			return false;
		}
	}
	return true;
}

// 检查是否有机器人已经开火
static bool 有开火(bot_status_t* 状态, uint8_t 个数)
{
	for (uint8_t i = 0; i < 个数; i++)
	{
		if (状态[i] == BOT_STATUS_SHOOT)
		{
			return true;
		}
	}
	return false;
}

// 模拟一个规模的队伍，返回开火步数（-1 表示失败）
static int 模拟(uint8_t 个数, bool 打印)
{
	bot_status_t* 旧 = 缓冲[0];
	bot_status_t* 新 = 缓冲[1];
	for (uint8_t i = 0; i < 个数; i++)
	{
		旧[i] = BOT_STATUS_REST;
	}
	旧[0] = BOT_STATUS_1;
	if (打印)
	{
		printf("step %3d: ", 0);
		打印一行(旧, 个数);
	}
	for (int 步 = 1; 步 <= MAX_STEPS; 步++)
	{
		推进一步(旧, 新, 个数);
		if (打印)
		{
			printf("step %3d: ", 步);
			打印一行(新, 个数);
		}
		if (全体开火(新, 个数))
		{
			return 步;
		}
		if (有开火(新, 个数))
		{
			return -1; // 有人提前开火
		}
		bot_status_t* 交换 = 旧;
		旧 = 新;
		新 = 交换;
	}
	return -1;
}

int main(int argc, char* argv[])
{
	bool 打印 = ((argc > 1) && (argv[1][0] == 'v'));
	uint8_t 只测 = 0;
	if (argc > 2)
	{
		只测 = (uint8_t)atoi(argv[2]);
	}
	for (uint8_t n = 1; n <= MAX_BOTS; n++)
	{
		if (只测 != 0 && n != 只测)
		{
			continue;
		}
		int 步 = 模拟(n, 打印);
		if (步 < 0)
		{
			printf("n=%2u  失败（未能在 %d 拍内全体同时开火）\n", n, MAX_STEPS);
		}
		else
		{
			printf("n=%2u  全体同时开火于第 %3d 拍（理论最快 2n-2 = %d）\n",
				n, 步, (n == 1) ? 0 : (2 * n - 2));
		}
	}
	return 0;
}
