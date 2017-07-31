/* See LICENSE file for copyright and license details. */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "arg.h"
#include "util.h"

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

enum {
	BOTTOM,
	TOP,
	LEFT,
	RIGHT
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
int timeout;
int blink;

#include "config.h"

unsigned long cmap[LEN(colors)];

void
setup(void)
{
	XSetWindowAttributes attr;
	XColor color, exact;
	XTextProperty text;
	Atom wintype, wintype_dock;
	static char *name = "xbattmon";
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

	if (placement == BOTTOM || placement == TOP) {
		if (thickness > height)
			thickness = height;
	} else {
		if (thickness > width)
			thickness = width;
	}

	switch (placement) {
	case BOTTOM:
		barx = 0;
		bary = height - thickness;
		barwidth = width;
		barheight = thickness;
		break;
	case TOP:
		barx = 0;
		bary = 0;
		barwidth = width;
		barheight = thickness;
		break;
	case LEFT:
		barx = 0;
		bary = 0;
		barwidth = thickness;
		barheight = height;
		break;
	case RIGHT:
		barx = width - thickness;
		bary = 0;
		barwidth = thickness;
		barheight = height;
		break;
	}

	winbar = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), barx, bary, barwidth,
				     barheight, 0, BlackPixel(dpy, screen),
				     WhitePixel(dpy, screen));

	attr.override_redirect = True;
	XChangeWindowAttributes(dpy, winbar, CWOverrideRedirect, &attr);

	XStringListToTextProperty(&name, 1, &text);
	XSetWMName(dpy, winbar, &text);

	wintype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	wintype_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	XChangeProperty(dpy, winbar, wintype, XA_ATOM, 32,
	    PropModeReplace, (unsigned char *)&wintype_dock, 1);

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

	critical = critical * maxcap / 100;
}

void
redraw(void)
{
	int pos;
	unsigned long done, left;
	static struct timespec oldtp = { 0 };
	struct timespec tp;
	unsigned int delta;

	if (state == AC_OFF && batcap <= critical) {
		clock_gettime(CLOCK_MONOTONIC, &tp);
		delta = (tp.tv_sec * 1000 + tp.tv_nsec / 1000000)
		    - (oldtp.tv_sec * 1000 + oldtp.tv_nsec / 1000000);
		if (delta < 500) {
			timeout = 500 - delta;
		} else {
			timeout = 500;
			blink = !blink;
			oldtp = tp;
		}
	} else {
		timeout = 500;
		blink = 0;
	}

	if (placement == BOTTOM || placement == TOP)
		pos = barwidth * batcap / maxcap;
	else
		pos = barheight * batcap / maxcap;

	if (state == AC_ON) {
		done = cmap[COLOR_BAT_CHARGED];
		left = cmap[COLOR_BAT_LEFT2CHARGE];
	} else {
		done = cmap[!blink ? COLOR_BAT_LEFT2DRAIN : COLOR_BAT_DRAINED];
		left = cmap[COLOR_BAT_DRAINED];
	}

	if (placement == BOTTOM || placement == TOP) {
		XSetForeground(dpy, gcbar, done);
		XFillRectangle(dpy, winbar, gcbar, 0, 0, pos, thickness);
		XSetForeground(dpy, gcbar, left);
		XFillRectangle(dpy, winbar, gcbar, pos, 0, barwidth, thickness);
	} else {
		XSetForeground(dpy, gcbar, done);
		XFillRectangle(dpy, winbar, gcbar, 0, barheight - pos, thickness, barheight);
		XSetForeground(dpy, gcbar, left);
		XFillRectangle(dpy, winbar, gcbar, 0, 0, thickness, barheight - pos);
	}

	if (transparent) {
		if (!blink)
			XMapWindow(dpy, winbar);
		else
			XUnmapWindow(dpy, winbar);
		XSetForeground(dpy, gcbar, done);
		if (placement == BOTTOM || placement == TOP) {
			XResizeWindow(dpy, winbar, pos, thickness);
			XFillRectangle(dpy, winbar, gcbar, 0, 0, pos, thickness);
		} else {
			XMoveResizeWindow(dpy, winbar, barx, barheight - pos, thickness, pos);
			XFillRectangle(dpy, winbar, gcbar, 0, 0, thickness, barheight);
		}
	}

	XFlush(dpy);
}

