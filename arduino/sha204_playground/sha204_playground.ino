/*
 * sha204_playground.ino
 * (c) 2014 flabbergast
 *
 *  Firmware for USB capable ATMEL chips (tested on atmega32U4 and
 *    atxmega128a3u) to communicate with ATSHA204 (single-wire
 *    interface) interactively, over Serial.
 *
 * The code is quite ugly ... one reason being that it's just an Arduino adaptatation
 *  of the original avr-gcc/LUFA code, so some of the things are unnecessarily complicated.
 *  I like to keep it this way, so that it's easier to merge any patches/changes to the
 *  original code.
 */

// use I2C or single wire interface to ATSHA204?
#define USE_I2C_INTERFACE 0
// Configure the SDA pin for single wire interface in the library.
// I2C uses the standard hardware I2C pins.

#if (USE_I2C_INTERFACE)
#include <SHA204TWI.h>
#define SHA204CLASS SHA204TWI
#else
#include <SHA204SWI.h>
#define SHA204CLASS SHA204SWI
#endif

#include "SHA204Definitions.h" // for constants and such
#include "SHA204ReturnCodes.h" // want messages for return codes

/*************************************************************************
 * ----------------------- Global variables -----------------------------*
 *************************************************************************/

// when this byte is received, switch to binary mode
#define BINARY_MODE_CHAR 0xFD

#define MAX_BUFFER_SIZE 100
volatile uint8_t idle = 0;
volatile uint8_t hexprint_separator = ' ';

SHA204CLASS sha204;


/*************************************************************************
 * ----------------------- Helper functions -----------------------------*
 *************************************************************************/
void hexprint(uint8_t *p, uint16_t length);
void hexprint_noln(uint8_t *p, uint16_t length);
void hexprint_byte(uint8_t b);
void hexprint_byte_sep(uint8_t b);

uint8_t get_bytes_serial(uint8_t *output, uint16_t len);
void print_help(void);
void print_executing(void);
void print_received_from_sha(uint8_t *rx_buffer);
void print_execute_params(uint8_t opcode, uint8_t param1);
void print_return_code(uint8_t code);

void process_config(uint8_t *config);
void sleep_or_idle(SHA204CLASS *sha204);
uint8_t receive_serial_binary_transaction(uint8_t *buffer, uint8_t len);
uint8_t binary_mode_transaction(uint8_t *data, uint8_t rxsize, uint8_t *rx_buffer, SHA204CLASS *sha204);
#define BINARY_TRANSACTION_OK 0
#define BINARY_TRANSACTION_RECEIVE_ERROR 1
#define BINARY_TRANSACTION_PARAM_ERROR 2
#define BINARY_TRANSACTION_EXECUTE_ERROR 3

/* A hack so that I don't have to edit too much code from the original AVR code */
// Declarations originally from LufaLayer.h
uint16_t usb_serial_available(void); // number of getchars guaranteed to succeed immediately
int16_t usb_serial_getchar(void); // negative values mean error in receiving (not connected or no input)
void usb_serial_putchar(uint8_t ch);
void usb_serial_write(const char* const buffer);
void usb_serial_write_P(PGM_P data);;
uint16_t usb_serial_readline(char *buffer, const uint16_t buffer_size, const bool obscure_input); // BLOCKING (takes care of _tasks)
#define W(s) Serial.print(F(s))
#define Wl(s) Serial.println(F(s))    

/* Setup */
void setup(void)
{
#if (USE_I2C_INTERFACE)
  sha204.init_i2c();
#endif

  /* Initialisation */
  Serial.begin(115200);
  
  print_help();
}

