# Makefile for MyRTOS QEMU MPS2-AN385 Demo
# 使用 arm-none-eabi-gcc 工具链

# 工程名称
TARGET = myrtos_qemu_demo

# 工具链
CC = arm-none-eabi-gcc
AS = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE = arm-none-eabi-size

# 目录定义
MYRTOS_DIR = ../../MyRTOS
BUILD_DIR = build

# 源文件 (完整 Shell/Init 支持)
C_SOURCES = \
	src/main.c \
	src/startup.c \
	CMSIS/system_CMSDK_CM3.c \
	$(MYRTOS_DIR)/arch/ARM_CM3/ARM_CM3.c \
	$(MYRTOS_DIR)/boot/MyRTOS_Boot.c \
	$(MYRTOS_DIR)/kernel/myrtos_init.c \
	$(MYRTOS_DIR)/kernel/myrtos_memory.c \
	$(MYRTOS_DIR)/kernel/myrtos_scheduler.c \
	$(MYRTOS_DIR)/kernel/myrtos_task.c \
	$(MYRTOS_DIR)/kernel/myrtos_tick.c \
	$(MYRTOS_DIR)/kernel/myrtos_queue.c \
	$(MYRTOS_DIR)/kernel/myrtos_semaphore.c \
	$(MYRTOS_DIR)/kernel/myrtos_mutex.c \
	$(MYRTOS_DIR)/kernel/myrtos_signal.c \
	$(MYRTOS_DIR)/kernel/myrtos_extension.c \
	$(MYRTOS_DIR)/services/MyRTOS_IO.c \
	$(MYRTOS_DIR)/services/MyRTOS_AsyncIO.c \
	$(MYRTOS_DIR)/services/MyRTOS_Log.c \
	$(MYRTOS_DIR)/services/MyRTOS_Timer.c \
	$(MYRTOS_DIR)/services/MyRTOS_Monitor.c \
	$(MYRTOS_DIR)/services/MyRTOS_Utils.c \
	$(MYRTOS_DIR)/services/MyRTOS_VTS.c \
	$(MYRTOS_DIR)/services/MyRTOS_Process.c \
	$(MYRTOS_DIR)/programs/init_main.c \
	$(MYRTOS_DIR)/programs/log_main.c \
	$(MYRTOS_DIR)/programs/shell_main.c \
	$(MYRTOS_DIR)/programs/shell_core.c \
	$(MYRTOS_DIR)/programs/shell_builtin.c \
	$(MYRTOS_DIR)/programs/shell_log.c \
	$(MYRTOS_DIR)/programs/shell_monitor.c \
	$(MYRTOS_DIR)/programs/shell_platform.c \
	$(MYRTOS_DIR)/programs/shell_process.c \
	$(MYRTOS_DIR)/programs/shell_process_main.c \
	$(MYRTOS_DIR)/programs/shell_sysinfo.c \
	platform/platform_hw.c \
	platform/platform_console.c \
	platform/platform_timer.c \
	platform/platform_hooks.c \
	platform/platform_error_handler.c

# 头文件目录
C_INCLUDES = \
	-Iconfig \
	-ICMSIS \
	-Iplatform/include \
	-I$(MYRTOS_DIR)/include \
	-I$(MYRTOS_DIR)/private \
	-I$(MYRTOS_DIR)/kernel/include \
	-I$(MYRTOS_DIR)/boot/include \
	-I$(MYRTOS_DIR)/services/include \
	-I$(MYRTOS_DIR)/programs/include

# CPU 和 FPU 配置
CPU = -mcpu=cortex-m3
FPU =
FLOAT-ABI =

# 汇编标志
ASFLAGS = $(CPU) -mthumb $(C_INCLUDES) -g

# C 编译标志
CFLAGS = $(CPU) -mthumb $(C_INCLUDES) -g -O2
CFLAGS += -Wall -Wextra
CFLAGS += -Wno-discarded-qualifiers -Wno-unused-variable
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -std=gnu11

# 链接标志
LDSCRIPT = linker.ld
LDFLAGS = $(CPU) -mthumb
LDFLAGS += -T$(LDSCRIPT)
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map
LDFLAGS += --specs=nosys.specs
LDFLAGS += -lc -lm -lnosys

# 目标文件列表
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

# 默认目标
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin size

# 创建目标文件
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "CC $<"
	@$(CC) -c $(CFLAGS) $< -o $@

# 链接
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	@echo "LD $@"
	@$(LD) $(OBJECTS) $(LDFLAGS) -o $@

# 生成二进制文件
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	@echo "OBJCOPY $@"
	@$(OBJCOPY) -O binary $< $@

# 创建构建目录
$(BUILD_DIR):
	@mkdir -p $@

# 显示大小
size: $(BUILD_DIR)/$(TARGET).elf
	@echo ""
	@$(SIZE) $<

# 反汇编
disasm: $(BUILD_DIR)/$(TARGET).elf
	@$(OBJDUMP) -d $< > $(BUILD_DIR)/$(TARGET).dis

# 清理
clean:
	@rm -rf $(BUILD_DIR)

# 运行 QEMU
run: $(BUILD_DIR)/$(TARGET).elf
	qemu-system-arm -M mps2-an385 -cpu cortex-m3 -nographic -kernel $<

# 运行 QEMU（带 GDB 服务器）
debug: $(BUILD_DIR)/$(TARGET).elf
	qemu-system-arm -M mps2-an385 -cpu cortex-m3 -nographic -kernel $< -s -S

.PHONY: all clean size disasm run debug
