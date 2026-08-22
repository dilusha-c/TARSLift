################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../AGV_Communication/agv_stm32_c.c 

OBJS += \
./AGV_Communication/agv_stm32_c.o 

C_DEPS += \
./AGV_Communication/agv_stm32_c.d 


# Each subdirectory must supply rules for building sources it contributes
AGV_Communication/%.o AGV_Communication/%.su AGV_Communication/%.cyclo: ../AGV_Communication/%.c AGV_Communication/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I"F:/Projects/AGV/TARSLift_AGV/AGV_Communication" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-AGV_Communication

clean-AGV_Communication:
	-$(RM) ./AGV_Communication/agv_stm32_c.cyclo ./AGV_Communication/agv_stm32_c.d ./AGV_Communication/agv_stm32_c.o ./AGV_Communication/agv_stm32_c.su

.PHONY: clean-AGV_Communication

