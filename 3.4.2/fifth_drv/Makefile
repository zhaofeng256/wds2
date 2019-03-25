CFILE:=fifth_drv
obj-m:=${CFILE}.o
KERNEL_SOURCE=/home/zf/work/system/linux-3.4.2
TOOL_CHAIN=/home/zf/work/cross_compile/4.3.2/bin/arm-none-linux-gnueabi-
PWD=$(shell pwd)
TESTFILE:=fifthdrvtest
all:
	make ARCH=arm CROSS_COMPILE=$(TOOL_CHAIN) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) modules
	chmod 777 ${CFILE}.ko
	sudo cp ${CFILE}.ko /nfs/s3c2440_new/${CFILE}.ko -f
	$(TOOL_CHAIN)gcc $(TESTFILE).c -o $(TESTFILE)
	chmod 777 $(TESTFILE)
	sudo cp $(TESTFILE) /nfs/s3c2440_new/$(TESTFILE) -f
clean:
	make ARCH=arm CROSS_COMPILE=$(TOOL_CHAIN) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) clean
	rm $(TESTFILE)
