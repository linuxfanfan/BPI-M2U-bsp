/*
 * Allwinner SoCs hdmi2.0 driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
 #include "config.h"
#if defined(__LINUX_PLAT__)
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of_irq.h>
#include <linux/clk-private.h>
#include <linux/clkdev.h>
#include <linux/switch.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#endif
#include "hdmi_tx.h"
/*#include "hdmi_test.h"*/

#define DDC_PIN_ACTIVE "ddc_active"
#define CEC_PIN_ACTIVE "cec_active"
#define DDC_PIN_SLEEP "sleep"

#define ESM_REG_BASE_OFFSET 0x8000
#define HDCP22_FIRMWARE_SIZE	(1024 * 256)
#define HDCP22_DATA_SIZE	(1024 * 128)

#if defined(__LINUX_PLAT__)
static dev_t devid;
static struct cdev *hdmi_cdev;

static struct class *hdmi_class;
static struct device *hdev;
#endif

static struct hdmi_tx_drv	*hdmi_drv;

/*0x10: force unplug;
* 0x11: force plug;
* 0x1xx: unreport hpd state
* 0x1xxx: mask hpd
*/
u32 hdmi_hpd_mask;

u32 hdmi_clk_enable_mask;
static u32 hdmi_pin_config_mask;
static u32 hdmi_enable_mask;
static u32 hdmi_suspend_mask;

static bool boot_hdmi;
static bool video_on;
static u32 hpd_state;
static u8 hdcp_encrypt_status;

u32 hdmi_printf;

#if defined(CONFIG_SWITCH) || defined(CONFIG_ANDROID_SWITCH)
static struct switch_dev hdmi_switch_dev = {
	.name = "hdmi",
};
#endif
#if defined(__LINUX_PLAT__)

static int hdmi_run_thread(void *parg);
#endif
struct hdmi_tx_drv *get_hdmi_drv(void)
{
	return hdmi_drv;
}

u32 get_drv_hpd_state(void)
{
	return hpd_state;
}

/**
 * @short List of the devices
 * Linked list that contains the installed devices
 */
static LIST_HEAD(devlist_global);

#ifdef CONFIG_AW_AXP
int hdmi_power_enable(char *name)
{
	struct regulator *regu = NULL;
	int ret = -1;

	regu = regulator_get(NULL, name);
	if (IS_ERR(regu)) {
		pr_err("%s: some error happen, fail to get regulator %s\n", __func__, name);
		goto exit;
	}

	/* enalbe regulator */
	ret = regulator_enable(regu);
	if (0 != ret) {
		pr_err("%s: some error happen, fail to enable regulator %s!\n", __func__, name);
		goto exit1;
	} else {
		HDMI_INFO_MSG("suceess to enable regulator %s!\n", name);
	}

exit1:
	/* put regulater, when module exit */
	regulator_put(regu);
exit:
	return ret;
}

int hdmi_power_disable(char *name)
{
	struct regulator *regu = NULL;
	int ret = 0;
	regu = regulator_get(NULL, name);
	if (IS_ERR(regu)) {
		pr_err("%s: some error happen, fail to get regulator %s\n", __func__, name);
		goto exit;
	}

	/*disalbe regulator*/
	ret = regulator_disable(regu);
	if (0 != ret) {
		pr_err("%s: some error happen, fail to disable regulator %s!\n", __func__, name);
		goto exit1;
	} else {
		HDMI_INFO_MSG("suceess to disable regulator %s!\n", name);
	}

exit1:
	/*put regulater, when module exit*/
	regulator_put(regu);
exit:
	return ret;
}
#else
int hdmi_power_enable(char *name) {return 0; }
int hdmi_power_disable(char *name) {return 0; }
#endif


#if defined(__LINUX_PLAT__)

static void hdmi_clk_enable(void)
{
	if (hdmi_clk_enable_mask)
		return;

	clk_set_rate(hdmi_drv->hdmi_hdcp_clk, 300000000);

	if (hdmi_drv->hdmi_clk != NULL)
		if (clk_prepare_enable(hdmi_drv->hdmi_clk) != 0)
			pr_info("hdmi clk enable failed!\n");

	if (hdmi_drv->hdmi_ddc_clk != NULL)
		if (clk_prepare_enable(hdmi_drv->hdmi_ddc_clk) != 0)
			pr_info("hdmi ddc clk enable failed!\n");

	if (hdmi_drv->hdmi_hdcp_clk != NULL)
		if (clk_prepare_enable(hdmi_drv->hdmi_hdcp_clk) != 0)
			pr_info("hdmi ddc clk enable failed!\n");

	if (hdmi_drv->hdmi_cec_clk != NULL) {
		struct clk *cec_clk_parent = NULL;
		cec_clk_parent = clk_get(NULL, "periph32k");
		if (cec_clk_parent == NULL || IS_ERR(cec_clk_parent)) {
			pr_err("periph32k clk get failed\n");
		} else {
			if (clk_set_parent(hdmi_drv->hdmi_cec_clk, cec_clk_parent) != 0)
				pr_err("hdmi ddc clk set parent clk 'periph32k' failed\n");
		}
		if (clk_prepare_enable(hdmi_drv->hdmi_cec_clk) != 0)
			pr_info("hdmi cec clk enable failed!\n");
	}


	hdmi_clk_enable_mask = 1;
	resistor_calibration_core(hdmi_drv->hdmi_core, 0x10004, 0x80c00000);

}

static void hdmi_clk_disable(void)
{
	if (!hdmi_clk_enable_mask)
		return;

	hdmi_clk_enable_mask = 0;

	if (hdmi_drv->hdmi_hdcp_clk != NULL)
		clk_disable_unprepare(hdmi_drv->hdmi_hdcp_clk);
	if (hdmi_drv->hdmi_ddc_clk != NULL)
		clk_disable_unprepare(hdmi_drv->hdmi_ddc_clk);
	if (hdmi_drv->hdmi_clk != NULL)
		clk_disable_unprepare(hdmi_drv->hdmi_clk);
	if (hdmi_drv->hdmi_cec_clk != NULL)
		clk_disable_unprepare(hdmi_drv->hdmi_cec_clk);
}

static void hdmi_pin_configure(void)
{
	s32 ret = 0;
	struct pinctrl_state *state, *cec_state;

	if (hdmi_pin_config_mask)
		return;

	/*pin configuration for ddc*/
	if (hdmi_drv->pctl != NULL) {
		state = pinctrl_lookup_state(hdmi_drv->pctl, DDC_PIN_ACTIVE);
		if (IS_ERR(state)) {
			pr_info("pinctrl_lookup_state for HDMI2.0 SCL fail\n");
			return;
		}
		ret = pinctrl_select_state(hdmi_drv->pctl, state);
		if (ret < 0)
			pr_info("pinctrl_select_state for HDMI2.0 DDC fail\n");

		cec_state = pinctrl_lookup_state(hdmi_drv->pctl, CEC_PIN_ACTIVE);
		if (IS_ERR(state))
			pr_info("pinctrl_lookup_state for HDMI2.0 CEC active fail\n");
		ret = pinctrl_select_state(hdmi_drv->pctl, cec_state);
		if (ret < 0)
			pr_info("pinctrl_select_state for HDMI2.0 CEC active fail\n");
	}

	hdmi_pin_config_mask = 1;
}

static void hdmi_pin_release(void)
{
	s32 ret = 0;
	struct pinctrl_state *state;

	if (!hdmi_pin_config_mask)
		return;

	/*pin configuration for ddc*/
	if (hdmi_drv->pctl != NULL) {
		state = pinctrl_lookup_state(hdmi_drv->pctl, DDC_PIN_SLEEP);
		if (IS_ERR(state))
			pr_info("pinctrl_lookup_state for HDMI2.0 SCL fail\n");
		ret = pinctrl_select_state(hdmi_drv->pctl, state);
		if (ret < 0)
			pr_info("pinctrl_select_state for HDMI2.0 DDC fail\n");
	}

	hdmi_pin_config_mask = 0;
}

static void hdmi_sys_source_configure(void)
{
	LOG_TRACE();
	hdmi_clk_enable();
	hdmi_pin_configure();
}

static void hdmi_sys_source_release(void)
{
	LOG_TRACE();
	hdmi_clk_disable();
	hdmi_pin_release();
}

static void hdmi_clk_reset(void)
{
	hdmi_clk_disable();
	udelay(10);
	hdmi_clk_enable();

	hdmi_clk_disable();
	udelay(10);
	hdmi_clk_enable();
}

/*static void hdmi_sys_source_reset(void)
{
	hdmi_sys_source_release();
	mdelay(10);
	hdmi_sys_source_configure();
}*/
#endif

static void set_hdcp_status(u8 status)
{
	hdcp_encrypt_status = status;
}

static s32 hdmi_enable(void)
{
	s32 ret = 0;

	LOG_TRACE();

	mutex_lock(&hdmi_drv->ctrl_mutex);
	if (hdmi_enable_mask == 1) {
		mutex_unlock(&hdmi_drv->ctrl_mutex);
		return 0;
	}
	if (/*hpd_state &&*/ !video_on)
		ret = hdmi_enable_core();
#if defined(__LINUX_PLAT__)
	hdmi_cec_enable(1);
	hdmi_cec_wakup_request();
#endif
	hdmi_enable_mask = 1;

	mutex_unlock(&hdmi_drv->ctrl_mutex);
	return ret;
}

