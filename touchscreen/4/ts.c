#include <linux/module.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/input.h>

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
static 	struct input_dev *idev;



static int s3c_filter_ts(int x[], int y[])
{
#define ERR_LIMIT 10

	int avr_x, avr_y;
	int det_x, det_y;

	avr_x = (x[0] + x[1])/2;
	avr_y = (y[0] + y[1])/2;

	det_x = (x[2] > avr_x) ? (x[2] - avr_x) : (avr_x - x[2]);
	det_y = (y[2] > avr_y) ? (y[2] - avr_y) : (avr_y - y[2]);

	if ((det_x > ERR_LIMIT) || (det_y > ERR_LIMIT))
		return 0;

	avr_x = (x[1] + x[2])/2;
	avr_y = (y[1] + y[2])/2;

	det_x = (x[3] > avr_x) ? (x[3] - avr_x) : (avr_x - x[3]);
	det_y = (y[3] > avr_y) ? (y[3] - avr_y) : (avr_y - y[3]);

	if ((det_x > ERR_LIMIT) || (det_y > ERR_LIMIT))
		return 0;

	return 1;
}


static irqreturn_t up_down_interrupt(int irq, void *dev_id)
{
	if (gp_adc_regs->adcdat0 & (1<<15)) {
		/* enter wait for stylus down interrupt */
		gp_adc_regs->adctsc = 0xd3;

		/* report touch up */
		input_report_key(idev, BTN_TOUCH, 0);
		input_report_abs(idev, ABS_PRESSURE, 0);
		input_sync(idev);

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
	static int cnt = 0;
	static int x[4], y[4];
	int adcdat0 = gp_adc_regs->adcdat0 ;
	int adcdat1 = gp_adc_regs->adcdat1 ;

	/* is pen up ? */
	if (gp_adc_regs->adcdat0 & (1<<15)) {
		/* pen is up */
		cnt = 0;
		input_report_abs(idev, ABS_PRESSURE, 0);
		input_report_key(idev, BTN_TOUCH, 0);
		input_sync(idev);

		/* enter wait for stylus down interrupt */
		gp_adc_regs->adctsc = 0xd3;

	} else {

		/* pen is down */
		x[cnt] = adcdat0 & 0x3ff;
		y[cnt] = adcdat1 & 0x3ff;
		if (++cnt == 4) {
			cnt = 0;
			if (s3c_filter_ts(x, y)) {

				/* report touch down value */

				int val_x = (x[0]+x[1]+x[2]+x[3])/4;
				int val_y = (y[0]+y[1]+y[2]+y[3])/4;
				//printk("x : %d y : %d\n", val_x, val_y);

				input_report_abs(idev, ABS_X, val_x);
				input_report_abs(idev, ABS_Y, val_y);

				input_report_key(idev, BTN_TOUCH, 1);
				input_report_abs(idev, ABS_PRESSURE, 1);
				input_sync(idev);
			}

			/* enter wait for stylus up interrupt */
			gp_adc_regs->adctsc = 0x1d3;
			mod_timer(&ts_timer, jiffies + HZ/100);

		} else {
			/* AUTO PST */
			gp_adc_regs->adctsc = (0x1 << 3 | 0x1 << 2);

			/* start adc */
			gp_adc_regs->adccon |= 0x1;
		}
	}

	return IRQ_HANDLED;
}

void ts_timer_func(unsigned long v)
{
	if (gp_adc_regs->adcdat0 & (1<<15)) {
		input_report_abs(idev, ABS_PRESSURE, 0);
		input_report_key(idev, BTN_TOUCH, 0);
		input_sync(idev);
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

	idev = input_allocate_device();

	idev->name = "s3c_ts";
	set_bit(EV_ABS, idev->evbit);
	set_bit(EV_KEY, idev->evbit);
	set_bit(BTN_TOUCH, idev->keybit);

	input_set_abs_params(idev, ABS_X, 0, 0x3FF, 0, 0);
	input_set_abs_params(idev, ABS_Y, 0, 0x3FF, 0, 0);
	input_set_abs_params(idev, ABS_PRESSURE, 0, 1, 0, 0);

	if (input_register_device(idev)) {
		printk("input_register_device err\n");
		return -1;
	}

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
	ts_timer.data	  = 0;
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

	input_unregister_device(idev);
	input_free_device(idev);
	del_timer(&ts_timer);
}

module_init(ts_init);
module_exit(ts_exit);
MODULE_LICENSE("GPL");
