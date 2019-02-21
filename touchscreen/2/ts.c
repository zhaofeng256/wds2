#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>

struct adc_regs {
	u32 adccon;
	u32 adctsc;
	u32 adcdly;
	u32 adcdat0;
	u32 adcdat1;
	u32 adcupdn;
};


static struct adc_regs * gp_adc_regs;
static struct clk *gp_adc_clk;
static 	struct timer_list ts_timer;

static irqreturn_t up_down_interrupt(int irq, void *dev_id)
{
	if (gp_adc_regs->adcdat0 & (1<<15)) {
		/* enter wait for stylus down interrupt */
		gp_adc_regs->adctsc = 0xd3;
	} else {
		/* AUTO PST */
		gp_adc_regs->adctsc = (0x1 << 3 | 0x1 << 2);

		/* start adc */
		gp_adc_regs->adccon |= 0x1;
	}

	return IRQ_HANDLED;
}

static irqreturn_t adc_interrupt(int irq, void *dev_id)
{
	u32 val_x = 0, val_y = 0;
	if (gp_adc_regs->adcdat0 & (1<<15)) {
		/* enter wait for stylus down interrupt */
		gp_adc_regs->adctsc = 0xd3;
	} else {
		val_x = gp_adc_regs->adcdat0 & 0x3ff;
		val_y = gp_adc_regs->adcdat1 & 0x3ff;
		printk("x : %d y : %d\n", val_x, val_y);
		/* enter wait for stylus up interrupt */
		gp_adc_regs->adctsc = 0x1d3;

		mod_timer(&ts_timer, jiffies + HZ/100);
	}

	return IRQ_HANDLED;
}

void ts_timer_func(unsigned long v)
{
	if (gp_adc_regs->adcdat0 & (1<<15)) {
		/* enter wait for stylus down interrupt */
		gp_adc_regs->adctsc = 0xd3;
	} else {
		/* AUTO PST */
		gp_adc_regs->adctsc = (0x1 << 3 | 0x1 << 2);

		/* start adc */
		gp_adc_regs->adccon |= 0x1;
	}
}

static int __init ts_init(void)
{
	int ret = 0;

	/* enable adc clock */
	gp_adc_clk = clk_get(NULL, "adc");
	if (!gp_adc_clk || IS_ERR(gp_adc_clk)) {
		printk(KERN_ERR "failed to get lcd clock source\n");
		return -ENOENT;
	}
	clk_enable(gp_adc_clk);

	/* config adc registers */
	gp_adc_regs = ioremap(0x58000000, sizeof(struct adc_regs));
	if (!gp_adc_regs) {
		printk("adc memory map error\n");
		return -ENOMEM;
	}

	/*
		PRSCEN : 1 << 14
		PRSCVL : 49
	*/
	gp_adc_regs->adccon = 0x1 << 14 | 49 << 6;

	/* interval delay*/
	gp_adc_regs->adcdly = 0xffff;

	/* register stylus irq */
	ret = request_irq(IRQ_TC, up_down_interrupt,IRQF_SAMPLE_RANDOM, "ts_pen", NULL);
	if (ret) {
		printk("request stylus irq failed : %d\n", ret);
		return ret;
	}


	/* register adc irq */
	ret = request_irq(IRQ_ADC, adc_interrupt,IRQF_SAMPLE_RANDOM, "adc", NULL);
	if (ret) {
		printk("request adc irq failed : %d\n", ret);
		return ret;
	}


	init_timer(&ts_timer);
	ts_timer.expires  = jiffies + HZ/100;
	ts_timer.function = ts_timer_func;
	ts_timer.data	  = NULL;
	add_timer(&ts_timer);


	/* wait for stylus down interrupt */
	gp_adc_regs->adctsc = 0xd3;

	return 0;
}

static __exit void ts_exit(void)
{
	disable_irq(IRQ_TC);
	disable_irq(IRQ_ADC);
	free_irq(IRQ_TC, NULL);
	free_irq(IRQ_ADC, NULL);
	iounmap(gp_adc_regs);
	clk_put(gp_adc_clk);
	clk_disable(gp_adc_clk);
}

module_init(ts_init);
module_exit(ts_exit);
MODULE_LICENSE("GPL");