static s32 hdmi_disable(void)
{
	s32 ret;

	LOG_TRACE();

	if (hdmi_enable_mask == 0)
		return 0;
#if defined(__LINUX_PLAT__)
	hdmi_cec_enable(0);
	ret = hdmi_disable_core();
#endif
	video_on = 0;
	hdmi_enable_mask = 0;

	return ret;
}
#if defined(CONFIG_SND_SUNXI_SOC_HDMIAUDIO)
s32 hdmi_audio_enable(u8 enable, u8 channel)
{
	s32 ret = 0;

	ret = hdmi_core_audio_enable(enable, channel);

	return ret;
}
#endif

#if defined(__LINUX_PLAT__)
static s32 hdmi_suspend(void)
{
	LOG_TRACE();

	if (hdmi_suspend_mask)
		return 0;

	cec_thread_exit();
	if (hdmi_drv->hdmi_core->mode.pHdcp.hdcp_on)
		set_hdcp_status(HDCP_ING);
	if (hdmi_drv->hdmi_core->mode.pHdcp.use_hdcp)
		hdmi_drv->hdmi_core->dev_func.hdcp_close();

	if (hdmi_drv->hdmi_task) {
		kthread_stop(hdmi_drv->hdmi_task);
		hdmi_drv->hdmi_task = NULL;
	}
	hdmi_sys_source_release();

	hdmi_suspend_mask = 1;
	return 0;
}

static s32 hdmi_resume(void)
{
	s32 ret = 0;
	/*u32 hpd_state = 0;*/

	LOG_TRACE();
	if (hdmi_suspend_mask == 0)
		return 0;

	hdmi_sys_source_configure();

	/*enable hpd sense*/
	hpd_sense_enbale_core(hdmi_drv->hdmi_core);
	/*
	hpd_state = hdmi_core_get_hpd_state();
	if (hpd_state)
		edid_read_cap();
		*/
	hpd_state = 0;
/*#if defined(CONFIG_SWITCH) || defined(CONFIG_ANDROID_SWITCH)
	switch_set_state(&hdmi_switch_dev, hpd_state);
#endif
*/
	hdmi_drv->hdmi_task = kthread_create(hdmi_run_thread, (void *)0, "hdmi proc");
	if (IS_ERR(hdmi_drv->hdmi_task)) {
		pr_info("Unable to start kernel thread %s.\n\n", "hdmi proc");
		hdmi_drv->hdmi_task = NULL;
	}
	wake_up_process(hdmi_drv->hdmi_task);
	cec_thread_init(hdmi_drv->parent_dev);
	hdmi_suspend_mask = 0;

	return ret;
}

/*static void hdcp_handler(struct work_struct *work)
{
	LOG_TRACE();

	hdcp_handler_core();

}*/

static void edid_check(void)
{
	if (!hdmi_drv->hdmi_core->mode.edid_done) {
		msleep(3000);
		edid_read_cap();
#if defined(CONFIG_SWITCH) || defined(CONFIG_ANDROID_SWITCH)
		if (!(hdmi_hpd_mask & 0x100))
			switch_set_state(&hdmi_switch_dev, 1);
#endif
	}
}


static void hdmi_configure(struct hdmi_tx_drv *drv)
{
	hdmi_configure_core(drv->hdmi_core);
}

/*sys_config.fex setting for hpd*/
static void hdmi_hpd_sys_config_set(void)
{
	/*To enhance the ddc ability*/
	if (hdmi_drv->ddc_ctrl_en == 1) {
		if (0 != gpio_direction_output(hdmi_drv->ddc_ctrl.gpio, 1)) {
			pr_info("ddc ctrl gpio set 1 error!\n");
			return;
		}
	}
}

/*release sys_config.fex setting for hpd*/
static void hdmi_hpd_sys_config_release(void)
{
	if (hdmi_drv->ddc_ctrl_en == 1) {
		if (0 != gpio_direction_output(hdmi_drv->ddc_ctrl.gpio, 0)) {
			pr_info("ddc ctrl gpio set 0 error!\n");
			return;
		}
	}
}

static void hdmi_plugin_proc(void)
{
	LOG_TRACE();
	pr_info("HDMI cable is connected\n");
	if (!(hdmi_hpd_mask & 0x10))
		hpd_state = 1;

	hdmi_hpd_sys_config_set();
	edid_read_cap();
	if (hdmi_drv->hdmi_core->mode.edid_done)
		edid_set_video_prefered_core();/*update some configs by edid*/
	mutex_lock(&hdmi_drv->ctrl_mutex);
	if ((!video_on) && (hdmi_enable_mask)) {
		hdmi_configure(hdmi_drv);
		video_on = true;
	}
	hdmi_cec_enable(1);
	hdmi_cec_wakup_request();
	mutex_unlock(&hdmi_drv->ctrl_mutex);
#if defined(CONFIG_SWITCH) || defined(CONFIG_ANDROID_SWITCH)
	if ((!(hdmi_hpd_mask & 0x100)) && hdmi_drv->hdmi_core->mode.edid_done)
		switch_set_state(&hdmi_switch_dev, 1);
#endif

	edid_check();
	edid_correct_hardware_config();
}

static void hdmi_plugout_proc(void)
{
	LOG_TRACE();
	pr_info("HDMI cable is disconnected\n");
	if (!(hdmi_hpd_mask & 0x10))
		hpd_state = 0;
	video_on = false;

	/*hdcp_enable_core(hdmi_drv->hdmi_core, 0);*/
	hdmi_hpd_sys_config_release();
	hdmi_cec_enable(0);
	if (hdmi_drv->hdmi_core->mode.pHdcp.hdcp_on)
		set_hdcp_status(HDCP_ING);
	hdmi_hpd_out_core_process(hdmi_drv->hdmi_core);
	hdmi_clk_reset();
	hpd_sense_enbale_core(hdmi_drv->hdmi_core);
#if defined(CONFIG_SWITCH) || defined(CONFIG_ANDROID_SWITCH)
	if (!(hdmi_hpd_mask & 0x100))
		switch_set_state(&hdmi_switch_dev, 0);
#endif
}

static void hdmi_hpd_mask_process(void)
{
	if (hdmi_hpd_mask == 0x10) {
		hdmi_plugout_proc();
	} else if (hdmi_hpd_mask == 0x11) {
		msleep(400);
		hdmi_plugin_proc();
	} else {
		pr_info("Unknow hpd event\n");
	}

	hdmi_hpd_mask &= 0x0f0f;
}

static int hdmi_run_thread(void *parg)
{
	u32 hpd_state_now = 0;
	u32 i;

	while (1) {
		if (kthread_should_stop())
			break;

		if (hdmi_hpd_mask & 0x1000) {
			msleep(200);
			continue;
		}

		if (hdmi_hpd_mask & 0x10) {
			hdmi_hpd_mask_process();
			continue;
		}
		hpd_state_now = hdmi_core_get_hpd_state();
		if (hpd_state_now != hpd_state) {
			/*HPD Event Happen*/
			if (!hpd_state_now) {
				hdmi_plugout_proc();
			} else {
				for (i = 0; i < 2; i++) {
					msleep(200);
					hpd_state_now = hdmi_core_get_hpd_state();
					if (hpd_state_now == hpd_state)
						break;/*it's not a real hpd event*/
				}

				if (i >= 2) {
					/*it's a real hpd event*/
					hdmi_plugin_proc();
				}
			}
		} else {
			if (hpd_state_now && (hpd_state || (hdmi_hpd_mask == 0x1)) &&
					hdmi_drv->hdmi_core->mode.pHdcp.hdcp_on &&
					hdmi_drv->hdmi_core->mode.pHdcp.use_hdcp) {
				mutex_lock(&hdmi_drv->hdcp_mutex);
				if (get_hdcp_status_core() == 0)
					set_hdcp_status(HDCP_SUCCESS);
				else if (get_hdcp_status_core() == -1)
					set_hdcp_status(HDCP_FAILED);
				else if (get_hdcp_status_core() == 1)
					set_hdcp_status(HDCP_ING);
				else
					pr_info("Error: Unkown hdcp status\n");
				mutex_unlock(&hdmi_drv->hdcp_mutex);
			}
		}

		msleep(80);
	}

	return 0;
}
#endif

static void drv_global_value_init(void)
{
	hdmi_printf = 0;
	hdmi_hpd_mask = 0;
	hdmi_clk_enable_mask = 0;
	hdmi_pin_config_mask = 0;
	hdmi_enable_mask = 0;
	hdmi_suspend_mask = 0;

	boot_hdmi = false;
	video_on = false;
	hpd_state = 0;
	hdcp_encrypt_status = HDCP_DISABLE;
}

#if defined(__LINUX_PLAT__)
static int hdmi_tx_init(struct platform_device *pdev)
#else
s32 hdmi_init(void)
#endif

