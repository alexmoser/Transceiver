################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CFG_SRCS += \
../packetTx.cfg 

CMD_SRCS += \
../CC2650STK.cmd 

C_SRCS += \
../CC2650STK.c \
../ccfg.c \
../packetTx.c 

GEN_CMDS += \
./configPkg/linker.cmd 

GEN_FILES += \
./configPkg/linker.cmd \
./configPkg/compiler.opt 

GEN_MISC_DIRS += \
./configPkg/ 

C_DEPS += \
./CC2650STK.d \
./ccfg.d \
./packetTx.d 

GEN_OPTS += \
./configPkg/compiler.opt 

OBJS += \
./CC2650STK.obj \
./ccfg.obj \
./packetTx.obj 

GEN_MISC_DIRS__QUOTED += \
"configPkg/" 

OBJS__QUOTED += \
"CC2650STK.obj" \
"ccfg.obj" \
"packetTx.obj" 

C_DEPS__QUOTED += \
"CC2650STK.d" \
"ccfg.d" \
"packetTx.d" 

GEN_FILES__QUOTED += \
"configPkg/linker.cmd" \
"configPkg/compiler.opt" 

C_SRCS__QUOTED += \
"../CC2650STK.c" \
"../ccfg.c" \
"../packetTx.c" 


