#include <linux/input.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <linux/slab.h>

static struct input_dev *p_input_dev;
static unsigned char *g_pdata;
static int g_pkt_len;


static struct usb_device_id usb_drv_table [] = {
	{ USB_DEVICE(0x17EF, 0x6019) },
	{ }             /* Terminating entry */
};

void usb_trans_complete(struct urb *purb)
{
	static unsigned char pre_val = 0;
	unsigned char val = g_pdata[0];


#if 0
	printk("recv usb data len %d\n", purb->actual_length);
	int i;
	for (i = 0; i < purb->actual_length; i++) {
		printk("%02x ", g_pdata[i]);
	}
	printk("\n");
#else


	if ((val & 0x1) != (pre_val & 0x1)) {
		input_event(p_input_dev, EV_KEY, KEY_L, (val & 0x1) ? 1 : 0);
		input_sync(p_input_dev);
	}

	if ((val & 0x2) != (pre_val & 0x2)) {
		input_event(p_input_dev, EV_KEY, KEY_S, (val & 0x2) ? 1 : 0);
		input_sync(p_input_dev);
	}

	if ((val & 0x4) != (pre_val & 0x4)) {
		input_event(p_input_dev, EV_KEY, KEY_ENTER, (val & 0x4) ? 1 : 0);
		input_sync(p_input_dev);
	}

	pre_val = val;

#endif
	/* 重新提交urb */
	usb_submit_urb(purb, GFP_KERNEL);
}

int usb_drv_probe(struct usb_interface *	iface,
		  const struct usb_device_id *	id)
{
	struct usb_device *udev = interface_to_usbdev(iface);
	struct usb_host_interface *iface_desc = NULL;
	struct usb_endpoint_descriptor *endpoint = NULL;
	int int_in_endpointAddr = 0;
	struct urb *purb = NULL;

	char phys[64] = { 0 };
	int ret = -ENOMEM;
	printk("%s : %d\n", __func__, __LINE__);

	iface_desc = iface->cur_altsetting;
	endpoint = &iface_desc->endpoint[0].desc;
	g_pkt_len = endpoint->wMaxPacketSize;
	int_in_endpointAddr = endpoint->bEndpointAddress;
	printk("%s : %d\n", __func__, __LINE__);

	purb = usb_alloc_urb(0, GFP_KERNEL);
	if (!purb)
		goto err_free_devs;
	printk("%s : %d\n", __func__, __LINE__);

	g_pdata = usb_buffer_alloc(udev, g_pkt_len, GFP_KERNEL,
				       &purb->transfer_dma);
	if (!g_pdata)
		goto err_free_urb;
	printk("%s : %d\n", __func__, __LINE__);

	usb_fill_int_urb(purb, udev,
			 usb_rcvintpipe(udev, int_in_endpointAddr),
			 g_pdata, g_pkt_len, usb_trans_complete, NULL, endpoint->bInterval);

	printk("%s : %d\n", __func__, __LINE__);

	usb_make_path(udev, phys, sizeof(phys));
	strlcat(phys, "/input0", sizeof(phys));

	p_input_dev = input_allocate_device();

	p_input_dev->name = "usb_key";
	p_input_dev->phys = phys;
	usb_to_input_id(udev, &p_input_dev->id);
	p_input_dev->dev.parent = &iface->dev;
	printk("%s : %d\n", __func__, __LINE__);

	//input_set_drvdata(input_dev, dev);

	//p_input_dev->open = atp_open;
	//p_input_dev->close = atp_close;
	set_bit(EV_KEY, p_input_dev->evbit);
	set_bit(EV_REP, p_input_dev->evbit);
	set_bit(KEY_L, p_input_dev->keybit);
	set_bit(KEY_S, p_input_dev->keybit);
	set_bit(KEY_ENTER, p_input_dev->keybit);

	ret = input_register_device(p_input_dev);
	if (ret)
		goto err_free_buffer;

	purb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;


	/* save our data pointer in this interface device */
	//usb_set_intfdata(iface, dev);

	/* 使用URB */
	usb_submit_urb(purb, GFP_KERNEL);


	return 0;


err_free_buffer:
	usb_buffer_free(udev, 8,
			g_pdata, purb->transfer_dma);
err_free_urb:
	usb_free_urb(purb);
err_free_devs:
	usb_set_intfdata(iface, NULL);
	input_free_device(p_input_dev);

	return 0;
}

void usb_drv_disconnect(struct usb_interface *intf)
{
	printk("usb disconnect\n");
	input_unregister_device(p_input_dev);
	input_free_device(p_input_dev);
}

static struct usb_driver usb_drv = {
	.name		= "usb_drv",
	.probe		= usb_drv_probe,
	.disconnect	= usb_drv_disconnect,
	.id_table	= usb_drv_table,
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