{
	int ret = 0;
	int phy_model = 301;
	uintptr_t reg_base = 0x0;
	struct disp_device_func disp_func;
#if defined(__LINUX_PLAT__)
	unsigned int value/*, value0[10]*/;
	unsigned int output_type0, output_mode0, output_type1, output_mode1;
	struct clk *hdmi_parent_clk = NULL, *ddc_parent_clk = NULL;
	struct device_node *esm_np;
	u32 dts_esm_buff_phy_addr = 0, dts_esm_size_phy_addr = 0;
	void *dts_esm_buff_vir_addr = NULL, *dts_esm_size_vir_addr = NULL;
	u8 *esm_firm_vir_addr = NULL, *esm_data_vir_addr = NULL;
	hdcpParams_t hdcp;
	/*unsigned int boot_type;
	struct disp_device_config config;*/
#endif
#if defined(CONFIG_SND_SUNXI_SOC_HDMIAUDIO)
	__audio_hdmi_func audio_func;
#if defined(CONFIG_SND_SUNXI_SOC_AUDIOHUB_INTERFACE)
	__audio_hdmi_func audio_func_muti;
#endif
#endif

	LOG_TRACE();

	pr_info("HDMI 2.0 driver init start!\n");
	drv_global_value_init();

	hdmi_drv = kmalloc(sizeof(struct hdmi_tx_drv), GFP_KERNEL);
	if (!hdmi_drv) {
		pr_err("%s:Could not allocated hdmi_tx_dev\n", FUNC_NAME);
		return -1;
	}
	memset(hdmi_drv, 0, sizeof(struct hdmi_tx_drv));

	hdmi_drv->hdmi_core = kmalloc(sizeof(struct hdmi_tx_core), GFP_KERNEL);
	if (!hdmi_drv->hdmi_core) {
		pr_err("%s:Could not allocated hdmi_tx_core\n", __func__);
		goto free_mem;
	}
	memset(hdmi_drv->hdmi_core, 0, sizeof(struct hdmi_tx_core));

	esm_firm_vir_addr = dma_alloc_coherent(&pdev->dev,
					HDCP22_FIRMWARE_SIZE,
					&hdcp.esm_firm_phy_addr,
					GFP_KERNEL | __GFP_ZERO);
	hdcp.esm_firm_vir_addr = (unsigned long)esm_firm_vir_addr;
	hdcp.esm_firm_size = HDCP22_FIRMWARE_SIZE;

	esm_data_vir_addr = dma_alloc_coherent(&pdev->dev,
					HDCP22_DATA_SIZE,
					&hdcp.esm_data_phy_addr,
					GFP_KERNEL | __GFP_ZERO);
	hdcp.esm_data_size = HDCP22_DATA_SIZE;
	hdcp.esm_data_vir_addr = (unsigned long)esm_data_vir_addr;

	hdmi_drv->hdmi_core->blacklist_sink = -1;
#if defined(__LINUX_PLAT__)

	hdmi_drv->pdev = pdev;
	hdmi_drv->parent_dev = &pdev->dev;

	/*Read video booting params from disp device tree*/
	value = disp_boot_para_parse("boot_disp");

	/*To check if hdmi has been configured in uboot*/
	output_type0 = (value >> 8) & 0xff;
	output_mode0 = (value) & 0xff;
	output_type1 = (value >> 24) & 0xff;
	output_mode1 = (value >> 16) & 0xff;
	if ((output_type0 == DISP_OUTPUT_TYPE_HDMI) ||
		(output_type1 == DISP_OUTPUT_TYPE_HDMI))
		boot_hdmi = true;
	pr_info("boot_hdmi=%s\n", boot_hdmi ? "ture" : "false");

	/*parse the booting video params*/
	/*boot_type = output_type0;
	config.mode = output_mode0;
	config.format = value1 & 0xff;
	config.bits = (value1 >> 8) & 0xff;
	config.cs = (value1 >> 16) & 0xffff;
	config.eotf = value2 & 0xff;*/

	/* iomap */
	reg_base = (uintptr_t __force)of_iomap(pdev->dev.of_node, 0);
	if (0 == reg_base) {

		pr_err("unable to map hdmi registers\n");
		ret = -EINVAL;
		goto free_mem;
	}
#else
	/* iomap */
	reg_base = disp_getprop_regbase("hdmi", "reg", 0);
	if (0 == reg_base) {
		pr_err("unable to map hdmi registers\n");
		ret = -1;
		kfree(hdmi_drv);
		return -1;
	}
#endif
	hdmi_core_set_base_addr(reg_base);
	hdmi_drv->reg_base = reg_base;
	hdcp.esm_hpi_base = reg_base + ESM_REG_BASE_OFFSET;
	pr_info("hdmi reg base:0x%llx esm_hpi_base:0x%llx\n", __pa(reg_base),
						__pa(hdcp.esm_hpi_base));
#if defined(__LINUX_PLAT__)

	/***********************get dts value configs*************************/
	/*get ddc control gpio enable config*/
	if (of_property_read_u32(pdev->dev.of_node,
				       "ddc_en_io_ctrl",
				       &hdmi_drv->ddc_ctrl_en))
		pr_info("ERROR: can not get ddc_en_io_ctrl\n");

	/*get hdcp configs*/
	if (of_property_read_u32_array(pdev->dev.of_node, "hdmi_hdcp_enable",
			(u32 *)&hdcp.use_hdcp, 1))
			pr_info("ERROR: can not get hdmi_hdcp_enable\n");
	if (of_property_read_u32_array(pdev->dev.of_node, "hdmi_hdcp22_enable",
			(u32 *)&hdcp.use_hdcp22, 1))
			pr_info("ERROR: can not get hdmi_hdcp22_enable\n");

	esm_np = of_find_node_by_name(NULL, "esm");
	if (esm_np == NULL) {
		pr_info("hdmi2.0 error: could not find the node of esm\n");
	} else if (hdcp.use_hdcp22) {
		if (!of_property_read_u32_array(esm_np,
						"esm_img_size_addr",
						&dts_esm_size_phy_addr,
						1)) {
			/*obtain esm firmware size*/
			dts_esm_size_vir_addr = __va(dts_esm_size_phy_addr);
			memcpy((void *)(&hdcp.esm_firm_size), dts_esm_size_vir_addr, 4);

			if (!of_property_read_u32_array(esm_np,
							"esm_img_buff_addr",
							&dts_esm_buff_phy_addr,
							1)) {
				/*obtain esm firmware*/
				dts_esm_buff_vir_addr = __va(dts_esm_buff_phy_addr);
				if (hdcp.esm_firm_size <= HDCP22_FIRMWARE_SIZE)
					memcpy(esm_firm_vir_addr,
						dts_esm_buff_vir_addr,
						hdcp.esm_firm_size);
				else
					pr_info("WARN: esm_firm_size is too big\n");
			} else {
				pr_info("ERROR: Can not read esm_img_buff_addr\n");
			}
		} else {
			pr_info("ERROR: Can not read esm_img_size_addr\n");
		}
	}

	/******************get dts pin ctrl and gpio configs******************/
	/*Get DCC GPIO*/
	hdmi_drv->pctl = pinctrl_get(&pdev->dev);
	if (IS_ERR(hdmi_drv->pctl))
		pr_err("ERROR: pinctrl_get for HDMI2.0 DDC fail\n");

	hdmi_drv->ddc_ctrl.gpio = of_get_named_gpio_flags(pdev->dev.of_node, "ddc_io_ctrl", 0,
				(enum of_gpio_flags *)(&(hdmi_drv->ddc_ctrl)));

	if (0 != gpio_request(hdmi_drv->ddc_ctrl.gpio, NULL)) {
		pr_info("ddc ctrl gpio_request is failed\n");
		return 0;
	}

	/**********************get dts clk configs****************************/
	hdmi_drv->hdmi_clk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(hdmi_drv->hdmi_clk)) {
		dev_err(&pdev->dev, "fail to get clk for hdmi\n");
		goto free_mem;
	}

	hdmi_drv->hdmi_ddc_clk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(hdmi_drv->hdmi_ddc_clk)) {
		dev_err(&pdev->dev, "fail to get clk for hdmi ddc\n");
		goto free_mem;
	}

	hdmi_drv->hdmi_hdcp_clk = of_clk_get(pdev->dev.of_node, 2);
	if (IS_ERR(hdmi_drv->hdmi_hdcp_clk)) {
		dev_err(&pdev->dev, "fail to get clk for hdmi hdcp\n");
		goto free_mem;
	}

	hdmi_drv->hdmi_cec_clk = of_clk_get(pdev->dev.of_node, 3);
	if (IS_ERR(hdmi_drv->hdmi_cec_clk)) {
		dev_err(&pdev->dev, "fail to get clk for hdmi ddc\n");
		goto free_mem;
	}

	/*ret = disp_sys_script_get_item("hdmi", "hdmi_power", (int*)hdmi_power, 2);
	if (2 == ret) {
		if (hdmi_power_used) {
			pr_info("[HDMI] power %s\n", hdmi_power);
			mutex_lock(&mlock);
			ret = hdmi_power_enable(hdmi_power);
			power_enable_count ++;
			mutex_unlock(&mlock);
			if (0 != ret) {
				power_enable_count --;
				dev_err(&pdev->dev, "fail to enable hdmi power %s\n", hdmi_power);
				goto err_power;
			}
		}
	}*/
