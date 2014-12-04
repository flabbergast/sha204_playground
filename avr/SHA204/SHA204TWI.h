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

#define SHA204_TWI_WAKE_DELAY twlo
#define SHA204_TWI_WARMUP_DELAY twhi

class SHA204TWI : public SHA204 {
private:
  const static uint16_t SHA204_RESPONSE_TIMEOUT_VALUE = ((uint16_t) SWI_RECEIVE_TIME_OUT + SWI_US_PER_BYTE);  //! SWI response timeout is the sum of receive timeout and the time it takes to send the TX flag.

  uint16_t SHA204_RESPONSE_TIMEOUT();
  uint8_t receive_bytes(uint8_t count, uint8_t *buffer);
  uint8_t send_bytes(uint8_t count, uint8_t *buffer);
  uint8_t send_byte(uint8_t value);
  uint8_t chip_wakeup();
  uint8_t receive_response(uint8_t size, uint8_t *response);
  uint8_t send_command(uint8_t count, uint8_t * command);

public:
  SHA204TWI(void);
  void    power_up();
  uint8_t sleep();
  uint8_t idle();
};

#endif
