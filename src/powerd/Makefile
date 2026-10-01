CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

BIN_DIR = ../../bin
TARGET = $(BIN_DIR)/yogabook-powerd

all: $(TARGET)

$(TARGET): main.c
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@
	chmod +x $@

clean:
	rm -f $(TARGET)

.PHONY: all clean
