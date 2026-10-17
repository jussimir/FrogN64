TARGET = audio-tests
OBJS = psp_main.o AudioBuffer.o
BUILD_PRX = 1
INCDIR = ../../Source
CFLAGS = -O2 -G0 -Wall -Wextra -DDAEDALUS_PSP=1 -DDAEDALUS_ENDIAN_LITTLE=1
CXXFLAGS = $(CFLAGS) -std=gnu++20
LIBS = -lstdc++ -lpspaudio -lm
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

AudioBuffer.o: ../../Source/HLEAudio/AudioBuffer.cpp ../../Source/HLEAudio/AudioBuffer.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

psp_main.o: psp_main.cpp buffer_cases.h ../../Source/HLEAudio/AudioBuffer.h
