FQBN := esp32:esp32:esp32cam
SKETCH := .
PORT := $(shell ls /dev/cu.SLAB_USBtoUART /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.usbmodem* 2>/dev/null | head -1)

all: build

build:
	arduino-cli compile --fqbn $(FQBN) $(SKETCH)

flash:
	@if [ -z "$(PORT)" ]; then echo "No USB port found"; exit 1; fi
	arduino-cli upload --fqbn $(FQBN) -p $(PORT) $(SKETCH)

monitor:
	@if [ -z "$(PORT)" ]; then echo "No USB port found"; exit 1; fi
	arduino-cli monitor -p $(PORT) \
		--config baudrate=115200 \
		--config dtr=off --config rts=off

# Fallback monitor using plain screen. Quit with Ctrl+A then K then y.
screen:
	@if [ -z "$(PORT)" ]; then echo "No USB port found"; exit 1; fi
	screen $(PORT) 9600

# Flash then immediately attach the monitor so you catch boot output.
run: flash monitor

.PHONY: all build flash monitor screen run
