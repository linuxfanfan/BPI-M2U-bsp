/*
 * drivers/usb/sunxi_usb/udc/sunxi_udc_dma.c
 * (C) Copyright 2010-2015
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * javen, 2010-3-3, create this file
 *
 * usb udc dma ops
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <asm/cacheflush.h>

#include  "sunxi_udc_config.h"
#include  "sunxi_udc_board.h"
#include  "sunxi_udc_dma.h"

dma_channel_t dma_chnl[DMA_CHAN_TOTAL];
extern int g_dma_debug;

/* switch usb bus for dma */
void sunxi_udc_switch_bus_to_dma(struct sunxi_udc_ep *ep, u32 is_tx)
{
	return;
}

/* switch usb bus for pio */
void sunxi_udc_switch_bus_to_pio(struct sunxi_udc_ep *ep, __u32 is_tx)
{
	return;
}

/* enable dma channel irq */
void sunxi_udc_enable_dma_channel_irq(struct sunxi_udc_ep *ep)
{
	DMSG_DBG_DMA("sunxi_udc_enable_dma_channel_irq\n");
	return;
}

/* disable dma channel irq */
void sunxi_udc_disable_dma_channel_irq(struct sunxi_udc_ep *ep)
{
}

#ifdef SW_UDC_DMA_INNER
dm_hdl_t sunxi_udc_dma_request(void)
{
	int i = 0;
	dma_channel_t *pchan = NULL;

	/* get a free channel */
	for(i = 0; i < DMA_CHAN_TOTAL; i++) {
		pchan = &dma_chnl[i];
		if (0 == pchan->used) {
			pchan->used = 1;
			pchan->channel_num = i;
			spin_lock_init(&pchan->lock);
			return (dm_hdl_t)pchan;
		}
	}

	return (dm_hdl_t)NULL;
}

int sunxi_udc_dma_release(dm_hdl_t dma_hdl)
{
	dma_channel_t *pchan = NULL;
	u32 reg_value = 0;

	if (dma_hdl == NULL) {
		DMSG_PANIC("ERR: sunxi_udc_dma_release failed dma_hdl is NULL\n");
		return -1;
	}

	pchan = (dma_channel_t *)dma_hdl;
	reg_value = USBC_Readw(USBC_REG_DMA_INTE(pchan->reg_base));
	reg_value &= ~(1 << (pchan->channel_num & 0xff));
	USBC_Writew(reg_value, USBC_REG_DMA_INTE(pchan->reg_base));

	pchan->used = 0;
	pchan->channel_num = 0;

	return 0;
}

int sunxi_udc_dma_chan_disable(dm_hdl_t dma_hdl)
{
	dma_channel_t *pchan = NULL;
	u32 reg_value = 0;

	if (dma_hdl == NULL) {
		DMSG_PANIC("[sunxi_udc_dma_chan_disable] dma_hdl is NULL\n");
		return -1;
	}

	pchan = (dma_channel_t *)dma_hdl;
	reg_value = USBC_Readl(USBC_REG_DMA_CHAN_CFN(pchan->reg_base,
					pchan->channel_num));
	reg_value &= ~(1 << 31);
	USBC_Writel(reg_value, USBC_REG_DMA_CHAN_CFN(pchan->reg_base,
					pchan->channel_num));

	pchan->used = 0;
	pchan->channel_num = 0;

	return 0;
}

