# 在 build 里面生成包括 .o 在内的中间文件，在 run 里面生成 .exe 文件
#
# 常用命令（在工程目录里执行）：
#   mingw32-make          编译全部源码，生成 run/bot_team.exe
#   mingw32-make all      同上（显式指定默认目标）
#   mingw32-make run      编译并运行 run/bot_team.exe
#   mingw32-make clean    删除 build/ 与 run/ 两个目录（下次编译会自动重建）

# 指定命令行为 PowerShell（必须带 .exe，否则 make 会静默退回 cmd）
SHELL       := powershell.exe
.SHELLFLAGS := -NoProfile -Command

CC      := gcc
# main.c 里的 switch 是有意贯穿的（case 1 -> case 0、default -> case 2），故屏蔽该警告
CFLAGS  := -std=c99 -Wall -Wextra -Wno-implicit-fallthrough -O2

# 当前绝对路径，即相当于 pathlib.Path(__file__).parent
# make 的 abspath/dir 在 Windows 上给出反斜杠，这里统一换成正斜杠
CURRENT_PATH := $(patsubst %/,%,$(subst \,/,$(dir $(abspath $(lastword $(MAKEFILE_LIST))))))

# make 把配方命令交给 shell 时按当前代码页解释，含中文的绝对路径会变成乱码，
# 于是 gcc / PowerShell 都找不到文件。所以：配方里只用相对路径，并要求在工程目录里执行。
ifeq ($(subst \,/,$(abspath .)),$(CURRENT_PATH))
else
$(error 请在工程目录里执行 make（或先 cd 到 Makefile 所在目录），否则中文路径会乱码)
endif

SRC_DIR := src
INC_DIR := include
BUILD   := build
RUN     := run

TARGET  := $(RUN)/bot_team.exe
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/%.o,$(SOURCES))
# 头文件改动也要触发重新编译
HEADERS := $(wildcard $(INC_DIR)/*.h)

CFLAGS  += -I$(INC_DIR)

all: $(TARGET)
	@Write-Output "build finished."

$(BUILD)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@if (-not (Test-Path -LiteralPath "$(BUILD)")) { New-Item -ItemType Directory -Path "$(BUILD)" | Out-Null }
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	@if (-not (Test-Path -LiteralPath "$(RUN)")) { New-Item -ItemType Directory -Path "$(RUN)" | Out-Null }
	$(CC) $(OBJECTS) -o $@

run: $(TARGET)
	& "./$(TARGET)"

clean:
	@if (Test-Path -LiteralPath "$(BUILD)") { Remove-Item -LiteralPath "$(BUILD)" -Recurse -Force }
	@if (Test-Path -LiteralPath "$(RUN)") { Remove-Item -LiteralPath "$(RUN)" -Recurse -Force }
	@Write-Output "clean finished."

.PHONY: all run clean
