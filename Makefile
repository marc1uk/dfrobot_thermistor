
all: build

.phony: upload

# comma-separated list of dirs
LIBDIRS=./Multi_Channel_Relay_Arduino_Library-master

PORT!=arduino-cli board list | grep micro | cut -d' ' -f 1

build: gad_micro.ino
	arduino-cli compile ./gad_micro.ino -b arduino:avr:micro --export-binaries --library $(LIBDIRS) -p $(PORT)
	# --output-dir ./bin --build-path ./bin --build-cache-path ./bin

upload: build/arduino.avr.micro/gad_micro.ino.with_bootloader.bin
	arduino-cli upload ./gad_micro.ino -b arduino:avr:micro -p $(PORT)

clean:
	rm -rf ./build
