CFILE:=nor
obj-m:=${CFILE}.o
KERNEL_SOURCE=/home/zf/work/system/linux-2.6.22.6
TOOL_CHAIN=/home/zf/work/cross_compile/gcc-3.4.5-glibc-2.3.6/bin/arm-linux-
PWD=$(shell pwd)
all:
	make ARCH=arm CROSS_COMPILE=$(TOOL_CHAIN) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) modules
	chmod 777 ${CFILE}.ko
	sudo cp ${CFILE}.ko /nfs/s3c2440/${CFILE}.ko -f
clean:
	make ARCH=arm CROSS_COMPILE=$(TOOL_CHAIN) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) clean
	rm Module.symvers
