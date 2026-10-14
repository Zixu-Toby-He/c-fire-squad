# 在 build 里面生成包括 .o 在内的中间文件，在 run 里面生成 .exe 文件
#
# 常用命令（在工程根目录执行）：
#   mingw32-make          编译全部源码，生成 run/bot_team.exe
#   mingw32-make run      编译并运行 run/bot_team.exe
#   mingw32-make check    编译并运行自检程序（检查是否全体同时开火）
#   mingw32-make clean    删除 build 与 run 里的中间文件和可执行文件

CC      := gcc
# main.c 里的 switch 是有意贯穿的（case 1 -> case 0、default -> case 2），故屏蔽该警告
CFLAGS  := -std=c99 -Wall -Wextra -Wno-implicit-fallthrough -Iinclude

BUILD   := build
RUN     := run
TARGET  := $(RUN)/bot_team.exe
CHECK   := $(RUN)/test_sync.exe

SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,$(BUILD)/%.o,$(SOURCES))

all: $(TARGET)

$(BUILD)/%.o: src/%.c
	@if not exist "$(BUILD)" mkdir "$(BUILD)"
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	@if not exist "$(RUN)" mkdir "$(RUN)"
	$(CC) $(OBJECTS) -o $@

run: $(TARGET)
	$(TARGET)

# 自检：不需要 main.c，直接检查 get_next_status_* 的规则
$(CHECK): $(BUILD)/bot_ctrl.o test/test_sync.c
	@if not exist "$(RUN)" mkdir "$(RUN)"
	$(CC) $(CFLAGS) $(BUILD)/bot_ctrl.o test/test_sync.c -o $@

check: $(CHECK)
	$(CHECK)

clean:
	@if exist "$(BUILD)" del /q "$(BUILD)\*.o"
	@if exist "$(RUN)" del /q "$(RUN)\*.exe"

.PHONY: all run check clean
