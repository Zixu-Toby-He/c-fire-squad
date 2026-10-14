#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "bot_ctrl.h"

//  缓冲区
bot_status_t  bots_buffer[64] = {
	BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST,
	BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST,
	BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST,
	BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST,
	BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST,
	BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST,
	BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST,
	BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST, BOT_STATUS_REST,
};

//  礼枪队
typedef struct
{
	uint8_t        n_bots;
	uint32_t       iter_count;
	bot_status_t*  bots_buffer[2];
}
bot_team_t;

// 初始化（n_bots > 0）
static void bot_team_init(bot_team_t* bot_team, uint8_t n_bots, bot_status_t* buffers[2])
{
	bot_team->iter_count     = 0;
	bot_team->n_bots         = n_bots;
	bot_team->bots_buffer[0] = buffers[0];
	bot_team->bots_buffer[1] = buffers[1];
	for (size_t i = 0; i < n_bots; i++)
	{
		bot_team->bots_buffer[0][i] = BOT_STATUS_REST;
		bot_team->bots_buffer[1][i] = BOT_STATUS_REST;
	}
}

// 开始命令
static void bot_team_start(bot_team_t* bot_team)
{
	bot_team->bots_buffer[0][0] = BOT_STATUS_1;
	bot_team->bots_buffer[1][0] = BOT_STATUS_1;
	bot_team->iter_count++;
}

// 迭代
static void bot_team_iter(bot_team_t* bot_team)
{
	switch (bot_team->n_bots)
	{
		case 1:
			bot_team->bots_buffer[bot_team->iter_count & 0x1][0] = get_next_status_single(bot_team->bots_buffer[~(bot_team->iter_count) & 0x1][0]);
		case 0:
			break;
		default:
			for (size_t i = 1; i < (bot_team->n_bots - 1); i++)
			{
				bot_team->bots_buffer[bot_team->iter_count & 0x1][i] = get_next_status_mid(
					bot_team->bots_buffer[~(bot_team->iter_count) & 0x1][i],
					bot_team->bots_buffer[~(bot_team->iter_count) & 0x1][i - 1],
					bot_team->bots_buffer[~(bot_team->iter_count) & 0x1][i + 1]
				);
			}
		case 2:
			bot_team->bots_buffer[bot_team->iter_count & 0x1][0                   ] = get_next_status_leftest  (bot_team->bots_buffer[~(bot_team->iter_count) & 0x1][0                   ], bot_team->bots_buffer[~(bot_team->iter_count) & 0x1][1                   ]);
			bot_team->bots_buffer[bot_team->iter_count & 0x1][bot_team->n_bots - 1] = get_next_status_rightest (bot_team->bots_buffer[~(bot_team->iter_count) & 0x1][bot_team->n_bots - 1], bot_team->bots_buffer[~(bot_team->iter_count) & 0x1][bot_team->n_bots - 2]);
			break;
	}
	bot_team->iter_count++;
}

// 判断开火
// 当前一拍的状态放在 bots_buffer[(iter_count - 1) & 0x1] 里
static bot_status_t* bot_team_current(bot_team_t* bot_team)
{
	return bot_team->bots_buffer[(bot_team->iter_count - 1) & 0x1];
}

// 只有全体机器人在同一拍都进入开火状态，才算开火
static bool bot_team_judge_shot(bot_team_t* bot_team)
{
	bot_status_t* current = bot_team_current(bot_team);
	for (size_t i = 0; i < bot_team->n_bots; i++)
	{
		if (current[i] != BOT_STATUS_SHOOT)
		{
			return false;
		}
	}
	return true;
}

static void bot_team_sprint(char* buffer, bot_team_t* bot_team)
{
	bot_status_t* current = bot_team_current(bot_team);
	int idx = 0;
	idx = sprintf(buffer, "step %3i: [ ", bot_team->iter_count);
	for (size_t i = 0; i < bot_team->n_bots; i++)
	{
		idx += sprintf(buffer + idx, "%8s, ", convert_status_to_name(current[i]));
	}
	if (bot_team_judge_shot(bot_team))
	{
		idx = sprintf(buffer + idx, "]    # FIRE!!!!");
	}
	else
	{
		idx = sprintf(buffer + idx, "]");
	}
}

int main()
{
	bot_status_t*  buffers[2] = { &(bots_buffer[0]), &(bots_buffer[32]) };
	bot_team_t     bot_team = {0};
	char           print_buffer[1024];
#define N_TESTS   8
	uint8_t        test_bot_nums[N_TESTS] = { 1, 2, 3, 4, 7, 8, 11, 12 };

	// 2 个机器人
	for (size_t i = 0; i < N_TESTS; i++)
	{
		puts("");
		puts("---------------");
		printf("%u bots\n", test_bot_nums[i]);
		bot_team_init(&bot_team, test_bot_nums[i], buffers);
		bot_team_start(&bot_team);
		bot_team_sprint(print_buffer, &bot_team);
		puts(print_buffer);
		// 加一个步数上限：规则还没跑通的规模不会无限循环
		while (!bot_team_judge_shot(&bot_team) && (bot_team.iter_count < 200))
		{
			bot_team_iter(&bot_team);
			bot_team_sprint(print_buffer, &bot_team);
			puts(print_buffer);
		}
		puts("");
		puts("---------------");
		puts("");
	}
}
