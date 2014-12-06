/*
 * i2c_master.c
 * (c) 2014 flabbergast
 *  I2C (TWI) master for AVR8 and XMEGA chips.
 *
 *  Based on LUFA code (GPL).
 */

#include "i2c_master.h"

#include <avr/io.h>
#include <util/delay.h>
#include <util/twi.h>

// implementation for xmega
#if defined(__AVR_ATxmega128A4U__) || defined(__AVR_ATxmega128A3U__)

void i2c_init(const uint8_t baud) {
  I2C_CONFIGURE_PINS;
  I2C_TWI_INTERFACE.CTRL   = 0x00;
  I2C_TWI_INTERFACE.MASTER.BAUD   = baud;
  I2C_TWI_INTERFACE.MASTER.CTRLA  = TWI_MASTER_ENABLE_bm;
  I2C_TWI_INTERFACE.MASTER.CTRLB  = 0;
  I2C_TWI_INTERFACE.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
}

void i2c_off(void) {
  I2C_TWI_INTERFACE.MASTER.CTRLA &= ~TWI_MASTER_ENABLE_bm;
}

void i2c_on(void) {
  I2C_TWI_INTERFACE.MASTER.CTRLA  = TWI_MASTER_ENABLE_bm;
}

uint8_t i2c_start(const uint8_t address, const uint8_t timeout) { // timeout in ms
  uint16_t timeout_remaining;

  // issue a START condition
  I2C_TWI_INTERFACE.MASTER.ADDR = address;

  timeout_remaining = (timeout * 100);
  while (timeout_remaining) {
    uint8_t status = I2C_TWI_INTERFACE.MASTER.STATUS;

    if ((status & (TWI_MASTER_WIF_bm | TWI_MASTER_ARBLOST_bm)) == (TWI_MASTER_WIF_bm | TWI_MASTER_ARBLOST_bm)) { // arbitration lost
      I2C_TWI_INTERFACE.MASTER.ADDR = address; // repeat the START condition
    } else if ((status & (TWI_MASTER_WIF_bm | TWI_MASTER_RXACK_bm)) == (TWI_MASTER_WIF_bm | TWI_MASTER_RXACK_bm)) { // got a NACK from slave
      i2c_stop();
      return I2C_ERROR_SlaveResponseTimeout;
    } else if (status & (TWI_MASTER_WIF_bm | TWI_MASTER_RIF_bm)) { // all OK (got ACK)
      return I2C_ERROR_NoError;
    }

    _delay_us(10);
    timeout_remaining--; // nothing happened yet, repeat
  }

  if (!(timeout_remaining)) {
    if (I2C_TWI_INTERFACE.MASTER.STATUS & TWI_MASTER_CLKHOLD_bm) { // ran out of time and we're still holding SCL low
      i2c_stop();
    }
  }

  return I2C_ERROR_BusCaptureTimeout;
}

void i2c_stop(void) {
  I2C_TWI_INTERFACE.MASTER.CTRLC = TWI_MASTER_ACKACT_bm | TWI_MASTER_CMD_STOP_gc;
}

bool i2c_write(const uint8_t byte) {
  I2C_TWI_INTERFACE.MASTER.DATA = byte; // transmit byte

  while (!(I2C_TWI_INTERFACE.MASTER.STATUS & TWI_MASTER_WIF_bm)); // wait for transmit to end

  return (I2C_TWI_INTERFACE.MASTER.STATUS & TWI_MASTER_WIF_bm) && !(I2C_TWI_INTERFACE.MASTER.STATUS & TWI_MASTER_RXACK_bm); // return true if ACKed
}

bool i2c_read(uint8_t* const byte_ptr, const bool last) {
  if ((I2C_TWI_INTERFACE.MASTER.STATUS & (TWI_MASTER_BUSERR_bm | TWI_MASTER_ARBLOST_bm)) == (TWI_MASTER_BUSERR_bm | TWI_MASTER_ARBLOST_bm)) { // bus error or arbitration lost
    return false;
  }

  while (!(I2C_TWI_INTERFACE.MASTER.STATUS & TWI_MASTER_RIF_bm)); // wait until we receive byte

  *byte_ptr = I2C_TWI_INTERFACE.MASTER.DATA; // extract the received byte

  if (last) // NACK or ACK?
    I2C_TWI_INTERFACE.MASTER.CTRLC = TWI_MASTER_ACKACT_bm | TWI_MASTER_CMD_STOP_gc;
  else
    I2C_TWI_INTERFACE.MASTER.CTRLC = TWI_MASTER_CMD_RECVTRANS_gc;

  return true;
}

#elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)

void i2c_init(const uint8_t prescale, const uint8_t bitlength) {
  TWCR |= (1 << TWEN);
  TWSR  = prescale;
  TWBR  = bitlength;
}

void i2c_off(void) {
  TWCR &= ~(1 << TWEN);
}
void i2c_on(void) {
  TWCR |= (1 << TWEN);
}

