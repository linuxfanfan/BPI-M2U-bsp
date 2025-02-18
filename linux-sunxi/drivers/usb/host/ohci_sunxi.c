/*
 * drivers/usb/host/ohci_sunxi.c
 * (C) Copyright 2010-2015
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * yangnaitian, 2011-5-24, create this file
 * javen, 2011-6-26, add suspend and resume
 * javen, 2011-7-18, move clock and power operations out from driver
 *
 * OHCI Driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/platform_device.h>
#include <linux/signal.h>

#include <linux/time.h>
#include <linux/timer.h>

#include <linux/clk.h>

#ifdef CONFIG_PM
#include <linux/arisc/arisc.h>
#include <linux/power/aw_pm.h>
#include <linux/power/scenelock.h>
static struct scene_lock  ohci_standby_lock[4];
static void sunxi_ohci_resume_work(struct work_struct *work);
#endif
#include "sunxi_hci.h"

#define   SUNXI_OHCI_NAME	"sunxi-ohci"
static const char ohci_name[] = SUNXI_OHCI_NAME;

#if defined CONFIG_USB_SUNXI_OHCI0 && !defined SUNXI_USB_FPGA
#define  SUNXI_OHCI0_OF_MATCH	"allwinner,sunxi-ohci0"
#else
#define  SUNXI_OHCI0_OF_MATCH	"null"
#endif

#if defined CONFIG_USB_SUNXI_OHCI1 && !defined SUNXI_USB_FPGA
#define  SUNXI_OHCI1_OF_MATCH	"allwinner,sunxi-ohci1"
#else
#define  SUNXI_OHCI1_OF_MATCH	"null"
#endif

#if defined CONFIG_USB_SUNXI_OHCI2 && !defined SUNXI_USB_FPGA
#define  SUNXI_OHCI2_OF_MATCH	"allwinner,sunxi-ohci2"
#else
#define  SUNXI_OHCI2_OF_MATCH	"null"
#endif

#if defined CONFIG_USB_SUNXI_OHCI3 && !defined SUNXI_USB_FPGA
#define  SUNXI_OHCI3_OF_MATCH	"allwinner,sunxi-ohci3"
#else
#define  SUNXI_OHCI3_OF_MATCH	"null"
#endif

static struct sunxi_hci_hcd *g_sunxi_ohci[4];
static u32 ohci_first_probe[4] = {1, 1, 1, 1};
static u32 ohci_enable[4] = {1, 1, 1, 1};

int sunxi_usb_disable_ohci(__u32 usbc_no);
int sunxi_usb_enable_ohci(__u32 usbc_no);

static ssize_t ohci_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sunxi_hci_hcd *sunxi_ohci = NULL;

	if(dev == NULL){
		DMSG_PANIC("ERR: Argment is invalid\n");
		return 0;
	}

	sunxi_ohci = dev->platform_data;
	if(sunxi_ohci == NULL){
		DMSG_PANIC("ERR: sw_ohci is null\n");
		return 0;
	}

	return sprintf(buf, "ohci:%d,probe:%u\n", sunxi_ohci->usbc_no, sunxi_ohci->probe);
}

static ssize_t ohci_enable_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sunxi_hci_hcd *sunxi_ohci = NULL;
	int value = 0;

	if(dev == NULL){
		DMSG_PANIC("ERR: Argment is invalid\n");
		return 0;
	}

	sunxi_ohci = dev->platform_data;
	if(sunxi_ohci == NULL){
		DMSG_PANIC("ERR: sw_ohci is null\n");
		return 0;
	}

	sunxi_ohci->host_init_state = 0;
	ohci_first_probe[sunxi_ohci->usbc_no] = 0;

	sscanf(buf, "%d", &value);
	if(value == 1){
		ohci_enable[sunxi_ohci->usbc_no] = 0;
		sunxi_usb_enable_ohci(sunxi_ohci->usbc_no);

		if(sunxi_ohci->usbc_no == HCI0_USBC_NO){
			sunxi_set_host_vbus(sunxi_ohci, 1);
		}

	}else if(value == 0){
		ohci_enable[sunxi_ohci->usbc_no] = 1;
		sunxi_usb_disable_ohci(sunxi_ohci->usbc_no);
		ohci_enable[sunxi_ohci->usbc_no] = 0;
	}else{
		DMSG_INFO("unkown value (%d)\n", value);
	}

	return count;
}

static DEVICE_ATTR(ohci_enable, 0644, ohci_enable_show, ohci_enable_store);


static int open_ohci_clock(struct sunxi_hci_hcd *sunxi_ohci)
{
	return sunxi_ohci->open_clock(sunxi_ohci, 1);
}

static int close_ohci_clock(struct sunxi_hci_hcd *sunxi_ohci)
{
	return sunxi_ohci->close_clock(sunxi_ohci, 1);
}

static void sunxi_ohci_set_vbus(struct sunxi_hci_hcd *sunxi_xhci, int is_on)
{
	sunxi_xhci->set_power(sunxi_xhci, is_on);
}

static void sunxi_ohci_set_passby(struct sunxi_hci_hcd *sunxi_xhci, int is_on)
{
	sunxi_xhci->usb_passby(sunxi_xhci, is_on);
}

static void sunxi_start_ohci(struct sunxi_hci_hcd *sunxi_ohci)
{
	open_ohci_clock(sunxi_ohci);
	sunxi_ohci_set_passby(sunxi_ohci, 1);
	sunxi_ohci_set_vbus(sunxi_ohci, 1);

	return;
}

static void sunxi_stop_ohci(struct sunxi_hci_hcd *sunxi_ohci)
{
	sunxi_ohci_set_vbus(sunxi_ohci, 0);
	sunxi_ohci_set_passby(sunxi_ohci, 0);
	close_ohci_clock(sunxi_ohci);

	return;
}

static int sunxi_ohci_start(struct usb_hcd *hcd)
{
	struct ohci_hcd	*ohci = hcd_to_ohci(hcd);
	int ret;

	if ((ret = ohci_init(ohci)) < 0)
		return ret;

	if ((ret = ohci_run(ohci)) < 0) {
		DMSG_PANIC("can't start %s", hcd->self.bus_name);
		ohci_stop(hcd);
		return ret;
	}

	return 0;
}

static const struct hc_driver sunxi_ohci_hc_driver ={
	.description	= hcd_name,
	.product_desc	= "SW USB2.0 'Open' Host Controller (OHCI) Driver",
	.hcd_priv_size	= sizeof(struct ohci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq			= ohci_irq,
	.flags			= HCD_USB11 | HCD_MEMORY,

	/*
	 * basic lifecycle operations
	 */
	.start			= sunxi_ohci_start,
	.stop			= ohci_stop,
	.shutdown		= ohci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue		= ohci_urb_enqueue,
	.urb_dequeue		= ohci_urb_dequeue,
	.endpoint_disable	= ohci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number	= ohci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data	= ohci_hub_status_data,
	.hub_control		= ohci_hub_control,

