KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

CC ?= gcc

obj-m := kbd_monitor.o
kbd_monitor-y := src/kbd_monitor.o

.PHONY: default module clean

default: module

module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
