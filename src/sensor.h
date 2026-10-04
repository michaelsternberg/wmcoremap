/* sensor.h - CPU temperature from hwmon / thermal zones
 *
 * Copyright (C) 2026 Michael Sternberg
 * SPDX-License-Identifier: GPL-2.0-or-later
 * This file is part of wmcoremap; see COPYING for the full license text.
 */
#ifndef WMCOREMAP_SENSOR_H
#define WMCOREMAP_SENSOR_H

/*
 * Find the CPU temperature input. path, if not NULL, is used as is (a
 * millidegree file such as /sys/class/hwmon/hwmon1/temp1_input).
 * Returns 0 when a sensor was found, -1 otherwise.
 */
int sensor_init(const char *path);
/* Temperature in degrees Celsius, or NAN when unavailable. */
double sensor_read(void);
/* The file being read, or "" when none. */
const char *sensor_path(void);

#endif
