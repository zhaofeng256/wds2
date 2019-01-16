#include <linux/io.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>

static struct usb_device_id usb_drv_table [] = {
	{ USB_DEVICE(0x17EF, 0x6019) },
	{ }             /* Terminating entry */
};

int usb_drv_probe(struct usb_interface *	intf,
		  const struct usb_device_id *	id)
{
	printk("usb probe, vendor id %#x product id %#x\n",
		id->idVendor, id->idProduct);
	return 0;
}

void usb_drv_disconnect(struct usb_interface *intf)
{
	printk("usb disconnect\n");
}

static struct usb_driver usb_drv = {
	.name = "usb_drv",
	.probe = usb_drv_probe,
	.disconnect = usb_drv_disconnect,
	.id_table = usb_drv_table,
};

static __init int usb_drv_init(void)
{
	usb_register(&usb_drv);
	return 0;
}

static __exit void usb_drv_exit(void)
{
	usb_deregister(&usb_drv);
}

module_init(usb_drv_init);
module_exit(usb_drv_exit);
MODULE_LICENSE("GPL");
