/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "disp_al.h"
#include "de_hal.h"

/*
 * disp_al_private_data - abstract layer private data
 * @output_type: current output type of specified device(eg. LCD/HDMI/TV)
 * @output_mode: current output mode of specified device(eg. 720P,1080P)
 * @output_format: current output color format of specified device
 * @output_color_space: current output color space of specified device
 * @tcon_id: the id of device connect to the specified de
 * @de_id: the id of de connect to the specified tcon
 * @disp_size: the output size of specified de
 * @output_fps: the output fps of specified device
 * @tcon_type: tcon type, 0: tcon0(drive panel), 1: tcon1(general drive HDMI/TV)
 */
struct disp_al_private_data {
	u32 output_type[DEVICE_NUM];
	u32 output_mode[DEVICE_NUM];
	u32 output_format[DEVICE_NUM];
	u32 output_color_space[DEVICE_NUM];
	u32 output_color_range[DEVICE_NUM];
	u32 output_eotf[DEVICE_NUM];
	u32 tcon_id[DE_NUM];
	u32 de_id[DEVICE_NUM];
	struct disp_rect disp_size[DE_NUM];
	u32 output_fps[DEVICE_NUM];
	u32 tcon_type[DEVICE_NUM];
	u32 de_backcolor[DE_NUM];
	bool direct_show[DE_NUM];
	bool direct_show_toggle[DE_NUM];
};

static struct disp_al_private_data al_priv;

static int disp_al_validate_direct_show(unsigned int disp,
			struct disp_layer_config_data *data,
			unsigned int layer_num)
{
	int tcon_id = al_priv.tcon_id[disp];
	bool direct_show = true;
	unsigned char i, vi_chn;

	/*
	 * We need to check several conditions:
	 * 1, the output must be TV(PAL/NTSC) type
	 * 2, the output must be in yuv color space,
	 * 3, only video channels are used,
	 * 4, input format must be yuv format.
	 * 5, input layer size must be in the p2p limit range
	 * 6, the layer must be display on the fullscreen or non-scalely
	 */
	if (al_priv.output_type[tcon_id] != DISP_OUTPUT_TYPE_TV) {
		direct_show = false;
		goto exit;
	}

	if ((al_priv.disp_size[disp].width != CVBS_PAL_WIDTH)
	    && (al_priv.disp_size[disp].width != CVBS_NTSC_WIDTH)) {
		direct_show = false;
		goto exit;
	}

	if ((al_priv.disp_size[disp].height != CVBS_PAL_HEIGHT)
	    && (al_priv.disp_size[disp].height != CVBS_NTSC_HEIGHT)) {
		direct_show = false;
		goto exit;
	}

	/*check if the output csc of DE is yuv*/
	if (al_priv.output_color_space[tcon_id] == 1) {
		direct_show = false;
		goto exit;
	}

	vi_chn = de_feat_get_num_vi_chns(disp);
	for (i = 0; i < layer_num; i++, data++) {
		struct disp_rect64 *crop64;
		struct disp_rect crop;
		struct disp_rect *screen_win;

		if (!data->config.enable)
			continue;

		if (data->config.channel >= vi_chn) {
			direct_show = false;
			goto exit;
		}

		if (data->config.info.fb.format < DISP_FORMAT_YUV444_I_AYUV) {
			direct_show = false;
			goto exit;
		}

		crop64 = &data->config.info.fb.crop;
		crop.x = crop64->x >> VSU_FB_FRAC_BITWIDTH;
		crop.y = crop64->y >> VSU_FB_FRAC_BITWIDTH;
		crop.width = crop64->width >> VSU_FB_FRAC_BITWIDTH;
		crop.height = crop64->height >> VSU_FB_FRAC_BITWIDTH;
		if ((crop.width < P2P_FB_MIN_WIDTH)
		    || (crop.width > P2P_FB_MAX_WIDTH)) {
			direct_show = false;
			goto exit;
		}

		if (crop.height != al_priv.disp_size[disp].height) {
			direct_show = false;
			goto exit;
		}

		screen_win = &data->config.info.screen_win;
		if ((screen_win->width != al_priv.disp_size[disp].width)
		    && (screen_win->width != crop.width)) {
			direct_show = false;
			goto exit;
		}

		if ((screen_win->height != al_priv.disp_size[disp].height)
		    && (screen_win->height != crop.height)) {
			direct_show = false;
			goto exit;
		}
	}

exit:
	if (al_priv.direct_show[disp] != direct_show)
		al_priv.direct_show_toggle[disp] = true;
	else
		al_priv.direct_show_toggle[disp] = false;
	al_priv.direct_show[disp] = direct_show;

	return 0;
}

