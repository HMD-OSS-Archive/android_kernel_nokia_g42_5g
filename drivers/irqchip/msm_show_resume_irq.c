// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011, 2014-2016, 2018, 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

// Add-begin by ning.wei for show device resume source
#if 0
int msm_show_resume_irq_mask;
#else
int msm_show_resume_irq_mask = 1;
#endif
// Add-end by ning.wei for show device resume source

module_param_named(
	debug_mask, msm_show_resume_irq_mask, int, 0664);
