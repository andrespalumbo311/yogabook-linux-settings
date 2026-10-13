CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

BIN_DIR = ../../bin

TARGETS = \
	$(BIN_DIR)/toggle-launcher \
	$(BIN_DIR)/toggle-control-center \
	$(BIN_DIR)/close-window \
	$(BIN_DIR)/ws-status \
	$(BIN_DIR)/yogabook-mako-status \
	$(BIN_DIR)/toggle-keyboard \
	$(BIN_DIR)/mango-workspace-watcher \
	$(BIN_DIR)/yogabook-battery-status \
	$(BIN_DIR)/yogabook-wifi-status \
	$(BIN_DIR)/yogabook-bt-audio-watcher

all: $(TARGETS)

$(BIN_DIR)/yogabook-bt-audio-watcher: yogabook-bt-audio-watcher.c
	$(CC) $(CFLAGS) $< -o $@ $(shell pkg-config --cflags --libs libpipewire-0.3)
	chmod +x $@

$(BIN_DIR)/mango-workspace-watcher: mango-workspace-watcher.c
	$(CC) $(CFLAGS) $< -o $@
	chmod +x $@

$(BIN_DIR)/toggle-keyboard: toggle-keyboard.c
	$(CC) $(CFLAGS) $< -o $@
	chmod +x $@

$(BIN_DIR)/toggle-launcher: toggle-launcher.c
	$(CC) $(CFLAGS) $< -o $@
	chmod +x $@

$(BIN_DIR)/toggle-control-center: toggle-control-center.c
	$(CC) $(CFLAGS) $< -o $@
	chmod +x $@

$(BIN_DIR)/close-window: close-window.c
	$(CC) $(CFLAGS) $< -o $@
	chmod +x $@

$(BIN_DIR)/ws-status: ws-status.c
	$(CC) $(CFLAGS) $< -o $@
	chmod +x $@

$(BIN_DIR)/yogabook-mako-status: yogabook-mako-status.c
	$(CC) $(CFLAGS) $< -o $@
	chmod +x $@

$(BIN_DIR)/yogabook-wifi-status: yogabook-wifi-status.c
	$(CC) $(CFLAGS) $< -o $@ -lsystemd
	chmod +x $@

$(BIN_DIR)/yogabook-battery-status: yogabook-battery-status.c
	$(CC) $(CFLAGS) $< -o $@ -lm
	chmod +x $@

clean:
	rm -f $(TARGETS)

.PHONY: all clean