void loop() {
  /* Variables */
  uint8_t rx_buffer[SHA204_RSP_SIZE_MAX];
  uint8_t tx_buffer[MAX_BUFFER_SIZE];
  uint8_t configuration_zone[88];
  uint8_t r;
  uint8_t param1;
  uint16_t param2;
  uint8_t data1[64];
  uint8_t data2[32];
  uint8_t data3[14];

  // main serial processing
  if(usb_serial_available() > 0) {
    byte c = (byte)usb_serial_getchar();
    if(c==BINARY_MODE_CHAR) { //  handle binary mode for one transaction
      r = receive_serial_binary_transaction(tx_buffer, MAX_BUFFER_SIZE); // blocking
      if(r == BINARY_TRANSACTION_OK)
        r = binary_mode_transaction(tx_buffer, SHA204_RSP_SIZE_MAX, rx_buffer, &sha204); // blocking
      // transmit the response
      usb_serial_putchar(r);
      if(r == BINARY_TRANSACTION_OK)
        for(r=0; r<rx_buffer[0]; r++)
          usb_serial_putchar(rx_buffer[r]);
    } else {
      if(idle)
        Wl("--- I ---");
      else
        Wl("--- S ---");
      switch(c) { // yes, code like this sucks
        case 'i': // serial number
          Wl("Request serial number.");
          sha204.wakeup(rx_buffer);
          r = sha204.serialNumber(rx_buffer);
          print_executing();
          print_return_code(r);
          Wl("Should get:  01 23 xx xx xx xx xx xx EE");
          W("Received SN: ");
          hexprint(rx_buffer, 9);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          break;
        case 'o': // config zone
          Wl("Request and display config zone contents.");
          print_executing();
          memset(configuration_zone,0,32);
          if(!(r=sha204.wakeup(rx_buffer))) { // read first 2 32-byte blocks manually
            if(!(r=sha204.read(tx_buffer,rx_buffer,SHA204_ZONE_CONFIG|READ_ZONE_MODE_32_BYTES,0))) {
              memcpy(configuration_zone,rx_buffer+1,32);
              if(!(r=sha204.read(tx_buffer,rx_buffer,SHA204_ZONE_CONFIG|READ_ZONE_MODE_32_BYTES,32))) {
                memcpy(configuration_zone+32, rx_buffer+1, 32);
                uint8_t addr = 64; // have to read the rest of the zone in 4-byte blocks
                while(addr < 88) {
                  if((r=sha204.read(tx_buffer,rx_buffer,SHA204_ZONE_CONFIG,addr)))
                    break;
                  memcpy(configuration_zone+addr, rx_buffer+1, 4);
                  addr+=4;
                }
              }
            }
          }
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          if(!r)
            process_config(configuration_zone);
          break;
        case 'c': // check_mac
          Wl("Calculate and compare MAC (CheckMAC command).");
          Wl("Enter mode (1 byte; default 0):");
          param1 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param1 = tx_buffer[0];
          Wl("Enter Slot ID (1 byte; default 0):");
          param2 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param2 = tx_buffer[0];
          Wl("Enter client challenge (32 bytes; default 0):");
          memset((void *)data1, 0, 32);
          get_bytes_serial(data1, 32);
          Wl("Enter client response (32 bytes; default 0):");
          memset((void *)data2, 0, 32);
          get_bytes_serial(data2, 32);
          Wl("Enter other data (13 bytes; default 0):");
          memset((void *)data3, 0, 13);
          get_bytes_serial(data3, 13);
          print_execute_params(SHA204_CHECKMAC,param1);
          hexprint_byte_sep((uint8_t)param2);
          W("data1 ");
          hexprint(data1, 32);
          W("data2 ");
          hexprint(data2, 32);
          W("data3 ");
          hexprint(data3, 13);
          print_executing();
          sha204.wakeup(rx_buffer);
          r = sha204.check_mac(tx_buffer, rx_buffer, param1, param2, data1, data2, data3);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          break;
        case 'd': // derive_key
          Wl("Combine current key with nonce and store in a key slot (DeriveKey command).");
          Wl("Enter random (1 byte: 00 or 04; default 0):");
          param1 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param1 = tx_buffer[0];
          Wl("Enter Target Slot ID (1 byte; default 0):");
          param2 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param2 = tx_buffer[0];
          Wl("Enter MAC for validation (0 or 32 bytes; default 0):");
          memset((void *)data1, 0, 32);
          get_bytes_serial(data1, 32);
          print_execute_params(SHA204_DERIVE_KEY,param1);
          hexprint_byte_sep((uint8_t)param2);
          W("data1 ");
          hexprint(data1, 32);
          print_executing();
          sha204.wakeup(rx_buffer);
          r = sha204.derive_key(tx_buffer, rx_buffer, param1, param2, data1);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          break;
        case 'v': // dev_rev
          Wl("Request device revision.");
          sha204.wakeup(rx_buffer);
          r = sha204.dev_rev(tx_buffer, rx_buffer);
          print_executing();
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          break;
        case 'g': // gen_dig
          Wl("Compute SHA-256 from TempKey+stored value, store result in TempKey (GenDig command).");
          Wl("Enter zone (1 byte: 00/Config or 01/OTP or 02/Data; default 0):");
          param1 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param1 = tx_buffer[0];
          Wl("Enter Key ID / OTP slot ID (1 byte; default 0):");
          param2 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param2 = tx_buffer[0];
          Wl("Enter other data (4 bytes when CheckKey, otherwise ignored; default 0):");
          memset((void *)data1, 0, 4);
          get_bytes_serial(data1, 4);
          print_execute_params(SHA204_GENDIG,param1);
          hexprint_byte_sep((uint8_t)param2);
          W("data1 ");
          hexprint(data1, 4);
          print_executing();
          sha204.wakeup(rx_buffer);
          r = sha204.gen_dig(tx_buffer, rx_buffer, param1, param2, data1);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          break;
        case 'h': // HMAC
          Wl("Compute HMAC/SHA-256 digest from key + other info on device (HMAC command).");
          Wl("Enter mode (1 byte; default 0):");
          param1 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param1 = tx_buffer[0];
          Wl("Enter Slot ID (2 bytes; default 0):");
          param2 = 0;
          if(2 == get_bytes_serial(tx_buffer, 2))
            param2 = tx_buffer[0]*256 + tx_buffer[1];
          print_execute_params(SHA204_HMAC,param1);
          hexprint_byte_sep((uint8_t)(param2>>8));
          hexprint_byte((uint8_t)param2);
          W("\n\r");
          print_executing();
          sha204.wakeup(rx_buffer);
          r = sha204.hmac(tx_buffer, rx_buffer, param1, param2);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          break;
        case 'm': // mac
          Wl("Compute SHA-256 from key + challenge + other info on device (MAC command).");
          Wl("Enter mode (1 byte; default 0):");
          param1 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param1 = tx_buffer[0];
          Wl("Enter Slot ID (2 bytes; default 0):");
          param2 = 0;
          if(2 == get_bytes_serial(tx_buffer, 2))
            param2 = tx_buffer[0]*256 + tx_buffer[1];
          Wl("Enter challenge (32 bytes; default 0):");
          memset((void *)data1, 0, 32);
          get_bytes_serial(data1, 32);
          print_execute_params(SHA204_MAC,param1);
          hexprint_byte_sep((uint8_t)(param2>>8));
          hexprint_byte_sep((uint8_t)param2);
          W("data1 ");
          hexprint(data1, 32);
          print_executing();
          sha204.wakeup(rx_buffer);
          r = sha204.mac(tx_buffer, rx_buffer, param1, param2, data1);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          break;
        case 'n': // nonce
          Wl("Generate a nonce for subsequent use by other commands (Nonce command).");
          Wl("Enter mode (00 to 03; default 0):");
          param1 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param1 = tx_buffer[0];
          Wl("Enter input value (20 or 32 bytes (dep on mode); default all 0):");
          memset((void *)data1, 0, 32);
          get_bytes_serial(data1, 32);
          print_execute_params(SHA204_NONCE, param1);
          Wl("none");
          print_executing();
          sha204.wakeup(rx_buffer);
          r = sha204.nonce(tx_buffer,rx_buffer,param1,data1);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          break;
        case 'r': // random
          Wl("Generate a random sequence.");
          Wl("Enter mode (00 or 01; default 0):");
          param1 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param1 = tx_buffer[0];
          print_execute_params(SHA204_RANDOM,param1);
          Wl("none");
          print_executing();
          sha204.wakeup(rx_buffer);
          r = sha204.random(tx_buffer,rx_buffer,param1);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          break;
        case 'e': // read
          Wl("Read from device.");
          Wl("Enter zone (1 byte: 00/Config or 01/OTP or 02/Data, +0x80 to read 32 instead of 4 bytes; default 0):");
          param1 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param1 = tx_buffer[0];
          Wl("Enter Address (2 bytes; default 0):");
          param2 = 0;
          if(2 == get_bytes_serial(tx_buffer, 2))
            param2 = tx_buffer[0]*256 + tx_buffer[1];
          print_execute_params(SHA204_READ,param1);
          hexprint_byte_sep((uint8_t)(param2>>8));
          hexprint_byte((uint8_t)param2);
          W("\n\r");
          print_executing();
          sha204.wakeup(rx_buffer);
          r = sha204.read(tx_buffer, rx_buffer, param1, param2);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          break;
        case 'w': // write
          Wl("Write to device.");
          Wl("Enter zone (1 byte: 00/Config or 01/OTP or 02/Data,");
          Wl("  +0x80 to write 32 instead of 4 bytes, +0x40 to require encryption; default 0):");
          param1 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param1 = tx_buffer[0];
          Wl("Enter Address (2 bytes; default 0):");
          param2 = 0;
          if(2 == get_bytes_serial(tx_buffer, 2))
            param2 = tx_buffer[0]*256 + tx_buffer[1];
          Wl("Enter data (4 or 32 bytes; default 0):");
          memset((void *)data1, 0, 32);
          get_bytes_serial(data1, 32);
          Wl("Enter MAC to validate address and data (0 or 32 bytes; default 0):");
          memset((void *)data2, 0, 32);
          get_bytes_serial(data2, 32);
          print_execute_params(SHA204_WRITE,param1);
          hexprint_byte_sep((uint8_t)(param2>>8));
          hexprint_byte_sep((uint8_t)param2);
          W("data1 ");
          hexprint_noln(data1, 32);
          W("data2 ");
          hexprint(data2, 32);
          print_executing();
          sha204.wakeup(rx_buffer);
          r = sha204.write(tx_buffer, rx_buffer, param1, param2, data1, data2);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          break;
        case 'u': // update_extra
          Wl("Update 'UserExtra' bytes (84 and 85) in the Conf zone after locking.");
          Wl("Enter mode (1 byte: 00->update 84, 01->update 85; default 0):");
          param1 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param1 = tx_buffer[0];
          Wl("Enter new value (1 byte; default 0):");
          param2 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param2 = tx_buffer[0];
          print_execute_params(SHA204_UPDATE_EXTRA,param1);
          hexprint_byte((uint8_t)param2);
          W("\n\r");
          print_executing();
          sha204.wakeup(rx_buffer);
          r = sha204.update_extra(tx_buffer, rx_buffer, param1, param2);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          break;
        case 's': // SHA
          Wl("Compute SHA256 of 64 bytes of data.");
          Wl("Enter message (64 bytes; default 0):");
          memset((void *)data1, 0, 64);
          get_bytes_serial(data1, 64);
          param1=0;
          print_execute_params(SHA204_SHA, param1);
          Wl("0x0000");
          print_executing();
          sha204.wakeup(rx_buffer);
          r = sha204.sha(tx_buffer, rx_buffer, param1, NULL);
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          param1 = 1;
          print_execute_params(SHA204_SHA, param1);
          W("0x0000 data1 ");
          hexprint(data1, 64);
          print_executing();
          r = sha204.sha(tx_buffer, rx_buffer, param1, data1);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          break;
        case 'k': // wake
          Wl("Test waking up.");
          print_executing();
          r = sha204.wakeup(rx_buffer);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          Wl("Should receive:         04 11 33 43");
          print_received_from_sha(rx_buffer);
          break;
        case 'L': // lock
          Wl("Lock a zone. This is a one time thing! Once you lock a zone, it CAN'T BE UNLOCKED. EVER!");
          Wl("Enter zone (1 byte: 00/Config or 01/OTP_or_Data, +0x80 for 'force' (CRC ignored); default 0):");
          param1 = 0;
          if(1 == get_bytes_serial(tx_buffer, 1))
            param1 = tx_buffer[0];
          Wl("Enter Summary / CRC-16 of the zone (2 bytes, should be 0 if 'force'; default 0):");
          param2 = 0;
          if(2 == get_bytes_serial(tx_buffer, 2))
            param2 = tx_buffer[0]*256 + tx_buffer[1];
          print_execute_params(SHA204_LOCK,param1);
          hexprint_byte_sep((uint8_t)(param2>>8));
          hexprint_byte((uint8_t)param2);
          W("\n\r");
          print_executing();
          sha204.wakeup(rx_buffer);
          r = sha204.lock(tx_buffer, rx_buffer, param1, param2);
          if(idle) { sha204.idle(); } else { sha204.sleep(); }
          print_return_code(r);
          print_received_from_sha(rx_buffer);
          break;
        case '\r': // enter
        case '?': // help
          print_help();
          break;
        case 'I': // switch idle and sleep
          W("Switching whether the ATSHA should be put to sleep or to idle mode after commands.\n\rCurrent setting: ");
          if(idle) {
            idle = 0;
            Wl("Sleep.");
          }
          else {
            idle = 1;
            Wl("Idle.");
          }
          break;
        default:
          break;
      }
    }
  }
}


