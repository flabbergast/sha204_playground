# sha204_playground firmware: straight avr-gcc version

Here are the source for the sha204_playground firmware, to be compiled
with avr-gcc against the [LUFA] library.

## Compilation / usage

Get the [LUFA] library and copy the `LUFA` folder to the subdirectory
`avr/LUFA`.

The SHA204 library supports both Single Wire interface and I2C interface
(on mega's hardware I2C=TWI pins). Just select the interface at the
beginning of `sha204_playground.cpp` file.

Modifications to match your hardware setup are done mostly in `makefile`
and `SHA204/SHA204SWI_hardware_config.h` or
`SHA204/SHA204TWI_hardware_config.h`.

Compile (`make`) and upload to your board with ATSHA204 (how to do this
depends on your bootloader).

After reset, your board should enumerate as a CDC Serial Device (also as
a keyboard, but that functionality isn't used yet). Note that an `.inf`
file might be required on Windows systems.

Connect to this Serial device using a serial terminal (e.g. puTTY,
picocom, minicom, screen, ...) and you should be presented with a "menu"
of commands that interact with ATSHA204 device.

## Communicating with the firmware

Probably the first thing to try is pressing `k` (to test waking the
ATSHA up) and pressing `s` (to print the serial number of your device).

Alternatively (depending on the functionality required), you can use the
`talk_to_sha204.py` script to talk to the ATSHA via this firmware.

### Binary mode

The firmware supports a "binary mode", for use with scripts. For a
demonstration on how this is done, have a look at the python script
`talk_to_sha204.py`.

## Notes

- The subdirectory `SHA204` contains a re-usable library, working on
  both AVR8 and XMEGA architectures. Note that only 16MHz and 32MHz CPU
  speeds are tested.
- At the moment, the Single-Wire Interface for ATSHA204 is implemented
  by bit-banging in C (so a speedy CPU is probably required). I2C
  interface uses hardware TWI module in (x)megas.
- The firmware also enumerates as a Keyboard. This functionality is not
  used at the moment; see `LufaLayer.h` for the functions that can
  generate "keypresses".

## VID/PID

Every USB device has a Vendor/Product identifying signature. This is set
in software here (`Descriptors.c`). The current code uses a pair which
belongs to the [LUFA] library. DO NOT USE IT for any other than
development purposes only! See [LUFA's VID/PID
page](http://www.fourwalledcubicle.com/files/LUFA/Doc/120730/html/_page__v_i_d_p_i_d.html).

## Credits

- The whole USB interface utilizes Dean Camera's excellent [LUFA]
  library, and builds on several LUFA demos supplied with the library.
- I've started with bettse's version of a SHA204 library for Arduino
  [on github](https://github.com/bettse/arduino_projects/tree/master/libraries/SHA204),
  and modified it to work on XMEGA firmware, and not necessarily as an
  Arduino library.

## License

My code is (c) flabbergast. MIT license (see LICENSE file). Portions
of the code come from LUFA demos, this is licensed by LUFA's license.
The original code from SHA204 library is licensed by Apache V2.0 license.


[LUFA]: http://www.fourwalledcubicle.com/LUFA.php