#ifdef	CONFIG_PM
	.bus_suspend		= ohci_bus_suspend,
	.bus_resume		= ohci_bus_resume,
#endif
	.start_port_reset	= ohci_start_port_reset,
};

static int sunxi_insmod_ohci(struct platform_device *pdev)
{
	int ret;
	struct usb_hcd *hcd = NULL;
	struct sunxi_hci_hcd *sunxi_ohci = NULL;

	if(pdev == NULL){
		DMSG_PANIC("ERR: Argment is invaild\n");
		return -1;
	}

	/* if usb is disabled, can not probe */
	if (usb_disabled()){
		DMSG_PANIC("ERR: usb hcd is disabled\n");
		return -ENODEV;
	}

	sunxi_ohci = pdev->dev.platform_data;
	if(!sunxi_ohci){
		DMSG_PANIC("ERR: sunxi_ohci is null\n");
		ret = -ENOMEM;
		goto ERR1;
	}

	sunxi_ohci->pdev = pdev;
	g_sunxi_ohci[sunxi_ohci->usbc_no] = sunxi_ohci;

	DMSG_INFO("[%s%d]: probe, pdev->name: %s, sunxi_ohci: 0x%p\n",
		ohci_name, sunxi_ohci->usbc_no, pdev->name, sunxi_ohci);

	sunxi_ohci->ohci_base = sunxi_ohci->usb_vbase + SUNXI_USB_OHCI_BASE_OFFSET;

	sunxi_ohci->ohci_reg_length = SUNXI_USB_OHCI_LEN;

	/*creat a usb_hcd for the ohci controller*/
	hcd = usb_create_hcd(&sunxi_ohci_hc_driver, &pdev->dev, ohci_name);
	if(!hcd){
		DMSG_PANIC("ERR: usb_ohci_create_hcd failed\n");
		ret = -ENOMEM;
		goto ERR2;
	}

	hcd->rsrc_start = sunxi_ohci->usb_base_res->start;
	hcd->rsrc_len 	= SUNXI_USB_OHCI_LEN;
	hcd->regs 	= sunxi_ohci->ohci_base;
	sunxi_ohci->hcd	= hcd;

	/* ochi start to work */
	sunxi_start_ohci(sunxi_ohci);

	ohci_hcd_init(hcd_to_ohci(hcd));

	ret = usb_add_hcd(hcd, sunxi_ohci->irq_no, IRQF_DISABLED | IRQF_SHARED);
	if(ret != 0){
		DMSG_PANIC("ERR: usb_add_hcd failed\n");
		ret = -ENOMEM;
		goto ERR3;
	}

	platform_set_drvdata(pdev, hcd);

#ifndef USB_SYNC_SUSPEND
	device_enable_async_suspend(&pdev->dev);
#endif

	sunxi_ohci->probe = 1;

#ifdef	CONFIG_PM
	if (sunxi_ohci->wakeup_suspend)
		scene_lock_init(&ohci_standby_lock[sunxi_ohci->usbc_no], SCENE_USB_STANDBY,  "ohci_standby");
	else
		INIT_WORK(&sunxi_ohci->resume_work, sunxi_ohci_resume_work);
#endif
	/* Disable ohci, when driver probe */
	if(sunxi_ohci->host_init_state == 0){
		if(ohci_first_probe[sunxi_ohci->usbc_no]){
			sunxi_usb_disable_ohci(sunxi_ohci->usbc_no);
			ohci_first_probe[sunxi_ohci->usbc_no] = 0;
		}
	}

	return 0;

ERR3:
	usb_put_hcd(hcd);
	sunxi_stop_ohci(sunxi_ohci);

ERR2:
	sunxi_ohci->hcd = NULL;

ERR1:
	return ret;
}

