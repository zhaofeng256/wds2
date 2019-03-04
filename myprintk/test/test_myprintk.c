#include <linux/module.h>


static __init int test_myprintk_init(void)
{
	int i = 0;
	for (; i < 10; i++)
		myprintk("test my printk %d\n", i);

	return 0;
}


static __exit void test_myprintk_exit(void)
{
}

module_init(test_myprintk_init);
module_exit(test_myprintk_exit);
MODULE_LICENSE("GPL");

