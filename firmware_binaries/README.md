# sha204_playground firmware binaries

Supplied in Intel HEX format.

The filename (mostly) describes the hardware setup, so:

- `sha204_m32u4_sdaPB1_1W_r1.hex` is the firmware revision 1, for
  atmega32U4, 16MHz, with ATSHA's GND and VCC pin connected to GND and
  VCC of the main board, and the SDA pin connected to PB1. Communication
  to ATSHA is via Single Wire interface.
- `sha204_x128a3u_sdaPB2_gndPB3_vccPB1_1W_r1.hex`: revision 1, for
  atxmega128a3u, 32MHz, with ATSHA pins connected as follows: GND to
  PB3, SDA to PB2, VCC to PB1. This is precisely the setup for [AVR
  stick] documented on the picture in the main README.

