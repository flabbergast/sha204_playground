/*
 * SHA204TWI.c
 * (c) 2014 flabbergast
 *  Communicate with ATSHA204 using I2C Interface:
 *  Main file: extends the generic SHA204 class and implements the hardware protocol.
 *
 */

#include "SHA204.h"
#include "SHA204ReturnCodes.h"
#include "SHA204Definitions.h"
#include "SHA204TWI.h"
#include "i2clib/i2c_master.h"
#include <util/delay.h>
#include <avr/interrupt.h>

uint16_t SHA204TWI::SHA204_RESPONSE_TIMEOUT() {
  return SHA204_RESPONSE_TIMEOUT_VALUE;
}

// atsha204Class Constructor
// nothing to do (pin settings via defines)
SHA204TWI::SHA204TWI() {
}

// power up and initialise i2c
void SHA204TWI::power_up(void) {
  S_POWER_UP;
  #if (defined(__AVR_ATxmega128A3U__)) || (defined(__AVR_ATxmega128A4U__))
  i2c_init(I2C_BAUD_FROM_FREQ(400000));
  #else // assuming AVR8
  i2c_init(I2C_BIT_PRESCALE_1, I2C_BITLENGTH_FROM_FREQ(1, 400000));
  #endif
}

/* TWI functions */

uint8_t SHA204TWI::chip_wakeup() {
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)
  uint8_t sda_ddr_state = SHA204_SDA_DDR; // save pin config
#elif defined(__AVR_ATxmega128A3U__) || defined(__AVR_ATxmega128A4U__)
  uint8_t sda_port_state = SHA204_SDA_PORT.DIR; // save pin config
#endif
  SDA_PIN_DIR_OUT;
  SDA_PIN_LOW;
  _delay_us(SHA204_WAKEUP_PULSE_WIDTH);
  SDA_PIN_HIGH;
  _delay_ms(SHA204_WAKEUP_DELAY);
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)
  SHA204_SDA_DDR = sda_ddr_state;
#elif defined(__AVR_ATxmega128A3U__) || defined(__AVR_ATxmega128A4U__)
  SHA204_SDA_PORT.DIR = sda_port_state; // save pin config
#endif

  return SHA204_SUCCESS;
}

uint8_t SHA204TWI::sleep() {
  return send_byte(SHA204_TWI_SLEEP_CMD);
}

uint8_t SHA204TWI::idle() {
  return send_byte(SHA204_TWI_IDLE_CMD);
}

uint8_t SHA204TWI::resync(uint8_t size, uint8_t *response) {
  // Try to re-synchronize without sending a Wake token
  // (step 1 of the re-synchronization process).
  _delay_ms(SHA204_SYNC_TIMEOUT);
  uint8_t ret_code = receive_response(size, response);
  if (ret_code == SHA204_SUCCESS)
    return ret_code;

  // We lost communication. Send a Wake pulse and try
  // to receive a response (steps 2 and 3 of the
  // re-synchronization process).
  (void) sleep();
  ret_code = wakeup(response);

  // Translate a return value of success into one
  // that indicates that the device had to be woken up
  // and might have lost its TempKey.
  return (ret_code == SHA204_SUCCESS ? SHA204_RESYNC_WITH_WAKEUP : ret_code);
}

uint8_t SHA204TWI::send_bytes(uint8_t count, uint8_t *buffer) {
  uint8_t i;

  if(i2c_start(SHA204_TWI_WR, SHA204_TWI_TIMEOUT_MS) == I2C_ERROR_NoError) {
    for(i=0; i<count; i++) {
      if(!i2c_write(buffer[i])) {
        i2c_stop();
        return TWI_FUNCTION_RETCODE_TX_FAIL;
      }
    }
    i2c_stop();
    return TWI_FUNCTION_RETCODE_SUCCESS;
  }

  i2c_stop();
  return TWI_FUNCTION_RETCODE_TIMEOUT;
}

uint8_t SHA204TWI::send_byte(uint8_t value) {
  return send_bytes(1, &value);
}

uint8_t SHA204TWI::receive_bytes(uint8_t count, uint8_t *buffer)  {
  uint8_t i;

  if(i2c_start(SHA204_TWI_RE, SHA204_TWI_TIMEOUT_MS) == I2C_ERROR_NoError) {
    for(i=0; i<count-1; i++) {
      if(!i2c_read(buffer+i, false)) {
        i2c_stop();
        return TWI_FUNCTION_RETCODE_RX_FAIL;
      }
    }
    if(!ic2_read(buffer+count-1,true)) {
      i2c_stop();
      return TWI_FUNCTION_RETCODE_RX_FAIL;
    }
    i2c_stop();
    return TWI_FUNCTION_RETCODE_SUCCESS;
  }
  i2c_stop();
  return TWI_FUNCTION_RETCODE_TIMEOUT;
}

uint8_t SHA204TWI::receive_response(uint8_t size, uint8_t *response) {
  uint8_t count_byte;
  uint8_t i;
  uint8_t ret_code;

  for (i = 0; i < size; i++)
    response[i] = 0;

  ret_code = receive_bytes(size, response);
  if (ret_code == TWI_FUNCTION_RETCODE_SUCCESS || ret_code == TWI_FUNCTION_RETCODE_RX_FAIL) {
    count_byte = response[SHA204_BUFFER_POS_COUNT];
    if ((count_byte < SHA204_RSP_SIZE_MIN) || (count_byte > size))
      return SHA204_INVALID_SIZE;

    return SHA204_SUCCESS;
  }

  // Translate error so that the Communication layer
  // can distinguish between a real error or the
  // device being busy executing a command.
  if (ret_code == TWI_FUNCTION_RETCODE_TIMEOUT)
    return SHA204_RX_NO_RESPONSE;
  else
    return SHA204_RX_FAIL;
}

uint8_t SHA204TWI::send_command(uint8_t count, uint8_t * command) {
  uint8_t ret_code = send_byte(SHA204_SWI_FLAG_CMD);
  if (ret_code != SWI_FUNCTION_RETCODE_SUCCESS)
    return SHA204_COMM_FAIL;

  return send_bytes(count, command);
}
