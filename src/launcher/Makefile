CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra $(shell pkg-config --cflags gtk+-3.0 gtk-layer-shell-0 gio-2.0)
LIBS ?= $(shell pkg-config --libs gtk+-3.0 gtk-layer-shell-0 gio-2.0)

TARGET = ../../bin/yogabook-launcher

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c $(LIBS) -o $(TARGET)
	chmod +x $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
