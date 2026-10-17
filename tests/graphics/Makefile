TARGET = projection-tests
OBJS = psp_main.o
BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = FrogN64 Projection Tests
INCDIR = ../../Source
CFLAGS = -O2 -G0 -Wall -Wextra -DDAEDALUS_PSP=1 -DDAEDALUS_ENDIAN_LITTLE=1
CXXFLAGS = $(CFLAGS) -std=gnu++20
LIBS = -lstdc++ -lm
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
psp_main.o: psp_main.cpp projection_cases.h ../../Source/HLEGraphics/VertexProjectionCache.h
