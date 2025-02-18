
/*
 * header for cameras.
 *
 * Copyright (c) 2017 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zhao Wei <zhaowei@allwinnertech.com>
 *	Yang Feng <yangfeng@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CAMERA__H__
#define __CAMERA__H__

#include <media/v4l2-subdev.h>
#include <linux/videodev2.h>
#include "../../vin-video/vin_core.h"
#include "../../utility/vin_supply.h"
#include "../../vin-cci/cci_helper.h"
#include "camera_cfg.h"
#include "../../platform/platform_cfg.h"
/*
 * Basic window sizes.  These probably belong somewhere more globally
 * useful.
 */
#define ABS_SENSOR(x)                 ((x) > 0 ? (x) : -(x))

#define HXGA_WIDTH    4000
#define HXGA_HEIGHT   3000
#define QUXGA_WIDTH   3264
#define QUXGA_HEIGHT  2448
#define QSXGA_WIDTH   2592
#define QSXGA_HEIGHT  1936
#define QXGA_WIDTH    2048
#define QXGA_HEIGHT   1536
#define HD1080_WIDTH  1920
#define HD1080_HEIGHT 1080
#define UXGA_WIDTH    1600
#define UXGA_HEIGHT   1200
#define SXGA_WIDTH    1280
#define SXGA_HEIGHT   960
#define HD720_WIDTH   1280
#define HD720_HEIGHT  720
#define XGA_WIDTH     1024
#define XGA_HEIGHT    768
#define SVGA_WIDTH    800
#define SVGA_HEIGHT   600
#define VGA_WIDTH     640
#define VGA_HEIGHT    480
#define QVGA_WIDTH    320
#define QVGA_HEIGHT   240
#define CIF_WIDTH     352
#define CIF_HEIGHT    288
#define QCIF_WIDTH    176
#define QCIF_HEIGHT   144

#define CSI_GPIO_HIGH     1
#define CSI_GPIO_LOW     0
#define CCI_BITS_8           8
#define CCI_BITS_16         16
#define SENSOR_MAGIC_NUMBER 0x156977

struct sensor_format_struct {
	__u8 *desc;
	enum v4l2_mbus_pixelcode mbus_code;
	struct regval_list *regs;
	int regs_size;
	int bpp; /* Bytes per pixel */
};

struct sensor_info {
	struct v4l2_subdev sd;
	struct media_pad sensor_pads[SENSOR_PAD_NUM];
	struct mutex lock;
	struct sensor_format_struct *fmt;	/* Current format */
	struct sensor_win_size *current_wins;
	struct sensor_format_struct *fmt_pt;	/* format start */
	struct sensor_win_size *win_pt;		/* win start */
	unsigned int width;
	unsigned int height;
	unsigned int use_current_win;
	unsigned int capture_mode;	/*V4L2_MODE_VIDEO/V4L2_MODE_IMAGE*/
	unsigned int af_first_flag;
	unsigned int preview_first_flag;
	unsigned int auto_focus;	/*0:not in contin_focus 1: contin_focus*/
	unsigned int focus_status;	/*0:idle 1:busy*/
	unsigned int low_speed;		/*0:high speed 1:low speed*/
	int brightness;
	int contrast;
	int saturation;
	int hue;
	unsigned int hflip;
	unsigned int vflip;
	unsigned int gain;
	unsigned int autogain;
	unsigned int exp;
	int exp_bias;
	enum v4l2_exposure_auto_type autoexp;
	unsigned int autowb;
	enum v4l2_auto_n_preset_white_balance wb;
	enum v4l2_colorfx clrfx;
	enum v4l2_flash_led_mode flash_mode;
	enum v4l2_power_line_frequency band_filter;
	struct v4l2_fract tpf;
	unsigned int stream_seq;
	unsigned int fmt_num;
	unsigned int win_size_num;
	unsigned int sensor_field;
	unsigned int combo_mode;
	unsigned int isp_wdr_mode;
	unsigned int magic_num;
};

#endif /*__CAMERA__H__*/
