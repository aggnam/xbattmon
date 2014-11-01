/*
 * Copyright (c) 2014 sin <sin@2f30.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/ioctl.h>
#include <sys/select.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <machine/apmvar.h>
#include <X11/Xlib.h>

#include "arg.h"

#define LEN(x) (sizeof(x) / sizeof(*(x)))

enum {
	AC_ON,
	AC_OFF
};

enum {
	COLOR_BATT_CHARGED,
	COLOR_BATT_LEFT2CHARGE,
	COLOR_BATT_DRAINED,
	COLOR_BATT_LEFT2DRAIN
};

char *argv0;
Display *dpy;
Window winbar;
GC gcbar;
int barx;
int bary;
unsigned int barwidth;
unsigned int barheight;
int state;			/* AC_ON or AC_OFF */
int howmuch;			/* 0 if completely discharged or `maxcap' if completely charged */

#include "config.h"

unsigned long cmap[LEN(colors)];

void
setup(void)
{
	XSetWindowAttributes attr;
	XColor color, exact;
	int r;
	int screen;
	unsigned int width, height;
	size_t i;

	dpy = XOpenDisplay(NULL);
	if (!dpy)
		errx(1, "cannot open display");

	screen = DefaultScreen(dpy);
	width = DisplayWidth(dpy, screen);
	height = DisplayHeight(dpy, screen);

	if (bottom == 1) {	
		barx = 0;
		bary = height - thickness;
		barwidth = width;
		barheight = thickness;
	} else {
		barx = 0;
		bary = 0;
		barwidth = width;
		barheight = thickness;
	}

	winbar = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), barx, bary, barwidth,
				     barheight, 0, BlackPixel(dpy, screen),
				     WhitePixel(dpy, screen));

	attr.override_redirect = True;
	XChangeWindowAttributes(dpy, winbar, CWOverrideRedirect, &attr);

	XSelectInput(dpy, winbar, ExposureMask | VisibilityChangeMask);
	XMapRaised(dpy, winbar);

	gcbar = XCreateGC(dpy, winbar, 0, 0);

	for (i = 0; i < LEN(colors); i++) {
		r = XAllocNamedColor(dpy, DefaultColormap(dpy, 0),
				     colors[i], &color, &exact);
		if (r == 0)
			errx(1, "cannot allocate color resources");
		cmap[i] = color.pixel;
	}
}

void
redraw(void)
{
	int pos;

	pos = barwidth * howmuch / maxcap;
	switch (state) {
	case AC_ON:
		XSetForeground(dpy, gcbar, cmap[COLOR_BATT_CHARGED]);
		XFillRectangle(dpy, winbar, gcbar, 0, 0, pos, thickness);
		XSetForeground(dpy, gcbar, cmap[COLOR_BATT_LEFT2CHARGE]);
		XFillRectangle(dpy, winbar, gcbar, pos, 0, barwidth, thickness);
		break;
	case AC_OFF:
		XSetForeground(dpy, gcbar, cmap[COLOR_BATT_LEFT2DRAIN]);
		XFillRectangle(dpy, winbar, gcbar, 0, 0, pos, thickness);
		XSetForeground(dpy, gcbar, cmap[COLOR_BATT_DRAINED]);
		XFillRectangle(dpy, winbar, gcbar, pos, 0, barwidth, thickness);
		break;
	}
	XFlush(dpy);
}

void
recalc(void)
{
	int r;
	int fd;
	int v;
	struct apm_power_info info;

#define _PATH_APM "/dev/apm"
	fd = open(_PATH_APM, O_RDONLY);
	if (fd < 0)
		err(1, "open %s", _PATH_APM);
	r = ioctl(fd, APM_IOC_GETPOWER, &info);
	if (r != 0)
		err(1, "APM_IOC_GETPOWER %s", _PATH_APM);
	close(fd);

	howmuch = info.battery_life;
	if (howmuch > maxcap)
		howmuch = maxcap;

	state = info.ac_state == APM_AC_ON ? AC_ON : AC_OFF;
}

Bool
evpredicate()
{
	return True;
}

void
loop(void)
{
	XEvent ev;
	fd_set rfds;
	struct timeval tv;
	int dpyfd;
	int n;

	dpyfd = ConnectionNumber(dpy);
	while (1) {
		FD_ZERO(&rfds);
		FD_SET(dpyfd, &rfds);
		tv.tv_sec = pollinterval;
		tv.tv_usec = 0;
again:
		n = select(dpyfd + 1, &rfds, NULL, NULL, &tv);
		if (n < 0) {
			if (errno == EINTR)
				goto again;
			err(1, "select");
		}
		if (n == 0) {
			recalc();
			redraw();
			continue;
		}
		if (FD_ISSET(dpyfd, &rfds) != 0) {
			while (XCheckIfEvent(dpy, &ev, evpredicate, NULL) == True) {
				switch (ev.type) {
				case Expose:
					redraw();
					break;
				case VisibilityNotify:
					if (ev.xvisibility.state != VisibilityUnobscured)
						XRaiseWindow(dpy, winbar);
					break;
				}
			}
		}
	}
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-i interval]\n", argv0);
	fprintf(stderr, " -i\tbattery poll interval\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *arg;
	const char *errstr;

	ARGBEGIN {
	case 'i':
		arg = EARGF(usage());
		pollinterval = strtonum(arg, 0, INT_MAX, &errstr);
		if (errstr)
			errx(1, "%s: %s", arg, errstr);
		break;
	default:
		usage();
	} ARGEND;

	setup();
	recalc();
	redraw();
	loop();
	return 0;
}
