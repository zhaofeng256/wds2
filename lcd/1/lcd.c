#include <linux/module.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/time.h>
#include <linux/dma-mapping.h>

#define LCD_RES_X (480)
#define LCD_RES_Y (272)
#define LCD_PIX_BYTES (2)
#define LCD_REG_ADDR (0x4D000000)


struct lcd_regs {
	u32 lcdcon1;
	u32 lcdcon2;
	u32 lcdcon3;
	u32 lcdcon4;
	u32 lcdcon5;
	u32 lcdsaddr1;
	u32 lcdsaddr2;
	u32 lcdsaddr3;
	u32 redlut;
	u32 greenlut;
	u32 bluelut;
	u32 reserved[9];
	u32 dithmode;
	u32 tpal;
	u32 lcdintpnd;
	u32 lcdsrcpnd;
	u32 lcdintmsk;
	u32 tconsel;
};


static struct lcd_regs *gplcd_regs;
static struct clk * gp_lcd_clk;
static struct fb_info * gpfb;
static u32 pseudo_palette[16];
static u32 * gpbcon;
static u32 * gpbdat;
static u32 * gpccon;
static u32 * gpdcon;
static u32 * gpgcon;

static inline u_int chan_to_field(u_int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}


static int gfb_setcolreg(unsigned int regno, unsigned int red,
			     unsigned int green, unsigned int blue,
			     unsigned int transp, struct fb_info *info)
{
	unsigned int val;

	if (regno > 16)
		return 1;

	/* 用red,green,blue三原色构造出val */
	val  = chan_to_field(red,	&info->var.red);
	val |= chan_to_field(green, &info->var.green);
	val |= chan_to_field(blue,	&info->var.blue);

	//((u32 *)(info->pseudo_palette))[regno] = val;
	pseudo_palette[regno] = val;
	return 0;
}


static struct fb_ops gfb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg	= gfb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};


