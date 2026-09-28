CC ?= gcc
CFLAGS ?= -O2 -pipe -Wall -std=gnu99 -DWITH_WAYLAND_SHM -DLAYOUT=\"layout.yogabook.h\" -DKEYMAP=\"keymap.mobintl.h\" -DVERSION=\"0.21\" -D_XOPEN_SOURCE=700
PKGS = wayland-client xkbcommon pangocairo
CFLAGS += $(shell pkg-config --cflags $(PKGS)) -I. -I./build
LDFLAGS += $(shell pkg-config --libs $(PKGS)) -lm -lutil -lrt

WAYLAND_XMLS = $(wildcard proto/*.xml)
PROTO_HDRS = $(WAYLAND_XMLS:.xml=-client-protocol.h)
PROTO_SRCS = $(WAYLAND_XMLS:.xml=-client-protocol.c)
PROTO_OBJS = $(PROTO_SRCS:.c=.o)

SRCS = drw.c keyboard.c main.c os-compatibility.c shm_open.c
OBJS = $(SRCS:.c=.o) $(PROTO_OBJS)

TARGET = ../../bin/wvkbd

all: $(TARGET)

build/config.h: config.yogabook.h
	mkdir -p build
	cp $< $@

proto/%-client-protocol.c: proto/%.xml
	wayland-scanner private-code < $< > $@

proto/%-client-protocol.h: proto/%.xml
	wayland-scanner client-header < $< > $@

%.o: %.c build/config.h $(PROTO_HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): build/config.h $(PROTO_HDRS) $(OBJS)
	mkdir -p ../../bin
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	rm -rf build $(OBJS) $(PROTO_HDRS) $(PROTO_SRCS)

.PHONY: all clean