int disp_al_layer_apply(unsigned int disp, struct disp_layer_config_data *data,
			unsigned int layer_num)
{
	struct disp_csc_config csc_cfg;
	int tcon_id = al_priv.tcon_id[disp];
	bool direct_show;

	disp_al_validate_direct_show(disp, data, layer_num);
	direct_show = al_priv.direct_show[disp];

	memset(&csc_cfg, 0, sizeof(struct disp_csc_config));
	csc_cfg.out_fmt = al_priv.output_format[tcon_id];

	csc_cfg.out_color_range =
	    al_priv.output_color_range[tcon_id];
	csc_cfg.out_mode =
	    al_priv.output_color_space[tcon_id];
	csc_cfg.out_eotf =
	    al_priv.output_eotf[tcon_id];
	csc_cfg.color = al_priv.de_backcolor[disp];

	return de_al_lyr_apply(disp, data, layer_num, &csc_cfg, direct_show);
}

int disp_al_manager_init(unsigned int disp)
{
	return de_clk_enable(DE_CLK_CORE0 + disp);
}

int disp_al_manager_exit(unsigned int disp)
{
	return de_clk_disable(DE_CLK_CORE0 + disp);
}

int disp_al_manager_apply(unsigned int disp, struct disp_manager_data *data)
{
	if (data->flag & MANAGER_ENABLE_DIRTY) {
		struct disp_color *back_color = &data->config.back_color;

		al_priv.disp_size[disp].width = data->config.size.width;
		al_priv.disp_size[disp].height = data->config.size.height;
		al_priv.tcon_id[disp] = data->config.hwdev_index;
		al_priv.de_id[al_priv.tcon_id[disp]] = disp;
		al_priv.output_format[al_priv.tcon_id[disp]] = data->config.cs;
		al_priv.output_color_range[al_priv.tcon_id[disp]] =
		    data->config.color_range;
		al_priv.output_color_space[al_priv.tcon_id[disp]] =
		    data->config.color_space;
		al_priv.output_eotf[al_priv.tcon_id[disp]] =
		    data->config.eotf;
		al_priv.de_backcolor[disp] =
		    (back_color->alpha << 24) | (back_color->red << 16)
		    | (back_color->green << 8) | (back_color->blue << 0);
	}

	if (data->flag & MANAGER_COLOR_SPACE_DIRTY) {
		al_priv.output_format[al_priv.tcon_id[disp]] = data->config.cs;
		al_priv.output_color_range[al_priv.tcon_id[disp]] =
		    data->config.color_range;
	}

	de_update_clk_rate(data->config.de_freq);

	return de_al_mgr_apply(disp, data);
}

int disp_al_manager_sync(unsigned int disp)
{
	return de_al_mgr_sync(disp);
}

int disp_al_manager_update_regs(unsigned int disp)
{
	return de_al_mgr_update_regs(disp);
}

int disp_al_manager_query_irq(unsigned int disp)
{
	return de_al_query_irq(disp);
}

int disp_al_manager_enable_irq(unsigned int disp)
{
	return de_al_enable_irq(disp, 1);
}

int disp_al_manager_disable_irq(unsigned int disp)
{
	return de_al_enable_irq(disp, 0);
}

int disp_al_enhance_apply(unsigned int disp, struct disp_enhance_config *config)
{
#if 0
	if (config->flags & ENH_MODE_DIRTY) {
		struct disp_csc_config csc_config;

		de_dcsc_get_config(disp, &csc_config);
		csc_config.enhance_mode = (config->info.mode >> 16);
		de_dcsc_apply(disp, &csc_config);
	}
#endif
	return de_enhance_apply(disp, config);
}

int disp_al_enhance_update_regs(unsigned int disp)
{
	return de_enhance_update_regs(disp);
}

int disp_al_enhance_sync(unsigned int disp)
{
	return de_enhance_sync(disp);
}

int disp_al_enhance_tasklet(unsigned int disp)
{
	return de_enhance_tasklet(disp);
}

int disp_al_capture_init(unsigned int disp)
{
	de_clk_enable(DE_CLK_WB);
	wb_ebios_dereset(disp);
	return 0;
}

int disp_al_capture_exit(unsigned int disp)
{
	wb_ebios_reset(disp);
	de_clk_disable(DE_CLK_WB);
	return 0;
}

int disp_al_capture_sync(u32 disp)
{
	wb_ebios_update_regs(disp);
	wb_ebios_writeback_enable(disp, 1);

	return 0;
}

int disp_al_capture_apply(unsigned int disp, struct disp_capture_config *cfg)
{
	return wb_ebios_apply(disp, cfg);
}

int disp_al_capture_get_status(unsigned int disp)
{
	return wb_ebios_get_status(disp);
}

int disp_al_smbl_apply(unsigned int disp, struct disp_smbl_info *info)
{
	return de_smbl_apply(disp, info);
}

