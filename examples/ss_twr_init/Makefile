# Project Name
TARGET_NAME := ss_twr_init

# Toolchain
TOOLCHAIN_PREFIX ?= arm-none-eabi-
CC      := $(TOOLCHAIN_PREFIX)gcc
AS      := $(TOOLCHAIN_PREFIX)as
LD      := $(TOOLCHAIN_PREFIX)gcc # Use GCC as linker driver
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
SIZE    := $(TOOLCHAIN_PREFIX)size

# Build Directory
BUILD_DIR := _build

# Paths relative to this Makefile
SDK_ROOT := ../../nRF5_SDK_14.2.0
DECA_ROOT := ../../deca_driver
FREERTOS_SRC := $(SDK_ROOT)/external/freertos/source

# Source Files
C_SOURCES := \
  ./main.c \
  ./ss_init_main.c \
  $(SDK_ROOT)/components/toolchain/system_nrf52.c \
  $(SDK_ROOT)/components/boards/boards.c \
  $(SDK_ROOT)/components/libraries/bsp/bsp.c \
  $(SDK_ROOT)/components/libraries/bsp/bsp_nfc.c \
  $(SDK_ROOT)/components/drivers_nrf/common/nrf_drv_common.c \
  $(SDK_ROOT)/components/drivers_nrf/gpiote/nrf_drv_gpiote.c \
  $(SDK_ROOT)/components/drivers_nrf/spi_master/nrf_drv_spi.c \
  $(SDK_ROOT)/components/drivers_nrf/uart/nrf_drv_uart.c \
  $(SDK_ROOT)/components/drivers_nrf/clock/nrf_drv_clock.c \
  $(SDK_ROOT)/components/libraries/button/app_button.c \
  $(SDK_ROOT)/components/libraries/util/app_error.c \
  $(SDK_ROOT)/components/libraries/util/app_error_weak.c \
  $(SDK_ROOT)/components/libraries/scheduler/app_scheduler.c \
  $(SDK_ROOT)/components/libraries/util/app_util_platform.c \
  $(SDK_ROOT)/components/libraries/util/nrf_assert.c \
  $(SDK_ROOT)/components/libraries/balloc/nrf_balloc.c \
  $(SDK_ROOT)/external/fprintf/nrf_fprintf.c \
  $(SDK_ROOT)/external/fprintf/nrf_fprintf_format.c \
  $(SDK_ROOT)/components/libraries/experimental_memobj/nrf_memobj.c \
  $(SDK_ROOT)/components/libraries/strerror/nrf_strerror.c \
  $(SDK_ROOT)/components/libraries/fifo/app_fifo.c \
  $(SDK_ROOT)/components/libraries/uart/app_uart_fifo.c \
  $(SDK_ROOT)/components/libraries/uart/retarget.c \
  $(SDK_ROOT)/components/libraries/experimental_log/src/nrf_log_backend_rtt.c \
  $(SDK_ROOT)/components/libraries/experimental_log/src/nrf_log_backend_serial.c \
  $(SDK_ROOT)/components/libraries/experimental_log/src/nrf_log_backend_uart.c \
  $(SDK_ROOT)/components/libraries/experimental_log/src/nrf_log_default_backends.c \
  $(SDK_ROOT)/components/libraries/experimental_log/src/nrf_log_frontend.c \
  $(SDK_ROOT)/components/libraries/experimental_log/src/nrf_log_str_formatter.c \
  $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT.c \
  $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT_printf.c \
  $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT_Syscalls_KEIL.c \
  $(DECA_ROOT)/deca_device.c \
  $(DECA_ROOT)/deca_params_init.c \
  $(DECA_ROOT)/port/port_platform.c \
  $(FREERTOS_SRC)/croutine.c \
  $(FREERTOS_SRC)/event_groups.c \
  $(FREERTOS_SRC)/list.c \
  $(FREERTOS_SRC)/queue.c \
  $(FREERTOS_SRC)/tasks.c \
  $(FREERTOS_SRC)/timers.c \
  $(FREERTOS_SRC)/portable/MemMang/heap_1.c \
  $(SDK_ROOT)/components/libraries/timer/app_timer_freertos.c \
  $(FREERTOS_SRC)/portable/CMSIS/nrf52/port_cmsis.c \
  $(FREERTOS_SRC)/portable/CMSIS/nrf52/port_cmsis_systick.c \
  $(FREERTOS_SRC)/portable/GCC/nrf52/port.c \
  ./UART/UART.c

ASM_SOURCES := \
  $(SDK_ROOT)/components/toolchain/gcc/gcc_startup_nrf52.S
  # SES/RTE/thumb_crt0.s # SES specific, startup handled by ses_nrf52_startup.s

# Object files
OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(C_SOURCES)))
OBJECTS += $(patsubst %.s,$(BUILD_DIR)/%.o,$(notdir $(filter %.s,$(ASM_SOURCES))))
OBJECTS += $(patsubst %.S,$(BUILD_DIR)/%.o,$(notdir $(filter %.S,$(ASM_SOURCES))))

