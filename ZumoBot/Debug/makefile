################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include Source/FreeRTOS-Products/FreeRTOS/portable/MemMang/subdir.mk
-include Source/FreeRTOS-Products/FreeRTOS/portable/GCC/ARM_CM3/subdir.mk
-include Source/FreeRTOS-Products/FreeRTOS-Plus-IO/Device/LPC17xx/subdir.mk
-include Source/FreeRTOS-Products/FreeRTOS-Plus-IO/Common/subdir.mk
-include Source/FreeRTOS-Products/FreeRTOS-Plus-CLI/subdir.mk
-include Source/FreeRTOS-Products/FreeRTOS/subdir.mk
-include Source/Examples/UART/subdir.mk
-include Source/Examples/SSP/subdir.mk
-include Source/Examples/I2C/subdir.mk
-include Source/Examples/GPIO/subdir.mk
-include Source/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: ZumoBot.axf

# Tool invocations
ZumoBot.axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: MCU Linker'
	arm-none-eabi-gcc -nostdlib -L"/Users/JABV/Documents/LPCXpresso_8.0.0/ZumoBot/lpc17xx.cmsis.driver.library/Debug" -L"/Users/JABV/Documents/LPCXpresso_8.0.0/ZumoBot/CMSISv2p00_LPC17xx/Debug" -Xlinker -Map=ZumoBot.map -Xlinker --gc-sections -mcpu=cortex-m3 -mthumb -T "FreeRTOS-Plus-Demo-1_Debug.ld" -o "ZumoBot.axf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) ZumoBot.axf
	-@echo ' '

post-build:
	-@echo 'Performing post-build steps'
	-arm-none-eabi-size ZumoBot.axf; # arm-none-eabi-objcopy -O binary ZumoBot.axf ZumoBot.bin ; checksum -p LPC1769 -d ZumoBot.bin;
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY: post-build

-include ../makefile.targets
