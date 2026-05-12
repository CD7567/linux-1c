KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

CC ?= gcc
CFLAGS_TGCTL := -Wall -Wextra -O2

obj-m := minifs.o
minifs-y := src/minifs.o

ifneq ($(DEBUG),)
    ccflags-y += -DDEBUG
endif

.PHONY: default module clean

default: module

module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	$(RM) -r $(PWD)/bin
