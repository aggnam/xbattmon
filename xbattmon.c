/* See LICENSE file for copyright and license details. */
#include <sys/ioctl.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>

#ifdef __OpenBSD__
#include <machine/apmvar.h>
#elif __linux__
#include <bsd/stdlib.h>
#endif

#include "arg.h"

#define LEN(x) (sizeof(x) / sizeof(*(x)))

enum {
	AC_ON,
	AC_OFF
};

enum {
	COLOR_BAT_CHARGED,
	COLOR_BAT_LEFT2CHARGE,
	COLOR_BAT_DRAINED,
	COLOR_BAT_LEFT2DRAIN
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
int batcap;			/* 0 if completely discharged or `maxcap' if completely charged */

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

	if (thickness > height)
		thickness = height;

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
	if (raise == 1)
		XMapRaised(dpy, winbar);
	else
		XMapWindow(dpy, winbar);

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

	pos = barwidth * batcap / maxcap;
	switch (state) {
	case AC_ON:
		XSetForeground(dpy, gcbar, cmap[COLOR_BAT_CHARGED]);
		XFillRectangle(dpy, winbar, gcbar, 0, 0, pos, thickness);
		XSetForeground(dpy, gcbar, cmap[COLOR_BAT_LEFT2CHARGE]);
		XFillRectangle(dpy, winbar, gcbar, pos, 0, barwidth, thickness);
		break;
	case AC_OFF:
		XSetForeground(dpy, gcbar, cmap[COLOR_BAT_LEFT2DRAIN]);
		XFillRectangle(dpy, winbar, gcbar, 0, 0, pos, thickness);
		XSetForeground(dpy, gcbar, cmap[COLOR_BAT_DRAINED]);
		XFillRectangle(dpy, winbar, gcbar, pos, 0, barwidth, thickness);
		break;
	}
	XFlush(dpy);
}

#ifdef __OpenBSD__
void
pollbat(void)
{
	struct apm_power_info info;
	int r;
	int fd;

	fd = open(PATH_APM, O_RDONLY);
	if (fd < 0)
		err(1, "open %s", PATH_APM);
	r = ioctl(fd, APM_IOC_GETPOWER, &info);
	if (r != 0)
		err(1, "APM_IOC_GETPOWER %s", PATH_APM);
	close(fd);

	batcap = info.battery_life;
	if (batcap > maxcap)
		batcap = maxcap;

	if (info.ac_state == APM_AC_UNKNOWN)
		warnx("unknown AC state");

	state = info.ac_state == APM_AC_ON ? AC_ON : AC_OFF;
}
#elif __linux__
void
pollbat(void)
{
	FILE *fp;
	int acon;

	fp = fopen(PATH_BAT_CAP, "r");
	if (fp == NULL)
		err(1, "fopen %s", PATH_BAT_CAP);
	fscanf(fp, "%d", &batcap);
	fclose(fp);

	if (batcap > maxcap)
		batcap = maxcap;

	fp = fopen(PATH_AC_ONLINE, "r");
	if (fp == NULL)
		err(1, "fopen %s", PATH_AC_ONLINE);
	fscanf(fp, "%d", &acon);
	fclose(fp);

	state = acon != 0 ? AC_ON : AC_OFF;
}
#endif

Bool
evpredicate()
{
	return True;
}

void
loop(void)
{
	XEvent ev;
	struct pollfd pfd[1];
	int dpyfd;

	dpyfd = ConnectionNumber(dpy);
	while (1) {
		pfd[0].fd = dpyfd;
		pfd[0].events = POLLIN;
		switch (poll(pfd, 1, pollinterval * 1000)) {
		case -1:
			if (errno != EINTR)
				err(1, "poll");
			break;
		case 0:
			pollbat();
			redraw();
			break;
		default:
			if ((pfd[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
				errx(1, "bad fd: %d", pfd[0].fd);
			while (XCheckIfEvent(dpy, &ev, evpredicate, NULL) == True) {
				switch (ev.type) {
				case Expose:
					pollbat();
					redraw();
					break;
				case VisibilityNotify:
					if (ev.xvisibility.state != VisibilityUnobscured)
						if (raise == 1)
							XRaiseWindow(dpy, winbar);
					break;
				}
			}
			break;
		}
	}
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-c capacity] [-i interval] [-p bottom | top] [-t thickness]\n", argv0);
	fprintf(stderr, " -c\tspecify battery capacity\n");
	fprintf(stderr, " -i\tbattery poll interval in seconds\n");
	fprintf(stderr, " -p\tbar placement\n");
	fprintf(stderr, " -t\tbar thickness\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *arg;
	const char *errstr;

	ARGBEGIN {
	case 'c':
		arg = EARGF(usage());
		maxcap = strtonum(arg, 1, 100, &errstr);
		if (errstr)
			errx(1, "%s: %s", arg, errstr);
		break;
	case 'i':
		arg = EARGF(usage());
		pollinterval = strtonum(arg, 1, 60, &errstr);
		if (errstr)
			errx(1, "%s: %s", arg, errstr);
		break;
	case 'p':
		arg = EARGF(usage());
		if (strcmp(arg, "bottom") == 0)
			bottom = 1;
		else if (strcmp(arg, "top") == 0)
			bottom = 0;
		else
			errx(1, "%s: invalid placement", arg);
		break;
	case 't':
		arg = EARGF(usage());
		thickness = strtonum(arg, 1, INT_MAX, &errstr);
		if (errstr)
			errx(1, "%s: %s", arg, errstr);
		break;
	default:
		usage();
	} ARGEND;

	if (argc != 0)
		usage();

	setup();
	pollbat();
	redraw();
	loop();
	return 0;
}
