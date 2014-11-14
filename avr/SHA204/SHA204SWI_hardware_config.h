/*
 * SHA204SWI_hardware_config.h
 * (c) 2014 flabbergast
 *  Define macros to be used for Single Wire bit-bang communication *
 *
 *  EDIT THIS FILE TO MATCH YOUR HARDWARE CONFIG!
 *
 */

#ifndef SHA204SWI_hardware_config_h
#define SHA204SWI_hardware_config_h

// Note: pull-up on the signal pin is assumed to exist (ie internal pullup is not used)

/*************************\
 **** FOR XMEGA CHIPS ****
\*************************/
#if (defined(__AVR_ATxmega128A3U__))
  // Modify below!
  //  -> If you use actual GND and VCC pins for power, ignore the _VCC_ and _GND_ settings
  //     and redefine the S_POWER_UP macro to be blank
  #define SHA204_PORT PORTB
  #define SHA204_BIT (1 << 2)
  #define SHA204_GND_PORT PORTB
  #define SHA204_GND_BIT (1 << 3)
  #define SHA204_VCC_PORT PORTB
  #define SHA204_VCC_BIT (1 << 1)

  // Choose the appropriate S_POWER_UP macro according to your power setup
  // #define S_POWER_UP {}  // use this if using actual GND and VCC supply
  #define S_POWER_UP SHA204_GND_PORT.DIRSET = SHA204_GND_BIT; SHA204_GND_PORT.OUTCLR = SHA204_GND_BIT; SHA204_VCC_PORT.DIRSET = SHA204_VCC_BIT; SHA204_VCC_PORT.OUTSET = SHA204_VCC_BIT // use this if powering from PORT pins

  // Leave the following code alone
  #define S_PIN_DIR_OUT SHA204_PORT.DIRSET = SHA204_BIT
  #define S_PIN_DIR_IN SHA204_PORT.DIRCLR = SHA204_BIT
  #define S_PIN_HIGH SHA204_PORT.OUTSET = SHA204_BIT
  #define S_PIN_LOW SHA204_PORT.OUTCLR = SHA204_BIT
  #define S_PIN_IS_HIGH SHA204_PORT.IN & SHA204_BIT

/********************************\
 **** FOR AVR8/Arduino CHIPS ****
\********************************/
#else
  // Modify below!
  //  -> If you use actual GND and VCC pins for power, ignore the _VCC_ and _GND_ settings
  //     and redefine the S_POWER_UP macro to be blank
  //  -> Have a look at http://arduino.cc/en/Hacking/PinMapping168 to find out
  //     the correct port/bit from an Arduino pin number (just need to change the
  //     last letters and the bit, e.g. Arduino's digital pin 7 is "PD7", so one
  //     would use PORTD, PIND, DDRD and the _BIT would be set to (1<<7).
  #define SHA204_PORT PORTB
  #define SHA204_PIN PINB
  #define SHA204_DDR DDRB
  #define SHA204_BIT (1<<1)
  #define SHA204_GND_PORT PORTB
  #define SHA204_GND_DDR DDRB
  #define SHA204_GND_BIT (1 << 0)
  #define SHA204_VCC_PORT PORTB
  #define SHA204_VCC_DDR DDRB
  #define SHA204_VCC_BIT (1 << 2)

  // Choose the appropriate S_POWER_UP macro according to your power setup
  #define S_POWER_UP {}  // use this if using actual GND and VCC supply
  // #define S_POWER_UP SHA204_GND_DDR|=SHA204_GND_BIT; SHA204_GND_PORT&=~SHA204_GND_BIT; SHA204_VCC_DDR|=SHA204_VCC_BIT; SHA204_VCC_PORT|=SHA204_VCC_BIT;  // use this if powering from PORT pins

  // Leave the following code alone
  #define S_PIN_DIR_OUT SHA204_DDR |= SHA204_BIT
  #define S_PIN_DIR_IN SHA204_DDR &= ~SHA204_BIT
  #define S_PIN_HIGH SHA204_PORT |= SHA204_BIT
  #define S_PIN_LOW SHA204_PORT &= ~SHA204_BIT
  #define S_PIN_IS_HIGH SHA204_PIN & SHA204_BIT
#endif

#endif