int disp_al_smbl_update_regs(unsigned int disp)
{
	return de_smbl_update_regs(disp);
}

int disp_al_smbl_sync(unsigned int disp)
{
	return 0;
}

int disp_al_smbl_tasklet(unsigned int disp)
{
	return de_smbl_tasklet(disp);
}

int disp_al_smbl_get_status(unsigned int disp)
{
	return de_smbl_get_status(disp);
}

/* tcon_div >= 6 when
 * lcd_dev[sel]->tcon0_dclk.bits.tcon0_dclk_en = 0xf
 * */
static struct lcd_clk_info clk_tbl[] = {
	{LCD_IF_HV, 6, 1, 1, 0},
	{LCD_IF_CPU, 12, 1, 1, 0},
	{LCD_IF_LVDS, 7, 1, 1, 0},
	{LCD_IF_DSI, 4, 1, 4, 0},
};

/* lcd */
/* lcd_dclk_freq * div -> lcd_clk_freq * div2 -> pll_freq */
/* lcd_dclk_freq * dsi_div -> lcd_dsi_freq */
int disp_al_lcd_get_clk_info(u32 screen_id, struct lcd_clk_info *info,
			     disp_panel_para *panel)
{
	int tcon_div = 6;
	int lcd_div = 1;
	int dsi_div = 4;
	int dsi_rate = 0;
	int i;
	int find = 0;

	if (panel == NULL) {
		__wrn("panel is NULL\n");
		return -1;
	}

	memset(info, 0, sizeof(struct lcd_clk_info));
	for (i = 0; i < sizeof(clk_tbl) / sizeof(struct lcd_clk_info); i++) {
		if (clk_tbl[i].lcd_if == panel->lcd_if) {
			tcon_div = clk_tbl[i].tcon_div;
			lcd_div = clk_tbl[i].lcd_div;
			dsi_div = clk_tbl[i].dsi_div;
			dsi_rate = clk_tbl[i].dsi_rate;
			find = 1;
			break;
		}
	}

#if !defined(DSI_VERSION_28)
	if (panel->lcd_if == LCD_IF_DSI) {
		u32 lane = panel->lcd_dsi_lane;
		u32 bitwidth = 0;

		switch (panel->lcd_dsi_format) {
		case LCD_DSI_FORMAT_RGB888:
			bitwidth = 24;
			break;
		case LCD_DSI_FORMAT_RGB666:
			bitwidth = 24;
			break;
		case LCD_DSI_FORMAT_RGB565:
			bitwidth = 16;
			break;
		case LCD_DSI_FORMAT_RGB666P:
			bitwidth = 18;
			break;
		}

		dsi_div = bitwidth / lane;
	}
#endif /*endif DSI_VERSION_28 */

	if (find == 0)
		__wrn("cant find clk info for lcd_if %d\n", panel->lcd_if);
	if (panel->lcd_if == LCD_IF_HV &&
	    panel->lcd_hv_if == LCD_HV_IF_CCIR656_2CYC &&
	    panel->ccir_clk_div > 0)
		info->tcon_div = panel->ccir_clk_div;
	else if (panel->lcd_tcon_mode == DISP_TCON_DUAL_DSI &&
		 panel->lcd_if == LCD_IF_DSI) {
		info->tcon_div = tcon_div / 2;
		dsi_div /= 2;
	} else
		info->tcon_div = tcon_div;

	info->lcd_div = lcd_div;
	info->dsi_div = dsi_div;
	info->dsi_rate = dsi_rate;

	return 0;
}

