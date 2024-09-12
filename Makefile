###########
# WARNING #
###########
# arduino-cli is dumb and will try to compile all files with a *.cpp, *.cxx, *.c, file extension
# and will then choke on c++ files that aren't sketches.
# there is no way to specify what files it attempts to compile.
# so every such file that isn't a sketch must be enclosed within '#ifndef ARDUINO ... #endif' guards

all: sketch control

.phony: upload

# comma-separated list of dirs
LIBDIRS=./Multi_Channel_Relay_Arduino_Library-master

PORT!=arduino-cli board list | grep micro | cut -d' ' -f 1

# simple c++ serial comms library
ToolDAQPath=/home/gad/GDConcMeasure/ToolDAQ
SerialCppLib = -L $(ToolDAQPath)/SerialCpp -lserial
SerialCppInclude = -I $(ToolDAQPath)/SerialCpp

sketch: gad_micro.ino
	arduino-cli compile ./gad_micro.ino -b arduino:avr:micro --export-binaries --library $(LIBDIRS) -p $(PORT)
	# --output-dir ./bin --build-path ./bin --build-cache-path ./bin

ArduinoControl.o: src/ArduinoControl.cpp include/ArduinoControl.h
	g++ -x c++ -g -std=c++11 -fPIC -c $< -o $@ -I ./include $(SerialCppInclude)
	#g++ -x c++ -g -std=c++11 -fPIC -c $< -o $@ -I ./include $(SerialCppInclude)

control: src/main.cpp ArduinoControl.o
	g++ -g -std=c++11 $^ -o $@ -I ./include $(SerialCppInclude) $(SerialCppLib)

upload: build/arduino.avr.micro/gad_micro.ino.with_bootloader.bin
	arduino-cli upload ./gad_micro.ino -b arduino:avr:micro -p $(PORT)

clean:
	rm -rf ./build control ./*.o
