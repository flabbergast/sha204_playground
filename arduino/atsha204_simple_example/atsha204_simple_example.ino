/* ATSHA204 Library Simple Example
   by: Jim Lindblom
   SparkFun Electronics
   date: November 8, 2012
   
   Modified by flabbergast for a slightly different SHA204 library.
   Date: November 13, 2014
   
   This code shows how to wake up and verify that an SHA204 is
   connected and operational. And how to obtain an SHA204's unique serial
   number, and send it a MAC challenge.

   The SDA, VCC and GND pin connections need to be set in the library
   itself, in the SHA204/SHA204SWI_hardware_config.h file.   
   
   The ATSHA204 can be powered between 3.3V and 5V.
*/
#include <SHA204SWI.h>
#include <SHA204Definitions.h>

SHA204SWI sha204;

void setup()
{
  sha204.power_up();
  Serial.begin(115200);
  Serial.println("Sending a Wakup Command. Response should be:\r\n4 11 33 43:");
  Serial.println("Response is:");
  wakeupExample();
  Serial.println();
  Serial.println("Asking the SHA204's serial number. Response should be:");
  Serial.println("1 23 x x x x x x x EE");
  Serial.println("Response is:");
  serialNumberExample();
  Serial.println();
  Serial.println("Sending a MAC Challenge. Response should be (on a ATSHA in factory state):");
  Serial.println("23 6 67 0 4F 28 4D 6E 98 62 4 F4 60 A3 E8 75 8A 59 85 A6 79 96 C4 8A 88 46 43 4E B3 DB 58 A4 FB E5 73");
  Serial.println("Response is:");
  macChallengeExample();
}

void loop()
{
}

byte wakeupExample()
{
  uint8_t response[SHA204_RSP_SIZE_MIN];
  byte returnValue;
  
  returnValue = sha204.wakeup(&response[0]);
  for (int i=0; i<SHA204_RSP_SIZE_MIN; i++)
  {
    Serial.print(response[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  return returnValue;
}

byte serialNumberExample()
{
  uint8_t serialNumber[9];
  byte returnValue;
  
  returnValue = sha204.serialNumber(serialNumber);
  for (int i=0; i<9; i++)
  {
    Serial.print(serialNumber[i], HEX);
    Serial.print(" ");
  }
  Serial.println(); 
  
  return returnValue;
}

byte macChallengeExample()
{
  uint8_t command[MAC_COUNT_LONG];
  uint8_t response[MAC_RSP_SIZE];

  const uint8_t challenge[MAC_CHALLENGE_SIZE] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
  };

  uint8_t ret_code = sha204.mac(&command[0], &response[0], 0, 0, (uint8_t *) challenge);

  for (int i=0; i<SHA204_RSP_SIZE_MAX; i++)
  {
    Serial.print(response[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
  
  return ret_code;
}



