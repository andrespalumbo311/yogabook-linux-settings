CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra $(shell pkg-config --cflags libsystemd)
LIBS ?= $(shell pkg-config --libs libsystemd) -lm

TARGET = ../../bin/yogabook-autobrightness

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c $(LIBS) -o $(TARGET)
	chmod +x $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
