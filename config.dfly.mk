PREFIX = /usr/local
MANPREFIX = $(PREFIX)/man

CPPFLAGS = -DVERSION=\"${VERSION}\"
CFLAGS = -I/usr/local/include ${CPPFLAGS}
LDFLAGS = -L/usr/local/lib
LDLIBS = -lX11
