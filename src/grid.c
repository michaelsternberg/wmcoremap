/* grid.c - core heatmap grid, colour ramp and history graphs
 *
 * Copyright (C) 2026 Michael Sternberg
 * SPDX-License-Identifier: GPL-2.0-or-later
 * This file is part of wmcoremap; see COPYING for the full license text.
 *
 * Every logical CPU is one dot. An idle CPU is still drawn, in the dark
 * grey-green of wmsysmon's unlit LEDs; moderate load lights it up in
 * wmsysmon's bright green, and towards 100% it turns yellow, orange, red.
 */
#include "grid.h"

static const struct {
	float at;
	uint32_t rgb;
} stops[] = {
	{   0, 0x283C38 },	/* wmsysmon unlit LED */
	{  40, 0x00EB00 },	/* wmsysmon lit LED */
	{  70, 0xE8E020 },
	{  85, 0xF08020 },
	{ 100, 0xFF2A1A },
};

static uint32_t table[101];

static uint32_t mix(uint32_t a, uint32_t b, float t)
{
	uint32_t out = 0;
	int sh;

	for (sh = 0; sh <= 16; sh += 8) {
		float ca = (float)((a >> sh) & 0xff), cb = (float)((b >> sh) & 0xff);

		out |= (uint32_t)(ca + (cb - ca) * t + 0.5f) << sh;
	}
	return out;
}

void ramp_init(void)
{
	int i, s = 0;

	for (i = 0; i <= 100; i++) {
		while (s < 3 && i > stops[s + 1].at)
			s++;
		table[i] = mix(stops[s].rgb, stops[s + 1].rgb,
		               ((float)i - stops[s].at) / (stops[s + 1].at - stops[s].at));
	}
}

uint32_t ramp(float pct)
{
	int i = (int)(pct + 0.5f);

	return table[i < 0 ? 0 : i > 100 ? 100 : i];
}

/*
 * Space between LEDs: 1 pixel for wmsysmon-sized ones and smaller,
 * otherwise about half the LED size (6 -> 3, 9 -> 5).
 */
static int gap(int d)
{
	return d <= 4 ? 1 : (d + 1) / 2;
}

void grid_layout(struct grid *g, int n, int x, int y, int w, int h, int maxd)
{
	int c, best = 0, best_c = 1, best_waste = 0, best_skew = 0;

	if (n < 1)
		n = 1;
	for (c = 1; c <= n; c++) {
		int r = (n + c - 1) / c;
		int p = w / c < h / r ? w / c : h / r;
		int waste = c * r - n, skew = c > r ? c - r : r - c;

		/* pitches above the LED size only add spacing, so treat them alike */
		if (p > maxd + gap(maxd))
			p = maxd + gap(maxd);
		/* larger dots first, then fewest empty cells, then the squarer grid */
		if (p > best || (p == best && (waste < best_waste ||
		    (waste == best_waste && skew <= best_skew)))) {
			best = p;
			best_c = c;
			best_waste = waste;
			best_skew = skew;
		}
	}
	g->cols = best_c;
	g->rows = (n + best_c - 1) / best_c;
	/* the requested size, or smaller when that many CPUs do not fit */
	g->d = best - 1 < maxd ? best - 1 : maxd;
	if (g->d < 1)
		g->d = 1;
	/* with few CPUs keep the LEDs together rather than spread out */
	g->pitch = best < g->d + gap(g->d) ? best : g->d + gap(g->d);
	if (g->pitch < 1)
		g->pitch = 1;
	g->x0 = x + (w - (g->cols - 1) * g->pitch - g->d) / 2;
	g->y0 = y + (h - (g->rows - 1) * g->pitch - g->d) / 2;
}

/* Is pixel (i, j) of a d x d dot inside the round shape? */
static int inside(int i, int j, int d, int square)
{
	int a = 2 * i - d + 1, b = 2 * j - d + 1;

	if (i < 0 || j < 0 || i >= d || j >= d)
		return 0;
	if (square || d <= 3)
		return 1;
	if (d == 4)	/* wmsysmon's LED: a 4x4 square without its corners */
		return a * a + b * b < 18;
	return a * a + b * b <= d * d + d / 2;
}

/* load (0..1) brightens the glint of wmsysmon-sized LEDs */
static void dot(struct display *dp, int x, int y, int d, uint32_t c,
                int square, int ring, float load)
{
	int i, j;

	for (j = 0; j < d; j++)
		for (i = 0; i < d; i++) {
			if (!inside(i, j, d, square))
				continue;
			if (ring && d > 2 && inside(i - 1, j, d, square) &&
			    inside(i + 1, j, d, square) && inside(i, j - 1, d, square) &&
			    inside(i, j + 1, d, square))
				continue;
			display_px(dp, x + i, y + j, c);
		}
	/* LED glint in the upper left */
	if (!ring && d == 4 && !square)
		display_px(dp, x + 1, y + 1, mix(c, 0xF7F3FF, load));
	if (!ring && d >= 6) {
		int k = d / 4;
		uint32_t hi = mix(c, 0xFFFFFF, 0.35f);

		display_px(dp, x + k + (square ? 0 : 1), y + k, hi);
		if (d >= 8)
			display_px(dp, x + k, y + k + (square ? 0 : 1), hi);
	}
}

void grid_draw(struct display *d, const struct grid *g, const struct cpu *c,
               int square)
{
	int i;

	for (i = 0; i < c->n; i++) {
		int x = g->x0 + (i % g->cols) * g->pitch;
		int y = g->y0 + (i / g->cols) * g->pitch;
		const struct cpu_slot *s = &c->core[i];
		float pct = s->online ? s->pct : 0;

		dot(d, x, y, g->d, ramp(pct), square, !s->online, pct / 100);
	}
}

void history_draw(struct display *d, const struct history *h,
                  int x, int y, int w, int ht, float lo, float hi,
                  uint32_t fg, uint32_t bg)
{
	uint32_t grid = mix(bg, ramp(0), 0.6f);
	int i, j;

	/* dotted LCD guide lines at 25 / 50 / 75 % */
	for (j = 1; j < 4; j++)
		for (i = 0; i < w; i += 2)
			display_px(d, x + i, y + ht - 1 - (ht - 1) * j / 4, grid);
	/* baseline */
	display_fill(d, x, y + ht - 1, w, 1, ramp(0));

	for (i = 0; i < h->count && i < w; i++) {
		float v = history_get(h, i), t = hi > lo ? (v - lo) / (hi - lo) : 0;
		int bh, col = x + w - 1 - i;

		if (v != v)	/* NaN: no reading */
			continue;
		if (t < 0)
			t = 0;
		if (t > 1)
			t = 1;
		bh = (int)(t * (float)(ht - 1) + 0.5f);
		if (bh < 1)
			bh = 1;
		display_fill(d, col, y + ht - bh, 1, bh, fg);
	}
}
