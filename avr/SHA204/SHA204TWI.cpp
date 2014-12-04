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

uint16_t SHA204TWI::SHA204_RESPONSE_TIMEOUT() {
  return SHA204_RESPONSE_TIMEOUT_VALUE;
}

// atsha204Class Constructor
// nothing to do (pin settings via defines)
SHA204TWI::SHA204TWI() {
}

// power up (powered from appropriate pin)
void SHA204TWI::power_up(void) {
  S_POWER_UP;
}

/* TWI functions */

uint8_t SHA204TWI::chip_wakeup() {
  SDA_PIN_DIR_OUT;
  SDA_PIN_LOW;
  _delay_us(SHA204_WAKEUP_PULSE_WIDTH);
  SDA_PIN_HIGH;
  _delay_ms(SHA204_WAKEUP_DELAY);
  SDA_PIN_DIR_IN;

  return SHA204_SUCCESS;
}

uint8_t SHA204TWI::sleep() {
  return send_byte(SHA204_SWI_FLAG_SLEEP);
}

uint8_t SHA204TWI::idle() {
  return send_byte(SHA204_SWI_FLAG_IDLE);
}

uint8_t SHA204TWI::send_bytes(uint8_t count, uint8_t *buffer) {
  uint8_t i, bit_mask;

  // Disable interrupts while sending.
  cli();

  S_PIN_DIR_OUT;

  // Wait turn around time.
  _delay_us(RX_TX_DELAY);

  for (i = 0; i < count; i++) {
    for (bit_mask = 1; bit_mask > 0; bit_mask <<= 1) {
      if (bit_mask & buffer[i]) {
        S_PIN_LOW;
        _delay_us(BIT_DELAY);  //BIT_DELAY_1;
        S_PIN_HIGH;
        _delay_us(7*BIT_DELAY);  //BIT_DELAY_7;
      } else {
        // Send a zero bit.
        S_PIN_LOW;
        _delay_us(BIT_DELAY);  //BIT_DELAY_1;
        S_PIN_HIGH;
        _delay_us(BIT_DELAY);  //BIT_DELAY_1;
        S_PIN_LOW;
        _delay_us(BIT_DELAY);  //BIT_DELAY_1;
        S_PIN_HIGH;
        _delay_us(5*BIT_DELAY);  //BIT_DELAY_5;
      }
      _delay_us(2); // since 8*BIT_DELAY < 37 us (datasheet / Table 7-3)
    }
  }
  sei();  // enable_interrupts();
  return SWI_FUNCTION_RETCODE_SUCCESS;
}

uint8_t SHA204TWI::send_byte(uint8_t value) {
  return send_bytes(1, &value);
}

uint8_t SHA204TWI::receive_bytes(uint8_t count, uint8_t *buffer)  {
  uint8_t status = SWI_FUNCTION_RETCODE_SUCCESS;
  uint8_t i;
  uint8_t bit_mask;
  uint8_t pulse_count;
  uint16_t timeout_count;

  // Disable interrupts while receiving.
  cli();

  // Configure signal pin as input.
  S_PIN_DIR_IN;

  // Receive bits and store in buffer.
  for (i = 0; i < count; i++) {
    for (bit_mask = 1; bit_mask > 0; bit_mask <<= 1) {
      pulse_count = 0;

      // Make sure that the variable below is big enough.
      // Change it to uint16_t if 255 is too small, but be aware that
      // the loop resolution decreases on an 8-bit controller in that case.
      timeout_count = START_PULSE_TIME_OUT;

      // Detect start bit.
      while (--timeout_count > 0) {
        // Wait for falling edge.
        if ((S_PIN_IS_HIGH) == 0)
          break;
      }

      if (timeout_count == 0) {
        status = SWI_FUNCTION_RETCODE_TIMEOUT;
        break;
      }

      do {
        // Wait for rising edge.
        if (S_PIN_IS_HIGH) {
          // For an Atmel microcontroller this might be faster than "pulse_count++".
          pulse_count = 1;
          break;
        }
      } while (--timeout_count > 0);

      if (pulse_count == 0) {
        status = SWI_FUNCTION_RETCODE_TIMEOUT;
        break;
      }

      // Trying to measure the time of start bit and calculating the timeout
      // for zero bit detection is not accurate enough for an 8 MHz 8-bit CPU.
      // (NB by flabbergast: running now on 32MHz XMEGA. so maybe...)
      // So let's just wait the maximum time for the falling edge of a zero bit
      // to arrive after we have detected the rising edge of the start bit.
      timeout_count = ZERO_PULSE_TIME_OUT;

      // Detect possible edge indicating zero bit.
      do {
        if ((S_PIN_IS_HIGH) == 0) {
          // For an Atmel microcontroller this might be faster than "pulse_count++".
          pulse_count = 2;
          break;
        }
      } while (--timeout_count > 0);

      // Wait for rising edge of zero pulse before returning. Otherwise we might interpret
      // its rising edge as the next start pulse.
      if (pulse_count == 2) {
        do {
          if (S_PIN_IS_HIGH)
            break;
        } while (timeout_count-- > 0);
      }

      // Update byte at current buffer index.
      else
        buffer[i] |= bit_mask;  // received "one" bit
    }

    if (status != SWI_FUNCTION_RETCODE_SUCCESS)
      break;
  }
  sei(); // enable_interrupts();

  if (status == SWI_FUNCTION_RETCODE_TIMEOUT) {
    if (i > 0)
    // Indicate that we timed out after having received at least one byte.
    status = SWI_FUNCTION_RETCODE_RX_FAIL;
  }
  return status;
}

uint8_t SHA204TWI::receive_response(uint8_t size, uint8_t *response) {
  uint8_t count_byte;
  uint8_t i;
  uint8_t ret_code;

  for (i = 0; i < size; i++)
    response[i] = 0;

  (void) send_byte(SHA204_SWI_FLAG_TX);

  ret_code = receive_bytes(size, response);
  if (ret_code == SWI_FUNCTION_RETCODE_SUCCESS || ret_code == SWI_FUNCTION_RETCODE_RX_FAIL) {
    count_byte = response[SHA204_BUFFER_POS_COUNT];
    if ((count_byte < SHA204_RSP_SIZE_MIN) || (count_byte > size))
      return SHA204_INVALID_SIZE;

    return SHA204_SUCCESS;
  }

  // Translate error so that the Communication layer
  // can distinguish between a real error or the
  // device being busy executing a command.
  if (ret_code == SWI_FUNCTION_RETCODE_TIMEOUT)
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
