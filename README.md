# sha204_playground

Firmware for ATMEL chips to communicate with ATSHA204 chip interactively
over Serial. Sources are supplied in two forms:

 - As AVR-GCC project utilizing the [LUFA] library (for USB capable
   ATMEL chips, tested on atmega32U4 and atxmega128a3u).
 - As an [Arduino] sketch.

Along with this, a python script `talk_to_sha204.py` is provided to
access some of the functionality of the ATSHA204 chip, talking to
it via the firmware above (in a binary mode), a-la [hashlet].

## Usage / Installation

Please see the READMEs in the individual subdirectories for some info on
how to use them.

## Pictures

The original impetus to write this stuff for me was that I had a couple
of ATSHA204 breakout boards from [Sparkfun] and I wanted to play with
them outside Arduino, using just avr-gcc and [LUFA], on some USB sticks
with ATMEL chips. Here are pics with two of them:

![A pic of atmega32U4 USB stick with ATSHA204](https://dl.dropboxusercontent.com/u/638633/scriptogram/sha204_m32u4_stick.jpg "atmega32u4 USB stick with ATSHA204")

And matrixstorm's [AVR stick] with the same Sparkfun's ATSHA breakout:

![A pic of AVR stick with ATSHA204](https://dl.dropboxusercontent.com/u/638633/scriptogram/sha204_avrstick.jpg "AVR stick with ATSHA204")


## Idle thoughts / ramblings

So, what can be ATSHA204 used for? Well, it performs hashing (SHA256)
and it can securely store "keys" (32 bytes long blocks of data).

The important point (for me) is that to verify that a "signature" (hash,
MAC) was generated on a particular ATSHA requires knowing all the
"secrets" that were used to generate the signature. This works fine for
Client/Server-like situations: "Client" ATSHA generates a signature for,
say, a file, (e.g. on a local computer), which is then sent to "Server"
for verification. However the "Server" has to know the "secret keys" on
ATSHA that were used in the signature computation; so either the
"Server" has a copy of the keys stored in ATSHA, or there's another
ATSHA on the "Server" with the keys.

This kind of thing can be used for instance to roll one's own
authentication service a-la [YubiKey].

What I wanted to use it for is to securely store encryption keys
locally, for a "two-factor key storage". This is also possible to do:
user enters a password, this is passed to ATSHA which generates a hash
of the user's password, together with one of its "secret keys". This
hash is then used as an encryption key. So to obtain the
encryption key, one has to know the password and have the ATSHA along.

## Notes

Please see the READMEs in the individual subdirectories for more info
about the various parts of the project, as well as credits.

## TODO / Roadmap

- Make also binary-mode-only versions of the firmware (for smaller than
  32kB chips).

## License

My code is (c) flabbergast. MIT license (see LICENSE file). Portions
of the code come from LUFA demos, this is licensed by LUFA's license.
The original code from SHA204 library is licensed by Apache V2.0 license.


[AVRstick]: http://matrixstorm.com/avr/avrstick/
[LUFA]: http://www.fourwalledcubicle.com/LUFA.php
[hashlet]: https://github.com/cryptotronix/hashlet
[YubiKey]: https://www.yubico.com/products/yubikey-hardware/yubikey-2/
[Arduino]: http://www.arduino.cc/
[Sparkfun]: https://www.sparkfun.com/products/11551
[AVR stick]: http://matrixstorm.com/avr/avrstick/
