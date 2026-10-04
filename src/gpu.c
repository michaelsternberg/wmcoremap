/* gpu.c - amdgpu busy % and VRAM use from sysfs
 *
 * Copyright (C) 2026 Michael Sternberg
 * SPDX-License-Identifier: GPL-2.0-or-later
 * This file is part of wmcoremap; see COPYING for the full license text.
 *
 * Only amdgpu exports gpu_busy_percent; the older radeon driver does not,
 * so on radeon (and other) cards the GPU screen is simply left out.
 */
#include <glob.h>
#include <stdio.h>
#include <string.h>

#include "gpu.h"

static char dev[512];

static int read_num(const char *name, double *v)
{
	char path[600];
	FILE *f;
	int ok;

	snprintf(path, sizeof(path), "%s/%s", dev, name);
	f = fopen(path, "r");
	if (!f)
		return -1;
	ok = fscanf(f, "%lf", v) == 1;
	fclose(f);
	return ok ? 0 : -1;
}

int gpu_init(void)
{
	glob_t g;

	dev[0] = '\0';
	if (glob("/sys/class/drm/card[0-9]*/device/gpu_busy_percent", 0, NULL, &g))
		return -1;
	snprintf(dev, sizeof(dev), "%.*s",
	         (int)(strlen(g.gl_pathv[0]) - strlen("/gpu_busy_percent")),
	         g.gl_pathv[0]);
	globfree(&g);
	return 0;
}

void gpu_read(double *busy, double *vram)
{
	double used, total;

	*busy = *vram = -1;
	if (!dev[0])
		return;
	if (read_num("gpu_busy_percent", busy) < 0)
		*busy = -1;
	if (read_num("mem_info_vram_used", &used) == 0 &&
	    read_num("mem_info_vram_total", &total) == 0 && total > 0)
		*vram = 100.0 * used / total;
}