/* Helper functions implementation */

void hexprint_byte(uint8_t b) {
  uint8_t high, low;
  low = b & 0xF;
  high = b >> 4;
  usb_serial_putchar(high+'0'+7*(high/10));
  usb_serial_putchar(low+'0'+7*(low/10));
}

void hexprint_byte_sep(uint8_t b) {
  hexprint_byte(b);
  usb_serial_putchar(hexprint_separator);
}

void hexprint_noln(uint8_t *p, uint16_t length) {
  for(uint16_t i=0; i<length; i++) {
    hexprint_byte(p[i]);
    if(hexprint_separator!=0)
      usb_serial_putchar(hexprint_separator);
  }
}

void hexprint(uint8_t *p, uint16_t length) {
  hexprint_noln(p, length);
  usb_serial_write_P(PSTR("\n\r"));
}

void hexprint_4bits(uint8_t b) {
  usb_serial_putchar((b&0xF)+'0'+7*((b&0xF)/10));
}

void hexprint_1bit(uint8_t b) {
  usb_serial_putchar((b&1)+'0');
}

void print_help(void) {
  Wl("*** SHA204 playground [(c) 2014 flabbergast] ***\n\r");
  Wl("Raw commands: wa[k]e [c]heckMAC [d]erive_key dev_re[v]ision [g]en_dig [h]MAC");
  Wl("              [m]ac [n]once [r]andom r[e]ad [w]rite [u]date_extra [s]ha");
  Wl("Processed commands: ser[i]al c[o]nfig_zone");
  Wl("Playground config: [I]dle-or-sleep");
  Wl("'?' -> this help");
  Wl("Dangerous/one-time only! [L]ock\n\r");
  Wl("Additional comments:");
  Wl(" - Format of ATSHA204 command responses:");
  Wl("    <1byte:packet_size> <msg_byte> <msg_byte> ... <1byte:crc_1> <1byte:crc_2>");
  Wl(" - ATSHA204 is sent to sleep or to idle mode after every command, select via [I].");
  Wl(" - The 'sent packet' info does not always match what's actually exactly sent. It's provided");
  Wl("    mainly to check the entered parameters.");
  Wl(" - Input, when requested, is expected in (padded) HEX format, e.g. 'AB01' for two bytes: 171 1.\n\r");
}

