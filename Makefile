PREFIX ?= /usr/local
BINARY_DAEMON = keystroke
BINARY_STATS  = keystroke-stats
SERVICE       = keystats.service
SYSTEMD_USER  = /usr/lib/systemd/user

.PHONY: all build install uninstall status logs clean

all: build

build:
	cmake -S . -B build
	cmake --build build

install: build
	sudo cp build/$(BINARY_DAEMON) $(PREFIX)/bin/$(BINARY_DAEMON)
	sudo cp build/$(BINARY_STATS)  $(PREFIX)/bin/$(BINARY_STATS)
	sudo cp $(SERVICE) $(SYSTEMD_USER)/$(SERVICE)

uninstall:
	systemctl --user stop $(BINARY_DAEMON) || true
	systemctl --user disable $(BINARY_DAEMON) || true
	sudo rm -f $(SYSTEMD_USER)/$(SERVICE)
	sudo rm -f $(PREFIX)/bin/$(BINARY_DAEMON)
	sudo rm -f $(PREFIX)/bin/$(BINARY_STATS)

status:
	systemctl --user status $(BINARY_DAEMON)

logs:
	journalctl --user -u $(BINARY_DAEMON) -f

clean:
	rm -rf build