# Include directories
INC_DIRS := \
  . \
  ../config \
  ../UART \
  ../../boards \
  ../../.. \
  $(DECA_ROOT) \
  $(DECA_ROOT)/port \
  $(SDK_ROOT)/components \
  $(SDK_ROOT)/components/boards \
  $(SDK_ROOT)/components/device \
  $(SDK_ROOT)/components/drivers_nrf/clock \
  $(SDK_ROOT)/components/drivers_nrf/common \
  $(SDK_ROOT)/components/drivers_nrf/delay \
  $(SDK_ROOT)/components/drivers_nrf/gpiote \
  $(SDK_ROOT)/components/drivers_nrf/hal \
  $(SDK_ROOT)/components/drivers_nrf/nrf_soc_nosd \
  $(SDK_ROOT)/components/drivers_nrf/spi_master \
  $(SDK_ROOT)/components/drivers_nrf/uart \
  $(SDK_ROOT)/components/libraries/atomic \
  $(SDK_ROOT)/components/libraries/balloc \
  $(SDK_ROOT)/components/libraries/bsp \
  $(SDK_ROOT)/components/libraries/button \
  $(SDK_ROOT)/components/libraries/fifo \
  $(SDK_ROOT)/components/libraries/experimental_log \
  $(SDK_ROOT)/components/libraries/experimental_log/src \
  $(SDK_ROOT)/components/libraries/experimental_memobj \
  $(SDK_ROOT)/components/libraries/experimental_section_vars \
  $(SDK_ROOT)/components/libraries/scheduler \
  $(SDK_ROOT)/components/libraries/strerror \
  $(SDK_ROOT)/components/libraries/timer \
  $(SDK_ROOT)/components/libraries/util \
  $(SDK_ROOT)/components/libraries/uart \
  $(SDK_ROOT)/components/toolchain \
  $(SDK_ROOT)/components/toolchain/cmsis/include \
  $(SDK_ROOT)/external/fprintf \
  $(SDK_ROOT)/external/segger_rtt \
  $(SDK_ROOT)/external/freertos/source/include \
  $(SDK_ROOT)/external/freertos/source/portable/GCC/nrf52 \
  $(SDK_ROOT)/external/freertos/source/portable/CMSIS/nrf52 \
  $(SDK_ROOT)/external/freertos/config

CFLAGS := -Wall -Wextra -g -O3
CFLAGS += -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
# Defines from SES project
CFLAGS += -DBOARD_DW1001_DEV -DBSP_SIMPLE -DCONFIG_GPIO_AS_PINRESET -DFLOAT_ABI_HARD -DNRF52 -DNRF52832_XXAA -DNRF52_PAN_74 -DSWI_DISABLE0
CFLAGS += $(addprefix -I,$(INC_DIRS))
CFLAGS += -std=c99 # Or c11

# Linker flags
LDFLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
LDFLAGS += -L$(SDK_ROOT)/components/toolchain/gcc
LDFLAGS += -T$(SDK_ROOT)/components/toolchain/gcc/nrf52_xxaa.ld # Use the specific chip linker script that defines memory regions
LDFLAGS += --specs=nosys.specs # Use no system call specifications
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET_NAME).map # Generate map file
LDFLAGS += -lm # Link math library

# Assembler flags
ASFLAGS := -g -mcpu=cortex-m4 -mthumb

# Default target
all: $(BUILD_DIR)/$(TARGET_NAME).hex $(BUILD_DIR)/$(TARGET_NAME).elf

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Rule to link object files into ELF file
$(BUILD_DIR)/$(TARGET_NAME).elf: $(OBJECTS) | $(BUILD_DIR)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@
	@echo ""
	@echo "== Size Info =="
	$(SIZE) $@
	@echo ""

# Rule to create HEX file from ELF file
$(BUILD_DIR)/$(TARGET_NAME).hex: $(BUILD_DIR)/$(TARGET_NAME).elf | $(BUILD_DIR)
	$(OBJCOPY) -O ihex $< $@

# Rule to compile C source files into object files
define CompileC
$(BUILD_DIR)/$(notdir $(basename $1).o): $1 | $(BUILD_DIR)
	@echo Compiling $$<
	$(CC) $(CFLAGS) -c $$< -o $$@
endef
$(foreach src,$(C_SOURCES),$(eval $(call CompileC,$(src))))

# Rule to assemble Assembly source files into object files (.s files)
define CompileS
$(BUILD_DIR)/$(notdir $(basename $1).o): $1 | $(BUILD_DIR)
	@echo Assembling $$<
	$(AS) $(ASFLAGS) $$< -o $$@
endef
$(foreach src,$(filter %.s,$(ASM_SOURCES)),$(eval $(call CompileS,$(src))))

# Rule to compile .S assembly files (uppercase extension used by GCC)
define CompileCapitalS
$(BUILD_DIR)/$(notdir $(basename $1).o): $1 | $(BUILD_DIR)
	@echo Assembling $$<
	$(CC) $(CFLAGS) -c $$< -o $$@
endef
$(foreach src,$(filter %.S,$(ASM_SOURCES)),$(eval $(call CompileCapitalS,$(src))))

# Clean target
clean:
	rm -rf $(BUILD_DIR)

# Phony targets
.PHONY: all clean flash

# Flash target (requires specific programmer setup)
flash: $(BUILD_DIR)/$(TARGET_NAME).hex
	@echo "Flashing target... (Please configure JLink or OpenOCD commands)"
# Example using JLinkExe (requires JLink software installed):
#	JLinkExe -device NRF52832_XXAA -if SWD -speed 4000 -autoconnect 1 -CommandFile jlink_flash.script $(BUILD_DIR)/$(TARGET_NAME).hex
# Create jlink_flash.script containing:
# r
# loadfile $(1)
# r
# g
# exit

# Rule for finding source files from object files
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(filter %.s,$(ASM_SOURCES))))
vpath %.S $(sort $(dir $(filter %.S,$(ASM_SOURCES))))