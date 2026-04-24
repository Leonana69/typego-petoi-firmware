FQBN := esp32:esp32:XIAO_ESP32S3:PSRAM=opi
SKETCH := .

all: build

build:
	arduino-cli compile --fqbn $(FQBN) $(SKETCH)

flash:
	-make close
	@usbport=$$(ls /dev/cu.usbmodem* | head -1); \
	if [ -z "$$usbport" ]; then \
		echo "No USB port found"; \
	else \
		arduino-cli upload --fqbn $(FQBN) -p $$usbport $(SKETCH); \
	fi
	script -q /dev/null screen /dev/cu.usbmodem* 115200 | ts '[%H:%M:%.S]'

open:
	@screen -r

close:
	@echo "Closing screen"
	@screen -X -S $$(screen -ls | grep Detached | awk '{print $$1}') quit

.PHONY: all build flash open close
