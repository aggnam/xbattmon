PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

CPPFLAGS = \
	-DVERSION=\"${VERSION}\" \
	-DPATH_LIST_BAT='{ "/sys/class/power_supply/BAT0", "/sys/class/power_supply/BAT1" }' \
	-DPATH_AC_ONLINE=\"/sys/class/power_supply/AC/online\"
LDLIBS = -lX11
