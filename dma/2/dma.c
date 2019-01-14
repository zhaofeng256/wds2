#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <asm/types.h>
#include <asm/io.h>
#include <asm/irq.h>

static int g_dma_major = 0;
static struct class *g_dma_cls = NULL;

static char *g_dma_src = NULL;
static u32 g_dma_src_phys = 0;

static char *g_dma_des = NULL;
static u32 g_dma_des_phys = 0;


#define MEM_CPY_NO_DMA 0
#define MEM_CPY_DMA    1

#define DMA_BUFF_SIZE (512 * 1024)

#define DMA0_BASE_ADDR (0x4B000000)
#define DMA1_BASE_ADDR (0x4B000040)
#define DMA2_BASE_ADDR (0x4B0000B0)
#define DMA3_BASE_ADDR (0x4B0000C0)

struct s3c_dma_regs {
	u32 disrc;
	u32 disrcc;
	u32 didst;
	u32 didstc;
	u32 dcon;
	u32 dstat;
	u32 dcsrc;
	u32 dcdst;
	u32 dmasktrig;
};

static volatile struct s3c_dma_regs *g_dma_addr;
static volatile int ev_dma = 0;
DECLARE_WAIT_QUEUE_HEAD(dma_waitq);

static int s3c_dma_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	memset(g_dma_src, 0xAA, DMA_BUFF_SIZE);
	memset(g_dma_des, 0x55, DMA_BUFF_SIZE);

	switch (cmd) {
	case MEM_CPY_NO_DMA:
	{
		memcpy(g_dma_des, g_dma_src, DMA_BUFF_SIZE);
		if (memcmp(g_dma_des, g_dma_src, DMA_BUFF_SIZE))
			printk("MEM_CPY_NO_DMA error\n");
		else
			printk("MEM_CPY_NO_DMA ok\n");
		break;
	}
	case MEM_CPY_DMA:
	{
		ev_dma = 0;
		memset((void *)g_dma_addr, 0, sizeof(struct s3c_dma_regs));
		g_dma_addr->disrc = g_dma_src_phys;
		g_dma_addr->didst = g_dma_des_phys;
		/* enable interrupt & APB system & whole service mode && autoreload */
		g_dma_addr->dcon = (1 << 30) | (1 << 29) | (1 << 27) | (DMA_BUFF_SIZE << 0);
		g_dma_addr->dmasktrig = (1 << 1) | (1 << 0);

		wait_event_interruptible(dma_waitq, ev_dma);
		if (memcmp(g_dma_des, g_dma_src, DMA_BUFF_SIZE))
			printk("MEM_CPY_DMA error\n");
		else
			printk("MEM_CPY_DMA ok\n");

		break;
	}
	default:
		break;
	}
	return 0;
}

static struct file_operations g_dma_fops = {
	.owner	= THIS_MODULE,
	.ioctl	= s3c_dma_ioctl,
};

static irqreturn_t s3c_dma_irq(int irq, void * devid)
{
	ev_dma = 1;
	wake_up_interruptible(&dma_waitq);
	return IRQ_HANDLED;
}

static int __init s3c_dma_init(void)
{
	if (request_irq(IRQ_DMA3, s3c_dma_irq, 0, "s3c_dma", NULL)) {
		printk("request dma irq error\n");
		return -EBUSY;
	}


	g_dma_src = dma_alloc_writecombine(NULL, DMA_BUFF_SIZE, &g_dma_src_phys, GFP_KERNEL);
	if (NULL == g_dma_src) {
		printk("alloc dma src buf error\n");
		return -ENOMEM;
	}

	g_dma_des = dma_alloc_writecombine(NULL, DMA_BUFF_SIZE, &g_dma_des_phys, GFP_KERNEL);
	if (NULL == g_dma_des) {
		dma_free_writecombine(NULL, DMA_BUFF_SIZE, g_dma_src, g_dma_src_phys);
		printk("alloc dma des buf error\n");
		return -ENOMEM;
	}


	g_dma_major = register_chrdev(0, "s3c_dma", &g_dma_fops);
	g_dma_cls = class_create(THIS_MODULE, "s3c_dma");
	class_device_create(g_dma_cls, NULL, MKDEV(g_dma_major, 0), NULL, "dma");

	g_dma_addr = ioremap(DMA3_BASE_ADDR, sizeof(struct s3c_dma_regs));

	return 0;
}

static void __exit s3c_dma_exit(void)
{
	iounmap(g_dma_addr);

	class_device_destroy(g_dma_cls, MKDEV(g_dma_major, 0));
	class_destroy(g_dma_cls);
	unregister_chrdev(g_dma_major, "s3c_dma");

	dma_free_writecombine(NULL, DMA_BUFF_SIZE, g_dma_src, g_dma_src_phys);
	dma_free_writecombine(NULL, DMA_BUFF_SIZE, g_dma_des, g_dma_des_phys);
}

module_init(s3c_dma_init);
module_exit(s3c_dma_exit);

MODULE_LICENSE("GPL");
