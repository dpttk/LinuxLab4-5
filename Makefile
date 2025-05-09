obj-m += int_stack.o

all: kernel_stack
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean: clean_client
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
kernel_stack: kernel_stack.c
	gcc -Wall -o kernel_stack kernel_stack.c

clean_client:
	rm -f kernel_stack
