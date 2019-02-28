#include <linux/i2c.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/string.h>

#include <linux/major.h>
#include <linux/errno.h>
#include <linux/module.h>

#include <linux/kobject.h>
#include <asm/uaccess.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>

#include <linux/cdev.h>
#include <linux/device.h>

#define DEVICE_NAME "at24"
#define CLASS_NAME "at24_cls"


static struct class *at24_class;
static int at24_major;
static char at24_name[256];
static struct kobject at24_kobj;
static struct i2c_client *at24_client;
static unsigned short ignore[] = { I2C_CLIENT_END };
static unsigned short normal_addr[] = { 0x50, I2C_CLIENT_END };

static int at24_detect(struct i2c_adapter *adapter, int address, int kind);
static ssize_t show_name(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t store_name(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
ssize_t at24_read(struct file *fp, char __user *buf, size_t sz, loff_t *off);
ssize_t at24_write(struct file *fp, const char __user *buf, size_t sz, loff_t *off);

struct file_operations at24_fops = {
	.owner	= THIS_MODULE,
	.read	= at24_read,
	.write	= at24_write,
};

static struct i2c_client_address_data at24_addr = {
	.normal_i2c	= normal_addr,
	.probe		= ignore,
	.ignore		= ignore,
};

static DEVICE_ATTR(at24, S_IRUGO, show_name, store_name);


static struct attribute *at24_attributes[] = {
	&dev_attr_at24.attr,
	NULL
};

static const struct attribute_group at24_group = {
	.attrs	= at24_attributes,
};

static ssize_t show_name(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", at24_name);
}

static ssize_t store_name(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	snprintf(at24_name, sizeof(at24_name), "%s", buf);
	return count;
}

static int at24_attach_adapter(struct i2c_adapter *adapter)
{
	if (!(adapter->class & I2C_CLASS_HWMON))
		return 0;
	return i2c_probe(adapter, &at24_addr, at24_detect);
}

static int at24_detach_client(struct i2c_client *client)
{
	sysfs_remove_group(&at24_kobj, &at24_group);
	class_destroy(at24_class);
	unregister_chrdev(at24_major, "at24");
	i2c_detach_client(at24_client);
	kfree(at24_client);

	return 0;
}

static struct i2c_driver at24_driver = {
	.driver		= {
		.name	= "at24",
	},
	//.id = I2C_DRIVERID_DS1374,
	.attach_adapter = at24_attach_adapter,
	.detach_client	= at24_detach_client,
};

/* This function is called by i2c_probe */
static int at24_detect(struct i2c_adapter *adapter, int address,
		       int kind)
{
	int err = 0;

	if (at24_client)
		return 0;

	at24_client = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (NULL == at24_client) {
		err = -ENOMEM;
		goto exit;
	}
	at24_client->addr = address;
	at24_client->adapter = adapter;
	at24_client->driver = &at24_driver;
	at24_client->flags = 0;
	snprintf(at24_client->name, I2C_NAME_SIZE, "%s", "at24");

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(at24_client)))
		goto exit_free;

	/* Create char device */
	at24_major = register_chrdev(0, "at24", &at24_fops);
	at24_class = class_create(THIS_MODULE, "at24");
	if (0 > class_device_create(at24_class, NULL, MKDEV(at24_major, 0), NULL, "at24")) {
		printk("at24 create class device failed\n");
		goto exit_remove_files;
	}

	/* Register sysfs hooks */
	err = sysfs_create_group(&at24_client->dev.kobj, &at24_group);
	if (err) {
		printk("at24 create sysfs failed\n");
		goto exit_detach;
	}

	return 0;

exit_remove_files:
	class_destroy(at24_class);
	unregister_chrdev(at24_major, "at24");

exit_detach:
	i2c_detach_client(at24_client);

exit_free:
	kfree(at24_client);
exit:
	return err;
}

ssize_t at24_read(struct file *fp, char __user *buf, size_t sz, loff_t *off)
{
	char addr;
	char data;
	struct i2c_msg msg[2];
	int ret;

	if (1 != sz)
		return 0;

	ret = copy_from_user(&addr, buf, 1);
	if (1 != ret)
		return -ENOMEM;

	msg[0].addr = at24_client->addr;
	msg[0].buf = &addr;
	msg[0].len = 1;
	msg[0].flags = 0;

	msg[1].addr = at24_client->addr;
	msg[1].buf = &data;
	msg[1].len = 1;
	msg[1].flags = I2C_M_RD;

	ret = i2c_transfer(at24_client->adapter, msg, 2);
	if (2 == ret) {
		ret = copy_to_user(buf + 1, &data, 1);
		if (1 != ret)
			return 0;
		else
			return ret;
	} else {
		return 0;
	}
}

ssize_t at24_write(struct file *fp, const char __user *buf, size_t sz, loff_t *off)
{
	char temp[2] = { 0 };
	struct i2c_msg msg;
	int ret;

	if (sz != 2)
		return 0;

	ret = copy_from_user(temp, buf, 2);
	if (2 != ret)
		return 0;

	msg.addr = at24_client->addr;
	msg.buf = temp;
	msg.len = 2;
	msg.flags = 0;

	ret = i2c_transfer(at24_client->adapter, &msg, 1);
	if (1 != ret)
		return 0;
	else
		return 2;
}

static __init int at24_init(void)
{
	return i2c_add_driver(&at24_driver);
}

static __exit void at24_exit(void)
{
	i2c_del_driver(&at24_driver);
}

module_init(at24_init);
module_exit(at24_exit);
MODULE_LICENSE("GPL");
