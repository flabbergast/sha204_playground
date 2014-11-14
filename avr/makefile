#
#             LUFA Library
#     Copyright (C) Dean Camera, 2014.
#
#  dean [at] fourwalledcubicle [dot] com
#           www.lufa-lib.org
#
# --------------------------------------
#         LUFA Project Makefile.
# --------------------------------------

# Run "make help" for target help.

## Boards
# xmega avr stick
#MCU          = atxmega128a3u
#ARCH         = XMEGA
#BOARD        = USER
#F_CPU        = 32000000
#F_USB        = 48000000
# atmega32u4 stick
MCU          = atmega32u4
ARCH         = AVR8
BOARD        = OLIMEX32U4
F_CPU        = 16000000
F_USB        = $(F_CPU)
AVRDUDE_PROGRAMMER = avr109
AVRDUDE_PORT = /dev/tty.usbmodemfd111

# Compile setting
OPTIMIZATION = s
TARGET       = sha204_playground
SRC          = $(TARGET).cpp LufaLayer.c Descriptors.c Timer.c $(shell find "SHA204" -name "*.cpp") $(LUFA_SRC_USB) $(LUFA_SRC_USBCLASS)
LUFA_PATH    = LUFA
CC_FLAGS     = -DUSE_LUFA_CONFIG_HEADER -IConfig/
LD_FLAGS     =

# Default target
all:

# Include LUFA build script makefiles
include $(LUFA_PATH)/Build/lufa_core.mk
include $(LUFA_PATH)/Build/lufa_sources.mk
include $(LUFA_PATH)/Build/lufa_build.mk
include $(LUFA_PATH)/Build/lufa_cppcheck.mk
include $(LUFA_PATH)/Build/lufa_doxygen.mk
include $(LUFA_PATH)/Build/lufa_dfu.mk
include $(LUFA_PATH)/Build/lufa_hid.mk
include $(LUFA_PATH)/Build/lufa_avrdude.mk
include $(LUFA_PATH)/Build/lufa_atprogram.mk
