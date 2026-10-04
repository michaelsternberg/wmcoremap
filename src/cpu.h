/* cpu.h - per-CPU and total utilisation from /proc/stat
 *
 * Copyright (C) 2026 Michael Sternberg
 * SPDX-License-Identifier: GPL-2.0-or-later
 * This file is part of wmcoremap; see COPYING for the full license text.
 */
#ifndef WMCOREMAP_CPU_H
#define WMCOREMAP_CPU_H

struct cpu_slot {
	unsigned long long total, idle;	/* jiffies at the previous sample */
	float pct;			/* smoothed busy %, 0..100 */
	int online;
};

struct cpu {
	int n;			/* number of logical CPUs shown */
	int fake;		/* simulate load instead of reading /proc/stat */
	unsigned long fake_tick;
	struct cpu_slot all;	/* the aggregate "cpu" line */
	struct cpu_slot *core;	/* n entries, indexed by CPU number */
};

/* fake > 0 simulates that many CPUs. Returns -1 on failure. */
int cpu_init(struct cpu *c, int fake);
void cpu_free(struct cpu *c);
/* Take a new sample and update the busy percentages. */
void cpu_sample(struct cpu *c);

#endif