int disp_al_lcd_cfg(u32 screen_id, disp_panel_para *panel,
		    panel_extend_para *extend_panel)
{
	struct lcd_clk_info info;

	memset(&info, 0, sizeof(struct lcd_clk_info));
	al_priv.output_type[screen_id] = (u32) DISP_OUTPUT_TYPE_LCD;
	al_priv.output_mode[screen_id] = (u32) panel->lcd_if;
	al_priv.output_fps[screen_id] =
	    panel->lcd_dclk_freq * 1000000 / panel->lcd_ht / panel->lcd_vt;

	de_update_device_fps(al_priv.de_id[screen_id],
			     al_priv.output_fps[screen_id]);

	tcon_init(screen_id);
	disp_al_lcd_get_clk_info(screen_id, &info, panel);
	tcon0_set_dclk_div(screen_id, info.tcon_div);

#if !defined(TCON1_DRIVE_PANEL)
	al_priv.tcon_type[screen_id] = 0;
	if (tcon0_cfg(screen_id, panel) != 0)
		DE_WRN("lcd cfg fail!\n");
	else
		DE_INF("lcd cfg ok!\n");

	tcon0_cfg_ext(screen_id, extend_panel);
	tcon0_src_select(screen_id, LCD_SRC_DE, al_priv.de_id[screen_id]);

	if (panel->lcd_if == LCD_IF_DSI)	{
#if defined(SUPPORT_DSI)
		if (panel->lcd_if == LCD_IF_DSI) {
			if (0 != dsi_cfg(screen_id, panel))
				DE_WRN("dsi %d cfg fail!\n", screen_id);
			if (panel->lcd_tcon_mode == DISP_TCON_DUAL_DSI &&
			    screen_id + 1 < DEVICE_DSI_NUM) {
				if (0 != dsi_cfg(screen_id + 1, panel))
					DE_WRN("dsi %d cfg fail!\n",
					       screen_id + 1);
			}
		}
#endif
	}
#else

	/* There is no tcon0 on this platform,
	 * At fpga period, we can use tcon1 to driver lcd pnael,
	 * so, here we need to config tcon1 here.
	 */
	al_priv.tcon_type[screen_id] = 1;
	if (tcon1_cfg_ex(screen_id, panel) != 0)
		DE_WRN("lcd cfg fail!\n");
	else
		DE_INF("lcd cfg ok!\n");
#endif
	return 0;
}

int disp_al_lcd_cfg_ext(u32 screen_id, panel_extend_para *extend_panel)
{
	tcon0_cfg_ext(screen_id, extend_panel);

	return 0;
}

int disp_al_lcd_enable(u32 screen_id, disp_panel_para *panel)
{
#if !defined(TCON1_DRIVE_PANEL)

	tcon0_open(screen_id, panel);
	if (panel->lcd_if == LCD_IF_LVDS) {
		lvds_open(screen_id, panel);
	} else if (panel->lcd_if == LCD_IF_DSI) {
#if defined(SUPPORT_DSI)
		dsi_open(screen_id, panel);
		if (panel->lcd_tcon_mode == DISP_TCON_DUAL_DSI &&
		    screen_id + 1 < DEVICE_DSI_NUM)
			dsi_open(screen_id + 1, panel);
#endif
	}

#else
	/* There is no tcon0 on this platform,
	 * At fpga period, we can use tcon1 to driver lcd pnael,
	 * so, here we need to open tcon1 here.
	 */
	tcon1_open(screen_id);
#endif

	return 0;
}

int disp_al_lcd_disable(u32 screen_id, disp_panel_para *panel)
{
	al_priv.output_type[screen_id] = (u32) DISP_OUTPUT_TYPE_NONE;

#if !defined(TCON1_DRIVE_PANEL)

	if (panel->lcd_if == LCD_IF_LVDS) {
		lvds_close(screen_id);
	} else if (panel->lcd_if == LCD_IF_DSI) {
#if defined(SUPPORT_DSI)
		dsi_close(screen_id);
		if (panel->lcd_tcon_mode == DISP_TCON_DUAL_DSI &&
		    screen_id + 1 < DEVICE_DSI_NUM)
			dsi_close(screen_id + 1);
#endif
	}
	tcon0_close(screen_id);
#else
	/* There is no tcon0 on platform sun50iw2,
	 * on fpga period, we can use tcon1 to driver lcd pnael,
	 * so, here we need to close tcon1.
	 */
	tcon1_close(screen_id);
#endif
	tcon_exit(screen_id);

	return 0;
}

/* query lcd irq, clear it when the irq queried exist
 */
int disp_al_lcd_query_irq(u32 screen_id, enum __lcd_irq_id_t irq_id,
			  disp_panel_para *panel)
{
#if defined(SUPPORT_DSI) && defined(DSI_VERSION_40)
	if (panel->lcd_if == LCD_IF_DSI) {
		enum __dsi_irq_id_t dsi_irq =
		    (irq_id == LCD_IRQ_TCON0_VBLK) ?
		    DSI_IRQ_VIDEO_VBLK : DSI_IRQ_VIDEO_LINE;

		return dsi_irq_query(screen_id, dsi_irq);
	}
#endif
	return tcon_irq_query(screen_id,
			      (al_priv.tcon_type[screen_id] == 0) ?
			      irq_id : LCD_IRQ_TCON1_VBLK);
}

int disp_al_lcd_enable_irq(u32 screen_id, enum __lcd_irq_id_t irq_id,
			   disp_panel_para *panel)
{
	int ret = 0;

#if defined(SUPPORT_DSI) && defined(DSI_VERSION_40)
	if (panel->lcd_if == LCD_IF_DSI) {
		enum __dsi_irq_id_t dsi_irq =
		    (irq_id == LCD_IRQ_TCON0_VBLK) ?
		    DSI_IRQ_VIDEO_VBLK : DSI_IRQ_VIDEO_LINE;

		ret = dsi_irq_enable(screen_id, dsi_irq);
	}
#endif
	ret =
	    tcon_irq_enable(screen_id,
			    (al_priv.tcon_type[screen_id] == 0) ?
			    irq_id : LCD_IRQ_TCON1_VBLK);

	return ret;
}

