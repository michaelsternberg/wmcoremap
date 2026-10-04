/* cpu.c - per-CPU and total utilisation from /proc/stat
 *
 * Copyright (C) 2026 Michael Sternberg
 * SPDX-License-Identifier: GPL-2.0-or-later
 * This file is part of wmcoremap; see COPYING for the full license text.
 *
 * Busy time is everything except idle and iowait. Each new reading is
 * blended with the previous one so the dots do not flicker.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"

#define SMOOTH 0.5f	/* weight of the new reading */

/* Highest CPU number in a kernel cpu list such as "0-3,8-11", plus one. */
static int count_present(void)
{
	FILE *f = fopen("/sys/devices/system/cpu/present", "r");
	char buf[4096], *p;
	int max = -1;

	if (!f)
		return 0;
	if (fgets(buf, sizeof(buf), f))
		for (p = buf; *p; p++)
			if (*p >= '0' && *p <= '9') {
				int v = (int)strtol(p, &p, 10);

				if (v > max)
					max = v;
				p--;
			}
	fclose(f);
	return max + 1;
}

static int count_stat(void)
{
	FILE *f = fopen("/proc/stat", "r");
	char line[512];
	int max = -1, id;

	if (!f)
		return 0;
	while (fgets(line, sizeof(line), f))
		if (!strncmp(line, "cpu", 3) && line[3] >= '0' && line[3] <= '9' &&
		    sscanf(line + 3, "%d", &id) == 1 && id > max)
			max = id;
	fclose(f);
	return max + 1;
}

static void update(struct cpu_slot *s, unsigned long long total,
                   unsigned long long idle)
{
	unsigned long long dt = total - s->total, di = idle - s->idle;
	float raw;

	if (s->total && total >= s->total && idle >= s->idle && dt > 0) {
		raw = 100.0f * (float)(dt - (di < dt ? di : dt)) / (float)dt;
		s->pct += SMOOTH * (raw - s->pct);
	}
	s->total = total;
	s->idle = idle;
	s->online = 1;
}

static void sample_proc(struct cpu *c)
{
	FILE *f = fopen("/proc/stat", "r");
	char line[512];
	int i;

	if (!f)
		return;
	for (i = 0; i < c->n; i++)
		c->core[i].online = 0;
	while (fgets(line, sizeof(line), f)) {
		unsigned long long v[10] = { 0 }, total = 0;
		struct cpu_slot *s;
		char *p;
		int id, k;

		if (strncmp(line, "cpu", 3))
			break;	/* the cpu lines come first */
		if (line[3] == ' ')
			s = &c->all;
		else if (line[3] >= '0' && line[3] <= '9' &&
		         sscanf(line + 3, "%d", &id) == 1 && id < c->n)
			s = &c->core[id];
		else
			continue;
		p = strchr(line, ' ');
		for (k = 0; k < 10 && p; k++)
			v[k] = strtoull(p, &p, 10);
		/* user nice system idle iowait irq softirq steal (guest is in user) */
		for (k = 0; k < 8; k++)
			total += v[k];
		update(s, total, v[3] + v[4]);
	}
	fclose(f);
	for (i = 0; i < c->n; i++)
		if (!c->core[i].online)
			c->core[i].pct = 0;
}

/* Synthetic load for testing layouts: a few busy cores, a few pegged,
 * the rest drifting at low load. One CPU is shown offline. */
static void sample_fake(struct cpu *c)
{
	double t = (double)c->fake_tick++;
	float sum = 0;
	int i;

	for (i = 0; i < c->n; i++) {
		struct cpu_slot *s = &c->core[i];
		double ph = i * 1.7, raw;

		if (i % 7 == 3)
			raw = 100;
		else if (i % 5 == 1)
			raw = 50 + 45 * sin(t / 6 + ph);
		else
			raw = 8 + 7 * sin(t / 4 + ph) + (rand() % 6);
		s->online = !(c->n > 8 && i == c->n - 2);
		s->pct = s->online ? (float)raw : 0;
		sum += s->pct;
	}
	c->all.pct = sum / (float)c->n;
	c->all.online = 1;
}

int cpu_init(struct cpu *c, int fake)
{
	memset(c, 0, sizeof(*c));
	c->fake = fake;
	if (fake > 0)
		c->n = fake;
	else {
		int a = count_present(), b = count_stat();

		c->n = a > b ? a : b;
	}
	if (c->n <= 0) {
		fprintf(stderr, "wmcoremap: cannot read /proc/stat\n");
		return -1;
	}
	c->core = calloc((size_t)c->n, sizeof(*c->core));
	if (!c->core)
		return -1;
	cpu_sample(c);
	return 0;
}

void cpu_free(struct cpu *c)
{
	free(c->core);
	c->core = NULL;
}

void cpu_sample(struct cpu *c)
{
	if (c->fake)
		sample_fake(c);
	else
		sample_proc(c);
}
