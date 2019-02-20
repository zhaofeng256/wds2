#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/module.h>

static struct gendisk *ram_disk;

static DEFINE_SPINLOCK(ram_spin_lock);

#define RAMBLOCK_SIZE (1024*1024)

static struct block_device_operations ram_disk_ops = {
	.owner = THIS_MODULE,
};

static void do_ramblock_request( request_queue_t * q )
{
	static int cnt = 0;
	struct request *req;
	printk("%s cnt %d\n", __func__, ++cnt);

	while (1) {
		req = elv_next_request(q);
		if (req) {
			end_request(req, 1);
		} else {
			break;
		}
	}
}

int __init init_ramblock(void)
{
	ram_disk = alloc_disk(1);
	ram_disk->queue = blk_init_queue(do_ramblock_request, &ram_spin_lock);
	ram_disk->fops = &ram_disk_ops;
	sprintf(ram_disk->disk_name, "ramblock");
	ram_disk->major = register_blkdev(0, "ramblock");
	ram_disk->first_minor = 0;
	set_capacity(ram_disk, RAMBLOCK_SIZE / 512);
	add_disk(ram_disk);

	return 0;
}

void __exit exit_ramblock(void)
{
	blk_cleanup_queue(ram_disk->queue);
	unregister_blkdev(ram_disk->major, "ramblock");
	del_gendisk(ram_disk);
	put_disk(ram_disk);
}

module_init(init_ramblock);
module_exit(exit_ramblock);
MODULE_LICENSE("GPL");
