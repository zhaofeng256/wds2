#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <linux/genhd.h>
#include <linux/module.h>

static struct gendisk *ram_disk;

static DEFINE_SPINLOCK(ram_spin_lock);

#define RAMBLOCK_SIZE (1024U * 1024U)

static unsigned char *ram_buf;;

int ramblk_getgeo(struct block_device * bdev, struct hd_geometry * geo)
{
	geo->heads = 2;
	geo->cylinders = 32;
	geo->sectors = (unsigned char)(RAMBLOCK_SIZE / (2 * 32 * 512));
	return 0;
}

static struct block_device_operations ram_disk_ops = {
	.owner = THIS_MODULE,
	.getgeo = ramblk_getgeo,
};

static void do_ramblock_request( struct request_queue * q )
{
	//static int cnt = 0;
	struct request *req = NULL;
	//printk("%s cnt %d\n", __func__, ++cnt);
	req = blk_fetch_request(q);
	while (req) {
		unsigned long offset = blk_rq_pos(req) << 9;
		size_t req_len = blk_rq_cur_bytes(req);

		if (rq_data_dir(req) == READ) {
			memcpy(req->buffer, ram_buf + offset, req_len);
		} else {
			memcpy(ram_buf + offset, req->buffer, req_len);
		}

		if (__blk_end_request_cur(req, 0)) {
			printk("blk_end_request_cur err\n");
		} else {
			req = blk_fetch_request(q);
		}
	}
}

int __init init_ramblock(void)
{
	ram_disk = alloc_disk(16);
	ram_disk->queue = blk_init_queue(do_ramblock_request, &ram_spin_lock);
	ram_disk->fops = &ram_disk_ops;
	sprintf(ram_disk->disk_name, "ramblock");
	ram_disk->major = register_blkdev(0, "ramblock");
	ram_disk->first_minor = 0;
	set_capacity(ram_disk, RAMBLOCK_SIZE / 512);
	ram_buf = kzalloc(RAMBLOCK_SIZE, GFP_KERNEL);
	add_disk(ram_disk);

	return 0;
}

void __exit exit_ramblock(void)
{
	blk_cleanup_queue(ram_disk->queue);
	unregister_blkdev(ram_disk->major, "ramblock");
	del_gendisk(ram_disk);
	put_disk(ram_disk);
	kfree(ram_buf);
}

module_init(init_ramblock);
module_exit(exit_ramblock);
MODULE_LICENSE("GPL");
