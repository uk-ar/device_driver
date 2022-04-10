CFILES = main.c sub.c

obj-m := hello.o
hello-objs :=$(CFILES:.c=.o)

ccflags-y += -std=gnu99 -Wall -Wno-declaration-after-statement

ARM_ARCH := ARCH=arm64
ARM_CC := CROSS_COMPILE=aarch64-buildroot-linux-uclibc-
KERNELDIR := /home/ubuntu/ubuntu/buildroot-2022.02/output/build/linux-5.15.18

PWD := $(shell pwd)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
#make $(ARM_ARCH) $(ARM_CC) -C $(KERNELDIR) M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
#make $(ARM_ARCH) $(ARM_CC) -C $(KERNELDIR) M=$(PWD) clean
