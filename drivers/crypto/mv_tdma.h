#ifndef _MV_TDMA_H
#define _MV_TDMA_H

/* TDMA_CTRL register bits */
#define TDMA_CTRL_DST_BURST(x)	(x)
#define TDMA_CTRL_DST_BURST_32	TDMA_CTRL_DST_BURST(3)
#define TDMA_CTRL_DST_BURST_128	TDMA_CTRL_DST_BURST(4)
#define TDMA_CTRL_OUTST_RD_EN	(1 << 4)
#define TDMA_CTRL_SRC_BURST(x)	(x << 6)
#define TDMA_CTRL_SRC_BURST_32	TDMA_CTRL_SRC_BURST(3)
#define TDMA_CTRL_SRC_BURST_128	TDMA_CTRL_SRC_BURST(4)
#define TDMA_CTRL_NO_CHAIN_MODE	(1 << 9)
#define TDMA_CTRL_NO_BYTE_SWAP	(1 << 11)
#define TDMA_CTRL_ENABLE	(1 << 12)
#define TDMA_CTRL_FETCH_ND	(1 << 13)
#define TDMA_CTRL_ACTIVE	(1 << 14)

#define TDMA_CTRL_INIT_VALUE ( \
	TDMA_CTRL_DST_BURST_128 | TDMA_CTRL_SRC_BURST_128 | \
	TDMA_CTRL_NO_BYTE_SWAP | TDMA_CTRL_ENABLE \
)

/* TDMA_ERR_CAUSE bits */
#define TDMA_INT_MISS		(1 << 0)
#define TDMA_INT_DOUBLE_HIT	(1 << 1)
#define TDMA_INT_BOTH_HIT	(1 << 2)
#define TDMA_INT_DATA_ERROR	(1 << 3)
#define TDMA_INT_ALL		0x0f

/* offsets of registers, starting at "regs control and error" */
#define TDMA_BYTE_COUNT		0x00
#define TDMA_SRC_ADDR		0x10
#define TDMA_DST_ADDR		0x20
#define TDMA_NEXT_DESC		0x30
#define TDMA_CTRL		0x40
#define TDMA_CURR_DESC		0x70
#define TDMA_ERR_CAUSE		0xc8
#define TDMA_ERR_MASK		0xcc

/* Owner bit in TDMA_BYTE_COUNT and descriptors' count field, used
 * to signal TDMA in descriptor chain when input data is complete. */
#define TDMA_OWN_BIT		(1 << 31)

extern void mv_tdma_memcpy(dma_addr_t, dma_addr_t, unsigned int);
extern void mv_tdma_separator(void);
extern void mv_tdma_clear(void);
extern void mv_tdma_trigger(void);


#endif /* _MV_TDMA_H */
