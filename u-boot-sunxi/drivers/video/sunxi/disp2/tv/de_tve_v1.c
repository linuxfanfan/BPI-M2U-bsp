#include "drv_tv.h"
#include "de_tvec.h"

static void __iomem *tve_reg_base[TVE_DEVICE_NUM];
static void __iomem *sid_reg_base;
static s32 tve_low_dac_auto_cali(u32 sel, u32 cali);

s32 tve_low_set_reg_base(u32 sel, void __iomem *address)
{
	if (sel < TVE_DEVICE_NUM)
		tve_reg_base[sel] = address;

	return 0;
}

s32 tve_low_set_top_reg_base(void __iomem *address)
{
	return 0;
}

s32 tve_low_set_sid_base(void __iomem *address)
{
	sid_reg_base = address;

	return 0;
}

s32 tve_low_init(u32 sel, u32 *dac_no, u32 *cali, s32 *offset,
		 u32 *dac_type, u32 num)
{
	u32 val = (*cali) ? (*cali) : 0x285;

	TVE_WUINT32(sel, TVE_304, val << 16);
	TVE_WUINT32(sel, TVE_30C, 0x00101110);
	TVE_WUINT32(sel, TVE_008, 0x433e12b1);
	TVE_WUINT32(sel, TVE_000, 0x80000000);

	return 0;
}

s32 tve_low_exit(u32 sel)
{
	return 0;
}

s32 tve_low_dac_enable(u32 sel)
{
	TVE_SET_BIT(sel, TVE_008, 0x1 << 0);

	return 0;
}

s32 tve_low_dac_disable(u32 sel)
{
	TVE_CLR_BIT(sel, TVE_008, 0x1 << 0);

	return 0;
}

s32 tve_low_open(u32 sel)
{
	TVE_CLR_BIT(sel, TVE_000, 0x1 << 31);
	TVE_SET_BIT(sel, TVE_000, 0x1 << 0);

	return 0;
}

s32 tve_low_close(u32 sel)
{
	TVE_CLR_BIT(sel, TVE_000, 0x1 << 0);
	TVE_SET_BIT(sel, TVE_000, 0x1 << 31);

	return 0;
}