int disp_al_lcd_disable_irq(u32 screen_id, enum __lcd_irq_id_t irq_id,
			    disp_panel_para *panel)
{
	int ret = 0;

#if defined(SUPPORT_DSI) && defined(DSI_VERSION_40)
	if (panel->lcd_if == LCD_IF_DSI) {
		enum __dsi_irq_id_t dsi_irq =
		    (irq_id == LCD_IRQ_TCON0_VBLK) ?
		    DSI_IRQ_VIDEO_VBLK : DSI_IRQ_VIDEO_LINE;

		ret = dsi_irq_disable(screen_id, dsi_irq);
	}
#endif
	ret =
	    tcon_irq_disable(screen_id,
			     (al_priv.tcon_type[screen_id] ==
			      0) ? irq_id : LCD_IRQ_TCON1_VBLK);

	return ret;
}

int disp_al_lcd_tri_busy(u32 screen_id, disp_panel_para *panel)
{
	int busy = 0;
	int ret = 0;

	busy |= tcon0_tri_busy(screen_id);
#if defined(SUPPORT_DSI)
	busy |= dsi_inst_busy(screen_id);
#endif
	ret = (busy == 0) ? 0 : 1;

	return ret;
}

/* take dsi irq s32o account, todo? */
int disp_al_lcd_tri_start(u32 screen_id, disp_panel_para *panel)
{
#if defined(SUPPORT_DSI)
	if (panel->lcd_if == LCD_IF_DSI)
		dsi_tri_start(screen_id);
#endif
	return tcon0_tri_start(screen_id);
}

int disp_al_lcd_io_cfg(u32 screen_id, u32 enable, disp_panel_para *panel)
{
#if defined(SUPPORT_DSI)
	if (panel->lcd_if == LCD_IF_DSI) {
		if (enable == 1) {
			dsi_io_open(screen_id, panel);
			if (panel->lcd_tcon_mode == DISP_TCON_DUAL_DSI &&
			    screen_id + 1 < DEVICE_DSI_NUM)
				dsi_io_open(screen_id + 1, panel);
		} else {
			dsi_io_close(screen_id);
			if (panel->lcd_tcon_mode == DISP_TCON_DUAL_DSI &&
			    screen_id + 1 < DEVICE_DSI_NUM)
				dsi_io_close(screen_id + 1);
		}
	}
#endif

	return 0;
}

int disp_al_lcd_get_cur_line(u32 screen_id, disp_panel_para *panel)
{
#if defined(SUPPORT_DSI) && defined(DSI_VERSION_40)
	if (panel->lcd_if == LCD_IF_DSI)
		return dsi_get_cur_line(screen_id);
#endif
	return tcon_get_cur_line(screen_id,
				 al_priv.tcon_type[screen_id]);
}

int disp_al_lcd_get_start_delay(u32 screen_id, disp_panel_para *panel)
{
#if defined(SUPPORT_DSI) && defined(DSI_VERSION_40)
	u32 lcd_start_delay = 0;
	u32 de_clk_rate = de_get_clk_rate() / 1000000;
	if (panel) {
		lcd_start_delay = ((tcon0_get_cpu_tri2_start_delay(screen_id)+1)
				  << 3) * (panel->lcd_dclk_freq)
				  / (panel->lcd_ht*de_clk_rate);
	}
	if (LCD_IF_DSI == panel->lcd_if)
		return dsi_get_start_delay(screen_id)+lcd_start_delay;
	else
#endif
	return tcon_get_start_delay(screen_id,
				    al_priv.tcon_type[screen_id]);
}

/* hdmi */
int disp_al_hdmi_enable(u32 screen_id)
{
	tcon1_hdmi_clk_enable(screen_id, 1);

	tcon1_open(screen_id);
	return 0;
}

int disp_al_hdmi_disable(u32 screen_id)
{
	al_priv.output_type[screen_id] = (u32) DISP_OUTPUT_TYPE_NONE;

	tcon1_close(screen_id);
	tcon_exit(screen_id);
	tcon1_hdmi_clk_enable(screen_id, 0);

	return 0;
}

