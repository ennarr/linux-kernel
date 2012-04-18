/*
 * Support for Marvell's TDMA engine found on Kirkwood chips,
 * used exclusively by the CESA crypto accelerator.
 *
 * Based on unpublished code for IDMA written by Sebastian Siewior.
 *
 * Copyright (C) 2012 Phil Sutter <phil.sutter@viprinet.com>
 * License: GPLv2
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include "mv_tdma.h"
#include "dma_desclist.h"

#define MV_TDMA "MV-TDMA: "

#define MV_DMA_INIT_POOLSIZE 16
#define MV_DMA_ALIGN 16

struct tdma_desc {
	u32 count;
	u32 src;
	u32 dst;
	u32 next;
} __attribute__((packed));

struct tdma_priv {
	struct device *dev;
	void __iomem *reg;
	int irq;
	/* protecting the dma descriptors and stuff */
	spinlock_t lock;
	struct dma_desclist desclist;
} tpg;

#define ITEM(x)		((struct tdma_desc *)DESCLIST_ITEM(tpg.desclist, x))
#define ITEM_DMA(x)	DESCLIST_ITEM_DMA(tpg.desclist, x)

static inline void wait_for_tdma_idle(void)
{
	while (readl(tpg.reg + TDMA_CTRL) & TDMA_CTRL_ACTIVE)
		mdelay(100);
}

static inline void switch_tdma_engine(bool state)
{
	u32 val = readl(tpg.reg + TDMA_CTRL);

	val |=  ( state * TDMA_CTRL_ENABLE);
	val &= ~(!state * TDMA_CTRL_ENABLE);

	writel(val, tpg.reg + TDMA_CTRL);
}

static struct tdma_desc *get_new_last_desc(void)
{
	if (unlikely(DESCLIST_FULL(tpg.desclist)) &&
	    set_dma_desclist_size(&tpg.desclist, tpg.desclist.length << 1)) {
		printk(KERN_ERR MV_TDMA "failed to increase DMA pool to %lu\n",
				tpg.desclist.length << 1);
		return NULL;
	}

	if (likely(tpg.desclist.usage))
		ITEM(tpg.desclist.usage - 1)->next =
			ITEM_DMA(tpg.desclist.usage);

	return ITEM(tpg.desclist.usage++);
}

static inline void mv_tdma_desc_dump(void)
{
	struct tdma_desc *tmp;
	int i;

	if (!tpg.desclist.usage) {
		printk(KERN_WARNING MV_TDMA "DMA descriptor list is empty\n");
		return;
	}

	printk(KERN_WARNING MV_TDMA "DMA descriptor list:\n");
	for (i = 0; i < tpg.desclist.usage; i++) {
		tmp = ITEM(i);
		printk(KERN_WARNING MV_TDMA "entry %d at 0x%x: dma addr 0x%x, "
		       "src 0x%x, dst 0x%x, count %u, own %d, next 0x%x", i,
		       (u32)tmp, ITEM_DMA(i) , tmp->src, tmp->dst,
		       tmp->count & ~TDMA_OWN_BIT, !!(tmp->count & TDMA_OWN_BIT),
		       tmp->next);
	}
}

static inline void mv_tdma_reg_dump(void)
{
#define PRINTREG(offset) \
	printk(KERN_WARNING MV_TDMA "tpg.reg + " #offset " = 0x%x\n", \
			readl(tpg.reg + offset))

	PRINTREG(TDMA_CTRL);
	PRINTREG(TDMA_BYTE_COUNT);
	PRINTREG(TDMA_SRC_ADDR);
	PRINTREG(TDMA_DST_ADDR);
	PRINTREG(TDMA_NEXT_DESC);
	PRINTREG(TDMA_CURR_DESC);

#undef PRINTREG
}

void mv_tdma_clear(void)
{
	if (!tpg.dev)
		return;

	spin_lock(&tpg.lock);

	/* make sure tdma is idle */
	wait_for_tdma_idle();
	switch_tdma_engine(0);
	wait_for_tdma_idle();

	/* clear descriptor registers */
	writel(0, tpg.reg + TDMA_BYTE_COUNT);
	writel(0, tpg.reg + TDMA_CURR_DESC);
	writel(0, tpg.reg + TDMA_NEXT_DESC);

	tpg.desclist.usage = 0;

	switch_tdma_engine(1);

	/* finally free system lock again */
	spin_unlock(&tpg.lock);
}
EXPORT_SYMBOL_GPL(mv_tdma_clear);

void mv_tdma_trigger(void)
{
	if (!tpg.dev)
		return;

	spin_lock(&tpg.lock);

	writel(ITEM_DMA(0), tpg.reg + TDMA_NEXT_DESC);

	spin_unlock(&tpg.lock);
}
EXPORT_SYMBOL_GPL(mv_tdma_trigger);

void mv_tdma_separator(void)
{
	struct tdma_desc *tmp;

	if (!tpg.dev)
		return;

	spin_lock(&tpg.lock);

	tmp = get_new_last_desc();
	memset(tmp, 0, sizeof(*tmp));

	spin_unlock(&tpg.lock);
}
EXPORT_SYMBOL_GPL(mv_tdma_separator);

void mv_tdma_memcpy(dma_addr_t dst, dma_addr_t src, unsigned int size)
{
	struct tdma_desc *tmp;

	if (!tpg.dev)
		return;

	spin_lock(&tpg.lock);

	tmp = get_new_last_desc();
	tmp->count = size | TDMA_OWN_BIT;
	tmp->src = src;
	tmp->dst = dst;
	tmp->next = 0;

	spin_unlock(&tpg.lock);
}
EXPORT_SYMBOL_GPL(mv_tdma_memcpy);

