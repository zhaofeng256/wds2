#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>

static int g_dma_major = 0;
static struct class *g_dma_cls = NULL;

static char *g_dma_src = NULL;
static u32 g_dma_src_phys = 0;

static char *g_dma_des = NULL;
static u32 g_dma_des_phys = 0;


#define MEM_CPY_NO_DMA 0
#define MEM_CPY_DMA    1

#define DMA_BUFF_SIZE (512 * 1024)

static int s3c_dma_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case MEM_CPY_NO_DMA:
		break;
	case MEM_CPY_DMA:
		break;
	default:
		break;
	}
	return 0;
}

static struct file_operations g_dma_fops = {
	.owner	= THIS_MODULE,
	.ioctl	= s3c_dma_ioctl,
};

static int __init s3c_dma_init(void)
{
	g_dma_src = dma_alloc_writecombine(NULL, DMA_BUFF_SIZE, &g_dma_src_phys, GFP_KERNEL);
	if (NULL ==  g_dma_src) {
		printk("alloc dma src buf error\n");
		return -ENOMEM;
	}

	g_dma_des = dma_alloc_writecombine(NULL, DMA_BUFF_SIZE, &g_dma_des_phys, GFP_KERNEL);
	if (NULL ==  g_dma_des) {
		printk("alloc dma des buf error\n");
		return -ENOMEM;
	}


	g_dma_major = register_chrdev(0, "s3c_dma", &g_dma_fops);
	g_dma_cls = class_create(THIS_MODULE, "s3c_dma");
	class_device_create(g_dma_cls, NULL, MKDEV(g_dma_major, 0), NULL, "dma");
	return 0;
}

static void __exit s3c_dma_exit(void)
{
	class_device_destroy(g_dma_cls, MKDEV(g_dma_major, 0));
	class_destroy(g_dma_cls);
	unregister_chrdev(g_dma_major, "s3c_dma");

	dma_free_writecombine(NULL, DMA_BUFF_SIZE, g_dma_src, g_dma_src_phys);
	dma_free_writecombine(NULL, DMA_BUFF_SIZE, g_dma_des, g_dma_des_phys);

}

module_init(s3c_dma_init);
module_exit(s3c_dma_exit);

MODULE_LICENSE("GPL");
