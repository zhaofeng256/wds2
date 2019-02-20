#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/string.h>

static struct nand_chip g_chip;
static struct mtd_info g_mtd;
static struct mtd_partition g_nand_partions[] = {
	[0] = {
		.name	= "bootloader",
		.size	= 0x00040000,
		.offset = 0,
	},
	[1] = {
		.name	= "params",
		.offset = MTDPART_OFS_APPEND,
		.size	= 0x00020000,
	},
	[2] = {
		.name	= "kernel",
		.offset = MTDPART_OFS_APPEND,
		.size	= 0x00200000,
	},
	[3] = {
		.name	= "root",
		.offset = MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL,
	}
};

static struct nand_regs_t {
	u32	nfconf;
	u32	nfcont;
	u32	nfcmd;
	u32	nfaddr;
	u32	nfdata;
	u32	nfmeccd0;
	u32	nfmeccd1;
	u32	nfseccd;
	u32	nfstat;
	u32	nfestat0;
	u32	nfestat1;
	u32	nfmecc0;
	u32	nfmecc1;
	u32	nfsecc;
	u32	nfsblk;
	u32	nfeblk;
} *g_p_nand_reg;

#ifdef HW_ECC

static struct nand_ecclayout nand_hw_eccoob = {
	.eccbytes	= 3,
	.eccpos		= { 0,1,  2 },
	.oobfree	= { { 8,8 } }
};

#endif
static void nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	writesb(g_chip.IO_ADDR_W, buf, len);
}

static void nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	readsb(g_chip.IO_ADDR_R, buf, len);
}

static void nand_select_chip(struct mtd_info *mtd, int chip)
{
	if (-1 == chip)
		g_p_nand_reg->nfcont |= (0x1 << 1);
	else
		g_p_nand_reg->nfcont &= ~(0x1 << 1);
}

static void nand_cmd_ctrl(struct mtd_info *mtd, int dat, unsigned int ctrl)
{
	if (ctrl & NAND_CLE)
		g_p_nand_reg->nfcmd = dat;
	else
		g_p_nand_reg->nfaddr = dat;
}

static int nand_dev_ready(struct mtd_info *mtd)
{
	return g_p_nand_reg->nfstat & (0x1);
}


#ifdef HW_ECC
void nand_enable_hwecc(struct mtd_info *mtd, int mode)
{
	g_p_nand_reg->nfcont |= (0x1 << 4);
}

int nand_calculate_ecc(struct mtd_info *mtd,
		       const uint8_t *	dat,
		       uint8_t *	ecc_code)
{
}
#endif


int __init nand_init(void)
{
	struct clk *nand_clock = 0;

	g_p_nand_reg = ioremap(0x4E000000, sizeof(struct nand_regs_t));
	memset(&g_chip, 0, sizeof(struct nand_chip));
	memset(&g_mtd, 0, sizeof(struct mtd_info));


	g_chip.write_buf = nand_write_buf;
	g_chip.read_buf = nand_read_buf;
	g_chip.select_chip = nand_select_chip;
	g_chip.cmd_ctrl = nand_cmd_ctrl;
	g_chip.dev_ready = nand_dev_ready;

	g_chip.IO_ADDR_R = &g_p_nand_reg->nfdata;
	g_chip.IO_ADDR_W = &g_p_nand_reg->nfdata;
	g_chip.chip_delay = 50;
	g_chip.options = 0;


#ifdef HW_ECC
	g_chip.ecc.hwctl = nand_enable_hwecc;
	g_chip.ecc.calculate = nand_calculate_ecc;
	g_chip.ecc.mode = NAND_ECC_HW;
	g_chip.ecc.size = 512;
	g_chip.ecc.bytes = 3;
	g_chip.ecc.layout = &nand_hw_eccoob;
#else
	g_chip.ecc.mode = NAND_ECC_SOFT;
#endif

	g_mtd.owner = THIS_MODULE;
	g_mtd.priv = &g_chip;

	nand_clock = clk_get(NULL, "nand");
	if (IS_ERR(nand_clock)) {
		printk("failed to get clock");
		goto exit_err;
	}

	clk_enable(nand_clock);

	/* enalbe controller */
	g_p_nand_reg->nfcont |= 0x1;
	/* nand timing */
	g_p_nand_reg->nfconf |= (0x0 << 12) | (0x1 << 8) | (0x0 << 4);
	/* select chip */
	g_p_nand_reg->nfcont |= (0x1 << 1);

	if (0 == nand_scan(&g_mtd, 1))
		add_mtd_partitions(&g_mtd, g_nand_partions, 4);

	return 0;
exit_err:
	return -1;
}

void __exit nand_exit(void)
{
	nand_release(&g_mtd);
	iounmap(g_p_nand_reg);
}
module_init(nand_init);
module_exit(nand_exit);
MODULE_LICENSE("GPL");