int disp_al_hdmi_cfg(u32 screen_id, struct disp_video_timings *video_info)
{
	struct disp_video_timings *timings = NULL;

	al_priv.output_type[screen_id] = (u32) DISP_OUTPUT_TYPE_HDMI;
	al_priv.output_mode[screen_id] = (u32) video_info->vic;
	al_priv.output_fps[screen_id] =
	    video_info->pixel_clk / video_info->hor_total_time /
	    video_info->ver_total_time * (video_info->b_interlace +
					  1) / (video_info->trd_mode + 1);
	al_priv.tcon_type[screen_id] = 1;

	de_update_device_fps(al_priv.de_id[screen_id],
			     al_priv.output_fps[screen_id]);
	timings = kmalloc(sizeof(struct disp_video_timings),
			  GFP_KERNEL | __GFP_ZERO);
	if (timings) {
		memcpy(timings, video_info, sizeof(struct disp_video_timings));
		if (al_priv.output_format[screen_id] == DISP_CSC_TYPE_YUV420) {
			timings->x_res /= 2;
			timings->hor_total_time /= 2;
			timings->hor_back_porch /= 2;
			timings->hor_front_porch /= 2;
			timings->hor_sync_time /= 2;
		}
	} else {
		__wrn("malloc memory for timings fail! size=0x%x\n",
		      (unsigned int)sizeof(struct disp_video_timings));
	}
	tcon_init(screen_id);

	if (timings)
		tcon1_set_timming(screen_id, timings);
	else
		tcon1_set_timming(screen_id, video_info);
	tcon1_src_select(screen_id, LCD_SRC_DE, al_priv.de_id[screen_id]);

	kfree(timings);

	return 0;
}

/* tv */
int disp_al_tv_enable(u32 screen_id)
{
	tcon1_tv_clk_enable(screen_id, 1);
	tcon1_open(screen_id);

	return 0;
}

int disp_al_tv_disable(u32 screen_id)
{
	al_priv.output_type[screen_id] = (u32) DISP_OUTPUT_TYPE_NONE;

	tcon1_close(screen_id);
	tcon_exit(screen_id);
	tcon1_tv_clk_enable(screen_id, 0);

	return 0;
}

#if defined(SUPPORT_EDP)
int disp_al_edp_cfg(u32 screen_id, u32 fps, u32 edp_index)
{
	al_priv.output_type[screen_id] = (u32) DISP_OUTPUT_TYPE_EDP;
	al_priv.output_mode[screen_id] = 6;
	al_priv.output_fps[screen_id] = fps;
	de_update_device_fps(al_priv.de_id[screen_id], fps);
	al_priv.tcon_type[screen_id] = 0;
	edp_de_attach(edp_index, al_priv.de_id[screen_id]);
	return 0;
}

int disp_al_edp_disable(u32 screen_id)
{
	al_priv.output_type[screen_id] = (u32)DISP_OUTPUT_TYPE_NONE;
	return 0;
}
#endif

int disp_al_tv_cfg(u32 screen_id, struct disp_video_timings *video_info)
{
	al_priv.output_type[screen_id] = (u32) DISP_OUTPUT_TYPE_TV;
	al_priv.output_mode[screen_id] = (u32) video_info->tv_mode;
	al_priv.output_fps[screen_id] =
	    video_info->pixel_clk / video_info->hor_total_time
	    / video_info->ver_total_time;
	al_priv.tcon_type[screen_id] = 1;

	de_update_device_fps(al_priv.de_id[screen_id],
			     al_priv.output_fps[screen_id]);

	tcon_init(screen_id);
/*#if defined(HAVE_DEVICE_COMMON_MODULE)*/
	/*rgb_src_sel(screen_id);*/
/*#endif*/
	tcon1_set_timming(screen_id, video_info);
	tcon1_yuv_range(screen_id, 1);
	tcon1_src_select(screen_id, LCD_SRC_DE, al_priv.de_id[screen_id]);

	return 0;
}

#if defined(SUPPORT_VGA)
/* vga interface
 */
int disp_al_vga_enable(u32 screen_id)
{
	tcon1_open(screen_id);

	return 0;
}

int disp_al_vga_disable(u32 screen_id)
{
	al_priv.output_type[screen_id] = (u32) DISP_OUTPUT_TYPE_NONE;

	tcon1_close(screen_id);
	tcon_exit(screen_id);

	return 0;
}

int disp_al_vga_cfg(u32 screen_id, struct disp_video_timings *video_info)
{
	al_priv.output_type[screen_id] = (u32) DISP_OUTPUT_TYPE_VGA;
	al_priv.output_mode[screen_id] = (u32) video_info->tv_mode;
	al_priv.output_fps[screen_id] =
	    video_info->pixel_clk / video_info->hor_total_time
	    / video_info->ver_total_time;
	al_priv.tcon_type[screen_id] = 1;

	de_update_device_fps(al_priv.de_id[screen_id],
			     al_priv.output_fps[screen_id]);

	tcon_init(screen_id);
	tcon1_set_timming(screen_id, video_info);
	tcon1_src_select(screen_id, LCD_SRC_DE, al_priv.de_id[screen_id]);

	return 0;
}
#endif

