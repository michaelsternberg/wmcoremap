/* wmcoremap - a Window Maker dockapp showing per-core CPU load as a heatmap
 *
 * Copyright (C) 2026 Michael Sternberg
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <X11/Xlib.h>

#include "cpu.h"
#include "display.h"
#include "font.h"
#include "gpu.h"
#include "grid.h"
#include "history.h"
#include "sensor.h"

#define VERSION "0.1"

/* Tile rows inside the bevel (inner area is 56x56 at 4,4). */
#define TEXT_X0  5
#define TEXT_X1  59	/* exclusive */
#define LABEL_Y  5
#define AREA_Y   13
#define AREA_H   37
#define STATUS_Y 52

#define TEMP_SPAN 40.0f	/* minimum height of the temperature graph, C */
#define TEMP_COOL 30.0f	/* graph scale before there are readings, C */
#define TEMP_HOT  95.0f

enum screen { SCR_CPU, SCR_LOAD, SCR_TEMP, SCR_GPU, SCR_COUNT };
static const char *screen_names[] = { "cpu", "load", "temp", "gpu" };
static const char *screen_labels[] = { "CPU", "LOAD", "TEMP", "GPU" };
static const char *size_names[] = { "small", "medium", "large" };
static const int size_dots[] = { DOT_SMALL, DOT_MEDIUM, DOT_LARGE };

static struct {
	struct display d;
	struct cpu c;
	struct grid g;
	struct history load, temp, gpu;
	enum screen screen;
	uint32_t fg, dim, bg;
	int interval_ms;
	int fahrenheit;
	int square;
	int dot;		/* requested LED diameter */
	int have_temp, have_gpu;
	double celsius;		/* latest reading, NAN if none */
	double vram;		/* percent, -1 if unknown */
	volatile sig_atomic_t quit;
} S;

static double now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int screen_available(enum screen s)
{
	return (s != SCR_TEMP || S.have_temp) && (s != SCR_GPU || S.have_gpu);
}

static void next_screen(int dir)
{
	int k;

	for (k = 0; k < SCR_COUNT; k++) {
		S.screen = (enum screen)((S.screen + dir + SCR_COUNT) % SCR_COUNT);
		if (screen_available(S.screen))
			return;
	}
}

static void text_left(int y, const char *s, uint32_t c)
{
	font_draw(S.d.fb, TILE, TEXT_X0, y, s, c, TEXT_X0, TEXT_X1);
}

static void text_right(int y, const char *s, uint32_t c)
{
	font_draw(S.d.fb, TILE, TEXT_X1 - font_text_width(s), y, s, c,
	          TEXT_X0, TEXT_X1);
}

/* "41`C" or "106`F"; "--" when there is no reading */
static void format_temp(char *buf, size_t len, double c)
{
	char unit = S.fahrenheit ? 'F' : 'C';

	if (isnan(c))
		snprintf(buf, len, "--`%c", unit);
	else
		snprintf(buf, len, "%d`%c", (int)lround(S.fahrenheit ? c * 9 / 5 + 32 : c), unit);
}

/* Time covered by a full graph: "54S", "4M", "2H". */
static void format_span(char *buf, size_t len)
{
	long s = (long)HIST_LEN * S.interval_ms / 1000;

	if (s < 120)
		snprintf(buf, len, "%ldS", s);
	else if (s < 7200)
		snprintf(buf, len, "%ldM", s / 60);
	else
		snprintf(buf, len, "%ldH", s / 3600);
}

/*
 * Temperature graph scale: from just below the coolest reading, at least
 * TEMP_SPAN degrees high, in steps of 10 C so it does not jump around.
 */
static void temp_scale(float *lo, float *hi)
{
	float mn = INFINITY, mx = -INFINITY;
	int i;

	for (i = 0; i < S.temp.count; i++) {
		float v = history_get(&S.temp, i);

		if (v != v)
			continue;
		if (v < mn)
			mn = v;
		if (v > mx)
			mx = v;
	}
	if (mn > mx) {
		*lo = TEMP_COOL;
		*hi = TEMP_HOT;
		return;
	}
	*lo = floorf((mn - 5) / 10) * 10;
	*hi = ceilf(mx / 10) * 10;
	if (*hi < *lo + TEMP_SPAN)
		*hi = *lo + TEMP_SPAN;
}