static int sunxi_rmmod_ohci(struct platform_device *pdev)
{
	struct usb_hcd *hcd = NULL;
	struct sunxi_hci_hcd *sunxi_ohci = NULL;

	if(pdev == NULL){
		DMSG_PANIC("ERR: Argment is invalid\n");
		return -1;
	}

	hcd = platform_get_drvdata(pdev);
	if(hcd == NULL){
		DMSG_PANIC("ERR: hcd is null\n");
		return -1;
	}

	sunxi_ohci = pdev->dev.platform_data;
	if(sunxi_ohci == NULL){
		DMSG_PANIC("ERR: sunxi_ohci is null\n");
		return -1;
	}

	DMSG_INFO("[%s%d]: remove, pdev->name: %s, sunxi_ohci: 0x%p\n",
		ohci_name, sunxi_ohci->usbc_no, pdev->name, sunxi_ohci);
#ifdef	CONFIG_PM
	if(sunxi_ohci->wakeup_suspend){
		scene_lock_destroy(&ohci_standby_lock[sunxi_ohci->usbc_no]);
	}
#endif

	usb_remove_hcd(hcd);

	sunxi_stop_ohci(sunxi_ohci);
	sunxi_ohci->probe = 0;

	usb_put_hcd(hcd);

	sunxi_ohci->hcd = NULL;

	return 0;
}