s32 tve_low_set_tv_mode(u32 sel, enum disp_tv_mode mode, u32 cali)
{
	tve_low_dac_auto_cali(sel, cali);
	if (DISP_TV_MOD_NTSC == mode) {
		TVE_WUINT32(sel, TVE_000, 0x00000300);
		TVE_WUINT32(sel, TVE_004, 0x07070000);
		TVE_WUINT32(sel, TVE_00C, 0x30001400);
		TVE_WUINT32(sel, TVE_010, 0x21F07C1F);
		TVE_WUINT32(sel, TVE_014, 0x00760020);
		TVE_WUINT32(sel, TVE_018, 0x00000016);
		TVE_WUINT32(sel, TVE_01C, 0x0016020D);
		TVE_WUINT32(sel, TVE_020, 0x00F0011A);
		TVE_WUINT32(sel, TVE_100, 0x00000001);
		TVE_WUINT32(sel, TVE_104, 0x00000000);
		TVE_WUINT32(sel, TVE_108, 0x00000002);
		TVE_WUINT32(sel, TVE_10C, 0x0000004F);
		TVE_WUINT32(sel, TVE_110, 0x00000000);
		TVE_WUINT32(sel, TVE_114, 0x0016447E);
		TVE_WUINT32(sel, TVE_118, 0x0000A0A0);
		TVE_WUINT32(sel, TVE_11C, 0x001000F0);
		TVE_WUINT32(sel, TVE_120, 0x01E80320);
		TVE_WUINT32(sel, TVE_124, 0x000005A0);
		TVE_WUINT32(sel, TVE_128, 0x00010000);
		TVE_WUINT32(sel, TVE_12C, 0x00000101);
		TVE_WUINT32(sel, TVE_130, 0x20050368);
		TVE_WUINT32(sel, TVE_134, 0x00000000);
		TVE_WUINT32(sel, TVE_138, 0x00000000);
		TVE_WUINT32(sel, TVE_13C, 0x00000000);
	} else {
		TVE_WUINT32(sel, TVE_000, 0x00000300);
		TVE_WUINT32(sel, TVE_004, 0x07070001);
		TVE_WUINT32(sel, TVE_00C, 0x30001400);
		TVE_WUINT32(sel, TVE_010, 0x2A098ACB);
		TVE_WUINT32(sel, TVE_014, 0x008A0018);
		TVE_WUINT32(sel, TVE_018, 0x00000016);
		TVE_WUINT32(sel, TVE_01C, 0x00160271);
		TVE_WUINT32(sel, TVE_020, 0x00FC00FC);
		TVE_WUINT32(sel, TVE_100, 0x00000000);
		TVE_WUINT32(sel, TVE_104, 0x00000001);
		TVE_WUINT32(sel, TVE_108, 0x00000005);
		TVE_WUINT32(sel, TVE_10C, 0x00002929);
		TVE_WUINT32(sel, TVE_110, 0x00000000);
		TVE_WUINT32(sel, TVE_114, 0x0016447E);
		TVE_WUINT32(sel, TVE_118, 0x0000A8A8);
		TVE_WUINT32(sel, TVE_11C, 0x001000FC);
		TVE_WUINT32(sel, TVE_120, 0x01E80320);
		TVE_WUINT32(sel, TVE_124, 0x000005A0);
		TVE_WUINT32(sel, TVE_128, 0x00010000);
		TVE_WUINT32(sel, TVE_12C, 0x00000101);
		TVE_WUINT32(sel, TVE_130, 0x2005000A);
		TVE_WUINT32(sel, TVE_134, 0x00000000);
		TVE_WUINT32(sel, TVE_138, 0x00000000);
		TVE_WUINT32(sel, TVE_13C, 0x00000000);
		TVE_WUINT32(sel, TVE_3A0, 0x00030001);
	}

	return 0;
}

/* 0:unconnected; 1:connected; 3:short to ground */
s32 tve_low_get_dac_status(u32 sel)
{
	u32 readval;

	readval = TVE_RUINT32(sel, TVE_038);

	return readval & 0x3;
}

s32 tve_low_dac_autocheck_enable(u32 sel)
{
	TVE_WUINT32(sel, TVE_0F8, 0x00000280);
	/* 20ms x 10 */
	TVE_WUINT32(sel, TVE_0FC, 0x028F00FF);
	/* 1.0v refer for 0.71v/1.43v detect */
	TVE_WUINT32(sel, TVE_03C, 0x00000009);
	/* detect enable */
	TVE_WUINT32(sel, TVE_030, 0x00000001);

	return 0;
}

s32 tve_low_dac_autocheck_disable(u32 sel)
{
	TVE_WUINT32(sel, TVE_030, 0x0);
	TVE_WUINT32(sel, TVE_0F8, 0x0);

	return 0;
}