void print_executing(void) {
  W("Executing: ");
}

void print_received_from_sha(uint8_t *rx_buffer) {
  W("Received from ATSHA204: ");
  hexprint(rx_buffer, rx_buffer[0]);
}

void print_execute_params(uint8_t opcode, uint8_t param1) {
  W("Will run with: opcode ");
  hexprint_byte_sep(opcode);
  W("param1 ");
  hexprint_byte_sep(param1);
  W("param2 ");
}

uint8_t get_bytes_serial(uint8_t *output, uint16_t len) {
  char buffer[2*MAX_BUFFER_SIZE];
  uint16_t input_length;
  uint16_t i;
  uint8_t low, high;

  input_length = usb_serial_readline(buffer, 2*len+1, false);
  strupr(buffer);
  for(i=0; i<input_length; i+=2) {
    high = buffer[i] - '0';
    if(high > 9)
      high -= 7;
    low = buffer[i+1] - '0';
    if(low > 9)
      low -= 7;
    output[i/2] = (uint8_t)(low + (high << 4));
  }

  return (input_length/2);
}

/* Read and Interpret ATSHA204 configuration */
void process_config(uint8_t *config) {
  // serial number
  W("Serial number: ");
  hexprint_noln(config+ADDRESS_SN03, 4);
  hexprint(config+ADDRESS_SN47, 5);
  // revision number
  W("Revision number: ");
  hexprint(config+ADDRESS_RevNum,4);
  // I2C setup
  if(config[ADDRESS_I2CEN]&1) {
    W("I2C enabled; Address: 0x");
    hexprint_byte(config[ADDRESS_I2CADD]>>1);
    W("\n\r");
  } else {
    W("SingleWire (I2C disabled); TTL input level: ");
    if(config[ADDRESS_I2CADD]&0b1000)
      Wl("Vcc");
    else
      Wl("fixed");
  }
  // OTP mode
  W("OTP mode: ");
  switch(config[ADDRESS_OTPMODE]) {
    case 0xAA:
      Wl("read-only");
      break;
    case 0x55:
      Wl("consumption");
      break;
    case 0x00:
      Wl("legacy");
      break;
    default:
      Wl("reserved value (problem!)");
      break;
  }
  // selector mode
  W("Selector: ");
  if(!config[ADDRESS_SELECTOR])
    Wl("can be updated with UpdateExtra.");
  else
    Wl("can be updated only if it is 0.");
  // User extra
  W("User Extra byte: ");
  hexprint_byte(config[84]);
  W("\n\r");
  // Selector
  W("Selector byte: ");
  hexprint_byte(config[85]);
  W("\n\r");
  // Lock data
  W("Data and OTP zones are ");
  if(config[86]==0x55)
    Wl("unlocked.");
  else
    Wl("locked!");
  // Lock config
  W("Config zone is ");
  if(config[87]==0x55)
    Wl("unlocked.");
  else
    Wl("locked!");
  // Slots
  uint8_t i;
  Wl("Configurations of slots: ");
  for(i=0; i<16; i++) {
    uint8_t addr = 20+(2*i);
    W("Slot:");
    hexprint_4bits(i);
    W(" ReadKey:");
    hexprint_4bits(config[addr]); // getting the 2 config bytes LSB first
    W(" CheckOnly:");
    hexprint_1bit(config[addr]>>4);
    W(" SingleUse:");
    hexprint_1bit(config[addr]>>5);
    W(" EncryptRead:");
    hexprint_1bit(config[addr]>>6);
    W(" IsSecret:");
    hexprint_1bit(config[addr]>>7);
    W("\n\r       WriteKey:");
    hexprint_4bits(config[addr+1]);
    W(" WriteConfig:");
    hexprint_4bits(config[addr+1]>>4);
    W("\n\r");
    if(i<8) { // slots 0-7 have extra data
      W("       UseFlag:");
      hexprint_byte(config[52+(2*i)]);
      W(" UpdateCount:");
      hexprint_byte(config[53+(2*i)]);
      W("\n\r");
    }
    if(i==15) { // slot15 has limit on usage
      W("       LastKeyUse: ");
      hexprint(config+68, 16);
    }
  }
}

