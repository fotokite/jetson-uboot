/*
 * Copyright (c) 2010-2019, NVIDIA CORPORATION.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <common.h>
#include <asm/arch-tegra/gpu.h>
#include <asm/arch-tegra/board.h>
#include "dt-edit.h"

/*
 * This function is called right before the kernel is booted. "blob" is the
 * device tree that will be passed to the kernel.
 */
int ft_system_setup(void *blob, bd_t *bd)
{
	const char *gpu_compats[] = {
#if defined(CONFIG_TEGRA124)
		"nvidia,gk20a",
#endif
	};
	int i, ret;

	/* Enable GPU node if GPU setup has been performed */
	for (i = 0; i < ARRAY_SIZE(gpu_compats); i++) {
		ret = tegra_gpu_enable_node(blob, gpu_compats[i]);
		if (ret)
			return ret;
	}

	fdt_del_env_nodelist(blob);
	fdt_del_env_proplist(blob);
	fdt_copy_env_nodelist(blob);
	fdt_copy_env_proplist(blob);

	return 0;
}
