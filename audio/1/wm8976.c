#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/arch/dma.h>

static struct clk * iis_clock;

struct gpio_regs {
	u32 gpcon;
	u32 gpdat;
	u32 gpup;
	u32 reserved;
};

struct iis_regs {
	u32 iiscon;
	u32 iismod;
	u32 iispsr;
	u32 iisfcon;
	u32 iisfifo;
};

static struct iis_regs *iis_base;
static struct gpio_regs * gpb_base;
static struct gpio_regs * gpe_base;

typedef struct {
	int size; /* buffer size */
	char *start; /* point to actual buffer */
	dma_addr_t dma_addr; /* physical buffer address */
	struct semaphore sem; /* down before touching the buffer */
	int master; /* owner for buffer allocation, contain size when true */
} audio_buf_t;

typedef struct {
	audio_buf_t *buffers; /* pointer to audio buffer structures */
	audio_buf_t *buf; /* current buffer used by read/write */
	u_int buf_idx; /* index for the pointer above */
	u_int fragsize; /* fragment i.e. buffer size */
	u_int nbfrags; /* nbr of fragments */
	dmach_t dma_ch; /* DMA channel (channel2 for audio) */
	u_int dma_ok;
} audio_stream_t;

static audio_stream_t output_stream;
static audio_stream_t input_stream; /* input */
static int audio_dev_dsp;
static int audio_dev_mixer;


static struct s3c2410_dma_client s3c2410iis_dma_out= {
	.name = "I2SSDO",
};

static struct s3c2410_dma_client s3c2410iis_dma_in = {
	.name = "I2SSDI",
};


static void audio_dmaout_done_callback(struct s3c2410_dma_chan *ch, void *buf, int size,
				       enum s3c2410_dma_buffresult result)
{
	audio_buf_t *b = (audio_buf_t *) buf;
	up(&b->sem);
	wake_up(&b->sem.wait);
}

static void audio_dmain_done_callback(struct s3c2410_dma_chan *ch, void *buf, int size,
				      enum s3c2410_dma_buffresult result)
{
	audio_buf_t *b = (audio_buf_t *) buf;
	b->size = size;
	up(&b->sem);
	wake_up(&b->sem.wait);
}

static int __init audio_init_dma(audio_stream_t * s, char *desc)
{
	int ret ;
	enum s3c2410_dmasrc source;
	int hwcfg;
	unsigned long devaddr;
	dmach_t channel;
	int dcon;
	unsigned int flags = 0;

	if(s->dma_ch == DMACH_I2S_OUT){
		channel = DMACH_I2S_OUT;
		source  = S3C2410_DMASRC_MEM;
		hwcfg   = BUF_ON_APB;
		devaddr = 0x55000010;
		dcon    = S3C2410_DCON_HANDSHAKE|S3C2410_DCON_SYNC_PCLK|S3C2410_DCON_INTREQ|S3C2410_DCON_TSZUNIT|S3C2410_DCON_SSERVE|S3C2410_DCON_CH2_I2SSDO|S3C2410_DCON_HWTRIG; // VAL: 0xa0800000;
		flags   = S3C2410_DMAF_AUTOSTART;

		ret = s3c2410_dma_request(s->dma_ch, &s3c2410iis_dma_out, NULL);
		s3c2410_dma_devconfig(channel, source, hwcfg, devaddr);
		s3c2410_dma_config(channel, 2, dcon);
		s3c2410_dma_set_buffdone_fn(channel, audio_dmaout_done_callback);
		s3c2410_dma_setflags(channel, flags);
        s->dma_ok = 1;
		return ret;
	}
	else if(s->dma_ch == DMACH_I2S_IN){
		channel = DMACH_I2S_IN;
		source  = S3C2410_DMASRC_HW;
		hwcfg   = BUF_ON_APB;
		devaddr = 0x55000010;
		dcon    = S3C2410_DCON_HANDSHAKE|S3C2410_DCON_SYNC_PCLK|S3C2410_DCON_INTREQ|S3C2410_DCON_TSZUNIT|S3C2410_DCON_SSERVE|S3C2410_DCON_CH1_I2SSDI|S3C2410_DCON_HWTRIG; // VAL: 0xa2800000;
		flags   = S3C2410_DMAF_AUTOSTART;

		ret = s3c2410_dma_request(s->dma_ch, &s3c2410iis_dma_in, NULL);
		s3c2410_dma_devconfig(channel, source, hwcfg, devaddr);
		s3c2410_dma_config(channel, 2, dcon);
		s3c2410_dma_set_buffdone_fn(channel, audio_dmain_done_callback);
		s3c2410_dma_setflags(channel, flags);
		s->dma_ok =1;
		return ret ;
	}
	else
		return 1;
}

