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
#include <util/delay.h>
#include <avr/interrupt.h>
#include "i2c_master.h"

uint16_t SHA204TWI::SHA204_RESPONSE_TIMEOUT() {
  return SHA204_RESPONSE_TIMEOUT_VALUE;
}

// atsha204Class Constructor
// nothing to do (pin settings via defines)
SHA204TWI::SHA204TWI() {
}

// initialise i2c
void SHA204TWI::init_i2c(void) { // use 400kHz by default
  #if (defined(__AVR_ATxmega128A3U__)) || (defined(__AVR_ATxmega128A4U__))
  i2c_init(I2C_BAUD_FROM_FREQ(400000));
  #else // assuming AVR8
  i2c_init(I2C_BIT_PRESCALE_1, I2C_BITLENGTH_FROM_FREQ(1, 400000));
  #endif
}

/* TWI functions */

uint8_t SHA204TWI::chip_wakeup() {
  i2c_off(); // disable I2C
  // pull SDA down manually
  SDA_PIN_DIR_OUT;
  SDA_PIN_LOW;
  _delay_us(SHA204_WAKEUP_PULSE_WIDTH);
  SDA_PIN_HIGH;
  _delay_ms(SHA204_WAKEUP_DELAY);
  i2c_on(); // enable I2C back

  return SHA204_SUCCESS;
}

uint8_t SHA204TWI::sleep() {
  return send_byte(SHA204_TWI_SLEEP_CMD);
}

uint8_t SHA204TWI::idle() {
  return send_byte(SHA204_TWI_IDLE_CMD);
}

uint8_t SHA204TWI::resync(uint8_t size, uint8_t *response) {
  uint8_t ret_code;
  // Try to software reset the I2C bus
  // (step 1 of the re-synchronization process).
  i2c_reset();
  ret_code = send_byte(SHA204_TWI_RESET_ADDRESS_COUNTER_CMD);
  if(ret_code == TWI_FUNCTION_RETCODE_SUCCESS)
    return SHA204_SUCCESS;

  ret_code = receive_response(size, response);
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
    if(!i2c_read(buffer+count-1,true)) {
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
  uint8_t i;

  if(i2c_start(SHA204_TWI_WR, SHA204_TWI_TIMEOUT_MS) == I2C_ERROR_NoError) {
    if(i2c_write(SHA204_TWI_COMMAND_CMD)) {
      for(i=0; i<count; i++) {
        if(!i2c_write(command[i])) {
          i2c_stop();
          return SHA204_COMM_FAIL;
        }
      }
      i2c_stop();
      return SHA204_SUCCESS;
    }
  }

  i2c_stop();
  return SHA204_COMM_FAIL;
}