#else
	int node_offset = 0;
	char io_name[32];
	disp_gpio_set_t  gpio_info;

	node_offset = disp_fdt_nodeoffset("hdmi");
	of_periph_clk_config_setup(node_offset);

	/* get clk */
	hdmi_drv->hdmi_clk = of_clk_get(node_offset, 0);
	if (IS_ERR(hdmi_drv->hdmi_clk)) {
		dev_err(&pdev->dev, "fail to get clk for hdmi\n");
		kfree(hdmi_drv);
		return -1;

	}

	hdmi_drv->hdmi_ddc_clk = of_clk_get(node_offset, 1);
	if (IS_ERR(hdmi_drv->hdmi_ddc_clk)) {
		dev_err(&pdev->dev, "fail to get clk for hdmi ddc\n");
		kfree(hdmi_drv);
		return -1;
	}

	hdmi_drv->hdmi_hdcp_clk = of_clk_get(node_offset, 2);
	if (IS_ERR(hdmi_drv->hdmi_hdcp_clk)) {
		dev_err(&pdev->dev, "fail to get clk for hdmi hdcp\n");
		kfree(hdmi_drv);
		return -1;
	}
	hdmi_drv->hdmi_cec_clk = of_clk_get(node_offset, 3);
	if (IS_ERR(hdmi_drv->hdmi_cec_clk)) {
		dev_err(&pdev->dev, "fail to get clk for hdmi cec\n");
		kfree(hdmi_drv);
		return -1;
	}

	clk_set_rate(hdmi_drv->hdmi_hdcp_clk, 300000000);

	ret = clk_prepare_enable(hdmi_drv->hdmi_clk);
	if (ret != 0)
		pr_info("hdmi clk enable failed!\n");
	ret = clk_prepare_enable(hdmi_drv->hdmi_ddc_clk);
	if (ret != 0)
		pr_info("hdmi ddc clk enable failed!\n");
	ret = clk_prepare_enable(hdmi_drv->hdmi_cec_clk);
	if (ret != 0)
		pr_info("hdmi cec clk enable failed!\n");
	ret = clk_prepare_enable(hdmi_drv->hdmi_hdcp_clk);
	if (ret != 0)
		pr_info("hdmi hdcp clk enable failed!\n");

	sprintf(io_name, "ddc_scl");
	ret = disp_sys_script_get_item("hdmi",
		io_name,
		(int *)&gpio_info,
		sizeof(disp_gpio_set_t)/sizeof(int));
	if (ret == 3) {
		disp_sys_gpio_request_simple(&gpio_info, 1);
		pr_info("enable ddc_scl pin\n");

	}
	memset(io_name, 0, 32);
	memset(&gpio_info, 0, sizeof(gpio_info));
	sprintf(io_name, "ddc_sda");
	ret = disp_sys_script_get_item("hdmi",
		io_name,
		(int *)&gpio_info,
		sizeof(disp_gpio_set_t)/sizeof(int));
	if (ret == 3) {
		disp_sys_gpio_request_simple(&gpio_info, 1);
		pr_info("enable ddc_sda pin\n");

	}
	/* cec pin */
	memset(io_name, 0, 32);
	memset(&gpio_info, 0, sizeof(gpio_info));
	sprintf(io_name, "cec_io");
	ret = disp_sys_script_get_item("hdmi",
		io_name,
		(int *)&gpio_info,
		sizeof(disp_gpio_set_t)/sizeof(int));
	if (ret == 3) {
		disp_sys_gpio_request_simple(&gpio_info, 1);
		pr_info("enable hmid cec pin\n");
	} else {
		pr_info("hmid cec pin not config\n");
	}
	hdmi_clk_enable_mask = 1;

#endif
	/*Init hdmi core and core params*/
	if (hdmi_tx_core_init(hdmi_drv->hdmi_core,
							phy_model, NULL,
							NULL, &hdcp)) {
		pr_err("Application init failed\n");
		goto free_mem;
	}

#if defined(__LINUX_PLAT__)
	/*init hdmi params*/
	/*set_static_config(&config);*/

#if defined(CONFIG_SWITCH) || defined(CONFIG_ANDROID_SWITCH)
	switch_dev_register(&hdmi_switch_dev);
#endif

	/*Check if hdmi has been configured during booting*/
	if (boot_hdmi) {
		HDMI_INFO_MSG("uboot has configured hdmi\n");
		video_on = true;
		hdmi_clk_enable_mask = 1;
		hdmi_pin_config_mask = 1;
		hdmi_enable_mask = 1;
#if !defined(CONFIG_COMMON_CLK_ENABLE_SYNCBOOT)
		hdmi_drv->hdmi_clk->enable_count = 1;
		hdmi_drv->hdmi_ddc_clk->enable_count = 1;
		hdmi_drv->hdmi_clk->prepare_count = 1;
		hdmi_drv->hdmi_ddc_clk->prepare_count = 1;
		hdmi_parent_clk = clk_get_parent(hdmi_drv->hdmi_clk);
		hdmi_parent_clk->enable_count++;
		hdmi_parent_clk->prepare_count++;
		ddc_parent_clk = clk_get_parent(hdmi_drv->hdmi_ddc_clk);
		ddc_parent_clk->enable_count++;
		ddc_parent_clk->prepare_count++;
#endif
		if (hdmi_drv->hdmi_hdcp_clk != NULL)
			if (clk_prepare_enable(hdmi_drv->hdmi_hdcp_clk) != 0)
				pr_info("hdmi hdcp clk enable failed!\n");
		if (hdmi_drv->hdmi_cec_clk != NULL) {
			struct clk *cec_clk_parent = NULL;
			cec_clk_parent = clk_get(NULL, "periph32k");
			if (cec_clk_parent == NULL || IS_ERR(cec_clk_parent)) {
				pr_err("periph32k clk get failed\n");
			} else {
				if (clk_set_parent(hdmi_drv->hdmi_cec_clk, cec_clk_parent) != 0)
					pr_err("hdmi ddc clk set parent clk 'periph32k' failed\n");
			}
			if (clk_prepare_enable(hdmi_drv->hdmi_cec_clk) != 0)
				pr_info("hdmi cec clk enable failed!\n");
		}

	} else {
		HDMI_INFO_MSG("uboot has not configured hdmi\n");
		video_on = false;
		hdmi_clk_enable_mask = 0;
		hdmi_pin_config_mask = 0;

		hdmi_sys_source_configure();
		/*enable hpd sense*/
		hpd_sense_enbale_core(hdmi_drv->hdmi_core);
/*
#if defined(CONFIG_SWITCH) || defined(CONFIG_ANDROID_SWITCH)
		switch_set_state(&hdmi_switch_dev, 0);
#endif
*/
	}


#if defined(CONFIG_SWITCH) || defined(CONFIG_ANDROID_SWITCH)
		if (!(hdmi_hpd_mask & 0x100))
			switch_set_state(&hdmi_switch_dev, hpd_state);
#endif

	/*Now that everything is fine, let's add it to device list*/
	list_add_tail(&hdmi_drv->devlist, &devlist_global);

	mutex_init(&hdmi_drv->ctrl_mutex);
	mutex_init(&hdmi_drv->hdcp_mutex);

	/*Create hdmi thread to poll hpd and hdcp status and handle hdcp and hpd event*/
	hdmi_drv->hdmi_task = kthread_create(hdmi_run_thread, (void *)0, "hdmi proc");
	if (IS_ERR(hdmi_drv->hdmi_task)) {
		pr_info("Unable to start kernel thread %s.\n\n", "hdmi proc");
		hdmi_drv->hdmi_task = NULL;
		goto free_mem;
	}
	wake_up_process(hdmi_drv->hdmi_task);
	cec_thread_init(hdmi_drv->parent_dev);

#endif
	memset(&disp_func, 0, sizeof(struct disp_device_func));
	disp_func.enable = hdmi_enable;
	disp_func.disable = hdmi_disable;
#if defined(__LINUX_PLAT__)
	disp_func.mode_support = hdmi_mode_support;
#endif
	disp_func.get_HPD_status = hdmi_get_HPD_status;
	disp_func.get_input_csc = hdmi_core_get_csc_type;
	disp_func.get_video_timing_info = hdmi_get_video_timming_info;
#if defined(__LINUX_PLAT__)
	disp_func.suspend = hdmi_suspend;
	disp_func.resume = hdmi_resume;
#endif
	disp_func.set_static_config = set_static_config;
	disp_func.get_static_config = get_static_config;
#if defined(__LINUX_PLAT__)
	disp_func.set_dynamic_config = set_dynamic_config;
	disp_func.get_dynamic_config = get_dynamic_config;
#endif
	disp_set_hdmi_func(&disp_func);

#if defined(CONFIG_SND_SUNXI_SOC_HDMIAUDIO)
	audio_func.hdmi_audio_enable = hdmi_audio_enable;
	audio_func.hdmi_set_audio_para = hdmi_set_audio_para;
	audio_set_hdmi_func(&audio_func);
	HDMI_INFO_MSG("audio_set_hdmi_func\n");