static int __init lcd_init(void)
{

	gpfb = framebuffer_alloc(0, NULL);
	if (!gpfb) {
		return -ENOMEM;
	}

	strcpy(gpfb->fix.id, "s3c_lcd");

	gpfb->fix.smem_len = LCD_RES_X * LCD_RES_Y * LCD_PIX_BYTES;
	gpfb->fix.line_length = LCD_RES_X * LCD_PIX_BYTES;
	gpfb->fix.visual = FB_VISUAL_TRUECOLOR;
	gpfb->fix.type = FB_TYPE_PACKED_PIXELS;

	/* alloc frame buffer memory */
	gpfb->screen_base = dma_alloc_writecombine(NULL, gpfb->fix.smem_len, &gpfb->fix.smem_start, GFP_KERNEL);
	if (NULL == gpfb->screen_base) {
		printk("alloc dma src buf error\n");
		framebuffer_release(gpfb);
		return -ENOMEM;
	}


	gpfb->var.xres = LCD_RES_X;
	gpfb->var.yres = LCD_RES_Y;
	gpfb->var.xres_virtual = LCD_RES_X;
	gpfb->var.yres_virtual = LCD_RES_Y;
	gpfb->var.bits_per_pixel = LCD_PIX_BYTES * 8;

	gpfb->var.blue.length = 5;
	gpfb->var.blue.offset = 0;

	gpfb->var.green.length = 6;
	gpfb->var.green.offset = 5;

	gpfb->var.red.length = 5;
	gpfb->var.red.offset = 11;

	gpfb->var.activate = FB_ACTIVATE_NOW;

	gpfb->fbops = &gfb_ops;

	gpfb->pseudo_palette = pseudo_palette;
	gpfb->screen_size = LCD_RES_X * LCD_RES_Y * LCD_PIX_BYTES;

	/* lcd gpio config */
	gpbcon = ioremap(0x56000010, 8);
	gpbdat = gpbcon + 1;

	gpccon = ioremap(0x56000020, 12);

	gpdcon = ioremap(0x56000030, 12);

	gpgcon = ioremap(0x56000060, 8);

	/* set led power pin as gpio output && close backlight */
	*gpbcon &= ~0x3;
	*gpbcon |= 0x1;
	*gpbdat &= ~0x1;

	/* set lcd data[24:0] ports */
	*gpccon = 0Xaaaaaaaa;
	*gpdcon = 0Xaaaaaaaa;

	/* set lcd power enable pin */
	*gpgcon |= (0x3 << 8);

	/* enable lcd clock */
	gp_lcd_clk = clk_get(NULL, "lcd");
	if (!gp_lcd_clk || IS_ERR(gp_lcd_clk)) {
		printk(KERN_ERR "failed to get lcd clock source\n");
		return -ENOENT;
	}
	clk_enable(gp_lcd_clk);
	printk("lcd clock enabled\n");


	/* lcd control registers config */
	gplcd_regs = ioremap(LCD_REG_ADDR, sizeof(struct lcd_regs));

	/*
		lcdcon1
		CLKVAL  : TFT 10MHz  0x4 << 8
		PNRMODE : TFT LCD 0x3 << 5
		BPPMODE : 16BPP FOR TFT  0xc << 1
		ENVID   : PWR DISABLE
	*/
	gplcd_regs->lcdcon1 = 0x4 << 8 | 0x3 << 5 | 0xc << 1;

	/*
		lcdcon2
		VBPD : (2 -1) << 24
		LINEVAL : (272 - 1) << 14
		VFPD : (2 -1) <<< 16
		VSPW : (10 -1) << 0
	*/
	gplcd_regs->lcdcon2 = 0x1 << 24 | 0X10f << 14 | 0x1 << 16 | 0x9 << 0;


	/*
		HBPD: (2 -1) << 19
		HOZVAL : (480 -1) << 8
		HFPD   ： (2 - 1) << 0
	*/
	gplcd_regs->lcdcon3 =0x1 << 19 | 0x1df << 8 | 0x1 << 0;

	/*
		HSPW : (41 - 1) << 0
	*/
	gplcd_regs->lcdcon4 = 0x28 << 0;

	/*
		lcdcon5
		FRM565: 0x1 << 11
		INVVCLK : 0
		INVVLINE: 1 << 9
		INVVFRAME: 1 << 8
		INVVD:
	*/
	gplcd_regs->lcdcon5 = 0x1 << 11 | 0x1 << 9 | 0x1 << 8;


	/*
		LCD panel = 480 x 272, color TFT
		Frame start address = 0x0c500000
		Offset dot number = 0

		LINEVAL = 272-1 = 0x10f
		PAGEWIDTH = 480 x 16 / 16 = 0x1e0
		OFFSIZE = 0x0
		LCDBANK = 0x0c500000 >> 22 = 0x31
		LCDBASEU = 0x100000 >> 1 = 0x80000
		LCDBASEL = 0x80000 + ( 0x1e0 + 0x0 ) x ( 0x10f + 1 ) = 0x9fe00
	*/

	gplcd_regs->lcdsaddr1 = gpfb->fix.smem_start >> 1;
	gplcd_regs->lcdsaddr2 = (gpfb->fix.smem_start + gpfb->fix.smem_len) >> 1;
	gplcd_regs->lcdsaddr3 = gpfb->fix.line_length >> 1;


	/* enable the video output and the LCD control signal */
	gplcd_regs->lcdcon1 |= 0x1;
	msleep(150);

	/* enable LCD_PWREN */
	gplcd_regs->lcdcon5 |= (0x1 << 3);
	msleep(200);

	/* enable backlight */
	*gpbdat |= 0x1;


	register_framebuffer(gpfb);

	return 0;
}

static void __exit lcd_exit(void)
{
	/* disable backlight */
	*gpbdat &= ~0x1;
	msleep(200);

	/* disable LCD_PWREN */
	gplcd_regs->lcdcon5 &= ~(0x1 << 3);
	msleep(150);

	/* disable LCD signal output */
	gplcd_regs->lcdcon1 &= ~0x1;

	unregister_framebuffer(gpfb);
	dma_free_writecombine(NULL, gpfb->fix.smem_len, gpfb->screen_base, gpfb->fix.smem_start);
	framebuffer_release(gpfb);

	iounmap(gplcd_regs);
	iounmap(gpbcon);
	iounmap(gpccon);
	iounmap(gpdcon);
	iounmap(gpgcon);

	clk_disable(gp_lcd_clk);
	clk_put(gp_lcd_clk);
}

module_init(lcd_init);
module_exit(lcd_exit);
MODULE_LICENSE("GPL");