/* config dma */
void sunxi_dma_set_config(dm_hdl_t dma_hdl, struct dma_config_t *pcfg)
{
	u32 reg_value = 0;
	unsigned long flags;
	dma_channel_t *pchan = NULL;

	if (dma_hdl == NULL || pcfg == NULL) {
		DMSG_PANIC("ERR: sunxi_dma_set_config failed dma_hdl or pcfg is NULL\n");
		return;
	}

	pchan = (dma_channel_t *)dma_hdl;

	spin_lock_irqsave(&pchan->lock, flags);

	reg_value = USBC_Readl(USBC_REG_DMA_CHAN_CFN(pchan->reg_base, pcfg->dma_num));
	reg_value &= ~((1 << 4) | (0xf << 0) | (0x7ff << 16));

	//eplen
	reg_value |=  (((pcfg->dma_bst_len) & 0x7ff) << 16);

	//DIR
	reg_value |= ((pcfg->dma_dir & 1) << 4);

	//ep num
	reg_value |=  ((pcfg->dma_for_ep & 0xf) << 0);
	USBC_Writel(reg_value, USBC_REG_DMA_CHAN_CFN(pchan->reg_base, pcfg->dma_num));

	//address
	USBC_Writel(pcfg->dma_sdram_str_addr, USBC_REG_DMA_SDRAM_ADD(pchan->reg_base, pcfg->dma_num));

	//transport len
	USBC_Writel((pcfg->dma_bc), USBC_REG_DMA_BC(pchan->reg_base, pcfg->dma_num));

	reg_value = USBC_Readw(USBC_REG_DMA_INTE(pchan->reg_base));
	reg_value |= (1 << (pcfg->dma_num & 0xff));
	USBC_Writew(reg_value, USBC_REG_DMA_INTE(pchan->reg_base));

	/* start dma */
	reg_value = USBC_Readl(USBC_REG_DMA_CHAN_CFN(pchan->reg_base, pcfg->dma_num));
	reg_value |=  (1U << 31);
	USBC_Writel(reg_value, USBC_REG_DMA_CHAN_CFN(pchan->reg_base, pcfg->dma_num));

	spin_unlock_irqrestore(&pchan->lock, flags);
}

void sunxi_udc_dma_set_config(struct sunxi_udc_ep *ep, struct sunxi_udc_request *req, __u32 buff_addr, __u32 len)
{
	dm_hdl_t dma_hdl = NULL;
	dma_channel_t *pchan = NULL;
	__u32 is_tx = 0;
	__u32 packet_size = 0;

	struct dma_config_t DmaConfig;

	memset(&DmaConfig, 0, sizeof(DmaConfig));

	is_tx = is_tx_ep(ep);
	packet_size = ep->ep.maxpacket;

	dma_hdl = sunxi_udc_dma_request();
	if (dma_hdl == NULL) {
		DMSG_PANIC("ERR: sunxi_udc_dma_request failed dma_hdl is NULL\n");
		return;
	}

	ep->dev->dma_hdle = dma_hdl;
	pchan = (dma_channel_t *)dma_hdl;
	if (is_tx)
		pchan->ep_num = ep_fifo_in[ep->num];
	else /* rx */
		pchan->ep_num = ep_fifo_out[ep->num];

	pchan->reg_base = ep->dev->sunxi_udc_io->usb_vbase,

	DmaConfig.dma_bst_len = packet_size;
	DmaConfig.dma_dir = !is_tx;
	DmaConfig.dma_for_ep = ep->num;
	DmaConfig.dma_bc = len;
	DmaConfig.dma_sdram_str_addr = buff_addr;
	DmaConfig.dma_num = pchan->channel_num;

	sunxi_dma_set_config(dma_hdl, &DmaConfig);
}

/* start dma transfer */
void sunxi_udc_dma_start(struct sunxi_udc_ep *ep, void __iomem  *fifo, __u32 buffer, __u32 len)
{
}

/* stop dma transfer */
void sunxi_udc_dma_stop(struct sunxi_udc_ep *ep)
{
}

/* query the length that has been transferred */
__u32 sunxi_udc_dma_transmit_length(struct sunxi_udc_ep *ep)
{
	return ep->dma_transfer_len;;
}

/* check if dma busy */
__u32 sunxi_udc_dma_is_busy(struct sunxi_udc_ep *ep)
{
	return ep->dma_working;
}

/* dma initilize */
__s32 sunxi_udc_dma_probe(struct sunxi_udc *dev)
{
	return 0;
}

/* dma remove */
__s32 sunxi_udc_dma_remove(struct sunxi_udc *dev)
{
	return 0;
}

#else

#ifdef CONFIG_DMA_ENGINE
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dma/sunxi-dma.h>
#endif
static sunxi_udc_dma_parg_t sunxi_udc_dma_para;

static void sunxi_udc_dma_callback1(void *parg)
{
	struct sunxi_udc *dev = NULL;
	struct sunxi_udc_request *req = NULL;
	struct sunxi_udc_ep *ep = NULL;

	dev = (struct sunxi_udc *)parg;
	if (dev == NULL) {
		DMSG_PANIC("ERR: sw_udc_dma_callback failed\n");
		return;
	}

	/* find ep */
	ep = sunxi_udc_dma_para.ep[1];
	if (sunxi_udc_dma_para.ep[1]) {
		sunxi_udc_dma_para.ep[1] = NULL;
	}
	if (ep) {
		/* find req */
		if (likely (!list_empty(&ep->queue))) {
			req = list_entry(ep->queue.next, struct sunxi_udc_request, queue);
		} else {
			req = NULL;
		}

		/* call back */
		if (req) {
			sunxi_udc_dma_completion(dev, ep, req);
		}
	} else {
		DMSG_PANIC("ERR: sw_udc_dma_callback: dma is remove, but dma irq is happened\n");
	}
}

