/*
 * SHA204TWI.h
 * (c) 2014 flabbergast
 *  Communicate with ATSHA204 using I2C:
 *  Main header file.
 */

#ifndef SHA204_Library_TWI_h
#define SHA204_Library_TWI_h

#include "SHA204.h"
#include "SHA204TWI_hardware_config.h"

// TWI communication functions return codes
#define TWI_FUNCTION_RETCODE_SUCCESS     ((uint8_t) 0x00) //!< Communication with device succeeded.
#define TWI_FUNCTION_RETCODE_TIMEOUT     ((uint8_t) 0xF1) //!< Communication timed out.
#define TWI_FUNCTION_RETCODE_RX_FAIL     ((uint8_t) 0xF9) //!< Communication failed after at least one byte was received.
#define TWI_FUNCTION_RETCODE_TX_FAIL     ((uint8_t) 0xFA) //!< Communication failed when sending data via I2C.

// timeout for I2C communication
#define SHA204_TWI_TIMEOUT_MS 10

// I2C commands from datasheet
#define SHA204_TWI_RESET_ADDRESS_COUNTER_CMD 0x00
#define SHA204_TWI_SLEEP_CMD 0x01
#define SHA204_TWI_IDLE_CMD 0x02
#define SHA204_TWI_COMMAND_CMD 0x03

class SHA204TWI : public SHA204 {
private:
  const static uint16_t SHA204_RESPONSE_TIMEOUT_VALUE = 0;

  uint16_t SHA204_RESPONSE_TIMEOUT();
  uint8_t receive_bytes(uint8_t count, uint8_t *buffer);
  uint8_t send_bytes(uint8_t count, uint8_t *buffer);
  uint8_t send_byte(uint8_t value);
  uint8_t chip_wakeup();
  uint8_t receive_response(uint8_t size, uint8_t *response);
  uint8_t send_command(uint8_t count, uint8_t * command);

public:
  SHA204TWI(void);
  void init_i2c(void);
  uint8_t sleep(void);
  uint8_t idle(void);
  uint8_t resync(uint8_t size, uint8_t *response);
};

#endif