#ifdef __OpenBSD__
#include <sys/ioctl.h>
#include <fcntl.h>
#include <machine/apmvar.h>

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
	if (r < 0)
		err(1, "APM_IOC_GETPOWER %s", PATH_APM);
	close(fd);

	batcap = info.battery_life;
	if (batcap > maxcap)
		batcap = maxcap;

	if (info.ac_state == APM_AC_UNKNOWN)
		warnx("unknown AC state");

	state = info.ac_state == APM_AC_ON ? AC_ON : AC_OFF;
}
#elif __DragonFly__
#include <sys/ioctl.h>
#include <fcntl.h>
#include <machine/apm_bios.h>

void
pollbat(void)
{
	struct apm_info ai;
	int r;
	int fd;

	fd = open(PATH_APM, O_RDONLY);
	if (fd < 0)
		err(1, "open %s", PATH_APM);
	r = ioctl(fd, APMIO_GETINFO, &ai);
	if (r < 0)
		err(1, "APMIO_GETINFO %s", PATH_APM);
	batcap = ai.ai_batt_life;
	if (batcap > maxcap)
		batcap = maxcap;
	state = ai.ai_acline ? AC_ON : AC_OFF;
	close(fd);
}
#elif __linux__
void
pollbat(void)
{
	FILE *fp;
	char *path_list_bat[] = PATH_LIST_BAT;
	char path_energy_full[PATH_MAX];
	char path_energy_now[PATH_MAX];
	int total_energy_full = 0;
	int total_energy_now = 0;
	int energy_full;
	int energy_now;
	int acon;
	int i;

	for (i = 0; i < LEN(path_list_bat); i++) {
		snprintf(path_energy_full, sizeof(path_energy_full),
			 "%s/energy_full", path_list_bat[i]);
		fp = fopen(path_energy_full, "r");
		if (!fp) {
			warn("fopen %s", path_energy_full);
			continue;
		}
		fscanf(fp, "%d", &energy_full);
		fclose(fp);

		snprintf(path_energy_now, sizeof(path_energy_now),
			 "%s/energy_now", path_list_bat[i]);
		fp = fopen(path_energy_now, "r");
		if (!fp) {
			warn("fopen %s", path_energy_now);
			continue;
		}
		fscanf(fp, "%d", &energy_now);
		fclose(fp);

		total_energy_full += energy_full / 1000;
		total_energy_now += energy_now / 1000;
	}

	if (total_energy_full > 0)
		batcap = 100 * total_energy_now / total_energy_full;
	else
		batcap = 0;

	if (batcap > maxcap)
		batcap = maxcap;

	fp = fopen(PATH_AC_ONLINE, "r");
	if (!fp)
		err(1, "fopen %s", PATH_AC_ONLINE);
	fscanf(fp, "%d", &acon);
	fclose(fp);

	state = acon ? AC_ON : AC_OFF;
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
		switch (poll(pfd, 1, timeout)) {
		case -1:
			if (errno != EINTR)
				err(1, "poll");
			break;
		case 0:
			pollbat();
			redraw();
			break;
		default:
			if ((pfd[0].revents & (POLLERR | POLLHUP | POLLNVAL)))
				errx(1, "bad fd: %d", pfd[0].fd);
			while (XCheckIfEvent(dpy, &ev, evpredicate, NULL)) {
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
	fprintf(stderr, "usage: %s [-c capacity] [-p bottom | top | left | right] [-t thickness] [-v]\n"
		" -c\tspecify battery capacity\n"
		" -p\tbar placement\n"
		" -t\tbar thickness\n"
		" -v\tshow version\n",
		argv0);
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
	case 'p':
		arg = EARGF(usage());
		if (strcmp(arg, "bottom") == 0)
			placement = BOTTOM;
		else if (strcmp(arg, "top") == 0)
			placement = TOP;
		else if (strcmp(arg, "left") == 0)
			placement = LEFT;
		else if (strcmp(arg, "right") == 0)
			placement = RIGHT;
		else
			errx(1, "%s: invalid placement", arg);
		break;
	case 't':
		arg = EARGF(usage());
		thickness = strtonum(arg, 1, INT_MAX, &errstr);
		if (errstr)
			errx(1, "%s: %s", arg, errstr);
		break;
	case 'v':
		printf("xbattmon-%s\n", VERSION);
		return 0;
	default:
		usage();
	} ARGEND;

	if (argc)
		usage();

	setup();
	pollbat();
	redraw();
	loop();
	return 0;
}