uint8_t receive_serial_binary_transaction(uint8_t *buffer, uint8_t len) {
  delay(100); // give the transmitting side the chance to push the rest through
  if( usb_serial_available() == 0 ) {
    return BINARY_TRANSACTION_RECEIVE_ERROR;
  }
  uint8_t n_bytes = usb_serial_getchar();
  if( n_bytes >= len ) {
    return BINARY_TRANSACTION_RECEIVE_ERROR;
  }
  buffer[0] = n_bytes;
  for(uint8_t i=1; i<=n_bytes; i++) {
    if( usb_serial_available() == 0 ) {
      return BINARY_TRANSACTION_RECEIVE_ERROR;
    }
    buffer[i] = (uint8_t)usb_serial_getchar();
    delay(3);
  }
  return BINARY_TRANSACTION_OK;
}

uint8_t binary_mode_transaction(uint8_t *data, uint8_t rxsize, uint8_t *rx_buffer, SHA204CLASS *sha204) {
  uint8_t i = 0;
  uint8_t len;
  uint8_t idle;
  uint8_t opcode;
  uint8_t param1;
  uint16_t param2;
  uint8_t datalen1=0;
  uint8_t data1[32];
  uint8_t datalen2=0;
  uint8_t data2[32];
  uint8_t datalen3=0;
  uint8_t data3[14];
  // process the input packet
  len = data[0];
  idle = data[1];
  if(len<5)
    return BINARY_TRANSACTION_PARAM_ERROR;
  opcode = data[2];
  param1 = data[3];
  param2 = data[4] + 256*data[5];
  if(len>5) {
    if((datalen1=data[6]) > 32)
      return BINARY_TRANSACTION_PARAM_ERROR;
    for(i=0; i<datalen1; i++)
      data1[i] = data[7+i];
  }
  if(len>6+datalen1) {
    if((datalen2=data[7+datalen1]) > 32)
      return BINARY_TRANSACTION_PARAM_ERROR;
    for(i=0; i<datalen2; i++)
      data2[i] = data[8+datalen1+i];
  }
  if(len>7+datalen1+datalen2) {
    if((datalen3=data[8+datalen1+datalen2]) > 13)
      return BINARY_TRANSACTION_PARAM_ERROR;
    for(i=0; i<datalen3; i++)
      data3[i] = data[9+datalen1+datalen2+i];
  }
  // run the transaction
  sha204->wakeup(data);
  i = sha204->execute(opcode, param1, param2,
          datalen1, data1, datalen2, data2, datalen3, data3,
          len, data, rxsize, rx_buffer);
  if(idle)
    sha204->idle();
  else
    sha204->sleep();
  if(i != SHA204_SUCCESS)
    return BINARY_TRANSACTION_EXECUTE_ERROR;
  return BINARY_TRANSACTION_OK;
}