static s32 tve_low_dac_auto_cali(u32 sel, u32 cali)
{
	u32 dac_err_reg[10];
	s32 dac_err_val[10];
	u32 cali_err_1[10];
	u32 cali_err_2[10];
	s32 dac_err_auto;
	s32 dac_err_efuse;
	s32 dac_err_opti;
	u32 dac_err_set;
	u32 i;

	TVE_WUINT32(sel, TVE_000, 0x80000300); /* tvclk enable */
	TVE_WUINT32(sel, TVE_030, 0x00000000); /* auto detect disable */
	TVE_WUINT32(sel, TVE_008, 0x433e12b1); /* dac setting */

	TVE_WUINT32(sel, TVE_304, cali << 16 | 0xc << 8);
	TVE_SET_BIT(sel, TVE_300, 0x1);	/* force DAC for cali */
	for (i = 0; i < 3; i++) {
		TVE_SET_BIT(sel, TVE_304, 0x1 << 4);
		mdelay(1);
		TVE_SET_BIT(sel, TVE_304, 0x1 << 0);
		mdelay(1);
		dac_err_reg[i] = (TVE_RUINT32(sel, TVE_308) >> 16) & 0x3ff;
		TVE_CLR_BIT(sel, TVE_304, 0x1 << 0);
		TVE_CLR_BIT(sel, TVE_304, 0x1 << 4);
		mdelay(1);
		if (dac_err_reg[i] & (1 << 9))
			dac_err_val[i] = 0 + (dac_err_reg[i] & 0x1ff);
		else
			dac_err_val[i] = 0 - (dac_err_reg[i] & 0x1ff);
	}

	cali_err_1[0] = abs(dac_err_val[1] - dac_err_val[0]);
	cali_err_1[1] = abs(dac_err_val[2] - dac_err_val[1]);
	cali_err_1[2] = abs(dac_err_val[0] - dac_err_val[2]);

	cali_err_2[0] = cali_err_1[2] + cali_err_1[0];
	cali_err_2[1] = cali_err_1[0] + cali_err_1[1];
	cali_err_2[2] = cali_err_1[1] + cali_err_1[2];

	if (cali_err_2[0] < cali_err_2[1] && cali_err_2[0] < cali_err_2[2])
		dac_err_auto = dac_err_val[0];
	else if (cali_err_2[1] < cali_err_2[0] && cali_err_2[1] < cali_err_2[2])
		dac_err_auto = dac_err_val[1];
	else
		dac_err_auto = dac_err_val[2];

	if (cali & (1 << 9))
		dac_err_efuse = 0 + (cali & 0x1ff);
	else
		dac_err_efuse = 0 - (cali & 0x1ff);

	if (abs(dac_err_auto - dac_err_efuse) < 100)
		dac_err_opti = dac_err_auto;
	else
		dac_err_opti = dac_err_efuse;

	if (dac_err_opti >= 0)
		dac_err_set = (1 << 9) | dac_err_opti;
	else
		dac_err_set = 0 - dac_err_opti;

	TVE_WUINT32(sel, TVE_304, dac_err_set << 16 | 0xc << 8);
	TVE_CLR_BIT(sel, TVE_300, 0x1);
	TVE_WUINT32(sel, TVE_030, 0x1);	/* enable plug detect */

	return 0;
}

s32 tve_low_enhance(u32 sel, u32 mode)
{
	if (0 == mode) {
		TVE_CLR_BIT(sel, TVE_000, 0xf << 10);	/* deflick off */
		TVE_SET_BIT(sel, TVE_000, 0x5 << 10);	/* deflick is 5 */
		TVE_SET_BIT(sel, TVE_00C, 0x1 << 31);	/* lti on */
		TVE_SET_BIT(sel, TVE_00C, 0x1 << 16);	/* notch off */
	} else if (1 == mode) {
		TVE_CLR_BIT(sel, TVE_000, 0xf << 10);	/* deflick off */
		TVE_SET_BIT(sel, TVE_000, 0x5 << 10);	/* deflick is 5 */
		TVE_SET_BIT(sel, TVE_00C, 0x1 << 31);	/* lti on */
		TVE_CLR_BIT(sel, TVE_00C, 0x1 << 16);	/* notch on */
	} else if (2 == mode) {
		TVE_CLR_BIT(sel, TVE_000, 0xf << 10);	/* deflick off */
		TVE_CLR_BIT(sel, TVE_00C, 0x1 << 31);	/* lti off */
		TVE_SET_BIT(sel, TVE_00C, 0x1 << 16);	/* notch off */
	}

	return 0;
}

u32 tve_low_get_sid(u32 index)
{
	u32 cali_value = 0;

	if (sid_reg_base) {
		cali_value = readl(sid_reg_base + index);
		cali_value &= 0x000003ff;
	}

	return cali_value;
}
