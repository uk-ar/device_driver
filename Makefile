CFILES = main.c sub.c scull/scull.c fifo.c

DEBUG=y

obj-m := hello.o
hello-objs :=$(CFILES:.c=.o)
ifeq ($(DEBUG),y)
  ccflags-y += -std=gnu99 -DSCULL_DEBUG -Wall -Wno-declaration-after-statement
else
  ccflags-y += -std=gnu99 -Wall -Wno-declaration-after-statement
endif

#ARM_ARCH := ARCH=arm64
#ARM_CC := CROSS_COMPILE=aarch64-buildroot-linux-uclibc-
#KERNELDIR := /home/ubuntu/buildroot-2022.02.1/output/build/linux-5.15.18
#KERNELDIR := /lib/modules/$(shell uname -r)/build
#KERNELDIR := /usr/src/linux-headers-5.11.0-49

all:
	make -C '$(LINUX_DIR)'  M='$(PWD)' modules
#	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
#	make $(ARM_ARCH) $(ARM_CC) -C $(KERNELDIR) M=$(PWD) modules
clean:
	make -C '$(LINUX_DIR)'  M='$(PWD)' clean
#	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
#	make $(ARM_ARCH) $(ARM_CC) -C $(KERNELDIR) M=$(PWD) clean
