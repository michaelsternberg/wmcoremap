/* history.h - fixed-size ring buffer of samples, one per graph column
 *
 * Copyright (C) 2026 Michael Sternberg
 * SPDX-License-Identifier: GPL-2.0-or-later
 * This file is part of wmcoremap; see COPYING for the full license text.
 */
#ifndef WMCOREMAP_HISTORY_H
#define WMCOREMAP_HISTORY_H

#define HIST_LEN 54	/* graph width in pixels */

struct history {
	float v[HIST_LEN];
	int head;	/* index of the next slot to write */
	int count;	/* samples stored, up to HIST_LEN */
};

static inline void history_push(struct history *h, float v)
{
	h->v[h->head] = v;
	h->head = (h->head + 1) % HIST_LEN;
	if (h->count < HIST_LEN)
		h->count++;
}

/* i = 0 is the newest sample, i = count - 1 the oldest. */
static inline float history_get(const struct history *h, int i)
{
	return h->v[(h->head - 1 - i + 2 * HIST_LEN) % HIST_LEN];
}

#endif
