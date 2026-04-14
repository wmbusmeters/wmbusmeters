.PHONY: all clean clean-all help

SCRIPT_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

all:
	@bash $(SCRIPT_DIR)build.sh

clean:
	rm -rf $(SCRIPT_DIR)dist $(SCRIPT_DIR)build $(SCRIPT_DIR)build

clean-all: clean
	rm -rf $(SCRIPT_DIR)../3rdparty/libxml2/.libs

help:
	@echo "Targets:"
	@echo "  make          - Build wmbusmeters.wasm + wmbusmeters.js"
	@echo "  make clean    - Delete dist/ (keeps libxml2 cache)"
	@echo "  make clean-all - Delete dist/ and libxml2 build cache"