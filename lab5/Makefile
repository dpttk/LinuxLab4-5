obj-m += int_stack.o

# Kernel module
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# User-space program
CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic -std=c99

all: kernel_module user_program

kernel_module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user_program: kernel_stack.c
	$(CC) $(CFLAGS) -o kernel_stack kernel_stack.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f kernel_stack

# Installation location
INSTALL_DIR := /lib/modules/$(shell uname -r)/kernel/drivers/usb/misc

install: kernel_module
	install -d $(INSTALL_DIR)
	install -m 644 int_stack.ko $(INSTALL_DIR)
	/sbin/depmod -a

uninstall:
	rm -f $(INSTALL_DIR)/int_stack.ko
	/sbin/depmod -a

.PHONY: all kernel_module user_program clean install uninstall 