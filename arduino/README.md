# sha204_playground firmware: Arduino version

## Installation / usage

Copy the `avr/SHA204` library to your Arduino IDE's `libraries` folder,
and copy the sketch `sha204_playground_arduino` to your sketchbook.

Make the modifications to the library file
`SHA204/SHA204SWI_hardware_config.h` to match your hardware setup (i.e.
to which pin is your ATSHA204 connected to).

Compile and upload the sketch to your Arduino using IDE. Open the IDE's
Serial Monitor to talk to the Arduino/ATSHA204.

Note that I've tested on Arduino IDE version 1.0.5.

## Problems

Arduino sets the size of the Serial buffer to 64. This seems to cause
problems when the data sent to firmware is longer than that (which it is
for instance with the `talk_to_sha204 check_mac` command). If things
work, but you get a weird error with this command, increase the size of
the Serial buffer. Unfortunately, this requires editing a file in the
Arduino directory:
`<ARDUINO_APP_DIR>/<MAYBE_SOME_MORE_DIRS/hardware/arduino/cores/arduino/HardwareSerial.cpp`,
change `#define SERIAL_BUFFER_SIZE 64` to `#define SERIAL_BUFFER_SIZE
100`.

## Communicating with the firmware

Probably the first thing to try is pressing `k` (to test waking the
ATSHA up) and pressing `s` (to print the serial number of your device).

Alternatively (depending on the functionality required), you can use the
`talk_to_sha204.py` script to talk to the ATSHA via this firmware.

### Binary mode

The firmware supports a "binary mode", for use with scripts. For a
demonstration on how this is done, have a look at the python script
`talk_to_sha204.py`.

## License

My code is (c) flabbergast. MIT license (see LICENSE file). Portions
of the code come from LUFA demos, this is licensed by LUFA's license.
The original code from SHA204 library is licensed by Apache V2.0 license.