static void sunxi_udc_dma_callback2(void *parg)
{
	struct sunxi_udc *dev = NULL;
	struct sunxi_udc_request *req = NULL;
	struct sunxi_udc_ep *ep = NULL;

	dev = (struct sunxi_udc *)parg;
	if (dev == NULL) {
		DMSG_PANIC("ERR: sw_udc_dma_callback failed\n");
		return;
	}

	/* find ep */
	ep = sunxi_udc_dma_para.ep[2];
	if (sunxi_udc_dma_para.ep[2]) {
		sunxi_udc_dma_para.ep[2] = NULL;
	}
	if (ep) {
		/* find req */
		if (likely (!list_empty(&ep->queue))) {
			req = list_entry(ep->queue.next, struct sunxi_udc_request, queue);
		} else {
			req = NULL;
		}

		/* call back */
		if (req) {
		sunxi_udc_dma_completion(dev, ep, req);
		}
	} else {
		DMSG_PANIC("ERR: sw_udc_dma_callback: dma is remove, but dma irq is happened\n");
	}
}

static void sunxi_udc_dma_callback3(void *parg)
{
	struct sunxi_udc *dev = NULL;
	struct sunxi_udc_request *req = NULL;
	struct sunxi_udc_ep *ep = NULL;

	dev = (struct sunxi_udc *)parg;
	if (dev == NULL) {
		DMSG_PANIC("ERR: sw_udc_dma_callback failed\n");
		return;
	}

	/* find ep */
	ep = sunxi_udc_dma_para.ep[3];
	if (sunxi_udc_dma_para.ep[3]) {
		sunxi_udc_dma_para.ep[3] = NULL;
	}
	if (ep) {
		/* find req */
		if (likely (!list_empty(&ep->queue))) {
			req = list_entry(ep->queue.next, struct sunxi_udc_request, queue);
		} else {
			req = NULL;
		}

		/* call back */
		if (req) {
		sunxi_udc_dma_completion(dev, ep, req);
		}
	} else {
		DMSG_PANIC("ERR: sw_udc_dma_callback: dma is remove, but dma irq is happened\n");
	}
}

static void sunxi_udc_dma_callback4(void *parg)
{
	struct sunxi_udc *dev = NULL;
	struct sunxi_udc_request *req = NULL;
	struct sunxi_udc_ep *ep = NULL;

	dev = (struct sunxi_udc *)parg;
	if (dev == NULL) {
		DMSG_PANIC("ERR: sw_udc_dma_callback failed\n");
		return;
	}

	/* find ep */
	ep = sunxi_udc_dma_para.ep[4];
	if (sunxi_udc_dma_para.ep[4]) {
		sunxi_udc_dma_para.ep[4] = NULL;
	}
	if (ep) {
		/* find req */
		if (likely (!list_empty(&ep->queue))) {
			req = list_entry(ep->queue.next, struct sunxi_udc_request, queue);
		} else {
			req = NULL;
		}

		/* call back */
		if (req) {
		sunxi_udc_dma_completion(dev, ep, req);
		}
	} else {
		DMSG_PANIC("ERR: sw_udc_dma_callback: dma is remove, but dma irq is happened\n");
	}
}

static void sunxi_udc_dma_callback5(void *parg)
{
	struct sunxi_udc *dev = NULL;
	struct sunxi_udc_request *req = NULL;
	struct sunxi_udc_ep *ep = NULL;

	dev = (struct sunxi_udc *)parg;
	if (dev == NULL) {
		DMSG_PANIC("ERR: sw_udc_dma_callback failed\n");
		return;
	}

	/* find ep */
	ep = sunxi_udc_dma_para.ep[5];
	if (sunxi_udc_dma_para.ep[5]) {
		sunxi_udc_dma_para.ep[5] = NULL;
	}
	if (ep) {
		/* find req */
		if (likely (!list_empty(&ep->queue))) {
			req = list_entry(ep->queue.next, struct sunxi_udc_request, queue);
		} else {
			req = NULL;
		}

		/* call back */
		if (req) {
		sunxi_udc_dma_completion(dev, ep, req);
		}
	} else {
		DMSG_PANIC("ERR: sw_udc_dma_callback: dma is remove, but dma irq is happened\n");
	}
}