int disp_al_vdevice_cfg(u32 screen_id, struct disp_video_timings *video_info,
			struct disp_vdevice_interface_para *para,
			u8 config_tcon_only)
{
	struct lcd_clk_info clk_info;
	disp_panel_para info;

	if (para->sub_intf == LCD_HV_IF_CCIR656_2CYC)
		al_priv.output_type[screen_id] = (u32) DISP_OUTPUT_TYPE_TV;
	else
		al_priv.output_type[screen_id] = (u32) DISP_OUTPUT_TYPE_LCD;
	al_priv.output_mode[screen_id] = (u32) para->intf;
	al_priv.output_fps[screen_id] =
	    video_info->pixel_clk / video_info->hor_total_time
	    / video_info->ver_total_time;
	al_priv.tcon_type[screen_id] = 0;

	de_update_device_fps(al_priv.de_id[screen_id],
			     al_priv.output_fps[screen_id]);

	memset(&info, 0, sizeof(disp_panel_para));
	info.lcd_if = para->intf;
	info.lcd_x = video_info->x_res;
	info.lcd_y = video_info->y_res;
	info.lcd_hv_if = (disp_lcd_hv_if) para->sub_intf;
	info.lcd_dclk_freq = video_info->pixel_clk;
	info.lcd_ht = video_info->hor_total_time;
	info.lcd_hbp = video_info->hor_back_porch + video_info->hor_sync_time;
	info.lcd_hspw = video_info->hor_sync_time;
	info.lcd_vt = video_info->ver_total_time;
	info.lcd_vbp = video_info->ver_back_porch + video_info->ver_sync_time;
	info.lcd_vspw = video_info->ver_sync_time;
	info.lcd_interlace = video_info->b_interlace;
	info.lcd_hv_syuv_fdly = para->fdelay;
	info.lcd_hv_clk_phase = para->clk_phase;
	info.lcd_hv_sync_polarity = para->sync_polarity;
	info.ccir_clk_div = para->ccir_clk_div;
	info.input_csc = para->input_csc;

	if (info.lcd_hv_if == LCD_HV_IF_CCIR656_2CYC)
		info.lcd_hv_syuv_seq = para->sequence;
	else
		info.lcd_hv_srgb_seq = para->sequence;
	tcon_init(screen_id);
	disp_al_lcd_get_clk_info(screen_id, &clk_info, &info);
	tcon0_set_dclk_div(screen_id, clk_info.tcon_div);

	if (para->sub_intf == LCD_HV_IF_CCIR656_2CYC)
		tcon1_yuv_range(screen_id, 1);
	if (tcon0_cfg(screen_id, &info) != 0)
		DE_WRN("lcd cfg fail!\n");
	else
		DE_INF("lcd cfg ok!\n");

	if (!config_tcon_only)
		tcon0_src_select(screen_id, LCD_SRC_DE,
				 al_priv.de_id[screen_id]);
	else
		DE_INF("%s:config_tcon_only is %d\n", __func__,
		       config_tcon_only);

	return 0;
}

int disp_al_vdevice_enable(u32 screen_id)
{
	disp_panel_para panel;

	memset(&panel, 0, sizeof(disp_panel_para));
	panel.lcd_if = LCD_IF_HV;
	tcon0_open(screen_id, &panel);

	return 0;
}

int disp_al_vdevice_disable(u32 screen_id)
{
	al_priv.output_type[screen_id] = (u32) DISP_OUTPUT_TYPE_NONE;

	tcon0_close(screen_id);
	tcon_exit(screen_id);

	return 0;
}

/* screen_id: used for index of manager */
int disp_al_device_get_cur_line(u32 screen_id)
{
	u32 tcon_type = al_priv.tcon_type[screen_id];

	return tcon_get_cur_line(screen_id, tcon_type);
}

int disp_al_device_get_start_delay(u32 screen_id)
{
	u32 tcon_type = al_priv.tcon_type[screen_id];

	tcon_type = (al_priv.tcon_type[screen_id] == 0) ? 0 : 1;
	return tcon_get_start_delay(screen_id, tcon_type);
}

int disp_al_device_query_irq(u32 screen_id)
{
	int ret = 0;
	int irq_id = 0;

	irq_id = (al_priv.tcon_type[screen_id] == 0) ?
	    LCD_IRQ_TCON0_VBLK : LCD_IRQ_TCON1_VBLK;
	ret = tcon_irq_query(screen_id, irq_id);

	return ret;
}

int disp_al_device_enable_irq(u32 screen_id)
{
	int ret = 0;
	int irq_id = 0;

	irq_id = (al_priv.tcon_type[screen_id] == 0) ?
	    LCD_IRQ_TCON0_VBLK : LCD_IRQ_TCON1_VBLK;
	ret = tcon_irq_enable(screen_id, irq_id);

	return ret;
}

