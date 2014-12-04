/*
 * SHA204SWI_hardware_config.h
 * (c) 2014 flabbergast
 *  Define macros to be used for Single Wire bit-bang communication *
 *
 *  EDIT THIS FILE TO MATCH YOUR HARDWARE CONFIG!
 *
 */

#ifndef SHA204TWI_hardware_config_h
#define SHA204TWI_hardware_config_h

// Note: pull-up on the SDA and SCL pins are assumed to exist (ie internal pullup is not used)

#define SHA204_TWI_ADDRESS 0xC8
#define SHA204_TWI_WR (SHA204_TWI_ADDRESS << 1) | I2C_WRITE
#define SHA204_TWI_RE (SHA204_TWI_ADDRESS << 1) | I2C_READ

/*************************\
 **** FOR XMEGA CHIPS ****
\*************************/
#if defined(__AVR_ATxmega128A3U__) || defined(__AVR_ATxmega128A4U__)
  // Modify below!
  //  -> If you use actual GND and VCC pins for power, ignore the _VCC_ and _GND_ settings
  //     and redefine the S_POWER_UP macro to be blank
  #define SHA204_GND_PORT PORTB
  #define SHA204_GND_BIT (1 << 3)
  #define SHA204_VCC_PORT PORTB
  #define SHA204_VCC_BIT (1 << 1)

  // Choose the appropriate S_POWER_UP macro according to your power setup
  #define S_POWER_UP {}  // use this if using actual GND and VCC supply
  // #define S_POWER_UP SHA204_GND_PORT.DIRSET = SHA204_GND_BIT; SHA204_GND_PORT.OUTCLR = SHA204_GND_BIT; SHA204_VCC_PORT.DIRSET = SHA204_VCC_BIT; SHA204_VCC_PORT.OUTSET = SHA204_VCC_BIT // use this if powering from PORT pins

  // TWI = I2C hardware config
  // there are several TWI possibilities on xmegas, so define here
  #if defined(__AVR_ATxmega128A3U__)
    #define SHA204_TWI_PORT TWIE
    #define SHA204_SDA_PORT PORTE
    #define SHA204_SDA_BIT (1 << 0)
  #elif defined(__AVR_ATxmega128A4U__)
    #define SHA204_TWI_PORT TWIC
    #define SHA204_SDA_PORT PORTC
    #define SHA204_SDA_BIT (1 << 0)
  #else
    #error "Define correct I2C pins for your atxmega!"
  #endif

  // Leave the following code alone
  #define SDA_PIN_DIR_OUT SHA204_SDA_PORT.DIRSET = SHA204_SDA_BIT
  #define SDA_PIN_DIR_IN SHA204_SDA_PORT.DIRCLR = SHA204_SDA_BIT
  #define SDA_PIN_HIGH SHA204_SDA_PORT.OUTSET = SHA204_SDA_BIT
  #define SDA_PIN_LOW SHA204_SDA_PORT.OUTCLR = SHA204_SDA_BIT
  #define SDA_PIN_IS_HIGH SHA204_SDA_PORT.IN & SHA204_SDA_BIT

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
  #if defined(__AVR_ATmega32U4__)
    #define SHA204_SDA_PORT PORTD
    #define SHA204_SDA_PIN PIND
    #define SHA204_SDA_DDR DDRD
    #define SHA204_SDA_BIT (1<<1)
  #elif defined(__AVR_ATmega328P__)
    #define SHA204_SDA_PORT PORTC
    #define SHA204_SDA_PIN PINC
    #define SHA204_SDA_DDR DDRC
    #define SHA204_SDA_BIT (1<<4)
  #else
    #error "Define correct I2C pins for your atmega!"
  #endif
  #define SDA_PIN_DIR_OUT SHA204_SDA_DDR |= SHA204_SDA_BIT
  #define SDA_PIN_DIR_IN SHA204_SDA_DDR &= ~SHA204_SDA_BIT
  #define SDA_PIN_HIGH SHA204_SDA_PORT |= SHA204_SDA_BIT
  #define SDA_PIN_LOW SHA204_SDA_PORT &= ~SHA204_SDA_BIT
  #define SDA_PIN_IS_HIGH SHA204_SDA_PIN & SHA204_SDA_BIT
#endif

#endif