static int sunxi_ohci_hcd_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct sunxi_hci_hcd *sunxi_ohci = NULL;

	if(pdev == NULL){
		DMSG_PANIC("ERR: %s, Argment is invalid\n", __func__);
		return -1;
	}

	/* if usb is disabled, can not probe */
	if (usb_disabled()) {
		DMSG_PANIC("ERR: usb hcd is disabled\n");
		return -ENODEV;
	}

	ret = init_sunxi_hci(pdev, SUNXI_USB_OHCI);
	if(ret != 0){
		dev_err(&pdev->dev, "init_sunxi_hci is fail\n");
		return -1;
	}

	sunxi_insmod_ohci(pdev);

	sunxi_ohci = pdev->dev.platform_data;
	if(sunxi_ohci == NULL){
		DMSG_PANIC("ERR: %s, sunxi_ohci is null\n", __func__);
		return -1;
	}
	if(ohci_enable[sunxi_ohci->usbc_no]){
		device_create_file(&pdev->dev, &dev_attr_ohci_enable);
		ohci_enable[sunxi_ohci->usbc_no] = 0;
	}

	return 0;
}

static int sunxi_ohci_hcd_remove(struct platform_device *pdev)
{
	struct sunxi_hci_hcd *sunxi_ohci = NULL;

	if(pdev == NULL){
		DMSG_PANIC("ERR: %s, Argment is invalid\n", __func__);
		return -1;
	}

	sunxi_ohci = pdev->dev.platform_data;
	if(sunxi_ohci == NULL){
		DMSG_PANIC("ERR: %s, sunxi_ohci is null\n", __func__);
		return -1;
	}

	if(ohci_enable[sunxi_ohci->usbc_no] == 0){
		device_remove_file(&pdev->dev, &dev_attr_ohci_enable);
	}

	return sunxi_rmmod_ohci(pdev);
}

static void sunxi_ohci_hcd_shutdown(struct platform_device* pdev)
{
	struct sunxi_hci_hcd *sunxi_ohci = NULL;

	sunxi_ohci = pdev->dev.platform_data;
	if(sunxi_ohci == NULL){
		DMSG_PANIC("ERR: %s sunxi_ohci is null\n", __func__);
		return ;
	}

	if(sunxi_ohci->probe == 0){
		DMSG_INFO("%s, %s is disable, need not shutdown\n",  __func__, sunxi_ohci->hci_name);
		return ;
	}

	pr_debug("[%s]: ohci shutdown start\n", sunxi_ohci->hci_name);

#ifdef	CONFIG_PM
	if(sunxi_ohci->wakeup_suspend){
		scene_lock_destroy(&ohci_standby_lock[sunxi_ohci->usbc_no]);
	}
#endif
	sunxi_ohci_set_vbus(sunxi_ohci, 0);

	usb_hcd_platform_shutdown(pdev);

	/* disable usb otg INTUSBE, To solve usb0 device mode catch audio udev on reboot system is fail*/
	if (sunxi_ohci->usbc_no == 0)
		if (sunxi_ohci->otg_vbase) {
			writel(0, (sunxi_ohci->otg_vbase
						+ SUNXI_USBC_REG_INTUSBE));
		}

	pr_debug("[%s]: ohci shutdown end\n", sunxi_ohci->hci_name);

	return;
}

#ifdef CONFIG_PM