static int audio_clear_dma(audio_stream_t * s, struct s3c2410_dma_client *client)
{
	s3c2410_dma_set_buffdone_fn(s->dma_ch, NULL);
	s3c2410_dma_free(s->dma_ch, client);
	return 0;
}

void init_wm8976(void)
{

}
static struct file_operations audio_dsp_fops = {
	llseek: smdk2410_audio_llseek,
	write: smdk2410_audio_write,
	read: smdk2410_audio_read,
	poll: smdk2410_audio_poll,
	ioctl: smdk2410_audio_ioctl,
	open: smdk2410_audio_open,
	release: smdk2410_audio_release
};

static struct file_operations  audio_mixer_fops = {
	ioctl: smdk2410_mixer_ioctl,
	open: smdk2410_mixer_open,
	release: smdk2410_mixer_release
};

int audio_iis_probe(struct device * dev)
{
	unsigned long flags = 0;


	iis_clock = clk_get(dev, "iis");
	if (iis_clock == NULL) {
		printk(KERN_ERR "failed to find clock source\n");
		return -ENOENT;
	}

	clk_enable(iis_clock);
	local_irq_save(flags);

	/* L3C adn I2S gpio registers init */

	/*
	 * L3MODE -- GPB2 pull up
	 * L3DATA -- GPB3
	 * L3CLOCK -- GPB4 pull up

	 * I2SLRCK -- GPE0
	 * I2SSCLK -- GPE1
	 * CDCLK -- GPE2
	 * I2SSDI -- GPE3
	 * I2SSDO -- GPE4
	 */

	gpb_base = ioremap(0x56000010, 16);
	gpe_base = ioremap(0x56000050, 16);

	gpb_base->gpcon &= ~((0x3 << 8) | (0x3 << 6) |(0x3 << 4));
	gpb_base->gpcon |= ((0x1 << 8) | (0x1 << 6) |(0x1 << 4));
	gpb_base->gpup |= ((0x1 << 2) | (0x1 << 4));

	gpe_base->gpcon &= ~((0x3 << 8) | (0x3 << 6) | (0x3 << 4) | (0x3 << 2) | \
		(0x3 << 0));
	gpe_base->gpcon |= ~((0x2 << 8) | (0x2 << 6) | (0x2 << 4) | (0x2 << 2) | \
		(0x2 << 0));
	gpe_base->gpup &= ~((0x1 << 4) | (0x1 << 3) | (0x1 << 2) | (0x1 << 1) | \
		(0x1 << 0));

	local_irq_restore(flags);


	/* iis registers init */
	iis_base = ioremap(0x55000000, sizeof(struct iis_regs));

	iis_base->iispsr = 0;
	iis_base->iiscon = 0;
	iis_base->iismod = 0;
	iis_base->iisfcon = 0;
	clk_disable(iis_clock);

	/* wm8976 registers init */
	init_wm8976();

	/* dma init */


	output_stream.dma_ch = DMACH_I2S_OUT;
	if (audio_init_dma(&output_stream, "UDA1341 out")) {
		audio_clear_dma(&output_stream,&s3c2410iis_dma_out);
		printk( KERN_WARNING ": unable to get DMA channels\n" );
		return -EBUSY;
	}

	input_stream.dma_ch = DMACH_I2S_IN;
	if (audio_init_dma(&input_stream, "UDA1341 in")) {
		audio_clear_dma(&input_stream,&s3c2410iis_dma_in);
		printk( KERN_WARNING ": unable to get DMA channels\n" );
		return -EBUSY;
	}
	audio_dev_dsp = register_sound_dsp(&audio_dsp_fops, -1);
	audio_dev_mixer = register_sound_mixer(&audio_mixer_fops, -1);

	return 0;
}

int audio_iis_remove(struct device * dev)
{
	return 0;
}


static struct device_driver audio_iis_driver = {
	.name = "audio-iis",
	.bus = &platform_bus_type,
	.probe = audio_iis_probe,
	.remove = audio_iis_remove,
};

static __init int audio_init(void)
{
	return driver_register(&audio_iis_driver);
}

static __exit void audio_exit(void)
{
	driver_unregister(&audio_iis_driver);
}

module_init(audio_init);
module_exit(audio_exit);
MODULE_LICENSE("GPL");