static void redraw(void)
{
	char left[32], right[32];
	float lo, hi;

	display_clear(&S.d, S.bg);

	if (S.screen == SCR_CPU)
		snprintf(right, sizeof(right), "%d", S.c.n);
	else
		format_span(right, sizeof(right));
	text_left(LABEL_Y, screen_labels[S.screen], S.fg);
	text_right(LABEL_Y, right, S.dim);

	switch (S.screen) {
	case SCR_CPU:
		grid_draw(&S.d, &S.g, &S.c, S.square);
		break;
	case SCR_LOAD:
		history_draw(&S.d, &S.load, TEXT_X0, AREA_Y, HIST_LEN, AREA_H, 0, 100, S.fg, S.bg);
		break;
	case SCR_TEMP:
		temp_scale(&lo, &hi);
		history_draw(&S.d, &S.temp, TEXT_X0, AREA_Y, HIST_LEN, AREA_H, lo, hi, S.fg, S.bg);
		break;
	case SCR_GPU:
		history_draw(&S.d, &S.gpu, TEXT_X0, AREA_Y, HIST_LEN, AREA_H, 0, 100, S.fg, S.bg);
		break;
	default:
		break;
	}

	if (S.screen == SCR_GPU) {
		float b = S.gpu.count ? history_get(&S.gpu, 0) : -1;

		if (b >= 0)
			snprintf(left, sizeof(left), "%d%%", (int)lround(b));
		else
			snprintf(left, sizeof(left), "--%%");
		if (S.vram >= 0)
			snprintf(right, sizeof(right), "VR %d%%", (int)lround(S.vram));
		else
			right[0] = '\0';
	} else {
		snprintf(left, sizeof(left), "%d%%", (int)lround(S.c.all.pct));
		if (S.have_temp)
			format_temp(right, sizeof(right), S.celsius);
		else
			right[0] = '\0';
	}
	text_left(STATUS_Y, left, S.fg);
	text_right(STATUS_Y, right, S.fg);

	display_flush(&S.d);
}

static void sample(void)
{
	double busy;

	cpu_sample(&S.c);
	history_push(&S.load, S.c.all.pct);
	if (S.have_temp) {
		S.celsius = sensor_read();
		history_push(&S.temp, (float)S.celsius);
	}
	if (S.have_gpu) {
		gpu_read(&busy, &S.vram);
		history_push(&S.gpu, busy >= 0 ? (float)busy : NAN);
	}
}

static void handle_x_events(void)
{
	XEvent ev;

	while (XPending(S.d.dpy)) {
		XNextEvent(S.d.dpy, &ev);
		switch (ev.type) {
		case Expose:
			display_flush(&S.d);
			break;
		case ButtonPress:
			switch (ev.xbutton.button) {
			case Button1:
			case Button5:
				next_screen(1);
				break;
			case Button4:
				next_screen(-1);
				break;
			case Button3:
				S.fahrenheit = !S.fahrenheit;
				break;
			default:
				continue;
			}
			redraw();
			break;
		case DestroyNotify:
			S.quit = 1;
			break;
		}
	}
}

/* ---- main ------------------------------------------------------------- */

static void usage(void)
{
	printf("wmcoremap " VERSION " - per-core CPU heatmap dockapp for Window Maker\n\n"
	       "usage: wmcoremap [options]\n\n"
	       "  -display DISPLAY    X display to use\n"
	       "  -c, -celsius        show temperatures in Celsius (default)\n"
	       "  -f, -fahrenheit     show temperatures in Fahrenheit\n"
	       "  -interval MS        update period in milliseconds (default 1000)\n"
	       "  -screen NAME        first screen: cpu, load, temp, gpu (default cpu)\n"
	       "  -sensor PATH        temperature input file (default: auto-detect)\n"
	       "  -fg COLOR           text colour (default #20B2AE)\n"
	       "  -bg COLOR           background colour (default #202020)\n"
	       "  -size small|medium|large\n"
	       "                      LED size (default large): small is 4x4 pixels\n"
	       "                      like wmsysmon, medium 6x6, large 9x9; LEDs\n"
	       "                      shrink if the CPUs do not fit\n"
	       "  -square             draw square cells instead of round LEDs\n"
	       "  -fake N             simulate N CPUs with synthetic load (testing)\n"
	       "  -h, -help           show this help\n\n"
	       "mouse: left / wheel = next / previous screen, right = Celsius / Fahrenheit\n");
}

static void on_signal(int sig)
{
	(void)sig;
	S.quit = 1;
}

