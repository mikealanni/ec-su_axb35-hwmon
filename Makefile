# Makefile for ec_su_axb35

obj-m += ec_su_axb35.o

KERNEL_BUILD ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KERNEL_BUILD) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_BUILD) M=$(PWD) clean
