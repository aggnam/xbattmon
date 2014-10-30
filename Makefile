CFLAGS = -I/usr/X11R6/include
LDLIBS = -L/usr/X11R6/lib -lX11
BIN = xbattmon

all: $(BIN)

clean:
	rm -f $(BIN)
