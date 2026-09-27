CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra $(shell pkg-config --cflags libudev)
LIBS ?= $(shell pkg-config --libs libudev) -lm

TARGET = ../../bin/yogabook-autorotate

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c $(LIBS) -o $(TARGET)
	chmod +x $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
