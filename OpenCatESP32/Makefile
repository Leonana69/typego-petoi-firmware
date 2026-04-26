FQBN    ?= esp32:esp32:esp32:PartitionScheme=huge_app
SKETCH  ?= .
BUILD   ?= build
BAUD    ?= 115200

# Auto-detect an ESP32 serial port (CP210x, CH340/CH343, FTDI, USB-modem).
# Override with: make flash PORT=/dev/cu.usbserial-XXXX
PORT ?= $(shell ls /dev/cu.SLAB_USBtoUART /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.usbmodem* 2>/dev/null | head -1)

.DEFAULT_GOAL := build
.PHONY: build flash monitor clean help

build:
	arduino-cli compile --fqbn $(FQBN) --build-path $(BUILD) $(SKETCH)

flash:
	@test -n "$(PORT)" || { echo "error: no serial port detected. Plug in the board or set PORT=/dev/cu.xxx"; exit 1; }
	arduino-cli upload --fqbn $(FQBN) --input-dir $(BUILD) -p $(PORT) $(SKETCH)

monitor:
	@test -n "$(PORT)" || { echo "error: no serial port detected. Set PORT=/dev/cu.xxx"; exit 1; }
	arduino-cli monitor -p $(PORT) -c baudrate=$(BAUD)

clean:
	rm -rf $(BUILD)

help:
	@echo "targets:"
	@echo "  build    compile the sketch"
	@echo "  flash    build then upload (auto-detects PORT, override with PORT=...)"
	@echo "  monitor  open the serial monitor at $(BAUD) baud"
	@echo "  clean    remove $(BUILD)/"
	@echo ""
	@echo "vars: FQBN=$(FQBN)"
	@echo "      PORT=$(PORT)"
	@echo "      BAUD=$(BAUD)"
