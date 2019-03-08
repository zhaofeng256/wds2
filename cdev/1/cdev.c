#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#define MAX_CDEV_NUM 2

dev_t devid;
int major;
struct class * cls;

struct cdev new_cdev;

int new_cdev_open(struct inode * id, struct file * f)
{
	printk("%s opened, %d:%d\n", f->f_path.dentry->d_iname,
		MAJOR(id->i_cdev->dev), MINOR(id->i_cdev->dev));
	return 0;
}

struct file_operations new_cdev_fop = {
	.owner = THIS_MODULE,
	.open = new_cdev_open,
};

int __init new_cdev_init(void)
{
	int ret = alloc_chrdev_region(&devid, 0, MAX_CDEV_NUM, "new_cdev");
	major = MAJOR(devid);
	if (ret < 0) {
		printk("alloc chrdev_region err: %d\n", ret);
		return -1;
	}
	cdev_init(&new_cdev, &new_cdev_fop);
	cdev_add(&new_cdev, devid, MAX_CDEV_NUM);

	cls = class_create(THIS_MODULE, "new_cdev");
	class_device_create(cls, NULL, MKDEV(major, 0), NULL, "new_cdev0");
	class_device_create(cls, NULL, MKDEV(major, 1), NULL, "new_cdev1");

	return 0;
}

void __exit new_cdev_exit(void)
{
	class_device_destroy(cls, MKDEV(major, 0));
	class_device_destroy(cls, MKDEV(major, 1));
	class_destroy(cls);

	cdev_del(&new_cdev);
	unregister_chrdev_region(devid, MAX_CDEV_NUM);
}

module_init(new_cdev_init);
module_exit(new_cdev_exit);
MODULE_LICENSE("GPL");