/* Return code stuff */

const char retcode_success[] PROGMEM            = "Success.";
const char retcode_parse_error[] PROGMEM        = "Parse error.";
const char retcode_cmd_fail[] PROGMEM           = "Command execution error.";
const char retcode_status_crc[] PROGMEM         = "CRC error.";
const char retcode_status_unknown[] PROGMEM     = "Unknown error.";
const char retcode_func_fail[] PROGMEM          = "Couldn't execute due to wrong condition/state.";
const char retcode_gen_fail[] PROGMEM           = "Unspecified error.";
const char retcode_bad_param[] PROGMEM          = "Bad parameter.";
const char retcode_invalid_id[] PROGMEM         = "Invalid device ID.";
const char retcode_invalid_size[] PROGMEM       = "Out of range error.";
const char retcode_bad_crc[] PROGMEM            = "Bad CRC received.";
const char retcode_rx_fail[] PROGMEM            = "Timeout while waiting for a response (got >0 bytes).";
const char retcode_rx_no_response[] PROGMEM     = "Timeout (not an error while busy).";
const char retcode_resync_with_wakeup[] PROGMEM = "Resync OK after wakeup.";
const char retcode_comm_fail[] PROGMEM          = "Communication failed";
const char retcode_timeout[] PROGMEM            = "Timeout while waiting for a response (got no bytes).";
const char retcode_unknown[] PROGMEM            = "Unknown error message.";

