/* gpu.h - amdgpu busy % and VRAM use from sysfs
 *
 * Copyright (C) 2026 Michael Sternberg
 * SPDX-License-Identifier: GPL-2.0-or-later
 * This file is part of wmcoremap; see COPYING for the full license text.
 */
#ifndef WMCOREMAP_GPU_H
#define WMCOREMAP_GPU_H

/* Returns 0 when an amdgpu card reporting gpu_busy_percent was found. */
int gpu_init(void);
/* busy and vram (percent) are set to -1 when a value cannot be read. */
void gpu_read(double *busy, double *vram);

#endif
