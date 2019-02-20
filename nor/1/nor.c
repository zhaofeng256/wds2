#include <linux/io.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/xip.h>
#include <linux/ioport.h>
#include <linux/io.h>


#define NOR_SIZE (0x1000000)

struct map_info g_nor_map;
struct mtd_info *g_p_mtd;


static struct mtd_partition g_nor_parts[] = {
	[0] = {
        .name   = "bootloader_nor",
        .size   = 0x00040000,
		.offset	= 0,
	},
	[1] = {
        .name   = "root_nor",
        .offset = MTDPART_OFS_APPEND,
        .size   = MTDPART_SIZ_FULL,
	}
};


static inline map_word nor_map_read(struct map_info *map, unsigned long ofs)
{
	map_word r;
	r.x[0] = *((u16 *)(map->virt + ofs));
	return r;
}
static void __xipram nor_map_write(struct map_info *map, const map_word datum, unsigned long ofs)
{
	*((short *)(map->virt + ofs)) = (short)datum.x[0];
}

static void __xipram nor_map_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->virt + from, len);
}

static void __xipram nor_map_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->virt + to, from, len);
}

int __init nor_init(void)
{
	int err = 0;

	g_nor_map.name = "nor_flash";
	g_nor_map.phys = 0;
	g_nor_map.size = NOR_SIZE + 1;
	g_nor_map.bankwidth = 2;
	g_nor_map.virt = ioremap(g_nor_map.phys, g_nor_map.size);
	if (g_nor_map.virt == NULL) {
		printk("Failed to ioremap flash region\n");
		err = EIO;
		goto err_out;
	}

	g_nor_map.read = nor_map_read;
	g_nor_map.write = nor_map_write;
	g_nor_map.copy_from = nor_map_copy_from;
	g_nor_map.copy_to = nor_map_copy_to;


	g_p_mtd = do_map_probe("cfi_probe", &g_nor_map);
	if (g_p_mtd== NULL) {
		printk("map_probe failed\n");
		err = -ENXIO;
		goto err_out;
	}

	g_p_mtd->owner = THIS_MODULE;

	add_mtd_partitions(g_p_mtd, g_nor_parts, 2);

	return 0;

err_out:
	if (g_nor_map.virt != NULL)
		iounmap(g_nor_map.virt);

	return err;
}

void __exit nor_exit(void)
{
	del_mtd_device(g_p_mtd);
	map_destroy(g_p_mtd);

	if (g_nor_map.virt != NULL)
		iounmap(g_nor_map.virt);

}

module_init(nor_init);
module_exit(nor_exit);
MODULE_LICENSE("GPL");