void print_return_code(uint8_t code) {
  const char *p PROGMEM;
  switch(code) {
    case(SHA204_SUCCESS):
      p = retcode_success;
      break;
    case(SHA204_PARSE_ERROR):
      p = retcode_parse_error;
      break;
    case(SHA204_CMD_FAIL):
      p = retcode_cmd_fail;
      break;
    case(SHA204_STATUS_CRC):
      p = retcode_status_crc;
      break;
    case(SHA204_STATUS_UNKNOWN):
      p = retcode_status_unknown;
      break;
    case(SHA204_FUNC_FAIL):
      p = retcode_func_fail;
      break;
    case(SHA204_GEN_FAIL):
      p = retcode_gen_fail;
      break;
    case(SHA204_BAD_PARAM):
      p = retcode_bad_param;
      break;
    case(SHA204_INVALID_ID):
      p = retcode_invalid_id;
      break;
    case(SHA204_INVALID_SIZE):
      p = retcode_invalid_size;
      break;
    case(SHA204_BAD_CRC):
      p = retcode_bad_crc;
      break;
    case(SHA204_RX_FAIL):
      p = retcode_rx_fail;
      break;
    case(SHA204_RX_NO_RESPONSE):
      p = retcode_rx_no_response;
      break;
    case(SHA204_RESYNC_WITH_WAKEUP):
      p = retcode_resync_with_wakeup;
      break;
    case(SHA204_COMM_FAIL):
      p = retcode_comm_fail;
      break;
    case(SHA204_TIMEOUT):
      p = retcode_timeout;
      break;
    default:
      p = retcode_unknown;
      break;
  }
  usb_serial_write_P(p);
  Serial.write("\n\r");
}

// implementation of "LufaLayer"
uint16_t usb_serial_available(void) {
  return Serial.available();
}

int16_t usb_serial_getchar(void) {
  return Serial.read();
}

void usb_serial_putchar(uint8_t ch) {
  Serial.write(ch);
}

void usb_serial_write_P(PGM_P data) {
  for (uint8_t c; (c = pgm_read_byte(data)); data++) Serial.write(c);
}

void usb_serial_write(const char* const buffer) {
  Serial.write(buffer);
}

uint16_t usb_serial_readline(char *buffer, const uint16_t buffer_size, const bool obscure_input)
{
  while(Serial.available() == 0) ;
  Serial.setTimeout(100);
  uint16_t r = Serial.readBytes(buffer, buffer_size);
  Serial.setTimeout(1000);
  return(r);
}