#if defined(CONFIG_SND_SUNXI_SOC_AUDIOHUB_INTERFACE)
	audio_func_muti.hdmi_audio_enable = hdmi_audio_enable;
	audio_func_muti.hdmi_set_audio_para = hdmi_set_audio_para;
	audio_set_muti_hdmi_func(&audio_func_muti);
#endif
#endif
	HDMI_INFO_MSG("HDMI2.0 DRIVER PROBE END\n");

	return ret;

free_mem:
	kfree(hdmi_drv->hdmi_core);
	kfree(hdmi_drv);
	pr_info("Free core and drv memory\n");
	return -1;
}

#if defined(__LINUX_PLAT__)

static int hdmi_tx_exit(struct platform_device *pdev)
{
	struct hdmi_tx_drv *dev;
	struct list_head *list;

	cec_thread_exit();
	while (!list_empty(&devlist_global)) {
		list = devlist_global.next;
		list_del(list);
		dev = list_entry(list, struct hdmi_tx_drv, devlist);

		if (dev == NULL)
			continue;
	}

	if (hdmi_drv->hdmi_task) {
		kthread_stop(hdmi_drv->hdmi_task);
		hdmi_drv->hdmi_task = NULL;
	}

	return 0;
}

/**
 * @short of_device_id structure
 */
static const struct of_device_id dw_hdmi_tx[] = {
	{ .compatible =	"allwinner,sunxi-hdmi" },
	{ }
};
MODULE_DEVICE_TABLE(of, dw_hdmi_tx);

/**
 * @short Platform driver structure
 */
static struct platform_driver __refdata dwc_hdmi_tx_pdrv = {
	.remove = hdmi_tx_exit,
	.probe = hdmi_tx_init,
	.driver = {
		.name = "allwinner,sunxi-hdmi",
		.owner = THIS_MODULE,
		.of_match_table = dw_hdmi_tx,
	},
};


static int hdmi_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int hdmi_release(struct inode *inode, struct file *file)
{
	return 0;
}


static ssize_t hdmi_read(struct file *file, char __user *buf,
						size_t count,
						loff_t *ppos)
{
	return -EINVAL;
}

static ssize_t hdmi_write(struct file *file, const char __user *buf,
						size_t count,
						loff_t *ppos)
{
	return -EINVAL;
}

static int hdmi_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;
}

static long hdmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}


static const struct file_operations hdmi_fops = {
	.owner		= THIS_MODULE,
	.open		= hdmi_open,
	.release	= hdmi_release,
	.write	  = hdmi_write,
	.read		= hdmi_read,
	.unlocked_ioctl	= hdmi_ioctl,
	.mmap	   = hdmi_mmap,
};




static int __parse_dump_str(const char *buf, size_t size,
				unsigned long *start, unsigned long *end)
{
	char *ptr = NULL;
	char *ptr2 = (char *)buf;
	int ret = 0, times = 0;

	/* Support single address mode, some time it haven't ',' */
next:

	/*Default dump only one register(*start =*end).
	If ptr is not NULL, we will cover the default value of end.*/
	if (times == 1)
		*start = *end;

	if (!ptr2 || (ptr2 - buf) >= size)
		goto out;

	ptr = ptr2;
	ptr2 = strnchr(ptr, size - (ptr - buf), ',');
	if (ptr2) {
		*ptr2 = '\0';
		ptr2++;
	}

	ptr = strim(ptr);
	if (!strlen(ptr))
		goto next;

	ret = kstrtoul(ptr, 16, end);
	if (!ret) {
		times++;
		goto next;
	} else
	pr_warn("String syntax errors: \"%s\"\n", ptr);

out:
	return ret;
}


static ssize_t hdmi_test_reg_read_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(count)] > hdmi_test_reg_read");
}

ssize_t hdmi_test_reg_read_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long start_reg = 0;
	unsigned long read_count = 0;
	u32 i;
	u8 *separator;
	u32 data = 0;

	separator = strchr(buf, ',');
	if (separator != NULL) {
		if (__parse_dump_str(buf, count, &start_reg, &read_count))
			pr_err("%s,%d err, invalid para!\n", __func__, __LINE__);

		pr_info("start_reg=0x%lx  read_count=%ld\n", start_reg, read_count);
		for (i = 0; i < read_count; i++) {
			pr_info("hdmi_addr_offset: 0x%lx = 0x%x\n", start_reg,
					hdmitx_read(start_reg * 4));

			start_reg++;
		}
	} else {
		separator = strchr(buf, ' ');
		if (separator != NULL) {
			start_reg = simple_strtoul(buf, NULL, 0);
			read_count = simple_strtoul(separator + 1, NULL, 0);
			for (i = 0; i < read_count; i += 4) {
				data = (u8)hdmitx_read((start_reg + i) * 4);
				data |= ((u8)hdmitx_read((start_reg + i + 1) * 4)) << 8;
				data |= ((u8)hdmitx_read((start_reg + i + 2) * 4)) << 16;
				data |= ((u8)hdmitx_read((start_reg + i + 3) * 4)) << 24;
				if ((i % 16) == 0)
					printk("\n0x%08lx: 0x%08x",
							(start_reg + i), data);
				else
					printk(" 0x%08x", data);
			}
		} else {
			start_reg = simple_strtoul(buf, NULL, 0);
			pr_info("hdmi_addr_offset: 0x%lx = 0x%x\n", start_reg,
					hdmitx_read(start_reg * 4));
		}
	}
	pr_info("\n");

	return count;
}

static DEVICE_ATTR(read, S_IRUGO|S_IWUSR|S_IWGRP,
					hdmi_test_reg_read_show,
					hdmi_test_reg_read_store);


static ssize_t hdmi_test_reg_write_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(value)] > hdmi_test_write");
}

ssize_t hdmi_test_reg_write_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long reg_addr = 0;
	unsigned long value = 0;
	u8 *separator1 = NULL;
	u8 *separator2 = NULL;

	separator1 = strchr(buf, ',');
	separator2 = strchr(buf, ' ');
	if (separator1 != NULL) {
		if (__parse_dump_str(buf, count, &reg_addr, &value))
			pr_err("%s,%d err, invalid para!\n", __func__, __LINE__);

		pr_info("reg_addr=0x%lx  write_value=0x%lx\n", reg_addr, value);
		hdmitx_write((reg_addr * 4), value);

		mdelay(1);
		pr_info("after write,red(%lx)=%x\n", reg_addr,
				hdmitx_read(reg_addr * 4));
	} else if (separator2 != NULL) {
		reg_addr = simple_strtoul(buf, NULL, 0);
		value = simple_strtoul(separator2 + 1, NULL, 0);
		pr_info("reg_addr=0x%lx  write_value=0x%lx\n", reg_addr, value);
		hdmitx_write((reg_addr * 4), value);

		mdelay(1);
		pr_info("after write,red(%lx)=%x\n", reg_addr,
				hdmitx_read(reg_addr * 4));
	} else {
		pr_info("ERROR:error input\n");
	}

	return count;
}

static DEVICE_ATTR(write, S_IRUGO|S_IWUSR|S_IWGRP,
					hdmi_test_reg_write_show,
					hdmi_test_reg_write_store);

static ssize_t phy_write_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	pr_info("OPMODE_PLLCFG-0x16\n");
	pr_info("CKSYMTXCTRL-0x09\n");
	pr_info("PLLCURRCTRL-0x10\n");
	pr_info("VLEVCTRL-0x0E\n");
	pr_info("PLLGMPCTRL-0x15\n");
	pr_info("TXTERM-0x19\n");

	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(value)] > phy_write");
}

static ssize_t phy_write_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	u8 reg_addr = 0;
	u16 value = 0;
	struct hdmi_tx_core *core = NULL;

	core = hdmi_drv->hdmi_core;

	if (__parse_dump_str(buf, count, (unsigned long *)&reg_addr, (unsigned long *)&value))
		pr_err("%s,%d err, invalid para!\n", __func__, __LINE__);

	pr_info("reg_addr=0x%x  write_value=0x%x\n", (u32)reg_addr, (u32)value);
	core->dev_func.phy_write(reg_addr, value);
	return count;
}

static DEVICE_ATTR(phy_write, S_IRUGO|S_IWUSR|S_IWGRP,
					phy_write_show,
					phy_write_store);


/*static DEVICE_ATTR(hdmi_test_print_core_structure, S_IRUGO|S_IWUSR|S_IWGRP,
					hdmi_test_print_core_structure_show,
					NULL);*/

static ssize_t phy_read_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	pr_info("OPMODE_PLLCFG-0x16\n");
	pr_info("CKSYMTXCTRL-0x09\n");
	pr_info("PLLCURRCTRL-0x10\n");
	pr_info("VLEVCTRL-0x0E\n");
	pr_info("PLLGMPCTRL-0x15\n");
	pr_info("TXTERM-0x19\n");

	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(count)] > phy_read");
}

