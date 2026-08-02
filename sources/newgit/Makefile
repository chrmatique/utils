SCRIPT := newgit.py
BINARY := newgit
DIST_DIR := dist
BUILD_DIR := build

.PHONY: all build clean install

all: build

build:
	python3 -m PyInstaller --onefile --name $(BINARY) --distpath $(DIST_DIR) --workpath $(BUILD_DIR) --specpath $(BUILD_DIR) $(SCRIPT)

clean:
	rm -rf $(DIST_DIR) $(BUILD_DIR)

install: build
	cp $(DIST_DIR)/$(BINARY) /usr/local/bin/$(BINARY)
