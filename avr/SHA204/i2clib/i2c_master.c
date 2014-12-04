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
  I2CPORT.CTRL   = 0x00;
  I2CPORT.MASTER.BAUD   = baud;
  I2CPORT.MASTER.CTRLA  = TWI_MASTER_ENABLE_bm;
  I2CPORT.MASTER.CTRLB  = 0;
  I2CPORT.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
}

void i2c_disable(void) {
  I2CPORT.MASTER.CTRLA &= ~TWI_MASTER_ENABLE_bm;
}

uint8_t i2c_start(const uint8_t address, const uint8_t timeout) { // timeout in ms
  uint16_t timeout_remaining;

  // issue a START condition
  I2CPORT.MASTER.ADDR = address;

  timeout_remaining = (timeout * 100);
  while (timeout_remaining) {
    uint8_t status = I2CPORT.MASTER.STATUS;

    if ((status & (TWI_MASTER_WIF_bm | TWI_MASTER_ARBLOST_bm)) == (TWI_MASTER_WIF_bm | TWI_MASTER_ARBLOST_bm)) { // arbitration lost
      I2CPORT.MASTER.ADDR = address; // repeat the START condition
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
    if (I2CPORT.MASTER.STATUS & TWI_MASTER_CLKHOLD_bm) { // ran out of time and we're still holding SCL low
      i2c_stop();
    }
  }

  return I2C_ERROR_BusCaptureTimeout;
}

void i2c_stop(void) {
  I2CPORT.MASTER.CTRLC = TWI_MASTER_ACKACT_bm | TWI_MASTER_CMD_STOP_gc;
}

bool i2c_write(const uint8_t byte) {
  I2CPORT.MASTER.DATA = byte; // transmit byte

  while (!(I2CPORT.MASTER.STATUS & TWI_MASTER_WIF_bm)); // wait for transmit to end

  return (I2CPORT.MASTER.STATUS & TWI_MASTER_WIF_bm) && !(I2CPORT.MASTER.STATUS & TWI_MASTER_RXACK_bm); // return true if ACKed
}

bool i2c_read(uint8_t* const byte_ptr, const bool last) {
  if ((I2CPORT.MASTER.STATUS & (TWI_MASTER_BUSERR_bm | TWI_MASTER_ARBLOST_bm)) == (TWI_MASTER_BUSERR_bm | TWI_MASTER_ARBLOST_bm)) { // bus error or arbitration lost
    return false;
  }

  while (!(I2CPORT.MASTER.STATUS & TWI_MASTER_RIF_bm)); // wait until we receive byte

  *byte_ptr = I2CPORT.MASTER.DATA; // extract the received byte

  if (last) // NACK or ACK?
    I2CPORT.MASTER.CTRLC = TWI_MASTER_ACKACT_bm | TWI_MASTER_CMD_STOP_gc;
  else
    I2CPORT.MASTER.CTRLC = TWI_MASTER_CMD_RECVTRANS_gc;

  return true;
}

#elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)

void i2c_init(const uint8_t prescale, const uint8_t bitlength) {
  TWCR |= (1 << TWEN);
  TWSR  = prescale;
  TWBR  = bitlength;
}

void i2c_disable(void) {
  TWCR &= ~(1 << TWEN);
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
