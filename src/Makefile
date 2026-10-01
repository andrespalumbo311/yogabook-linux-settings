SUBDIRS = control-center launcher autorotate autobrightness helpers wvkbd touch-keyboard powerd

all:
	@for dir in $(SUBDIRS); do \
		echo "==> Building $$dir..."; \
		$(MAKE) -C $$dir all || exit 1; \
	done

clean:
	@for dir in $(SUBDIRS); do \
		echo "==> Cleaning $$dir..."; \
		$(MAKE) -C $$dir clean || exit 1; \
	done

.PHONY: all clean $(SUBDIRS)
