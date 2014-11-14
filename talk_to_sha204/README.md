# talk_to_sha204.py

This python script gives access to some of the functionality of the
ATSHA204, by talking to the `sha204_playground` firmware. It requires
python 2.7, and its `pyserial` and `pyCrypto` libraries.

It was inspired by [hashlet] and sort-of emulates its functionality (and
it's perhaps more buggy). The point is that [hashlet] requires a
Raspberry Pi or a BeagleBone Black for talking to ATSHA204. This script
talks to ATSHA204 chips through another ATMEL chip, e.g. Arduino, so it
works on PCs and laptops as well (well, I haven't tried it on
Windows...)

## Basic usage

Before explaining what various commands do, it is useful to have a look
at [ATSHA204 datasheet]. Here's an excerpt: an ATSHA204's storage has 3
parts: "configuration zone" (where ATSHA's configuration parameters are
stored), "data zone" (where "keys" are stored), "OTP zone" (readable,
but mostly not-writable area). These zones can be "unlocked" (factory
state, things can be set up) or "locked" (when the zones are no longer
writable (mostly), some keys can be also not-readable ("secret")).
Locking is a one-time thing.

By "personalizing", I mean writing the desired configuration to ATSHA,
locking the configuration zone, writing random keys to the data zone and
locking it. There's space for 16 keys on an ATSHA, the access to them is
governed individually by some settings stored in the config zone.

An important thing to understand is that once the data zone is locked,
some keys will be not readable, or changeable, some will be readable and
writable, some only in a complicated way ("encrypted read/write").
So, when locking is done by this script, the keys are written to a file
(`keys.ini` by default). It is a good idea to retain this file!

The list of all command-line parameters can be printed by

        talk_to_sha204.py -h

The script talks to the firmware via a serial port: the path to the
port can be either set on command line (`-s`) or in `talk_to_sha204.ini`
config file.

One can set the verbosity of the output with the `-V` switch. Accepted
values are `error`,`warning`,`info`,`debug`. Default is `error`, which
means that the script is mostly silent and only informs about errors
(which usually means the execution is interrupted). The `debug` level
is relatively verbose about what's happening.

Here's a list of some commands accepted by `talk_to_sha204.py`:

### status

Prints 'factory' (nothing locked) or 'personalized' (everything locked)
or 'neither' (config locked, data not).

### show_config

Prints the contents of the configuration zone of the ATSHA in a readable
format (see the datasheet for explanation of the individual entries).

### personalize

This will write the configuration to ATSHA (see `personalize.ini` file),
lock the config zone; then write keys to the data zone, something to the
OTP zone and lock these zones.

The keys will be read from `keys.ini` file if it exists, any keys that
can't be found will be generated randomly. All the keys will then be
written to `keys.ini`.

The contents of the OTP zone can be configured in `personalize.ini`.

### random

This returns random 32 bytes (supplied by ATSHA). Note that before the
configuration zone is locked, ATSHA returns always the same fixed bytes.

### mac

Generates a MAC digest from a file or stdin. The data used to compute
the digest include the sha256 hash of the data (file or stdin) and one
of secret keys stored on the ATSHA (so it can be verified only when one
knows that secret key - so either on the ATSHA (see `check_mac`), or
knowing the contents of the `keys.ini` file (see `offline_mac`).
Example:

        talk_to_sha204.py --file README.md mac

It returns something like:

        data_sha256 : 94864636b6f0481d90d16229c07796a666ef377eeececeaa7d5d267ecdbd8787
        mac         : 1ae94ec8bd41bb57d429a0603d57cd1fb7af75c09db44cb9172a008e93c4da8d

The first thing is the sha256 hash of the contents of the `README.md`
file, this is then fed to ATSHA204 chip, which computes the resulting
`mac`.

### check_mac

Used for verification of a MAC. The MAC and "challenge" (data used to
compute the MAC in the first place; in our case it would be the
`data_sha256` produced by the `mac` command). The script returns '0' on
success and '1' on no-match or error. Silently. To see some output, we
also need to increase the verbosity level to (at least) `info`.

        talk_to_sha204.py --mac 1ae94ec8bd41bb57d429a0603d57cd1fb7af75c09db44cb9172a008e93c4da8d --challenge 94864636b6f0481d90d16229c07796a666ef377eeececeaa7d5d267ecdbd8787 --verbosity info check_mac

Result:

        INFO:root:Response: match!

### offline_mac

The `check_mac` command uses ATSHA to verify the MAC. To verify the MAC
without the chip, one needs to have the correct contents of `Slot0` in
the `keys.ini` file; then `offline_mac` command can be used. Example:

        talk_to_sha204.py --mac 1ae94ec8bd41bb57d429a0603d57cd1fb7af75c09db44cb9172a008e93c4da8d --challenge 94864636b6f0481d90d16229c07796a666ef377eeececeaa7d5d267ecdbd8787 --verbosity info offline_mac

Result:

        INFO:root:Response: match!

The point is that this can be used on, say, a server without the ATSHA
chip present (but with the `keys.ini` file available).



[hashlet]: https://github.com/cryptotronix/hashlet
[ATSHA204 datasheet]: http://www.atmel.com/Images/Atmel-8740-CryptoAuth-ATSHA204-Datasheet.pdf
