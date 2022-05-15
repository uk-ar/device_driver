CFILES = main.c sub.c scull/scull.c fifo.c

DEBUG=y

obj-m := hello.o
hello-objs :=$(CFILES:.c=.o)
ifeq ($(DEBUG),y)
  ccflags-y += -std=gnu99 -DSCULL_DEBUG -Wall -Wno-declaration-after-statement
else
  ccflags-y += -std=gnu99 -Wall -Wno-declaration-after-statement
endif

# obj-m += $(addsuffix .o, $(notdir $(basename $(wildcard $(BR2_EXTERNAL_HELLO_PATH)/*.c))))
# ccflags-y := -DDEBUG -g -std=gnu99 -Wno-declaration-after-statement

# .PHONY: all clean

# obj-m += $(addsuffix .o, $(notdir $(basename $(wildcard $(BR2_EXTERNAL_HELLO_PATH)/*.c))))
# ccflags-y := -DDEBUG -g -std=gnu99 -Wno-declaration-after-statement

# .PHONY: all clean

# all:
#     $(MAKE) -C '$(LINUX_DIR)' M='$(PWD)' modules

# clean:
#     $(MAKE) -C '$(LINUX_DIR)' M='$(PWD)' clean

#ARM_ARCH := ARCH=arm64
#ARM_CC := CROSS_COMPILE=aarch64-buildroot-linux-uclibc-
#KERNELDIR := /home/ubuntu/buildroot-2022.02.1/output/build/linux-5.15.18

all:
	make -C '$(LINUX_DIR)'  M='$(PWD)' modules
#	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
#	make $(ARM_ARCH) $(ARM_CC) -C $(KERNELDIR) M=$(PWD) modules
clean:
	make -C '$(LINUX_DIR)'  M='$(PWD)' clean
#	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
#	make $(ARM_ARCH) $(ARM_CC) -C $(KERNELDIR) M=$(PWD) clean
