#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>

#define PRINT_BUF_SIZE (4096)

static struct proc_dir_entry *mymsg_entry;
static char myprint_buf[PRINT_BUF_SIZE];
static int idx_read, idx_write;
static DECLARE_WAIT_QUEUE_HEAD(mymsg_q_head);
static int wait_evt;


int myprintk(const char *fmt, ...)
{
	va_list args;
	int i;
	char buf[1024] = { 0 };
	int printed_len;

	va_start(args, fmt);
	//r = vprintk(fmt, args);
	printed_len = vscnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	printk("<insert len %d>%s", printed_len, buf);

	for (i = 0; i < printed_len; i++) {
		idx_write = idx_write % PRINT_BUF_SIZE;
		myprint_buf[idx_write] = buf[i];
		if (idx_write == idx_read && idx_read > 0) {
			idx_read++;
		}
		idx_write++;
		wait_evt = 1;
		wake_up_interruptible(&mymsg_q_head);
	}
	printk("%s id_read %d id_write %d\n", __func__, idx_read, idx_write);

	return printed_len;
}

EXPORT_SYMBOL(myprintk);


static int mymsg_open (struct inode * in, struct file * fp)
{
	printk("%s\n", __func__);
	//idx_read = idx_write = 0;
	return 0;
}

static ssize_t mymsg_read(struct file * fp, char __user * data, size_t sz, loff_t * ofs)
{
	int i = 0;
	printk("%s id_read %d id_write %d need %d\n", __func__, idx_read, idx_write, sz);

	for (; i < sz;) {

		if (idx_read != idx_write) {
			printk("put [%d] %c\n", idx_read, myprint_buf[idx_read]);
			__put_user(myprint_buf[idx_read], data + i);
			i++;
			idx_read++;
			idx_read = idx_read % PRINT_BUF_SIZE;

		} else {
#if 0
			if (fp->f_flags & O_NONBLOCK) {
				printk("nonblock return\n");
				return i;
			} else {
				wait_evt = 0;
				wait_event_interruptible(mymsg_q_head, wait_evt);
			}
#else
			return i;
#endif
		}
	}

	return 0;
}


struct file_operations mymsg_fops = {
	.open = mymsg_open,
	.read = mymsg_read,
};

static __init int myprintk_init(void)
{
	mymsg_entry = create_proc_entry("mymsg", S_IRUSR, &proc_root);
	if (!mymsg_entry)
		return -ENOMEM;
	mymsg_entry->proc_fops = &mymsg_fops;

	memset(myprint_buf, 0, sizeof(myprint_buf));

	return 0;
}


static __exit void myprintk_exit(void)
{
	remove_proc_entry("mymsg", &proc_root);
}

module_init(myprintk_init);
module_exit(myprintk_exit);
MODULE_LICENSE("GPL");

