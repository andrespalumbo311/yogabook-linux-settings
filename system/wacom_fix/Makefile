CC ?= gcc
CFLAGS ?= -O2 -shared -fPIC -Wall -Wextra -DWLR_USE_UNSTABLE
CFLAGS += $(shell pkg-config --cflags wlroots-0.20 2>/dev/null || true)
TARGET = wacom_fix.so
DESTDIR ?= /usr/lib

all: $(TARGET)

$(TARGET): wacom_fix.c
	$(CC) $(CFLAGS) -o $@ $<

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)/$(TARGET)
	@if ! grep -q "$(DESTDIR)/$(TARGET)" /etc/ld.so.preload 2>/dev/null; then \
		echo "$(DESTDIR)/$(TARGET)" >> /etc/ld.so.preload; \
	fi

clean:
	rm -f $(TARGET)

.PHONY: all install clean
