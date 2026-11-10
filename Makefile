APP_ID  = io.github.aloran_dev.Gmk67Rgb
VERSION = 1.0.0

PREFIX  ?= /usr/local
DESTDIR ?=
# Empty to skip installing the rule (Flatpak, user install)
UDEVDIR ?= $(PREFIX)/lib/udev/rules.d

DATADIR = $(DESTDIR)$(PREFIX)/share

install:
	install -Dm644 gmk67.py       $(DATADIR)/gmk67-rgb/gmk67.py
	install -Dm644 colorpicker.py $(DATADIR)/gmk67-rgb/colorpicker.py
	install -Dm755 gmk67-rgb      $(DATADIR)/gmk67-rgb/gmk67-rgb
	install -Dm755 fx.py          $(DATADIR)/gmk67-rgb/fx.py
	install -d $(DESTDIR)$(PREFIX)/bin
	ln -sf ../share/gmk67-rgb/gmk67-rgb $(DESTDIR)$(PREFIX)/bin/gmk67-rgb
	ln -sf ../share/gmk67-rgb/fx.py $(DESTDIR)$(PREFIX)/bin/gmk67-fx
	install -Dm644 data/$(APP_ID).desktop      $(DATADIR)/applications/$(APP_ID).desktop
	install -Dm644 data/$(APP_ID).metainfo.xml $(DATADIR)/metainfo/$(APP_ID).metainfo.xml
	install -Dm644 data/$(APP_ID).svg          $(DATADIR)/icons/hicolor/scalable/apps/$(APP_ID).svg
ifneq ($(UDEVDIR),)
	install -Dm644 70-gmk67.rules $(DESTDIR)$(UDEVDIR)/70-gmk67.rules
endif

uninstall:
	rm -rf $(DATADIR)/gmk67-rgb
	rm -f $(DESTDIR)$(PREFIX)/bin/gmk67-rgb $(DESTDIR)$(PREFIX)/bin/gmk67-fx \
	      $(DATADIR)/applications/$(APP_ID).desktop \
	      $(DATADIR)/metainfo/$(APP_ID).metainfo.xml \
	      $(DATADIR)/icons/hicolor/scalable/apps/$(APP_ID).svg
ifneq ($(UDEVDIR),)
	rm -f $(DESTDIR)$(UDEVDIR)/70-gmk67.rules
endif

deb:
	packaging/deb/build-deb.sh $(VERSION)

.PHONY: install uninstall deb
