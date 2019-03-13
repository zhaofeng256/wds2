#define NFCONF   (*(volatile unsigned int  *)0x4E000000)
#define NFCONT   (*(volatile unsigned int  *)0x4E000004)
#define NFCMMD   (*(volatile unsigned char *)0x4E000008)
#define NFADDR   (*(volatile unsigned char *)0x4E00000C)
#define NFDATA   (*(volatile unsigned char *)0x4E000010)
#define NFSTAT   (*(volatile unsigned char *)0x4E000020)


#define GPHCON   (*(volatile unsigned int  *)0x56000070) //2 << 6 | 2 << 4
#define GPHUP    (*(volatile unsigned int  *)0x56000078) //1 << 3 | 1 << 2


#define ULCON0   (*(volatile unsigned int  *)0x50000000) //0000 0011
#define UCON0    (*(volatile unsigned int  *)0x50000004) //0000 0000 0000 1010
#define UFCON0   (*(volatile unsigned int  *)0x50000008) //0000 0000
#define UMCON0   (*(volatile unsigned int  *)0x5000000C) //0000 0000
#define UTRSTAT0 (*(volatile unsigned char *)0x50000010)
#define UTXH0    (*(volatile unsigned char *)0x50000020)
#define URXH0    (*(volatile unsigned char *)0x50000024)
#define UBRDIV0  (*(volatile unsigned int  *)0x50000028)


void nand_select(void)
{
	NFCONT &= ~(0x1 << 1);
}

void nand_deselect(void)
{
	NFCONT |= (0x1 << 1);
}

void nand_cmd(unsigned char cmd)
{
	volatile int i;
	NFCMMD = cmd;
	for (i = 0; i < 10; i++);
}

void nand_addr(unsigned int addr)
{
	volatile int i;
	volatile unsigned int col = addr % 2048;
	volatile unsigned int row = addr / 2048;

	NFADDR = col & 0xff;
	for (i = 0; i < 10; i++);
	NFADDR = (col >> 8) & 0xf;
	for (i = 0; i < 10; i++);

	NFADDR = row & 0xff;
	for (i = 0; i < 10; i++);
	NFADDR = (row >> 8) & 0xff;
	for (i = 0; i < 10; i++);
	NFADDR = (row >> 16) & 0x1;
	for (i = 0; i < 10; i++);
}

unsigned char nand_data(void)
{
	return NFDATA;
}

unsigned char nand_ready(void)
{
	return NFSTAT & 0x1;
}


void nand_read(unsigned int src, unsigned char *des, unsigned int len)
{
	volatile unsigned int col = src % 2048;
	volatile unsigned int cnt = 0;

	nand_select();

	while (cnt < len) {

		nand_cmd(0x0);
		nand_addr(src);
		nand_cmd(0x30);
		while(!nand_ready());

		for (; col < 2048 && cnt < len; col++) {
			des[cnt++] = nand_data();
			src++;
		}

		col = 0;
	}

	nand_deselect();
}

void nand_init(void)
{
	NFCONF = (0x1 << 8);
	NFCONT = (0x1 << 4 | 0x1 << 1 | 0x1);
}

unsigned char is_boot_from_nand_flash(void)
{
	unsigned int new = 0x12345678;
	unsigned int *addr = (unsigned int *)0;
	unsigned int old = *addr;

	*addr = new;
	if (*addr == new) {
		*addr = old;
		return 1;
	} else {
		return 0;
	}
}


void copy_code_to_sdram(unsigned char *src, unsigned char *des, unsigned int len)
{
	unsigned int i = 0;
	if (is_boot_from_nand_flash()) {
		nand_read((unsigned int)src, des, len);
	} else {
		for (;i < len;i++) {
			des[i] = src[i];
		}
	}
}

void clear_bss(void)
{
	extern int __bss_start, __bss_end;
	unsigned int *start = (unsigned int *)&__bss_start;
	while(start < (unsigned int *)&__bss_end) {
		*start++ = 0;
	}
}

char uart0_ready_send(void)
{
	return UTRSTAT0 & (0x1 << 2);
}

void putc(unsigned char ch)
{
	while(!uart0_ready_send());
	UTXH0 = ch;
}

void puts(char *str)
{
	while(*str)
		putc(*str++);
}

void put_hex(unsigned int val)
{
	int i = 0;
	unsigned char ch = 0;

	puts("0x");

	for (; i < 8; i++) {
		ch = (val >> (28 - i*4)) & 0xf;
		if (ch < 10)
			putc(ch + '0');
		else
			putc(ch + 'A' - 10);
	}
}

void uart0_init(void)
{
	GPHCON &= ~(0xf << 4);
	GPHCON |= (0xa << 4);
	GPHUP |= (0x3 << 2);

	ULCON0 = 0x3;
	UCON0 = 0x5;
	UFCON0 = 0;
	UMCON0 = 0;
	UBRDIV0 = 50000000/(115200*16) - 1;
}
