################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../sources/common.c \
../sources/server.c 

OBJS += \
./sources/common.o \
./sources/server.o 

C_DEPS += \
./sources/common.d \
./sources/server.d 


# Each subdirectory must supply rules for building sources it contributes
sources/%.o: ../sources/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