int disp_al_device_disable_irq(u32 screen_id)
{
	int ret = 0;
	int irq_id = 0;

	irq_id = (al_priv.tcon_type[screen_id] == 0) ?
	    LCD_IRQ_TCON0_VBLK : LCD_IRQ_TCON1_VBLK;
	ret = tcon_irq_disable(screen_id, irq_id);

	return ret;
}

int disp_al_device_get_status(u32 screen_id)
{
	int ret = 0;

	ret = tcon_get_status(screen_id, al_priv.tcon_type[screen_id]);

	return ret;
}

int disp_al_device_src_select(u32 screen_id, u32 src)
{
	int ret = 0;

	return ret;
}

void *disp_malloc(int, void *);
int disp_init_al(disp_bsp_init_para *para)
{
	int i;

	memset(&al_priv, 0, sizeof(struct disp_al_private_data));
	de_al_init(para);
	de_enhance_init(para);
	de_ccsc_init(para);
	wb_ebios_init(para);
	de_clk_set_reg_base(para->reg_base[DISP_MOD_DE]);

	for (i = 0; i < DEVICE_NUM; i++)
		tcon_set_reg_base(i, para->reg_base[DISP_MOD_LCD0 + i]);

	for (i = 0; i < DE_NUM; i++) {
		if (de_feat_is_support_smbl(i))
			de_smbl_init(i, para->reg_base[DISP_MOD_DE]);
	}

#if defined(HAVE_DEVICE_COMMON_MODULE)
	tcon_top_set_reg_base(0, para->reg_base[DISP_MOD_DEVICE]);
#endif
#if defined(SUPPORT_DSI)
#if defined(DEVICE_DSI_NUM)
	for (i = 0; i < DEVICE_DSI_NUM; ++i)
		dsi_set_reg_base(i, para->reg_base[DISP_MOD_DSI0 + i]);
#else
	dsi_set_reg_base(0, para->reg_base[DISP_MOD_DSI0]);
#endif /*endif DEVICE_DSI_NUM */
#endif

	if (para->boot_info.sync == 1) {
		u32 disp = para->boot_info.disp;
		u32 tcon_id;
		struct disp_video_timings tt;

		memset(&tt, 0, sizeof(struct disp_video_timings));
#if defined(HAVE_DEVICE_COMMON_MODULE)
		al_priv.tcon_id[disp] = tcon_get_attach_by_de_index(disp);
#else
		al_priv.tcon_id[disp] = de_rtmx_get_mux(disp);
#endif
		tcon_id = al_priv.tcon_id[disp];

		/*
		 * should take care about this,
		 * extend display treated as a LCD OUTPUT
		 */
		al_priv.output_type[tcon_id] = para->boot_info.type;
		al_priv.output_mode[tcon_id] = para->boot_info.mode;
		al_priv.tcon_type[tcon_id] = 0;
#if defined(SUPPORT_HDMI)
		if (para->boot_info.type == DISP_OUTPUT_TYPE_HDMI) {
			tcon_id = tcon_id - 1;
			al_priv.tcon_type[tcon_id] = 1;
		}
#endif
#if defined(SUPPORT_TV)
		al_priv.tcon_type[tcon_id] =
		    (para->boot_info.type == DISP_OUTPUT_TYPE_TV) ?
		    1 : al_priv.tcon_type[tcon_id];
#if defined(CONFIG_DISP2_TV_AC200)
		al_priv.tcon_type[tcon_id] =
		    (para->boot_info.type == DISP_OUTPUT_TYPE_TV)
			? 0
			: al_priv.tcon_type[tcon_id];
#endif /*endif CONFIG_DISP2_TV_AC200 */
#endif

		de_rtmx_sync_hw(disp);
		de_rtmx_get_display_size(disp, &al_priv.disp_size[disp].width,
					 &al_priv.disp_size[disp].height);

		al_priv.output_fps[tcon_id] = 60;
		de_update_device_fps(disp, al_priv.output_fps[tcon_id]);
	}

	return 0;
}

int disp_al_get_fb_info(unsigned int sel, struct disp_layer_info *info)
{
	return 0;
}

int disp_al_get_display_size(unsigned int screen_id, unsigned int *width,
			     unsigned int *height)
{
	*width = al_priv.disp_size[screen_id].width;
	*height = al_priv.disp_size[screen_id].height;

	return 0;
}

unsigned int disp_al_dump(unsigned int screen_id, char *buf, unsigned int flags)
{
	unsigned int count = 0;

	count += de_hal_dump(screen_id, buf + count, flags);

	return count;
}