ssize_t phy_read_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	u8 start_reg = 0;
	u16 value = 0;
	unsigned long read_count = 0;
	u32 i;
	struct hdmi_tx_core *core = get_platform();

	if (__parse_dump_str(buf, count, (unsigned long *)&start_reg, &read_count))
		pr_err("%s,%d err, invalid para!\n", __func__, __LINE__);

	pr_info("start_reg=0x%x  read_count=%ld\n", (u32)start_reg, read_count);
	for (i = 0; i < read_count; i++) {
		core->dev_func.phy_read(start_reg, &value);
		pr_info("hdmi_addr_offset: 0x%x = 0x%x\n", (u32)start_reg, value);
		start_reg++;
	}
	pr_info("\n");

	return count;
}

static DEVICE_ATTR(phy_read, S_IRUGO|S_IWUSR|S_IWGRP,
					phy_read_show,
					phy_read_store);


static ssize_t scdc_read_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(count)] > scdc_read");
}

ssize_t scdc_read_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	u8 start_reg = 0;
	u8 value = 0;
	unsigned long read_count = 0;
	u32 i;
	struct hdmi_tx_core *core = get_platform();

	if (__parse_dump_str(buf, count, (unsigned long *)&start_reg, &read_count))
		pr_err("%s,%d err, invalid para!\n", __func__, __LINE__);

	pr_info("start_reg=0x%x  read_count=%ld\n", (u32)start_reg, read_count);
	for (i = 0; i < read_count; i++) {
		core->dev_func.scdc_read(start_reg, 1, &value);
		pr_info("hdmi_addr_offset: 0x%x = 0x%x\n", (u32)start_reg, value);
		start_reg++;
	}
	pr_info("\n");

	return count;
}

static DEVICE_ATTR(scdc_read, S_IRUGO|S_IWUSR|S_IWGRP,
					scdc_read_show,
					scdc_read_store);

static ssize_t scdc_write_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(value)] > scdc_write");
}

static ssize_t scdc_write_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	u8 reg_addr = 0;
	u8 value = 0;
	struct hdmi_tx_core *core = NULL;

	core = hdmi_drv->hdmi_core;

	if (__parse_dump_str(buf, count, (unsigned long *)&reg_addr, (unsigned long *)&value))
		pr_err("%s,%d err, invalid para!\n", __func__, __LINE__);

	pr_info("reg_addr=0x%x  write_value=0x%x\n", reg_addr, value);
	core->dev_func.scdc_write(reg_addr, 1, &value);
	return count;
}

static DEVICE_ATTR(scdc_write, S_IRUGO|S_IWUSR|S_IWGRP,
					scdc_write_show,
					scdc_write_store);


static ssize_t hdmi_debug_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "debug=%d\n", hdmi_printf);

	return 0;
}

static ssize_t hdmi_debug_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strnicmp(buf, "1", 1) == 0)
		hdmi_printf = 1;
	else if (strnicmp(buf, "0", 1) == 0)
		hdmi_printf = 0;
	else
		HDMI_ERROR_MSG("Error Input!\n");

	pr_info("debug=%d\n", hdmi_printf);

	return count;
}
static DEVICE_ATTR(debug, S_IRUGO|S_IWUSR|S_IWGRP, hdmi_debug_show,
							hdmi_debug_store);

static ssize_t hdmi_rgb_only_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (hdmi_drv->hdmi_core->mode.sink_cap->edid_mYcc444Support ||
		hdmi_drv->hdmi_core->mode.sink_cap->edid_mYcc422Support ||
		hdmi_drv->hdmi_core->mode.sink_cap->edid_mYcc420Support)
		return sprintf(buf, "rgb_only=%s\n", "off");
	else
		return sprintf(buf, "rgb_only=%s\n", "on");

	return 0;
}

static ssize_t hdmi_rgb_only_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int mYcc444Support =
		hdmi_drv->hdmi_core->mode.sink_cap->edid_mYcc444Support;
	int mYcc422Support =
		hdmi_drv->hdmi_core->mode.sink_cap->edid_mYcc422Support;
	int mYcc420Support =
		hdmi_drv->hdmi_core->mode.sink_cap->edid_mYcc420Support;

	if (count < 1)
		return -EINVAL;

	if (strnicmp(buf, "on", 2) == 0 || strnicmp(buf, "1", 1) == 0) {
		hdmi_drv->hdmi_core->mode.sink_cap->edid_mYcc444Support = 0;
		hdmi_drv->hdmi_core->mode.sink_cap->edid_mYcc422Support = 0;
		hdmi_drv->hdmi_core->mode.sink_cap->edid_mYcc420Support = 0;
	} else if (strnicmp(buf, "off", 3) == 0 || strnicmp(buf, "0", 1) == 0) {
		hdmi_drv->hdmi_core->mode.sink_cap->edid_mYcc444Support =
								mYcc444Support;
		hdmi_drv->hdmi_core->mode.sink_cap->edid_mYcc422Support =
								mYcc422Support;
		hdmi_drv->hdmi_core->mode.sink_cap->edid_mYcc420Support =
								mYcc420Support;
		} else {
			return -EINVAL;
		}

	return count;
}

static DEVICE_ATTR(rgb_only, S_IRUGO|S_IWUSR|S_IWGRP, hdmi_rgb_only_show,
							hdmi_rgb_only_store);


static ssize_t hdmi_hpd_mask_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "0x%x\n", hdmi_hpd_mask);
}

static ssize_t hdmi_hpd_mask_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int err;
	unsigned long val;

	if (count < 1)
		return -EINVAL;

	err = kstrtoul(buf, 16, &val);
	if (err) {
		pr_info("Invalid size\n");
		return err;
	}

	pr_info("val=0x%x\n", (u32)val);
	hdmi_hpd_mask = val;

	return count;
}

static DEVICE_ATTR(hpd_mask, S_IRUGO|S_IWUSR|S_IWGRP, hdmi_hpd_mask_show,
							hdmi_hpd_mask_store);


static ssize_t hdmi_edid_show(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	u8 pedid[1024];

	if ((hdmi_drv->hdmi_core->mode.edid != NULL) &&
		(hdmi_drv->hdmi_core->mode.edid_ext != NULL)) {
		/*EDID_block0*/
		memcpy(pedid, hdmi_drv->hdmi_core->mode.edid, 0x80);
		/*EDID_block1*/
		memcpy(pedid+0x80, hdmi_drv->hdmi_core->mode.edid_ext, 0x380);

		memcpy(buf, pedid, 0x400);

		return 0x400;
	} else {
		return 0;
	}
}

static ssize_t hdmi_edid_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR(edid, S_IRUGO|S_IWUSR|S_IWGRP,
			hdmi_edid_show,
			hdmi_edid_store);


static ssize_t hdmi_hdcp_enable_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n",
				hdmi_drv->hdmi_core->mode.pHdcp.hdcp_on);
}

static ssize_t hdmi_hdcp_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	mutex_lock(&hdmi_drv->hdcp_mutex);
	if (strnicmp(buf, "1", 1) == 0) {
		set_hdcp_status(HDCP_ING);
		hdcp_enable_core(hdmi_drv->hdmi_core, 1);
	} else {
		hdcp_enable_core(hdmi_drv->hdmi_core, 0);
		set_hdcp_status(HDCP_DISABLE);
	}
	mutex_unlock(&hdmi_drv->hdcp_mutex);

	return count;
}

static DEVICE_ATTR(hdcp_enable, S_IRUGO|S_IWUSR|S_IWGRP,
				hdmi_hdcp_enable_show,
				hdmi_hdcp_enable_store);


static ssize_t hpi_write_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(value)] > hpi_write");
}

static ssize_t hpi_write_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	u32 reg_addr = 0;
	u32 value = 0;
	struct hdmi_tx_core *core = NULL;

	core = hdmi_drv->hdmi_core;

	if (__parse_dump_str(buf, count, (unsigned long *)&reg_addr, (unsigned long *)&value))
		pr_err("%s,%d err, invalid para!\n", __func__, __LINE__);

	pr_info("reg_addr=0x%x  write_value=0x%x\n", reg_addr, value);
	*((u32 *)(core->mode.pHdcp.esm_hpi_base + reg_addr)) = (u32)value;
	return count;
}

static DEVICE_ATTR(hpi_write, S_IRUGO|S_IWUSR|S_IWGRP,
					hpi_write_show,
					hpi_write_store);

static ssize_t hpi_read_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%s\n", "echo [0x(address offset), 0x(count)] > hpi_read");
}

ssize_t hpi_read_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	u32 start_reg = 0;
	u32 value = 0;
	unsigned long read_count = 0;
	u32 i;
	struct hdmi_tx_core *core = get_platform();

	if (__parse_dump_str(buf, count, (unsigned long *)&start_reg, &read_count))
		pr_err("%s,%d err, invalid para!\n", __func__, __LINE__);

	pr_info("start_reg=0x%x  read_count=%ld\n", (u32)start_reg, read_count);
	for (i = 0; i < read_count; i++) {
		value = *((u32 *)(core->mode.pHdcp.esm_hpi_base + start_reg));
		pr_info("hdmi_addr_offset: 0x%x = 0x%x\n", (u32)start_reg, value);
		start_reg++;
	}
	pr_info("\n");

	return count;
}

static DEVICE_ATTR(hpi_read, S_IRUGO|S_IWUSR|S_IWGRP,
					hpi_read_show,
					hpi_read_store);


static ssize_t hdcp_status_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	u32 count = sizeof(u8);

	memcpy(buf, &hdcp_encrypt_status, count);

	return count;
}

