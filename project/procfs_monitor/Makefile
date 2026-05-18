KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

CC ?= gcc

obj-m := procfs_monitor.o
ccflags-y := -I$(src)/include
procfs_monitor-y := src/core/init.o      \
                    src/procfs/procfs.o

.PHONY: default module clean

default: module

module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
