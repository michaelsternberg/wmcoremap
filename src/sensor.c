/* sensor.c - CPU temperature from hwmon / thermal zones
 *
 * Copyright (C) 2026 Michael Sternberg
 * SPDX-License-Identifier: GPL-2.0-or-later
 * This file is part of wmcoremap; see COPYING for the full license text.
 *
 * hwmon drivers are tried in order of preference; within a driver the
 * temperature with the most CPU-wide label (Tctl, Package id 0, ...) wins.
 * ACPI / x86 package thermal zones are the fallback.
 */
#include <glob.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sensor.h"

static char spath[512];

static int read_line(const char *path, char *buf, int len)
{
	FILE *f = fopen(path, "r");

	if (!f)
		return -1;
	if (!fgets(buf, len, f)) {
		fclose(f);
		return -1;
	}
	fclose(f);
	buf[strcspn(buf, "\n")] = '\0';
	return 0;
}

/* In hwmon dir, pick the input whose label best matches labels[]. */
static int pick_input(const char *dir, const char *const *labels)
{
	char pat[512], lbl[64], best[512] = "";
	int best_rank = 1000;
	glob_t g;
	size_t i;

	snprintf(pat, sizeof(pat), "%s/temp*_input", dir);
	if (glob(pat, 0, NULL, &g))
		return -1;
	for (i = 0; i < g.gl_pathc; i++) {
		char lpath[512];
		int rank = 100, k;

		snprintf(lpath, sizeof(lpath), "%.*s_label",
		         (int)(strlen(g.gl_pathv[i]) - 6), g.gl_pathv[i]);
		if (read_line(lpath, lbl, sizeof(lbl)) == 0)
			for (k = 0; labels[k]; k++)
				if (!strcmp(lbl, labels[k])) {
					rank = k;
					break;
				}
		/* glob sorts, so temp1 wins among unlabelled inputs */
		if (rank < best_rank) {
			best_rank = rank;
			snprintf(best, sizeof(best), "%s", g.gl_pathv[i]);
		}
	}
	globfree(&g);
	if (!best[0])
		return -1;
	snprintf(spath, sizeof(spath), "%s", best);
	return 0;
}

static int find_hwmon(void)
{
	static const char *const drivers[] = {
		"k10temp", "zenpower", "coretemp", "cpu_thermal", "soc_thermal", NULL
	};
	static const char *const labels[] = {
		"Tctl", "Tdie", "Package id 0", NULL
	};
	char name[64];
	glob_t g;
	size_t i;
	int d;

	if (glob("/sys/class/hwmon/hwmon*", 0, NULL, &g))
		return -1;
	for (d = 0; drivers[d]; d++)
		for (i = 0; i < g.gl_pathc; i++) {
			char npath[512];

			snprintf(npath, sizeof(npath), "%s/name", g.gl_pathv[i]);
			if (read_line(npath, name, sizeof(name)) == 0 &&
			    !strcmp(name, drivers[d]) &&
			    pick_input(g.gl_pathv[i], labels) == 0) {
				globfree(&g);
				return 0;
			}
		}
	globfree(&g);
	return -1;
}

static int find_zone(void)
{
	static const char *const types[] = { "x86_pkg_temp", "acpitz", NULL };
	char type[64];
	glob_t g;
	size_t i;
	int t;

	if (glob("/sys/class/thermal/thermal_zone*", 0, NULL, &g))
		return -1;
	for (t = 0; types[t]; t++)
		for (i = 0; i < g.gl_pathc; i++) {
			char tpath[512];

			snprintf(tpath, sizeof(tpath), "%s/type", g.gl_pathv[i]);
			if (read_line(tpath, type, sizeof(type)) == 0 &&
			    !strcmp(type, types[t])) {
				snprintf(spath, sizeof(spath), "%s/temp", g.gl_pathv[i]);
				globfree(&g);
				return 0;
			}
		}
	globfree(&g);
	return -1;
}

int sensor_init(const char *path)
{
	spath[0] = '\0';
	if (path) {
		snprintf(spath, sizeof(spath), "%s", path);
		return isnan(sensor_read()) ? -1 : 0;
	}
	if (find_hwmon() == 0 || find_zone() == 0)
		return 0;
	spath[0] = '\0';
	return -1;
}

double sensor_read(void)
{
	char buf[32];

	if (!spath[0] || read_line(spath, buf, sizeof(buf)) < 0)
		return NAN;
	return atof(buf) / 1000.0;
}

const char *sensor_path(void)
{
	return spath;
}