static ssize_t hdcp_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strnicmp(buf, "1", 1) == 0) {

	} else {

	}

	return count;
}

static DEVICE_ATTR(hdcp_status, S_IRUGO|S_IWUSR|S_IWGRP, hdcp_status_show, hdcp_status_store);


struct hdmi_debug_video_mode {
	int hdmi_mode;/* vic */
	char *name;
};


static struct hdmi_debug_video_mode debug_video_mode[] = {
	{HDMI_VIC_720x480I_4_3, "480I"},
	{HDMI_VIC_720x480I_16_9, "480I"},
	{HDMI_VIC_720x576I_4_3,	"576I"},
	{HDMI_VIC_720x576I_16_9, "576I"},
	{HDMI_VIC_720x480P60_4_3, "720x480P"},
	{HDMI_VIC_720x480P60_16_9, "720x480P"},
	{HDMI_VIC_640x480P60, "640x480P"},
	{HDMI_VIC_720x576P_4_3,	"576P"},
	{HDMI_VIC_720x576P_16_9, "576P"},
	{HDMI_VIC_1280x720P24, "720P24"},
	{HDMI_VIC_1280x720P25, "720P25"},
	{HDMI_VIC_1280x720P30, "720P30"},
	{HDMI_VIC_1280x720P50, "720P50"},
	{HDMI_VIC_1280x720P60, "720P60"},
	{HDMI_VIC_1920x1080I50,	"1080I50"},
	{HDMI_VIC_1920x1080I60,	"1080I60"},
	{HDMI_VIC_1920x1080P24,	"1080P24"},
	{HDMI_VIC_1920x1080P50,	"1080P50"},
	{HDMI_VIC_1920x1080P60,	"1080P60"},
	{HDMI_VIC_1920x1080P25,	"1080P25"},
	{HDMI_VIC_1920x1080P30,	"1080P30"},
	{HDMI_VIC_3840x2160P30,	"2160P30"},
	{HDMI_VIC_3840x2160P25,	"2160PP25"},
	{HDMI_VIC_3840x2160P24,	"2160P24"},
	{HDMI_VIC_4096x2160P24,	"4096x2160P24"},
	{HDMI_VIC_4096x2160P25,	"4096x2160P25"},
	{HDMI_VIC_4096x2160P30, "4096x2160P30"},
	{HDMI_VIC_3840x2160P50, "2160P50"},
	{HDMI_VIC_4096x2160P50, "4096x2160P50"},
	{HDMI_VIC_3840x2160P60, "2160P60"},
	{HDMI_VIC_4096x2160P60, "4096x2160P60"},
};



static char *hdmi_vic_name[] = {
	"2160P30",
	"2160P25",
	"2160P24",
	"4096x2160P24",
};


static char *hdmi_audio_code_name[] = {
	"LPCM",
	"AC-3",
	"MPEG1",
	"MP3",
	"MPEG2",
	"AAC",
	"DTS",
	"ATRAC",
	"OneBitAudio",
	"DolbyDigital+",
	"DTS-HD",
	"MAT",
	"DST",
	"WMAPro",
};
static char *debug_get_video_name(int hdmi_mode)
{
	int i = 0;
	for (i = 0;
	i < sizeof(debug_video_mode)/sizeof(struct hdmi_debug_video_mode);
	 i++) {
		if (debug_video_mode[i].hdmi_mode == hdmi_mode)
			return debug_video_mode[i].name;
	}

	return NULL;
}
static ssize_t hdmi_sink_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t n = 0;
	int i = 0;
	sink_edid_t	      *sink_cap =  hdmi_drv->hdmi_core->mode.sink_cap;

	if (!sink_cap) {
		n += sprintf(buf+n, "%s\n", "Do not read edid from sink");
		return n;
	}

	/* Video Data Block */
	n += sprintf(buf+n, "\n\n%s", "Video Mode:");
	for (i = 0; i < sink_cap->edid_mSvdIndex; i++) {
		if (sink_cap->edid_mSvd[i].mLimitedToYcc420
		|| sink_cap->edid_mSvd[i].mYcc420)
			continue;

		n += sprintf(buf+n, "  %s",
			debug_get_video_name(
			(int)sink_cap->edid_mSvd[i].mCode));


		if (sink_cap->edid_mSvd[i].mNative)
			n += sprintf(buf+n, "%s", "(native)");
	}


	for (i = 0; i < sink_cap->edid_mHdmivsdb.mHdmiVicCount; i++) {
		if (sink_cap->edid_mHdmivsdb.mHdmiVic[i] <= 0x4) {
			n += sprintf(buf+n, "%s",
			hdmi_vic_name[
			sink_cap->edid_mHdmivsdb.mHdmiVic[i]-1]);
		}
	}


	/* YCC420 VDB  */
	n += sprintf(buf+n, "\n\n%s", "Only Support YUV420:");
	for (i = 0; i < sink_cap->edid_mSvdIndex; i++) {
		if (sink_cap->edid_mSvd[i].mLimitedToYcc420) {
			n += sprintf(buf+n, "  %s",
			debug_get_video_name(
			(int)sink_cap->edid_mSvd[i].mCode));
		}
	}

	/*YCC420 CMDB */
	n += sprintf(buf+n, "\n\n%s", "Also Support YUV420:");
	for (i = 0; i < sink_cap->edid_mSvdIndex; i++) {
		if (sink_cap->edid_mSvd[i].mYcc420) {
			n += sprintf(buf+n, "  %s",
			debug_get_video_name(
			(int)sink_cap->edid_mSvd[i].mCode));
		}
	}


	/*video pixel format */
	n += sprintf(buf+n, "\n\n%s", "Pixel Format: RGB");
	if (sink_cap->edid_mYcc444Support)
		n += sprintf(buf+n, "  %s", "YUV444");
	if (sink_cap->edid_mYcc422Support)
		n += sprintf(buf+n, "  %s", "YUV422");
	/*deepcolor */
	n += sprintf(buf+n, "\n\n%s", "Deep Color:");
	if (sink_cap->edid_mHdmivsdb.mDeepColor30) {
		n += sprintf(buf+n, "  %s", "RGB444_30bit");
		if (sink_cap->edid_mHdmivsdb.mDeepColorY444)
			n += sprintf(buf+n, "  %s", "YUV444_30bit");
	}

	if (sink_cap->edid_mHdmivsdb.mDeepColor36) {
		n += sprintf(buf+n, "  %s", "RGB444_36bit");
		if (sink_cap->edid_mHdmivsdb.mDeepColorY444)
			n += sprintf(buf+n, "  %s", "YUV444_36bit");
	}
	if (sink_cap->edid_mHdmivsdb.mDeepColor48) {
		n += sprintf(buf+n, "  %s", "RGB444_48bit");
		if (sink_cap->edid_mHdmivsdb.mDeepColorY444)
			n += sprintf(buf+n, "  %s", "YUV444_48bit");
	}

	if (sink_cap->edid_mHdmiForumvsdb.mDC_30bit_420)
		n += sprintf(buf+n, "  %s", "YUV420_30bit");
	if (sink_cap->edid_mHdmiForumvsdb.mDC_36bit_420)
		n += sprintf(buf+n, "  %s", "YUV420_36bit");
	if (sink_cap->edid_mHdmiForumvsdb.mDC_48bit_420)
		n += sprintf(buf+n, "  %s", "YUV420_48bit");

	/*3D format */
	if (sink_cap->edid_mHdmivsdb.m3dPresent) {
		n += sprintf(buf+n, "\n\n%s", "3D Mode:");
		for (i = 0; i < 16; i++) {
			if (sink_cap->edid_mHdmivsdb.mVideo3dStruct[i][0] == 1
			&& i < sink_cap->edid_mSvdIndex) {
				n += sprintf(buf+n, "  %s_FP",
				debug_get_video_name(
				(int)sink_cap->edid_mSvd[i].mCode));
			}
			if (sink_cap->edid_mHdmivsdb.mVideo3dStruct[i][6] == 1
			&& i < sink_cap->edid_mSvdIndex) {
				n += sprintf(buf+n, "  %s_SBS",
				debug_get_video_name(
				(int)sink_cap->edid_mSvd[i].mCode));
			}
			if (sink_cap->edid_mHdmivsdb.mVideo3dStruct[i][8] == 1
			&& i < sink_cap->edid_mSvdIndex) {
				n += sprintf(buf+n, "  %s_TAB",
				debug_get_video_name(
				(int)sink_cap->edid_mSvd[i].mCode));
			}
		}
	}

	/*TMDS clk rate */
	if (sink_cap->edid_mHdmiForumvsdb.mValid) {
		n += sprintf(buf+n, "\n\n%s", "MaxTmdsCharRate:");
		n += sprintf(buf+n, "  %d",
			sink_cap->edid_mHdmiForumvsdb.mMaxTmdsCharRate);
	}
	/*audio*/
	n += sprintf(buf+n, "\n\n%s", "Basic Audio Support:");
	n += sprintf(buf+n, "  %s",
		sink_cap->edid_mBasicAudioSupport ? "YES" : "NO");
	if (sink_cap->edid_mBasicAudioSupport) {
		n += sprintf(buf+n, "\n\n%s", "Audio Code:");
		for (i = 0; i < sink_cap->edid_mSadIndex; i++) {
			n += sprintf(buf+n, "  %s",
		hdmi_audio_code_name[sink_cap->edid_mSad[i].mFormat-1]);
		}
	}
	/*hdcp*/
	n += sprintf(buf+n, "\n\n%s", "HDCP Tpye:");

	n += sprintf(buf+n, "%c", '\n');
	return n;

}



