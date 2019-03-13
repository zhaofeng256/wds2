TOOL_CHAIN=/home/zf/work/cross_compile/gcc-3.4.5-glibc-2.3.6/bin/arm-linux-
GCC=$(TOOL_CHAIN)gcc
LD=$(TOOL_CHAIN)ld
OBJCOPY=$(TOOL_CHAIN)objcopy
OBJDUMP=$(TOOL_CHAIN)objdump
NAME=boot
OBJS=start.o init.o boot.o
CFLAGS := -Wall -fno-builtin

$(NAME).bin : $(OBJS)
	$(LD) -T $(NAME).lds -o $(NAME).elf $^
	$(OBJCOPY) -O binary -S $(NAME).elf $@
	$(OBJDUMP) -D -m arm $(NAME).elf > $(NAME).dis

%.o : %.S
	$(GCC) $(CFLAGS) -c $< -o $@

%.o : %.c
	$(GCC) $(CFLAGS) -c $< -o $@

clean:
	rm *.o *.dis *.bin *.elf
