TARGET = volume
OBJS = main.o
BUILD_PRX = 1
PRX_EXPORTS = exports.exp
USE_KERNEL_LIBC = 1
USE_KERNEL_LIBS = 1
CFLAGS = -Os -G0 -Wall -Wextra
LDFLAGS = -nostartfiles
LIBDIR = ../../Source/SysPSP/PRX/DveMgr
LIBS = -lpspimpose_driver
PSP_FW_VERSION = 500
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
