/* grid.h - core heatmap grid, colour ramp and history graphs
 *
 * Copyright (C) 2026 Michael Sternberg
 * SPDX-License-Identifier: GPL-2.0-or-later
 * This file is part of wmcoremap; see COPYING for the full license text.
 */
#ifndef WMCOREMAP_GRID_H
#define WMCOREMAP_GRID_H

#include <stdint.h>

#include "cpu.h"
#include "display.h"
#include "history.h"

/* LED diameters for -size; small matches wmsysmon's 4x4 LEDs. */
#define DOT_SMALL  4
#define DOT_MEDIUM 6
#define DOT_LARGE  9

struct grid {
	int cols, rows;
	int pitch;	/* distance between dot origins */
	int d;		/* dot diameter */
	int x0, y0;	/* top-left of the first dot */
};

/* Build the 101-entry colour table: dark grey-green, green, yellow, red. */
void ramp_init(void);
/* Colour for a load of pct (0..100, clamped). */
uint32_t ramp(float pct);

/*
 * Fit n dots into the w x h area at (x, y): maxd pixels across, or as
 * large as fits when there are too many CPUs for that.
 */
void grid_layout(struct grid *g, int n, int x, int y, int w, int h, int maxd);
void grid_draw(struct display *d, const struct grid *g, const struct cpu *c,
               int square);

/*
 * Bar graph of h (newest at the right) in the w x ht area at (x, y).
 * Values in [lo, hi] fill the height; bars are drawn in fg, the guide
 * lines in the ramp's idle colour blended into bg.
 */
void history_draw(struct display *d, const struct history *h,
                  int x, int y, int w, int ht, float lo, float hi,
                  uint32_t fg, uint32_t bg);

#endif
