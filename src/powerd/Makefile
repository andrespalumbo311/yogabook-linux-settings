CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra $(shell pkg-config --cflags libsystemd)
LDFLAGS ?= $(shell pkg-config --libs libsystemd)

BIN_DIR = ../../bin
TARGET = $(BIN_DIR)/yogabook-powerd

all: $(TARGET)

$(TARGET): main.c
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@
	chmod +x $@

clean:
	rm -f $(TARGET)

.PHONY: all clean
