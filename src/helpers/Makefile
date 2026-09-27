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
	$(BIN_DIR)/mango-workspace-watcher

all: $(TARGETS)

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

clean:
	rm -f $(TARGETS)

.PHONY: all clean
