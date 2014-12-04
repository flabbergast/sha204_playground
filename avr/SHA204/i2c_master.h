/*
 * i2c_master.h
 * (c) 2014 flabbergast
 *  I2C (TWI) master for AVR8 and XMEGA chips.
 *
 *  Based on LUFA code (GPL).
 */

#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#ifdef __cplusplus
extern "C"
{
#endif

// hardware config
// can use I2C_CONFIGURE_PINS macro to enable internal pull-ups
#if defined(__AVR_ATxmega128A3U__)
  #define I2C_TWI_INTERFACE TWIE
  #define I2C_TWI_PORT PORTE
  #define I2C_SDA_BIT (1<<0)
  #define I2C_SCL_BIT (1<<1)
  #define I2C_CONFIGURE_PINS I2C_TWI_PORT.DIRSET &= ~(I2C_SDA_BIT|I2C_SCL_BIT)
#elif defined(__AVR_ATxmega128A4U__)
  #define I2C_TWI_INTERFACE TWIC
  #define I2C_TWI_PORT PORTC
  #define I2C_SDA_BIT (1<<0)
  #define I2C_SCL_BIT (1<<1)
  #define I2C_CONFIGURE_PINS I2C_TWI_PORT.DIRSET &= ~(I2C_SDA_BIT|I2C_SCL_BIT)
#elif defined(__AVR_ATmega32U4__)
  #define I2C_TWI_PORT PORTD
  #define I2C_TWI_DDR DDRD
  #define I2C_SDA_BIT (1<<1)
  #define I2C_SCL_BIT (1<<0)
  #define I2C_CONFIGURE_PINS // nothing
#elif defined(__AVR_ATmega328P__)
  #define I2C_TWI_PORT PORTC
  #define I2C_TWI_DDR DDRC
  #define I2C_SDA_BIT (1<<4)
  #define I2C_SCL_BIT (1<<5)
  #define I2C_CONFIGURE_PINS // nothing
#else
  #error "Configure your hardware I2C port!"
#endif

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>

// general I2C address manipulation bits
#define I2C_READ  0x01
#define I2C_WRITE 0x00
#define I2C_ADDRESS_MASK 0xFE

// return codes
enum I2C_ErrorCodes_t
{
  I2C_ERROR_NoError              = 0, // success
  I2C_ERROR_BusFault             = 1, // error during claiming the bus
  I2C_ERROR_BusCaptureTimeout    = 2, // timeout during claiming the bus
  I2C_ERROR_SlaveResponseTimeout = 3, // no ACK from slave within timeout
  I2C_ERROR_SlaveNotReady        = 4, // slave NACKed the START condition
  I2C_ERROR_SlaveNAK             = 5, // slave NACKed during sending data
};

// function prototypes
uint8_t i2c_start(const uint8_t address, const uint8_t timeout); // timeout in ms
void i2c_stop(void);
bool i2c_write(const uint8_t byte);
bool i2c_read(uint8_t* const byte_ptr, const bool last);
void i2c_reset(void);
#if defined(__AVR_ATxmega128A4U__) || defined(__AVR_ATxmega128A3U__)
void i2c_init(const uint8_t baud); // speed is set differently on xmega
#elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)
void i2c_init(const uint8_t prescale, const uint8_t bitlength);
#endif
void i2c_off(void);
void i2c_on(void); // like _init, but do not set speed, just enable the TWI module

// architecture specific
#if defined(__AVR_ATxmega128A4U__) || defined(__AVR_ATxmega128A3U__)
  #define I2C_BAUD_FROM_FREQ(Frequency) ((F_CPU / (2 * Frequency)) - 5)
#elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega328P__)
  // bit length prescaler for i2c_init()
  #define I2C_BIT_PRESCALE_1       ((0 << TWPS1) | (0 << TWPS0))
  #define I2C_BIT_PRESCALE_4       ((0 << TWPS1) | (1 << TWPS0))
  #define I2C_BIT_PRESCALE_16      ((1 << TWPS1) | (0 << TWPS0))
  #define I2C_BIT_PRESCALE_64      ((1 << TWPS1) | (1 << TWPS0))
  #define I2C_BITLENGTH_FROM_FREQ(Prescale, Frequency) ((((F_CPU / (Prescale)) / (Frequency)) - 16) / 2)
#endif

#ifdef __cplusplus
}
#endif

#endif
