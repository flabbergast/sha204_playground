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
#define SHA204_TWI_WR SHA204_TWI_ADDRESS | I2C_WRITE
#define SHA204_TWI_RE SHA204_TWI_ADDRESS | I2C_READ

/*************************\
 **** FOR XMEGA CHIPS ****
\*************************/
#if defined(__AVR_ATxmega128A3U__) || defined(__AVR_ATxmega128A4U__)
  // Modify below!
  // there are several TWI possibilities on xmegas, so define here
  // need to manipulate SDA directly for wakeup, so defs here
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