uint8_t i2c_start(const uint8_t address, const uint8_t timeout) { // timeout in ms
  for (;;)
  {
    bool     bus_captured = false;
    uint16_t timeout_remaining;

    TWCR = ((1 << TWINT) | (1 << TWSTA) | (1 << TWEN)); // issue a START condition (+ clear the interrupt bit)

    timeout_remaining = (timeout * 100);
    while (timeout_remaining && !(bus_captured)) {
      if (TWCR & (1 << TWINT)) { // is TWI module finished ?
        switch (TWSR & TW_STATUS_MASK) {
          case TW_START:
          case TW_REP_START: // went OK, we've got the bus
            bus_captured = true;
            break;
          case TW_MT_ARB_LOST: // arbitration lost
            TWCR = ((1 << TWINT) | (1 << TWSTA) | (1 << TWEN)); // try STARTing again
            continue;
          default:
            TWCR = (1 << TWEN); // just keep TWI enabled, clear everything else
            return I2C_ERROR_BusFault;
        }
      }

      _delay_us(10);
      timeout_remaining--;
    }

    if (!(timeout_remaining)) { // we timed out
      TWCR = (1 << TWEN); // clear everything (keep TWI enabled)
      return I2C_ERROR_BusCaptureTimeout;
    }

    TWDR = address; // transmit slave address
    TWCR = ((1 << TWINT) | (1 << TWEN)); // clear the interrupt bit

    timeout_remaining = (timeout * 100);
    while (timeout_remaining) { // wait for the TWI module to finish
      if (TWCR & (1 << TWINT))
        break;
      _delay_us(10);
      timeout_remaining--;
    }

    if (!(timeout_remaining)) // we timed out
      return I2C_ERROR_SlaveResponseTimeout;

    switch (TWSR & TW_STATUS_MASK) {
      case TW_MT_SLA_ACK:
      case TW_MR_SLA_ACK:
        return I2C_ERROR_NoError; // got an ACK
      default:
        TWCR = ((1 << TWINT) | (1 << TWSTO) | (1 << TWEN)); // issue STOP
        return I2C_ERROR_SlaveNotReady;
    }
  }
}

void i2c_stop(void){
  TWCR = ((1 << TWINT) | (1 << TWSTO) | (1 << TWEN)); // issue STOP
}

bool i2c_write(const uint8_t byte) {
  TWDR = byte; // transmit
  TWCR = ((1 << TWINT) | (1 << TWEN)); // clear the interrupt bit
  while (!(TWCR & (1 << TWINT))); // wait for the TWI to do its job

  return ((TWSR & TW_STATUS_MASK) == TW_MT_DATA_ACK); // true if ACK
}

bool i2c_read(uint8_t* const byte_ptr, const bool last) {
  uint8_t TWCRMask;

  if (last) // want to ACK?
    TWCRMask = ((1 << TWINT) | (1 << TWEN));
  else
    TWCRMask = ((1 << TWINT) | (1 << TWEN) | (1 << TWEA));

  TWCR = TWCRMask;
  while (!(TWCR & (1 << TWINT))); // wait for TWI to finish
  *byte_ptr = TWDR; // read the received byte

  uint8_t status = (TWSR & TW_STATUS_MASK);

  return (last ? (status == TW_MR_DATA_NACK) : (status == TW_MR_DATA_ACK)); // received the expected NACK/ACK?
}

#endif

void i2c_reset(void) {
  i2c_off(); // disable I2C
  #define PAUSE _delay_us(5) // approximately 100kHz speed
#if defined(__AVR_ATxmega128A3U__) || defined(__AVR_ATxmega128A4U__)
  #define SDA_GO_HIGH I2C_TWI_PORT.OUTSET = I2C_SDA_BIT
  #define SDA_GO_LOW  I2C_TWI_PORT.OUTCLR = I2C_SDA_BIT
  #define SCL_GO_HIGH I2C_TWI_PORT.OUTSET = I2C_SCL_BIT
  #define SCL_GO_LOW  I2C_TWI_PORT.OUTCLR = I2C_SCL_BIT
  I2C_TWI_PORT.DIRSET = (I2C_SDA_BIT|I2C_SCL_BIT); // make SDA and SCL outputs
#elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega328P__)
  #define SDA_GO_HIGH I2C_TWI_PORT |= I2C_SDA_BIT
  #define SDA_GO_LOW  I2C_TWI_PORT &= ~I2C_SDA_BIT
  #define SCL_GO_HIGH I2C_TWI_PORT |= I2C_SCL_BIT
  #define SCL_GO_LOW  I2C_TWI_PORT &= ~I2C_SCL_BIT
  I2C_TWI_DDR |= (I2C_SDA_BIT|I2C_SCL_BIT); // make SDA and SCL outputs
#endif
  // make both high
  SDA_GO_HIGH;
  SCL_GO_HIGH;
  PAUSE;
  // START condition
  SDA_GO_LOW; PAUSE;
  SCL_GO_LOW; PAUSE;
  SDA_GO_HIGH; PAUSE;
  // 9 cycles of SCL while SDA is high
  uint8_t i;
  for(i=0; i<9; i++) {
    SCL_GO_HIGH; PAUSE; PAUSE;
    SCL_GO_LOW; PAUSE; PAUSE;
  }
  // another START condition
  SCL_GO_HIGH; PAUSE;
  SDA_GO_LOW; PAUSE; PAUSE;
  // STOP condition
  SDA_GO_HIGH; PAUSE;
  // enable I2C back
  i2c_on();
}