static DEVICE_ATTR(hdmi_sink, S_IRUGO|S_IWUSR|S_IWGRP,
				hdmi_sink_show,
				NULL);


static char *pixel_format_name[] = {
	"RGB",
	"YUV422",
	"YUV444",
	"YUV420",
};

static char *colorimetry_name[] = {
	"NULL",
	"ITU601",
	"ITU709",
	"XV_YCC601",
	"XV_YCC709",
	"S_YCC601",
	"ADOBE_YCC601",
	"ADOBE_RGB",
	"BT2020_Yc_Cbc_Crc",
	"BT2020_Y_CB_CR",
};


static ssize_t hdmi_source_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t n = 0;

	n += sprintf(buf+n, "\n%s%d\n",
		"HPD:  ",
		hdmi_core_get_hpd_state());
	n += sprintf(buf+n, "\n%s%d\n",
		"RxSense:  ",
		hdmi_core_get_rxsense_state());
	n += sprintf(buf+n, "\n%s%d\n",
		"PhyLock:  ",
		hdmi_core_get_phy_pll_lock_state());
	n += sprintf(buf+n, "\n%s%d\n",
		"PhyPower:  ",
		hdmi_core_get_phy_power_state());
	n += sprintf(buf+n, "\n%s%s\n",
		"TmdsMode:  ",
		hdmi_core_get_tmds_mode() ? "HDMI" : "DVI");
	n += sprintf(buf+n, "\n%s%d\n",
		 "Scramble:  ",
		hdmi_core_get_scramble_state());
	n += sprintf(buf+n, "\n%s%d\n",
		"AvMute:  ",
		hdmi_core_get_avmute_state());
	n += sprintf(buf+n, "\n%s%d\n",
		"PixelRepetion:  ",
		 hdmi_core_get_pixelrepetion());
	n += sprintf(buf+n, "\n%s%d\n",
		"BitDepth:  ",
		hdmi_core_get_color_depth());
	n += sprintf(buf+n, "\n%s%s\n",
		"PixelFormat:  ",
		pixel_format_name[hdmi_core_get_pixel_format()]);
	n += sprintf(buf+n, "\n%s%s\n",
		"Colorimetry:  ",
		 colorimetry_name[hdmi_core_get_colorimetry()]);
	n += sprintf(buf+n, "\n%s%s\n",
		"VideoFormat:  ",
		 debug_get_video_name((int)hdmi_core_get_video_code()));
	n += sprintf(buf+n, "\n%s%d\n",
		"AudioLayout:  ",
		 hdmi_core_get_audio_layout());
	n += sprintf(buf+n, "\n%s%d\n",
		"AudioChannelCnt:  ",
		hdmi_core_get_audio_channel_count());
	n += sprintf(buf+n, "\n%s%d\n",
		"AudioSamplingFreq:  ",
		hdmi_core_get_audio_sample_freq());
	n += sprintf(buf+n, "\n%s%d\n",
		"AudioSampleSize:  ",
		 hdmi_core_get_audio_sample_size());
	n += sprintf(buf+n, "\n%s%d\n",
		"AudioNvalue:  ",
		hdmi_core_get_audio_n());
	return n;
}


static DEVICE_ATTR(hdmi_source, S_IRUGO|S_IWUSR|S_IWGRP,
				hdmi_source_show,
				NULL);



static ssize_t hdmi_avmute_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n%s\n",
		"echo [value] > avmute",
		"-----value =1:avmute on; =0:avmute off");
}

static ssize_t hdmi_avmute_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strnicmp(buf, "1", 1) == 0)
		hdmi_core_avmute_enable(1);
	else
		hdmi_core_avmute_enable(0);

	return count;
}

static DEVICE_ATTR(avmute, S_IRUGO|S_IWUSR|S_IWGRP,
				hdmi_avmute_show,
				hdmi_avmute_store);



static ssize_t phy_power_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n%s\n",
		 "echo [value] > phy_power",
		"-----value =1:phy power on; =0:phy power off");
}

static ssize_t phy_power_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strnicmp(buf, "1", 1) == 0)
		hdmi_core_phy_power_enable(1);
	else
		hdmi_core_phy_power_enable(0);

	return count;
}


static DEVICE_ATTR(phy_power, S_IRUGO|S_IWUSR|S_IWGRP,
				phy_power_show,
				phy_power_store);


static ssize_t dvi_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t n = 0;

	n += sprintf(buf + n, "%s\n%s\n",
			 "echo [value] > dvi_mode",
			"-----value =0:HDMI mode; =1:DVI mode");
	return n;
}

static ssize_t dvi_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	if (count < 1)
		return -EINVAL;

	if (strnicmp(buf, "1", 1) == 0)
		hdmi_core_dvimode_enable(1);
	else
		hdmi_core_dvimode_enable(0);

	return count;
}


static DEVICE_ATTR(dvi_mode, S_IRUGO|S_IWUSR|S_IWGRP,
				dvi_mode_show,
				dvi_mode_store);


static struct attribute *hdmi_attributes[] = {
	&dev_attr_read.attr,
	&dev_attr_write.attr,
	&dev_attr_phy_write.attr,
	&dev_attr_phy_read.attr,
	&dev_attr_scdc_read.attr,
	&dev_attr_scdc_write.attr,
	&dev_attr_hdcp_status.attr,
	&dev_attr_hpi_read.attr,
	&dev_attr_hpi_write.attr,
	/*&dev_attr_hdmi_test_print_core_structure.attr,*/

	&dev_attr_debug.attr,
	&dev_attr_rgb_only.attr,
	&dev_attr_hpd_mask.attr,
	&dev_attr_edid.attr,
	&dev_attr_hdcp_enable.attr,

	&dev_attr_hdmi_sink.attr,
	&dev_attr_hdmi_source.attr,
	&dev_attr_avmute.attr,
	&dev_attr_phy_power.attr,
	&dev_attr_dvi_mode.attr,
	NULL
};

static struct attribute_group hdmi_attribute_group = {
	.name = "attr",
	.attrs = hdmi_attributes
};

static int __init hdmi_module_init(void)
{
	int ret = 0, err;

	/*Create and add a character device*/
	alloc_chrdev_region(&devid, 0, 1, "hdmi");/*corely for device number*/
	hdmi_cdev = cdev_alloc();
	cdev_init(hdmi_cdev, &hdmi_fops);
	hdmi_cdev->owner = THIS_MODULE;
	err = cdev_add(hdmi_cdev, devid, 1);/*/proc/device/hdmi*/
	if (err) {
		HDMI_ERROR_MSG("cdev_add fail.\n");
		return -1;
	}

	/*Create a path: sys/class/hdmi*/
	hdmi_class = class_create(THIS_MODULE, "hdmi");
	if (IS_ERR(hdmi_class)) {
		HDMI_ERROR_MSG("class_create fail\n");
		return -1;
	}

	/*Create a path "sys/class/hdmi/hdmi"*/
	hdev = device_create(hdmi_class, NULL, devid, NULL, "hdmi");

	/*Create a path: sys/class/hdmi/hdmi/attr*/
	ret = sysfs_create_group(&hdev->kobj, &hdmi_attribute_group);
	if (ret)
		HDMI_INFO_MSG("failed!\n");

	ret = platform_driver_register(&dwc_hdmi_tx_pdrv);
	if (ret)
		HDMI_ERROR_MSG("hdmi driver register fail\n");

	pr_info("HDMI2.0 module init end\n");

	return ret;
}

static void __exit hdmi_module_exit(void)
{
	pr_info("hdmi_module_exit\n");

	hdmi_tx_exit(hdmi_drv->pdev);

	dma_free_coherent(hdmi_drv->parent_dev,
				HDCP22_DATA_SIZE,
				&hdmi_drv->hdmi_core->mode.pHdcp.esm_data_phy_addr,
				GFP_KERNEL | __GFP_ZERO);
	dma_free_coherent(hdmi_drv->parent_dev,
					HDCP22_FIRMWARE_SIZE,
					&hdmi_drv->hdmi_core->mode.pHdcp.esm_firm_phy_addr,
					GFP_KERNEL | __GFP_ZERO);
	hdmi_core_exit(hdmi_drv->hdmi_core);

	kfree(hdmi_drv);

	platform_driver_unregister(&dwc_hdmi_tx_pdrv);

	sysfs_remove_group(&hdev->kobj, &hdmi_attribute_group);
	device_destroy(hdmi_class, devid);

	class_destroy(hdmi_class);
	cdev_del(hdmi_cdev);
}

late_initcall(hdmi_module_init);
module_exit(hdmi_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("zhengwanyu");
MODULE_DESCRIPTION("HDMI_TX20 module driver");
MODULE_VERSION("1.0");
#endif
