PREFIX = /usr/local
MANPREFIX = $(PREFIX)/man

CPPFLAGS = -DPATH_BAT_CAP='"/sys/class/power_supply/BAT0/capacity"' \
	-DPATH_AC_ONLINE='"/sys/class/power_supply/AC/online"'
LDLIBS = -lX11 -lbsd
