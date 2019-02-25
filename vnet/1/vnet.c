#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/errno.h>
#include <linux/ip.h>

struct net_device * gp_vnet_dev;

static void emulator_rx_packet(struct sk_buff *skb, struct net_device *dev)
{
	/* 参考LDD3 */
	unsigned char *type;
	struct iphdr *ih;
	__be32 *saddr, *daddr, tmp;
	unsigned char	tmp_dev_addr[ETH_ALEN];
	struct ethhdr *ethhdr;

	struct sk_buff *rx_skb;

	// 从硬件读出/保存数据
	/* 对调"源/目的"的mac地址 */
	ethhdr = (struct ethhdr *)skb->data;
	memcpy(tmp_dev_addr, ethhdr->h_dest, ETH_ALEN);
	memcpy(ethhdr->h_dest, ethhdr->h_source, ETH_ALEN);
	memcpy(ethhdr->h_source, tmp_dev_addr, ETH_ALEN);

	/* 对调"源/目的"的ip地址 */
	ih = (struct iphdr *)(skb->data + sizeof(struct ethhdr));
	saddr = &ih->saddr;
	daddr = &ih->daddr;

	tmp = *saddr;
	*saddr = *daddr;
	*daddr = tmp;

	//((u8 *)saddr)[2] ^= 1; /* change the third octet (class C) */
	//((u8 *)daddr)[2] ^= 1;
	type = skb->data + sizeof(struct ethhdr) + sizeof(struct iphdr);
	//printk("tx package type = %02x\n", *type);
	// 修改类型, 原来0x8表示ping
	*type = 0; /* 0表示reply */

	ih->check = 0;		   /* and rebuild the checksum (ip needs it) */
	ih->check = ip_fast_csum((unsigned char *)ih,ih->ihl);

	// 构造一个sk_buff
	rx_skb = dev_alloc_skb(skb->len + 2);
	skb_reserve(rx_skb, 2); /* align IP on 16B boundary */
	memcpy(skb_put(rx_skb, skb->len), skb->data, skb->len);

	/* Write metadata, and then pass to the receive level */
	rx_skb->dev = dev;
	rx_skb->protocol = eth_type_trans(rx_skb, dev);
	rx_skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb->len;

	// 提交sk_buff
	netif_rx(rx_skb);
}

int	vnet_send_packet(struct sk_buff *skb, struct net_device *dev)
{

	static int cnt = 0;
	netif_stop_queue(dev);

	/* Write the contents of the packet */

	/* fake receive packet */
	emulator_rx_packet(skb, dev);

	dev_kfree_skb (skb);
	netif_wake_queue(dev);

	printk("vnet sent %d\n", cnt++);

	return 0;
}


static __init int vnet_init(void)
{
	int ret = 0;
	char vnet_mac[6] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16};

	gp_vnet_dev = alloc_netdev(0, "vnet%d", ether_setup);
	if (NULL == gp_vnet_dev) {
		printk("alloc netdev failed\n");
		return -EINVAL;
	}

	gp_vnet_dev->hard_start_xmit = vnet_send_packet;
	memcpy(gp_vnet_dev->dev_addr, vnet_mac, 6);

	gp_vnet_dev->flags |= IFF_NOARP;
	gp_vnet_dev->features |= NETIF_F_NO_CSUM;
	ret = register_netdev(gp_vnet_dev);
	if (ret) {
		printk("regist vnet error %d\n", ret);
		free_netdev(gp_vnet_dev);
		return -EINVAL;
	}
	return 0;
}

static __exit void vnet_exit(void)
{
	unregister_netdev(gp_vnet_dev);
	free_netdev(gp_vnet_dev);
}

module_init(vnet_init);
module_exit(vnet_exit);
MODULE_LICENSE("GPL");