dm_hdl_t sunxi_udc_dma_request(void)
{
	return (dm_hdl_t)NULL;
}

int sunxi_udc_dma_release(dm_hdl_t dma_hdl)
{
	return 0;
}

void sunxi_udc_dma_set_config(struct sunxi_udc_ep *ep, struct sunxi_udc_request *req, __u32 buff_addr, __u32 len)
{
	__u32 is_tx = 0;
	void __iomem	*fifo_addr = NULL;
	unsigned int usbc_no = 0;
	struct dma_slave_config slave_config;

	if (ep->dev->sunxi_udc_dma[ep->num].chan ==  NULL) {
		DMSG_PANIC("udc_dma start error,DMA is NULL.\n");
		return ;
	}

	memset(&slave_config, 0, sizeof(slave_config));
	is_tx = is_tx_ep(ep);

	fifo_addr = USBC_REG_EPFIFOx(ep->dev->sunxi_udc_io->usb_vbase, ep->num);
	switch(ep->num) {
	case 1:
		usbc_no = DRQSRC_OTG_EP1;
		break;
	case 2:
		usbc_no = DRQSRC_OTG_EP2;
		break;
	case 3:
		usbc_no = DRQSRC_OTG_EP3;
		break;
#ifndef CONFIG_ARCH_SUN3IW1
	case 4:
		usbc_no = DRQSRC_OTG_EP4;
		break;
#endif
#if defined(CONFIG_ARCH_SUN50I) || defined(CONFIG_ARCH_SUN8IW6) \
	|| defined(CONFIG_ARCH_SUN8IW5)
	case 5:
		usbc_no = DRQSRC_OTG_EP5;
		break;
#endif
	default:
		usbc_no = 0;
	}

	sunxi_udc_dma_para.ep[ep->num] = ep;
	if (!is_tx) { /* ep in, rx*/
		slave_config.direction = DMA_DEV_TO_MEM;
		slave_config.src_addr = (dma_addr_t)fifo_addr;
		slave_config.dst_addr = buff_addr;
		slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		slave_config.src_maxburst = 1;
		slave_config.dst_maxburst = 1;
		slave_config.slave_id = sunxi_slave_id(DRQDST_SDRAM, usbc_no);
		dmaengine_slave_config(ep->dev->sunxi_udc_dma[ep->num].chan, &slave_config);
	} else { /* ep out, tx*/
		slave_config.direction = DMA_MEM_TO_DEV;
		slave_config.src_addr = buff_addr;
		slave_config.dst_addr = (dma_addr_t)fifo_addr;
		slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		slave_config.src_maxburst = 1;
		slave_config.dst_maxburst = 1;
		slave_config.slave_id = sunxi_slave_id(usbc_no, DRQSRC_SDRAM);
		dmaengine_slave_config(ep->dev->sunxi_udc_dma[ep->num].chan, &slave_config);
	}
}

