/*
 * Copyright (c) 2018, LG Eletronics. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/sysfs.h>
#include <linux/device.h>
#include <uapi/drm/sde_drm.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include "lge_prm.h"
#include "fbcn/lge_fbcn.h"
#include "fbcn/lge_intm.h"
#include "vfps/lge_vfps.h"
#include "sben/lge_sben.h"
#include "triton/lge_triton.h"

static struct lge_prm prm = {
	.cnode        = NULL,
	.enable_mask  = LGE_PRM_INIT_DONE,
	.init_mask    = LGE_PRM_INIT_MAX,
	.display_mode = LGE_PRM_DISPLAY_OFF
};

void lge_prm_display_set_power_mode(int power_mode)
{
	mutex_lock(&prm.prm_lock);

	prm.display_mode = power_mode;

	mutex_unlock(&prm.prm_lock);

	if (prm.enable_mask & LGE_PRM_INIT_SBEN)
		lge_sben_notify();

	if (prm.enable_mask & LGE_PRM_INIT_FBCN)
		lge_intv_panel_power_notify();
}

int lge_prm_get_info(int info)
{
	int data = 0;

	mutex_lock(&prm.prm_lock);

	switch (info) {
	case LGE_PRM_INFO_DISPLAY_MODE:
		data = prm.display_mode;
		break;
	case LGE_PRM_INFO_VFPS_ENABLED:
		data = !!(prm.enable_mask & LGE_PRM_INIT_VFPS);
		break;
	case LGE_PRM_INFO_SBEN_ENABLED:
		data = !!(prm.enable_mask & LGE_PRM_INIT_SBEN);
		break;
	case LGE_PRM_INFO_FBCN_ENABLED:
		data = !!(prm.enable_mask & LGE_PRM_INIT_FBCN);
		break;
	case LGE_PRM_INFO_TRITON_ENABLED:
		data = !!(prm.enable_mask & LGE_PRM_INIT_TRITON);
		break;
	default:
		pr_err(PRM_TAG "Unknown request of prm\n");
		data = 0;
		break;
	};

	mutex_unlock(&prm.prm_lock);

	return data;
}

int lge_prm_get_class_node(struct class **cdev) {
	if (!(prm.cnode)) {
		pr_err(PRM_TAG "Class node is null\n");
		return -ENODEV;
	}

	*cdev = prm.cnode;
	return 0;
}

static int lge_prm_probe(struct platform_device *pdev)
{
	static struct class *cn = NULL;
	struct device_node *node = NULL;
	int ret = 0;

	if (!cn) {
		cn = class_create(THIS_MODULE, "power");
		if (IS_ERR_OR_NULL(cn)) {
			pr_err(PRM_TAG "Failed to create power class node\n");
			return -EPROBE_DEFER;
		}
		prm.cnode = cn;
	}

	if (!pdev->dev.of_node) {
		pr_err(PRM_TAG "PRM of_node not exist\n");
		return -EINVAL;
	}

	node = pdev->dev.of_node;

	mutex_init(&prm.prm_lock);

	mutex_lock(&prm.prm_lock);

	/* Get properties */
	if (of_property_read_bool(node, "lge,vfps-enabled"))
		prm.enable_mask |= LGE_PRM_INIT_VFPS;

	if (of_property_read_bool(node, "lge,sben-enabled"))
		prm.enable_mask |= LGE_PRM_INIT_SBEN;

	if (of_property_read_bool(node, "lge,fbcn-enabled"))
		prm.enable_mask |= LGE_PRM_INIT_FBCN;

	if (of_property_read_bool(node, "lge,triton-enabled"))
		prm.enable_mask |= LGE_PRM_INIT_TRITON;

	/* Masking init_mask */
	prm.init_mask &= prm.enable_mask;

	pr_err(PRM_TAG "Enable_mask = 0x%x, Init_mask = 0x%x\n",
			prm.enable_mask, prm.init_mask);

	/* VFPS */
	if (prm.init_mask & LGE_PRM_INIT_VFPS) {
		ret = lge_vfps_init();
		if (ret)
			pr_err(PRM_TAG "vfps init failed, %d\n", ret);
		else
			prm.init_mask &= (~LGE_PRM_INIT_VFPS);
	} else {
		if (prm.enable_mask & LGE_PRM_INIT_VFPS)
			pr_err(PRM_TAG "vfps init already done\n");
		else
			pr_err(PRM_TAG "LGE VFPS is not enabled\n");
	}

	/* SBEN */
	if (prm.init_mask & LGE_PRM_INIT_SBEN) {
		ret = lge_sben_init();
		if (ret)
			pr_err(PRM_TAG "sben init failed, %d\n", ret);
		else
			prm.init_mask &= (~LGE_PRM_INIT_SBEN);
	} else {
		if (prm.enable_mask & LGE_PRM_INIT_SBEN)
			pr_err(PRM_TAG "sben init already done\n");
		else
			pr_err(PRM_TAG "LGE SBEN is not enabled\n");
	}

	/* FBCN */
	if (prm.init_mask & LGE_PRM_INIT_FBCN) {
		ret = lge_fbcn_init();
		if (ret)
			pr_err(PRM_TAG "fbcn init failed, %d\n", ret);
		else
			prm.init_mask &= (~LGE_PRM_INIT_FBCN);
	} else {
		if (prm.enable_mask & LGE_PRM_INIT_FBCN)
			pr_err(PRM_TAG "fbcn init already done\n");
		else
			pr_err(PRM_TAG "LGE FBCN is not enabled\n");
	}

	/* TRITON */
	if (prm.init_mask & LGE_PRM_INIT_TRITON) {
		ret = lge_triton_init();
		if (ret)
			pr_err(PRM_TAG "triton init failed, %d\n", ret);
		else
			prm.init_mask &= (~LGE_PRM_INIT_TRITON);
	} else {
		if (prm.enable_mask & LGE_PRM_INIT_TRITON)
			pr_err(PRM_TAG "triton init already done\n");
		else
			pr_err(PRM_TAG "LGE TRITON is not enabled\n");
	}

	mutex_unlock(&prm.prm_lock);

	/* Probe defer until init_mask to 0x0 */
	if (prm.init_mask != LGE_PRM_INIT_DONE) {
		pr_err(PRM_TAG "LGE PRM probe deferred, 0x%x, 0x%x\n",
				prm.enable_mask, prm.init_mask);
		return -EPROBE_DEFER;
	}

	pr_err(PRM_TAG "LGE PRM probe done\n");
	return 0;
}

static int lge_prm_remove(struct platform_device *pdev)
{
	pr_err(PRM_TAG "lge_prm_remove\n");
	return 0;
}

static struct of_device_id lge_prm_match[] = {
	{ .compatible = "lge,prm"},
	{}
};

static struct platform_driver lge_prm_driver = {
	.probe          = lge_prm_probe,
	.remove         = lge_prm_remove,
	.driver         = {
		.name   = "lge-prm",
		.owner  = THIS_MODULE,
		.of_match_table = lge_prm_match,
	},
};

static int __init lge_prm_init(void)
{
	return platform_driver_register(&lge_prm_driver);
}
late_initcall(lge_prm_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("LGE PRM");
