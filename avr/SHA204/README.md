SHA204 Library
==============

This is the main SHA204 library for the ATSHA204 chip from Atmel.

In order to use it you will need to copy this directory to a
subdirectory of your project, include `SHA204SWI.h` and instantiate a
`SHA204SWI` object.

Note that you'll need to modify the `SHA204SWI_hardware_config.h` to
match your hardware setup (the wiring is hardcoded during compilation!).
