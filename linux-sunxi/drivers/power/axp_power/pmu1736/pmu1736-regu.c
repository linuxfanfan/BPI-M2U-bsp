/*
 * Regulators driver for allwinnertech pmu1736
 *
 * Copyright (C) 2014 allwinnertech Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/module.h>
#include <linux/power/axp_depend.h>
#include "../axp-core.h"
#include "../axp-regulator.h"
#include "pmu1736.h"
#include "pmu1736-regu.h"

/* Reverse engineered partly from Platformx drivers */
enum axp_regls {
	VCC_DCDC1,
	VCC_DCDC2,
	VCC_DCDC3,
	VCC_DCDC4,
	VCC_DCDC5,
	VCC_DCDC6,
	VCC_LDO1,
	VCC_LDO2,
	VCC_LDO3,
	VCC_LDO4,
	VCC_LDO5,
	VCC_LDO6,
	VCC_LDO7,
	VCC_LDO8,
	VCC_LDO9,
	VCC_LDO10,
	VCC_LDO11,
	VCC_LDO12,
	VCC_LDO13,
	VCC_LDO14,
	VCC_LDO15,
	VCC_LDO16,
	VCC_DC1SW,
	VCC_LDOIO1,
	VCC_LDOIO2,
	VCC_PMU1736_MAX,
};

struct pmu1736_regulators {
	struct regulator_dev *regulators[VCC_PMU1736_MAX];
	struct axp_dev *chip;
};

#define PMU1736_LDO(_id, min, max, step1, vreg, shift, nbits,\
		ereg, emask, enval, disval, switch_vol, step2, new_level,\
		mode_addr, freq_addr, dvm_ereg, dvm_ebit, dvm_flag)\
	AXP_LDO(PMU1736, _id, min, max, step1, vreg, shift, nbits,\
		ereg, emask, enval, disval, switch_vol, step2, new_level,\
		mode_addr, freq_addr, dvm_ereg, dvm_ebit, dvm_flag)

#define PMU1736_DCDC(_id, min, max, step1, vreg, shift, nbits, ereg, emask,\
		enval, disval, switch_vol, step2, new_level, mode_addr,\
		mode_bit, freq_addr, dvm_ereg, dvm_ebit, dvm_flag) \
	AXP_DCDC(PMU1736, _id, min, max, step1, vreg, shift, nbits,\
		ereg, emask, enval, disval, switch_vol, step2, new_level,\
		mode_addr, mode_bit, freq_addr, dvm_ereg, dvm_ebit, dvm_flag)

#define PMU1736_SW(_id, min, max, step1, vreg, shift, nbits, ereg, emask,\
		enval, disval, switch_vol, step2, new_level,\
		mode_addr, freq_addr, dvm_ereg, dvm_ebit, dvm_flag) \
	AXP_SW(PMU1736, _id, min, max, step1, vreg, shift, nbits,\
		ereg, emask, enval, disval, switch_vol, step2, new_level,\
		mode_addr, freq_addr, dvm_ereg, dvm_ebit, dvm_flag)

