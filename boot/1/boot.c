#include "setup.h"

extern void puts(char *str);
extern void put_hex(unsigned int val);
extern void uart0_init(void);
extern void nand_read(unsigned int src, unsigned char *des, unsigned int len);

static struct tag *params = (struct tag *)0x30000100;

static void setup_start_tag (void)
{
	params->hdr.tag = ATAG_CORE;
	params->hdr.size = tag_size (tag_core);

	params->u.core.flags = 0;
	params->u.core.pagesize = 0;
	params->u.core.rootdev = 0;

	params = tag_next(params);
}

static void setup_memory_tags (void)
{
	params->hdr.tag = ATAG_MEM;
	params->hdr.size = tag_size (tag_mem32);

	params->u.mem.start = 0x30000000;
	params->u.mem.size = 64 * 1024 * 1024;

	params = tag_next (params);
}

int strlen(char *str)
{
	int len = 0;
	while ((str[len++]));
	return len;
}

void strcpy(char *des, char *src)
{
	while (*des++ = *src++);		
}

static void setup_commandline_tag (char *cmdline)
{
	params->hdr.tag = ATAG_CMDLINE;
	params->hdr.size = (sizeof (struct tag_header) + strlen (cmdline) + 4) >> 2;

	strcpy ((char *)(params->u.cmdline.cmdline), cmdline);

	params = tag_next (params);
}

static void setup_end_tag (void)
{
	params->hdr.tag = ATAG_NONE;
	params->hdr.size = 0;
}

int main(void)
{
	#define MACH_TYPE_S3C2440              362
	void (*theKernel)(int, int, unsigned int);

	puts("copy kernel from nand\r\n");

	nand_read((unsigned int)0x60040, (unsigned char *)0x30008000, (unsigned int)0x200000);
	
	puts("set boot params\r\n");
	setup_start_tag();
	setup_memory_tags();
	setup_commandline_tag("noinitrd root=/dev/nfs nfsroot=192.168.1.106:/nfs/s3c2440,nolock,nfsvers=3 "
	"ip=192.168.1.17:192.168.1.106:192.168.1.1:255.255.255.0::eth0:off init=/linuxrc console=ttySAC0");
	setup_end_tag();
	
	puts("booting kernel ...\r\n");
	theKernel = (void (*)(int, int, unsigned int))0x30008000;
	theKernel(0, MACH_TYPE_S3C2440, 0x30000100);
	
	puts("error\n");
	return -1;
}