static int sunxi_ohci_hcd_suspend(struct device *dev)
{
	struct sunxi_hci_hcd *sunxi_ohci  = NULL;
	struct usb_hcd *hcd	= NULL;
	struct ohci_hcd	*ohci	= NULL;
	int val = 0;

	if(dev == NULL){
		DMSG_PANIC("ERR: Argment is invalid\n");
		return 0;
	}

	hcd = dev_get_drvdata(dev);
	if(hcd == NULL){
		DMSG_PANIC("ERR: hcd is null\n");
		return 0;
	}

	sunxi_ohci = dev->platform_data;
	if(sunxi_ohci == NULL){
		DMSG_PANIC("ERR: sunxi_ohci is null\n");
		return 0;
	}

	if(sunxi_ohci->probe == 0){
		DMSG_INFO("[%s]: is disable, can not suspend\n",
			sunxi_ohci->hci_name);
		return 0;
	}

	ohci = hcd_to_ohci(hcd);
	if(ohci == NULL){
		DMSG_PANIC("ERR: ohci is null\n");
		return 0;
	}

	if(sunxi_ohci->wakeup_suspend){
		DMSG_INFO("[%s]: not suspend\n", sunxi_ohci->hci_name);
		scene_lock(&ohci_standby_lock[sunxi_ohci->usbc_no]);
		enable_wakeup_src(CPUS_USBMOUSE_SRC, 0);

		val = ohci_readl(ohci, &ohci->regs->control);
		val |= OHCI_CTRL_RWE;
		ohci_writel(ohci, val,  &ohci->regs->control);

		val = ohci_readl(ohci, &ohci->regs->intrenable);
		val |= OHCI_INTR_RD;
		val |= OHCI_INTR_MIE;
		ohci_writel(ohci, val, &ohci->regs->intrenable);

		if(sunxi_ohci->clk_usbohci12m && sunxi_ohci->clk_losc){
			clk_set_parent(sunxi_ohci->clk_usbohci12m, sunxi_ohci->clk_losc);
		}

	}else{
		DMSG_INFO("[%s]: sunxi_ohci_hcd_suspend\n", sunxi_ohci->hci_name);

		/* Root hub was already suspended. Disable irq emission and
		 * mark HW unaccessible, bail out if RH has been resumed. Use
		 * the spinlock to properly synchronize with possible pending
		 * RH suspend or resume activity.
		 *
		 * This is still racy as hcd->state is manipulated outside of
		 * any locks =P But that will be a different fix.
		 */
		ohci_suspend(hcd, device_may_wakeup(dev));
		sunxi_stop_ohci(sunxi_ohci);

		cancel_work_sync(&sunxi_ohci->resume_work);
	}

	return 0;
}

static void sunxi_ohci_resume_work(struct work_struct *work)
{
	struct sunxi_hci_hcd *sunxi_ohci = NULL;

	sunxi_ohci = container_of(work, struct sunxi_hci_hcd, resume_work);

	/* Waiting hci to resume. */
	msleep(5000);

	sunxi_ohci_set_vbus(sunxi_ohci, 1);
}