void sunxi_udc_dma_start(struct sunxi_udc_ep *ep,  void __iomem  *fifo, __u32 buffer, __u32 len)
{
	struct dma_async_tx_descriptor *dma_desc = NULL;
	__u32 is_tx = 0;

	is_tx = is_tx_ep(ep);
	if (ep->dev->sunxi_udc_dma[ep->num].chan ==  NULL) {
		DMSG_PANIC("udc_dma start error,DMA is NULL.\n");
		return ;
	}
	//if (!ep->dev->sunxi_udc_dma[ep->num].is_start) {
		ep->dev->sunxi_udc_dma[ep->num].is_start = 1;
		if (is_tx) {
			dma_desc = dmaengine_prep_slave_single(ep->dev->sunxi_udc_dma[ep->num].chan,buffer, len,
						DMA_MEM_TO_DEV, DMA_PREP_INTERRUPT|DMA_CTRL_ACK);
			if (!dma_desc) {
				DMSG_PANIC("[sunxi-udc-%d]dmaengine_prep_slave_sg() failed!\n", ep->num);
				//dma_unmap_sg(&sspi->pdev->dev, sspi->dma_rx.sg, sspi->dma_rx.nents, DMA_FROM_DEVICE);
				return;
			}
		} else {
			dma_desc = dmaengine_prep_slave_single(ep->dev->sunxi_udc_dma[ep->num].chan, buffer, len,
						DMA_DEV_TO_MEM, DMA_PREP_INTERRUPT|DMA_CTRL_ACK);
			if (!dma_desc) {
				DMSG_PANIC("[sunxi-udc-%d]dmaengine_prep_slave_sg() failed!\n", ep->num);
				//dma_unmap_sg(&sspi->pdev->dev, sspi->dma_rx.sg, sspi->dma_rx.nents, DMA_FROM_DEVICE);
				return ;
			}
		}

		switch(ep->num) {
		case 1:
			dma_desc->callback = sunxi_udc_dma_callback1;
			break;
		case 2:
			dma_desc->callback = sunxi_udc_dma_callback2;
			break;
		case 3:
			dma_desc->callback = sunxi_udc_dma_callback3;
			break;
		case 4:
			dma_desc->callback = sunxi_udc_dma_callback4;
			break;
		case 5:
			dma_desc->callback = sunxi_udc_dma_callback5;
			break;
		default:
			break;
		}

		dma_desc->callback_param = (void *)ep->dev;
		dmaengine_submit(dma_desc);
		dma_async_issue_pending(ep->dev->sunxi_udc_dma[ep->num].chan);
	//}
}

void sunxi_udc_dma_stop(struct sunxi_udc_ep *ep)
{
	int ret = 0;

	DMSG_DBG_DMA("line:%d, %s\n", __LINE__, __func__);

	ret = dmaengine_terminate_all(ep->dev->sunxi_udc_dma[ep->num].chan);
	if (ret != 0) {
		DMSG_PANIC("ERR: sunxi_dma_ctl stop  failed\n");
		return;
	}

	if (sunxi_udc_dma_para.ep[ep->num])
		sunxi_udc_dma_para.ep[ep->num] = NULL;
}

__u32 sunxi_udc_dma_transmit_length(struct sunxi_udc_ep *ep)
{
	return ep->dma_transfer_len;;
}

__u32 sunxi_udc_dma_is_busy(struct sunxi_udc_ep *ep)
{
	return ep->dma_working;
}

__s32 sunxi_udc_dma_probe(struct sunxi_udc *dev)
{
	dma_cap_mask_t mask;
	int i = 0;

	memset(&sunxi_udc_dma_para, 0, sizeof(sunxi_udc_dma_parg_t));
	sunxi_udc_dma_para.dev = dev;
	DMSG_INFO("sunxi_udc_dma_probe version77..\n");

	/* Try to acquire a generic DMA engine slave channel */
	for(i = 1; i <= (USBC_MAX_EP_NUM - 1); i++) {
		dma_cap_zero(mask);
		dma_cap_set(DMA_SLAVE, mask);

		if (dev->sunxi_udc_dma[i].chan == NULL) {
			dev->sunxi_udc_dma[i].chan = dma_request_channel(mask, NULL, NULL);
			if ( dev->sunxi_udc_dma[i].chan== NULL) {
				DMSG_PANIC("Request DMA(EP%d) failed!\n", i);
				return -EINVAL;
			}
		}
	}

	return 0;
}

__s32 sunxi_udc_dma_remove(struct sunxi_udc *dev)
{
#if 0
	int i = 0;
	int ret = 0;
#endif
	DMSG_DBG_DMA("line:%d, %s\n", __LINE__, __func__);
#if 0
	for(i = 1; i <= (USBC_MAX_EP_NUM - 1); i++) {
		if (dev->sunxi_udc_dma[i].chan!= NULL) {
			ret = dmaengine_terminate_all(dev->sunxi_udc_dma[i].chan);
			if (ret != 0) {
				DMSG_PANIC("ERR: sunxi_udc_dma_remove: stop failed\n");
			}
			dma_release_channel(dev->sunxi_udc_dma[i].chan);

			//dev->sunxi_udc_dma[i].dma_hdle = 0;
			dev->sunxi_udc_dma[i].is_start = 0;
			dev->ep[i].dma_working = 0;
			dev->ep[i].dma_transfer_len = 0;
		}
	}
#endif
	memset(&sunxi_udc_dma_para, 0, sizeof(sunxi_udc_dma_parg_t));
	return 0;
}

#endif /* SW_UDC_DMA_INNER */