int main(int argc, char **argv)
{
	const char *dpyname = NULL, *fg = "#20B2AE", *bg = "#202020", *sensor = NULL;
	int fake = 0, i;
	double next_tick;

	S.interval_ms = 1000;
	S.dot = DOT_LARGE;
	for (i = 1; i < argc; i++) {
		const char *a = argv[i];
		int more = i + 1 < argc;

		if (!strcmp(a, "-display") && more)
			dpyname = argv[++i];
		else if (!strcmp(a, "-c") || !strcmp(a, "-celsius"))
			S.fahrenheit = 0;
		else if (!strcmp(a, "-f") || !strcmp(a, "-fahrenheit"))
			S.fahrenheit = 1;
		else if (!strcmp(a, "-interval") && more) {
			S.interval_ms = atoi(argv[++i]);
			if (S.interval_ms < 100 || S.interval_ms > 3600000) {
				fprintf(stderr, "wmcoremap: interval must be 100..3600000 ms\n");
				return 1;
			}
		} else if (!strcmp(a, "-screen") && more) {
			const char *m = argv[++i];
			int k;

			for (k = 0; k < SCR_COUNT && strcmp(m, screen_names[k]); k++)
				;
			if (k == SCR_COUNT) {
				fprintf(stderr, "wmcoremap: unknown screen '%s'\n", m);
				return 1;
			}
			S.screen = (enum screen)k;
		} else if (!strcmp(a, "-sensor") && more)
			sensor = argv[++i];
		else if (!strcmp(a, "-fg") && more)
			fg = argv[++i];
		else if (!strcmp(a, "-bg") && more)
			bg = argv[++i];
		else if (!strcmp(a, "-size") && more) {
			const char *m = argv[++i];
			int k;

			for (k = 0; k < 3 && strcmp(m, size_names[k]); k++)
				;
			if (k == 3) {
				fprintf(stderr, "wmcoremap: unknown size '%s'\n", m);
				return 1;
			}
			S.dot = size_dots[k];
		} else if (!strcmp(a, "-square"))
			S.square = 1;
		else if (!strcmp(a, "-fake") && more) {
			fake = atoi(argv[++i]);
			if (fake < 1 || fake > 1024) {
				fprintf(stderr, "wmcoremap: -fake takes 1..1024\n");
				return 1;
			}
		} else if (!strcmp(a, "-h") || !strcmp(a, "-help") || !strcmp(a, "--help")) {
			usage();
			return 0;
		} else {
			fprintf(stderr, "wmcoremap: bad option '%s' (try -help)\n", a);
			return 1;
		}
	}

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	if (cpu_init(&S.c, fake) < 0)
		return 1;
	S.have_temp = sensor_init(sensor) == 0;
	if (sensor && !S.have_temp)
		fprintf(stderr, "wmcoremap: cannot read sensor %s\n", sensor);
	S.have_gpu = gpu_init() == 0;
	S.celsius = NAN;
	S.vram = -1;
	if (!screen_available(S.screen))
		next_screen(1);

	if (display_open(&S.d, dpyname, argc, argv) < 0)
		return 1;
	if (display_parse_color(&S.d, fg, &S.fg) < 0 ||
	    display_parse_color(&S.d, bg, &S.bg) < 0) {
		fprintf(stderr, "wmcoremap: bad colour\n");
		return 1;
	}
	/* dimmed text: halfway between fg and bg */
	S.dim = (((S.fg >> 1) & 0x7f7f7f) + ((S.bg >> 1) & 0x7f7f7f));

	ramp_init();
	grid_layout(&S.g, S.c.n, TEXT_X0, AREA_Y, TEXT_X1 - TEXT_X0, AREA_H, S.dot);
	sample();
	redraw();

	next_tick = now() + S.interval_ms / 1000.0;
	while (!S.quit) {
		struct pollfd fd = { ConnectionNumber(S.d.dpy), POLLIN, 0 };
		int timeout;

		handle_x_events();
		timeout = (int)((next_tick - now()) * 1000);
		if (timeout < 0)
			timeout = 0;
		if (poll(&fd, 1, timeout) < 0 && errno != EINTR)
			break;
		if (now() >= next_tick) {
			next_tick += S.interval_ms / 1000.0;
			if (next_tick < now())
				next_tick = now() + S.interval_ms / 1000.0;
			sample();
			redraw();
		}
	}

	cpu_free(&S.c);
	display_close(&S.d);
	return 0;
}
