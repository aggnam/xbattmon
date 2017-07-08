PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

# If you have a second battery, specify PATH_BAT1_CAP too.
CPPFLAGS = \
	-DVERSION=\"${VERSION}\" \
	-DPATH_BAT0_CAP=\"/sys/class/power_supply/BAT0/capacity\" \
	-DPATH_AC_ONLINE=\"/sys/class/power_supply/AC/online\"
LDLIBS = -lX11