static struct axp_regulator_info pmu1736_regulator_info[] = {
	PMU1736_DCDC(1,      1500, 3400, 100,  DCDC1, 0, 5,   DCDC1EN, 0x01,
		0x01,    0,     0, 0, 0,  0x1b, 0x01, 0x1d, 0, 0, 0),
	PMU1736_DCDC(2,       500, 1540,  10,  DCDC2, 0, 7,   DCDC2EN, 0x02,
		0x02,    0,  1220, 20, 0, 0x1b, 0x02, 0x1d, 0x1a, 0, 0),
	PMU1736_DCDC(3,       500, 1540,  10,  DCDC3, 0, 7,   DCDC3EN, 0x04,
		0x04,    0,  1220, 20, 0, 0x1b, 0x04, 0x1d, 0x1a, 1, 0),
	PMU1736_DCDC(4,       500, 1540,  10,  DCDC4, 0, 7,   DCDC4EN, 0x08,
		0x08,    0,  1220, 20, 0, 0x1b, 0x08, 0x1d, 0x1a, 2, 0),
	PMU1736_DCDC(5,       800, 1840,  10,  DCDC5, 0, 7,   DCDC5EN, 0x10,
		0x10,    0,  1140, 20, 0, 0x1b, 0x10, 0x1d, 0x1a, 3, 0),
	PMU1736_DCDC(6,       500, 3400, 100,  DCDC6, 0, 5,   DCDC6EN, 0x20,
		0x20,    0,      0, 0, 0, 0x1b, 0x20, 0x1d, 0, 0, 0),
	PMU1736_LDO(1,       1800, 1800,   0,    RTC, 0, 0,  RTCLDOEN, 0x40,
		0x40,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(2,        700, 3300, 100,  ALDO1, 0, 5,   ALDO1EN, 0x01,
		0x01,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(3,        700, 3300, 100,  ALDO2, 0, 5,   ALDO2EN, 0x02,
		0x02,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(4,        700, 3300, 100,  ALDO3, 0, 5,   ALDO3EN, 0x04,
		0x04,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(5,        700, 3300, 100,  ALDO4, 0, 5,   ALDO4EN, 0x08,
		0x08,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(6,        700, 3300, 100,  ALDO5, 0, 5,   ALDO5EN, 0x10,
		0x10,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(7,        700, 3300, 100,  BLDO1, 0, 5,   BLDO1EN, 0x20,
		0x20,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(8,        700, 3300, 100,  BLDO2, 0, 5,   BLDO2EN, 0x40,
		0x40,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(9,        700, 3300, 100,  BLDO3, 0, 5,   BLDO3EN, 0x80,
		0x80,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(10,       700, 3300, 100,  BLDO4, 0, 5,   BLDO4EN, 0x01,
		0x01,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(11,       700, 3300, 100,  BLDO5, 0, 5,   BLDO5EN, 0x02,
		0x02,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(12,       700, 3300, 100,  CLDO1, 0, 5,   CLDO1EN, 0x04,
		0x04,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(13,       700, 3300, 100,  CLDO2, 0, 5,   CLDO2EN, 0x08,
		0x08,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(14,       700, 3300, 100,  CLDO3, 0, 5,   CLDO3EN, 0x10,
		0x10,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(15,       700, 4200, 100,  CLDO4, 0, 5,   CLDO4EN, 0x20,
		0x20,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(16,       700, 1400,  50,   CPUS, 0, 4, CPUSLDOEN, 0x40,
		0x40,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_SW(1,        1500, 3400, 100,  DC1SW, 0, 5,   DC1SWEN, 0x80,
		0x80,    0,      0,    0,   0,      0, 0, 0, 0, 0),
	PMU1736_LDO(IO1,      700, 3300, 100, LDOIO1, 0, 5,  LDOIO1EN, 0x60,
		0x40,    0,      0, 0, 0,     0,    0,    0, 0, 0),
	PMU1736_LDO(IO2,      700, 4200, 100, LDOIO2, 0, 5,  LDOIO2EN, 0x07,
		0x02,    0,      0, 0, 0,     0,    0,    0, 0, 0),
};

static struct regulator_init_data axp_regl_init_data[] = {
	[VCC_DCDC1] = {
		.constraints = {
			.name = "pmu1736_dcdc1",
			.min_uV = 1500000,
			.max_uV = 3400000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_DCDC2] = {
		.constraints = {
			.name = "pmu1736_dcdc2",
			.min_uV =  500000,
			.max_uV = 1540000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_DCDC3] = {
		.constraints = {
			.name = "pmu1736_dcdc3",
			.min_uV =  500000,
			.max_uV = 1540000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_DCDC4] = {
		.constraints = {
			.name = "pmu1736_dcdc4",
			.min_uV =  500000,
			.max_uV = 1540000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_DCDC5] = {
		.constraints = {
			.name = "pmu1736_dcdc5",
			.min_uV =  800000,
			.max_uV = 1840000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_DCDC6] = {
		.constraints = {
			.name = "pmu1736_dcdc6",
			.min_uV =  500000,
			.max_uV = 3400000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO1] = {
		.constraints = {
			.name = "pmu1736_rtc",
			.min_uV = 1800000,
			.max_uV = 1800000,
		},
	},
	[VCC_LDO2] = {
		.constraints = {
			.name = "pmu1736_aldo1",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO3] = {
		.constraints = {
			.name = "pmu1736_aldo2",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO4] = {
		.constraints = {
			.name = "pmu1736_aldo3",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO5] = {
		.constraints = {
			.name = "pmu1736_aldo4",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO6] = {
		.constraints = {
			.name = "pmu1736_aldo5",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO7] = {
		.constraints = {
			.name = "pmu1736_bldo1",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO8] = {
		.constraints = {
			.name = "pmu1736_bldo2",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO9] = {
		.constraints = {
			.name = "pmu1736_bldo3",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO10] = {
		.constraints = {
			.name = "pmu1736_bldo4",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO11] = {
		.constraints = {
			.name = "pmu1736_bldo5",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO12] = {
		.constraints = {
			.name = "pmu1736_cldo1",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO13] = {
		.constraints = {
			.name = "pmu1736_cldo2",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO14] = {
		.constraints = {
			.name = "pmu1736_cldo3",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO15] = {
		.constraints = {
			.name = "pmu1736_cldo4",
			.min_uV =  700000,
			.max_uV = 4200000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDO16] = {
		.constraints = {
			.name = "pmu1736_cpus",
			.min_uV =  700000,
			.max_uV = 1400000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_DC1SW] = {
		.constraints = {
			.name = "pmu1736_dc1sw",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDOIO1] = {
		.constraints = {
			.name = "pmu1736_ldoio1",
			.min_uV =  700000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
	[VCC_LDOIO2] = {
		.constraints = {
			.name = "pmu1736_ldoio2",
			.min_uV =  700000,
			.max_uV = 4200000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
				| REGULATOR_CHANGE_STATUS,
		},
	},
};

static ssize_t workmode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	uint8_t val;
	struct regulator_dev *rdev;
	struct axp_regulator_info *info;
	struct axp_regmap *regmap;

	rdev = container_of(dev, struct regulator_dev, dev);
	info = rdev_get_drvdata(rdev);
	regmap = info->regmap;

	ret = axp_regmap_read(regmap, info->mode_reg, &val);
	if (ret)
		return sprintf(buf, "IO ERROR\n");

	if ((val & info->mode_mask) == info->mode_mask)
		return sprintf(buf, "PWM\n");
	else
		return sprintf(buf, "AUTO\n");
}

static ssize_t workmode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint8_t val;
	struct regulator_dev *rdev;
	struct axp_regulator_info *info;
	struct axp_regmap *regmap;
	unsigned int mode;
	int ret;

	rdev = container_of(dev, struct regulator_dev, dev);
	info = rdev_get_drvdata(rdev);
	regmap = info->regmap;

	ret = sscanf(buf, "%u", &mode);
	if (ret != 1)
		return -EINVAL;

	val = !!mode;
	if (val)
		axp_regmap_set_bits(regmap, info->mode_reg, info->mode_mask);
	else
		axp_regmap_clr_bits(regmap, info->mode_reg, info->mode_mask);

	return count;
}

static ssize_t frequency_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	uint8_t val;
	struct regulator_dev *rdev;
	struct axp_regulator_info *info;
	struct axp_regmap *regmap;

	rdev = container_of(dev, struct regulator_dev, dev);
	info = rdev_get_drvdata(rdev);
	regmap = info->regmap;

	ret = axp_regmap_read(regmap, info->freq_reg, &val);
	if (ret)
		return ret;

	ret = val & 0x0F;

	return sprintf(buf, "%d\n", (ret * 5 + 50));
}

static ssize_t frequency_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint8_t val, tmp;
	int var, err;
	struct regulator_dev *rdev;
	struct axp_regulator_info *info;
	struct axp_regmap *regmap;

	rdev = container_of(dev, struct regulator_dev, dev);
	info = rdev_get_drvdata(rdev);
	regmap = info->regmap;

	err = kstrtoint(buf, 10, &var);
	if (err)
		return err;

	if (var < 50)
		var = 50;

	if (var > 100)
		var = 100;

	val = (var - 50) / 5;
	val &= 0x0F;

	axp_regmap_read(regmap, info->freq_reg, &tmp);
	tmp &= 0xF0;
	val |= tmp;
	axp_regmap_write(regmap, info->freq_reg, val);

	return count;
}

static struct device_attribute axp_regu_attrs[] = {
	AXP_REGU_ATTR(workmode),
	AXP_REGU_ATTR(frequency),
};

static int axp_regu_create_attrs(struct device *dev)
{
	int j, ret;

	for (j = 0; j < ARRAY_SIZE(axp_regu_attrs); j++) {
		ret = device_create_file(dev, &axp_regu_attrs[j]);
		if (ret)
			goto sysfs_failed;
	}

	return 0;

sysfs_failed:
	while (j--)
		device_remove_file(dev, &axp_regu_attrs[j]);
	return ret;
}

static s32 pmu1736_regu_dependence(const char *ldo_name)
{
	s32 pmu1736_dependence = 0;

	if (strstr(ldo_name, "dcdc1") != NULL)
		pmu1736_dependence |= PMU1736_DCDC1;
	else if (strstr(ldo_name, "dcdc2") != NULL)
		pmu1736_dependence |= PMU1736_DCDC2;
	else if (strstr(ldo_name, "dcdc3") != NULL)
		pmu1736_dependence |= PMU1736_DCDC3;
	else if (strstr(ldo_name, "dcdc4") != NULL)
		pmu1736_dependence |= PMU1736_DCDC4;
	else if (strstr(ldo_name, "dcdc5") != NULL)
		pmu1736_dependence |= PMU1736_DCDC5;
	else if (strstr(ldo_name, "dcdc6") != NULL)
		pmu1736_dependence |= PMU1736_DCDC6;
	else if (strstr(ldo_name, "aldo1") != NULL)
		pmu1736_dependence |= PMU1736_ALDO1;
	else if (strstr(ldo_name, "aldo2") != NULL)
		pmu1736_dependence |= PMU1736_ALDO2;
	else if (strstr(ldo_name, "aldo3") != NULL)
		pmu1736_dependence |= PMU1736_ALDO3;
	else if (strstr(ldo_name, "aldo4") != NULL)
		pmu1736_dependence |= PMU1736_ALDO4;
	else if (strstr(ldo_name, "aldo5") != NULL)
		pmu1736_dependence |= PMU1736_ALDO5;
	else if (strstr(ldo_name, "bldo1") != NULL)
		pmu1736_dependence |= PMU1736_BLDO1;
	else if (strstr(ldo_name, "bldo2") != NULL)
		pmu1736_dependence |= PMU1736_BLDO2;
	else if (strstr(ldo_name, "bldo3") != NULL)
		pmu1736_dependence |= PMU1736_BLDO3;
	else if (strstr(ldo_name, "bldo4") != NULL)
		pmu1736_dependence |= PMU1736_BLDO4;
	else if (strstr(ldo_name, "bldo5") != NULL)
		pmu1736_dependence |= PMU1736_BLDO5;
	else if (strstr(ldo_name, "cldo1") != NULL)
		pmu1736_dependence |= PMU1736_CLDO1;
	else if (strstr(ldo_name, "cldo2") != NULL)
		pmu1736_dependence |= PMU1736_CLDO2;
	else if (strstr(ldo_name, "cldo3") != NULL)
		pmu1736_dependence |= PMU1736_CLDO3;
	else if (strstr(ldo_name, "cldo4") != NULL)
		pmu1736_dependence |= PMU1736_CLDO4;
	else if (strstr(ldo_name, "ldoio1") != NULL)
		pmu1736_dependence |= PMU1736_LDOIO1;
	else if (strstr(ldo_name, "ldoio2") != NULL)
		pmu1736_dependence |= PMU1736_LDOIO2;
	else if (strstr(ldo_name, "rtc") != NULL)
		pmu1736_dependence |= PMU1736_RTC;
	else if (strstr(ldo_name, "cpus") != NULL)
		pmu1736_dependence |= PMU1736_CPUS;
	else if (strstr(ldo_name, "dc1sw") != NULL)
		pmu1736_dependence |= PMU1736_DC1SW;
	else
		return -1;

	return pmu1736_dependence;
}

static int pmu1736_regulator_probe(struct platform_device *pdev)
{
	s32 i, ret = 0;
	struct axp_regulator_info *info;
	struct pmu1736_regulators *regu_data;
	struct axp_dev *axp_dev = dev_get_drvdata(pdev->dev.parent);

	if (pdev->dev.of_node) {
		ret = axp_regulator_dt_parse(pdev->dev.of_node,
					axp_regl_init_data,
					pmu1736_regu_dependence);
		if (ret) {
			pr_err("%s parse device tree err\n", __func__);
			return -EINVAL;
		}
	} else {
		pr_err("pmu1736 regulator device tree err!\n");
		return -EBUSY;
	}

	regu_data = devm_kzalloc(&pdev->dev, sizeof(*regu_data),
					GFP_KERNEL);
	if (!regu_data)
		return -ENOMEM;

	regu_data->chip = axp_dev;
	platform_set_drvdata(pdev, regu_data);

	for (i = 0; i < VCC_PMU1736_MAX; i++) {
		info = &pmu1736_regulator_info[i];
		info->pmu_num = axp_dev->pmu_num;
			regu_data->regulators[i] = axp_regulator_register(
					&pdev->dev, axp_dev->regmap,
					&axp_regl_init_data[i], info);

		if (IS_ERR(regu_data->regulators[i])) {
			dev_err(&pdev->dev,
				"failed to register regulator %s\n",
				info->desc.name);
			while (--i >= 0)
				axp_regulator_unregister(
					regu_data->regulators[i]);

			return -1;
		}

		if (info->desc.id >= AXP_DCDC_ID_START) {
			ret = axp_regu_create_attrs(
						&regu_data->regulators[i]->dev);
			if (ret)
				dev_err(&pdev->dev,
					"failed to register regulator attr %s\n",
					info->desc.name);
		}
	}

	return 0;
}

static int pmu1736_regulator_remove(struct platform_device *pdev)
{
	struct pmu1736_regulators *regu_data = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < VCC_PMU1736_MAX; i++)
		regulator_unregister(regu_data->regulators[i]);

	return 0;
}

static const struct of_device_id pmu1736_regu_dt_ids[] = {
	{ .compatible = "pmu1736-regulator", },
	{},
};
MODULE_DEVICE_TABLE(of, pmu1736_regu_dt_ids);

static struct platform_driver pmu1736_regulator_driver = {
	.driver     = {
		.name   = "pmu1736-regulator",
		.of_match_table = pmu1736_regu_dt_ids,
	},
	.probe      = pmu1736_regulator_probe,
	.remove     = pmu1736_regulator_remove,
};

static int __init pmu1736_regulator_initcall(void)
{
	int ret;

	ret = platform_driver_register(&pmu1736_regulator_driver);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: failed, errno %d\n", __func__, ret);
		return -EINVAL;
	}

	return 0;
}
subsys_initcall(pmu1736_regulator_initcall);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Qin <qinyongshen@allwinnertech.com>");
MODULE_DESCRIPTION("Regulator Driver for pmu1736 PMIC");
MODULE_ALIAS("platform:pmu1736-regulator");
