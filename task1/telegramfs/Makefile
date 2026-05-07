KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

CC ?= gcc
CFLAGS_TGCTL := -Wall -Wextra -O2 -I$(PWD)/include

obj-m := tgfs.o
ccflags-y := -I$(src)/include
tgfs-y := src/core/init.o       \
          src/device/fops.o     \
		  src/device/chrdev.o   \
		  src/chat/chat_store.o

ifneq ($(DEBUG),)
    ccflags-y += -DDEBUG
endif

.PHONY: default module tgctl clean

default: module tgctl

module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

tgctl: tools/tgctl.c
	mkdir -p bin
	$(CC) $(CFLAGS_TGCTL) -o $(PWD)/bin/tgctl $<

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	$(RM) -r $(PWD)/bin
