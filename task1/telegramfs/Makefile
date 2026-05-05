KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m := tgfs.o
ccflags-y := -I$(src)/include
tgfs-y := src/core/init.o     \
          src/device/fops.o   \
		  src/device/chrdev.o

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
