#include <linux/module.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>


static u32 *gpfcon;
static u32 *gpfdat;
static u32 *gpfup;
static u32 *gpgcon;
static u32 *gpgdat;
static u32 *gpgup;

/*
	EINT0  GPF0
	EINT2  GPF2
	EINT11 GPG3
	EINT19 GPG11

*/
static irqreturn_t key_irq_hdl(int irq_n, void *param)
{
	printk("%s irq %d\n", __func__, irq_n);

	switch (irq_n) {
		case IRQ_EINT0:
			printk("k1 = %d\n", *gpfdat & 0x1);
			break;
		case IRQ_EINT2:
			printk("k2 = %d\n", (*gpfdat >> 2) & 0x1);
			break;
		case IRQ_EINT11:
			printk("k3 = %d\n", (*gpgdat >> 3) & 0x1);
			break;
		case IRQ_EINT19:
			printk("k4 = %d\n", (*gpgdat >> 11) & 0x1);
			break;
		default:
			break;
	}

	return IRQ_HANDLED;
}

static __init int key_init(void)
{
	gpfcon = ioremap(0x56000050, 12);
	gpfdat = gpfcon + 1;
	gpfup = gpfcon + 2;

	*gpfcon &= ~((0x3 << 2) | 0x3);
	*gpfcon |= (0x2 << 2 | 0x2);
	*gpfup  |= (0x1 << 2 | 0x1);

	gpgcon = ioremap(0x56000060, 12);
	gpgdat = gpgcon + 1;
	gpgup = gpgcon + 2;

	*gpgcon &= ~((0x3 << 11) | 0x3 << 3);
	*gpgcon = (0x2 << 11 | 0x2 << 3);
	*gpgup |= (0x1 << 11 | 0x1 << 3);


	if (request_irq(IRQ_EINT0, key_irq_hdl, IRQT_BOTHEDGE, "key1", NULL)) {
		return -EAGAIN;
	}
	if (request_irq(IRQ_EINT2, key_irq_hdl, IRQT_BOTHEDGE, "key2", NULL)) {
		return -EAGAIN;
	}
	if (request_irq(IRQ_EINT11, key_irq_hdl, IRQT_BOTHEDGE, "key3", NULL)) {
		return -EAGAIN;
	}
	if (request_irq(IRQ_EINT19, key_irq_hdl, IRQT_BOTHEDGE, "key4", NULL)) {
		return -EAGAIN;
	}

	return 0;
}

static __exit void key_exit(void)
{

	free_irq(IRQ_EINT0, NULL);
	free_irq(IRQ_EINT2, NULL);
	free_irq(IRQ_EINT11, NULL);
	free_irq(IRQ_EINT19, NULL);

	iounmap(gpfcon);
	iounmap(gpgcon);
}

module_init(key_init);
module_exit(key_exit);
MODULE_LICENSE("GPL");