static int sunxi_ohci_hcd_resume(struct device *dev)
{
	struct sunxi_hci_hcd *sunxi_ohci = NULL;
	struct usb_hcd *hcd = NULL;

	if(dev == NULL){
		DMSG_PANIC("ERR: Argment is invalid\n");
		return 0;
	}

	hcd = dev_get_drvdata(dev);
	if(hcd == NULL){
		DMSG_PANIC("ERR: hcd is null\n");
		return 0;
	}

	sunxi_ohci = dev->platform_data;
	if(sunxi_ohci == NULL){
		DMSG_PANIC("ERR: sunxi_ohci is null\n");
		return 0;
	}

	if(sunxi_ohci->probe == 0){
		DMSG_INFO("[%s]: is disable, can not resume\n",
			sunxi_ohci->hci_name);
		return 0;
	}

	if(sunxi_ohci->wakeup_suspend){
		DMSG_INFO("[%s]: controller not suspend, need not resume\n", sunxi_ohci->hci_name);

		if(sunxi_ohci->clk_usbohci12m && sunxi_ohci->clk_hoscx2){
			clk_set_parent(sunxi_ohci->clk_usbohci12m, sunxi_ohci->clk_hoscx2);
		}

		scene_unlock(&ohci_standby_lock[sunxi_ohci->usbc_no]);
		disable_wakeup_src(CPUS_USBMOUSE_SRC, 0);

	}else{
		DMSG_INFO("[%s]: sunxi_ohci_hcd_resume\n", sunxi_ohci->hci_name);

		open_ohci_clock(sunxi_ohci);
		sunxi_ohci_set_passby(sunxi_ohci, 1);
		set_bit(HCD_FLAG_HW_ACCESSIBLE, &hcd->flags);
		ohci_resume(hcd, false);

		schedule_work(&sunxi_ohci->resume_work);
	}

	return 0;
}

static const struct dev_pm_ops sunxi_ohci_pmops = {
	.suspend	= sunxi_ohci_hcd_suspend,
	.resume		= sunxi_ohci_hcd_resume,
};

#define SUNXI_OHCI_PMOPS  &sunxi_ohci_pmops

#else

#define SUNXI_OHCI_PMOPS NULL

#endif

static const struct of_device_id sunxi_ohci_match[] = {
	{.compatible = SUNXI_OHCI0_OF_MATCH, },
	{.compatible = SUNXI_OHCI1_OF_MATCH, },
	{.compatible = SUNXI_OHCI2_OF_MATCH, },
	{.compatible = SUNXI_OHCI3_OF_MATCH, },
	{},
};
MODULE_DEVICE_TABLE(of, sunxi_ohci_match);


static struct platform_driver sunxi_ohci_hcd_driver = {
	.probe		= sunxi_ohci_hcd_probe,
	.remove		= sunxi_ohci_hcd_remove,
	.shutdown	= sunxi_ohci_hcd_shutdown,
	.driver		= {
		.name	= ohci_name,
		.owner	= THIS_MODULE,
		.pm	= SUNXI_OHCI_PMOPS,
		.of_match_table = sunxi_ohci_match,
	},
};

int sunxi_usb_disable_ohci(__u32 usbc_no)
{
	struct sunxi_hci_hcd *sunxi_ohci = NULL;

	sunxi_ohci = g_sunxi_ohci[usbc_no];
	if(sunxi_ohci == NULL){
		DMSG_PANIC("ERR: sunxi_ohci is null\n");
		return -1;
	}

	if(sunxi_ohci->probe == 0){
		DMSG_PANIC("ERR: sunxi_ohci is disable, can not disable again\n");
		return -1;
	}

	sunxi_ohci->probe = 0;

	DMSG_INFO("[%s]: sunxi_usb_disable_ohci\n", sunxi_ohci->hci_name);

	sunxi_rmmod_ohci(sunxi_ohci->pdev);

	return 0;
}
EXPORT_SYMBOL(sunxi_usb_disable_ohci);

int sunxi_usb_enable_ohci(__u32 usbc_no)
{
	struct sunxi_hci_hcd *sunxi_ohci = NULL;

	sunxi_ohci = g_sunxi_ohci[usbc_no];
	if(sunxi_ohci == NULL){
		DMSG_PANIC("ERR: sunxi_ohci is null\n");
		return -1;
	}

	if(sunxi_ohci->probe == 1){
		DMSG_PANIC("ERR: sunxi_ohci is already enable, can not enable again\n");
		return -1;
	}

	sunxi_ohci->probe = 1;

	DMSG_INFO("[%s]: sunxi_usb_enable_ohci\n", sunxi_ohci->hci_name);

	sunxi_insmod_ohci(sunxi_ohci->pdev);

	return 0;
}
EXPORT_SYMBOL(sunxi_usb_enable_ohci);