irqreturn_t tdma_int(int irq, void *priv)
{
	u32 val;

	val = readl(tpg.reg + TDMA_ERR_CAUSE);

	if (val & TDMA_INT_MISS)
		printk(KERN_ERR MV_TDMA "%s: miss!\n", __func__);
	if (val & TDMA_INT_DOUBLE_HIT)
		printk(KERN_ERR MV_TDMA "%s: double hit!\n", __func__);
	if (val & TDMA_INT_BOTH_HIT)
		printk(KERN_ERR MV_TDMA "%s: both hit!\n", __func__);
	if (val & TDMA_INT_DATA_ERROR)
		printk(KERN_ERR MV_TDMA "%s: data error!\n", __func__);
	if (val) {
		mv_tdma_reg_dump();
		mv_tdma_desc_dump();
	}

	switch_tdma_engine(0);
	wait_for_tdma_idle();

	/* clear descriptor registers */
	writel(0, tpg.reg + TDMA_BYTE_COUNT);
	writel(0, tpg.reg + TDMA_SRC_ADDR);
	writel(0, tpg.reg + TDMA_DST_ADDR);
	writel(0, tpg.reg + TDMA_CURR_DESC);

	/* clear error cause register */
	writel(0, tpg.reg + TDMA_ERR_CAUSE);

	/* initialize control register (also enables engine) */
	writel(TDMA_CTRL_INIT_VALUE, tpg.reg + TDMA_CTRL);
	wait_for_tdma_idle();

	return (val ? IRQ_HANDLED : IRQ_NONE);
}

static int mv_probe(struct platform_device *pdev)
{
	struct resource *res;
	int rc;

	if (tpg.dev) {
		printk(KERN_ERR MV_TDMA "second TDMA device?!\n");
		return -ENXIO;
	}
	tpg.dev = &pdev->dev;

	res = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "regs control and error");
	if (!res)
		return -ENXIO;

	if (!(tpg.reg = ioremap(res->start, resource_size(res))))
		return -ENOMEM;

	tpg.irq = platform_get_irq(pdev, 0);
	if (tpg.irq < 0 || tpg.irq == NO_IRQ) {
		rc = -ENXIO;
		goto out_unmap_reg;
	}

	if (init_dma_desclist(&tpg.desclist, tpg.dev,
			sizeof(struct tdma_desc), MV_DMA_ALIGN, 0)) {
		rc = -ENOMEM;
		goto out_free_irq;
	}
	if (set_dma_desclist_size(&tpg.desclist, MV_DMA_INIT_POOLSIZE)) {
		rc = -ENOMEM;
		goto out_free_desclist;
	}

	platform_set_drvdata(pdev, &tpg);

	switch_tdma_engine(0);
	wait_for_tdma_idle();

	/* clear descriptor registers */
	writel(0, tpg.reg + TDMA_BYTE_COUNT);
	writel(0, tpg.reg + TDMA_SRC_ADDR);
	writel(0, tpg.reg + TDMA_DST_ADDR);
	writel(0, tpg.reg + TDMA_CURR_DESC);

	/* have an ear for occurring errors */
	writel(TDMA_INT_ALL, tpg.reg + TDMA_ERR_MASK);
	writel(0, tpg.reg + TDMA_ERR_CAUSE);

	/* initialize control register (also enables engine) */
	writel(TDMA_CTRL_INIT_VALUE, tpg.reg + TDMA_CTRL);
	wait_for_tdma_idle();

	if (request_irq(tpg.irq, tdma_int, IRQF_DISABLED,
				dev_name(tpg.dev), &tpg)) {
		rc = -ENXIO;
		goto out_free_all;
	}

	spin_lock_init(&tpg.lock);

	printk(KERN_INFO MV_TDMA "up and running, IRQ %d\n", tpg.irq);
	return 0;
out_free_all:
	switch_tdma_engine(0);
	platform_set_drvdata(pdev, NULL);
out_free_desclist:
	fini_dma_desclist(&tpg.desclist);
out_free_irq:
	free_irq(tpg.irq, &tpg);
out_unmap_reg:
	iounmap(tpg.reg);
	tpg.dev = NULL;
	return rc;
}

static int mv_remove(struct platform_device *pdev)
{
	switch_tdma_engine(0);
	platform_set_drvdata(pdev, NULL);
	fini_dma_desclist(&tpg.desclist);
	free_irq(tpg.irq, &tpg);
	iounmap(tpg.reg);
	tpg.dev = NULL;
	return 0;
}

static struct platform_driver marvell_tdma = {
	.probe          = mv_probe,
	.remove         = mv_remove,
	.driver         = {
		.owner  = THIS_MODULE,
		.name   = "mv_tdma",
	},
};
MODULE_ALIAS("platform:mv_tdma");

static int __init mv_tdma_init(void)
{
	return platform_driver_register(&marvell_tdma);
}
module_init(mv_tdma_init);

static void __exit mv_tdma_exit(void)
{
	platform_driver_unregister(&marvell_tdma);
}
module_exit(mv_tdma_exit);

MODULE_AUTHOR("Phil Sutter <phil.sutter@viprinet.com>");
MODULE_DESCRIPTION("Support for Marvell's TDMA engine");
MODULE_LICENSE("GPL");

