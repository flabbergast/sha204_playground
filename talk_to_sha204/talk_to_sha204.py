#!/usr/bin/env python2

# talk_to_sha204.py
# (c) 2014 flabbergast
#
# Communicate with ATSHA204 via the binary mode of sha204_playground firmware.
#

import serial
from Crypto.Hash import SHA256

import binascii
import time
import os
import sys
import logging
import argparse
import ConfigParser
import pprint

BINARY_TRANSACTION_CODE = chr(0xFD)

# command op-code definitions
SHA204_CHECKMAC = chr(0x28)
SHA204_DERIVE_KEY = chr(0x1C)
SHA204_DEVREV = chr(0x30)
SHA204_GENDIG = chr(0x15)
SHA204_HMAC = chr(0x11)
SHA204_LOCK = chr(0x17)
SHA204_MAC = chr(0x08)
SHA204_NONCE = chr(0x16)
SHA204_PAUSE = chr(0x01)
SHA204_RANDOM = chr(0x1B)
SHA204_READ = chr(0x02)
SHA204_UPDATE_EXTRA = chr(0x20)
SHA204_WRITE = chr(0x12)
SHA204_SHA = chr(0x47)

# firmware binary mode return codes
BINARY_MODE_RETURN_CODES = {
    0: 'BINARY_TRANSACTION_OK',
    1: 'BINARY_TRANSACTION_RECEIVE_ERROR',
    2: 'BINARY_TRANSACTION_PARAM_ERROR',
    3: 'BINARY_TRANSACTION_EXECUTE_ERROR',
    99: 'PROBLEM WITH SERIAL COMMUNICATION (BUFFERING?)'
}

# idle versus sleep instruction
REQUEST_IDLE = chr(1)
REQUEST_SLEEP = chr(0)

# Parameters for MAC/checkMAC/offlineMAC
MAC_SLOT = 0
MAC_MODE = chr(0)
def MAC_OTHER_DATA(slot):
    return SHA204_MAC+MAC_MODE+chr(slot)+b'\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'


#########################################
### Low-Level communication functions ###
#########################################

def crc16(data):
    crc_register = 0
    polynomial = 0x8005
    for c in data:
        for shift_register in 1, 2, 4, 8, 16, 32, 64, 128:
            data_bit = 1 if (ord(c) & shift_register) > 0 else 0
            crc_bit = crc_register >> 15
            crc_register = (crc_register << 1) & 0xFFFF
            if (data_bit ^ crc_bit) != 0:
                crc_register ^= polynomial
    #return crc_register
    return chr(crc_register & 0xFF) + chr(crc_register >> 8)


class TransactionError(Exception):
    def __init__(self, message, value=0):
        self.message = message
        self.value = value

    def __str__(self):
        return repr(self.message + ': ' + BINARY_MODE_RETURN_CODES[self.value])


def do_transaction(buf, serport):
    # buf is assumed to have the following format:
    #  1 byte:  idle or sleep after command?
    #  1 byte:  opcode
    #  1 byte:  param1
    #  2 bytes: param2
    # --> from this point on, parameters go in pairs (length, data), and they are optional
    #  1 byte:  datalen1
    #  datalen1 bytes: data1
    #  1 byte:  datalen2
    #  datalen2 bytes: data2
    #  1 byte:  datalen3
    #  datalen3 bytes: data3
    if args.dry_run and buf[1] in [SHA204_WRITE, SHA204_LOCK, SHA204_UPDATE_EXTRA]:
        logging.info("Dry run! Not sending " + binascii.hexlify(buf))
        return chr(0)
    message = b'' + BINARY_TRANSACTION_CODE + chr(len(buf)) + buf
    serport.write(message)
    status = serport.read(1)  # read on byte, with timeout
    if status != chr(0):
        raise TransactionError("Firmware returned", ord(status))
    response_length = ord(serport.read(1))  # next byte is message length, this byte included
    response = serport.read(response_length - 1)  # read the rest of the message (timeout!)
    if len(response) != response_length - 1:
        raise TransactionError("Serial communication problem: did not receive the whole response", 99)
    crc = crc16(chr(response_length) + response[0:-2])
    if crc != response[-2:]:
        raise TransactionError("CRC error", 99)
    # flush the port to clean up the pipes
    serport.flushInput()
    serport.flushOutput()
    return response[0:-2]


###############################
### Mid-level communication ###
###############################

def receive_config_area(serport):
    config = do_transaction(REQUEST_SLEEP+SHA204_READ + b'\x80\x00\x00', serport)
    config += do_transaction(REQUEST_SLEEP+SHA204_READ + b'\x80\x08\x00', serport)
    for i in range(0x10, 0x16):
        config += do_transaction(REQUEST_SLEEP+SHA204_READ + b'\x00' + chr(i) + b'\x00', serport)
    return config


def read_config_area(serport):
    logging.debug("Reading and processing config area of ATSHA...")
    try:
        return receive_config_area(serport)
    except TransactionError, e:
        logging.error("ERROR communicating with firmware/ATSHA: " + str(e))
        exit(1)


def write_config_word(address, word, serport):
    if len(word) != 4:
        logging.error("Trying to write something else that 4 bytes to config area!")
        exit(1)
    logging.debug("Writing to config zone, word address " + hex(address) + ", word: 0x" + binascii.hexlify(word))
    try:
        response = do_transaction(REQUEST_SLEEP+SHA204_WRITE + b'\x00' + chr(address) + b'\x00\x04' + word, serport)
    except TransactionError, e:
        logging.error("ERROR communicating with firmware/ATSHA: " + str(e))
        exit(1)
    else:
        logging.debug("Got response: " + ("OK" if response == chr(0) else "not OK"))
        return response


def write_otp_area(contents, serport):
    if len(contents) != 64:
        logging.error("Trying to write something else that 64 bytes to OTP!")
        exit(1)
    logging.debug("Writing to OTP zone, word address 0x0, data: 0x" + binascii.hexlify(contents[0:32]))
    try:
        response = do_transaction(REQUEST_SLEEP+SHA204_WRITE + b'\x81\x00\x00\x20' + contents[0:32], serport)
    except TransactionError, e:
        logging.error("ERROR communicating with firmware/ATSHA: " + str(e))
        exit(1)
    else:
        logging.debug("Got response: " + ("OK" if response == chr(0) else "not OK"))
    logging.debug("Writing to OTP zone, word address 0x8, data: 0x" + binascii.hexlify(contents[32:64]))
    try:
        response = do_transaction(REQUEST_SLEEP+SHA204_WRITE + b'\x81\x08\x00\x20' + contents[32:64], serport)
    except TransactionError, e:
        logging.error("ERROR communicating with firmware/ATSHA: " + str(e))
        exit(1)
    else:
        logging.debug("Got response: " + ("OK" if response == chr(0) else "not OK"))
        return response


def update_config_area(config, serport):
    config_new = process_config_modifications(config)
    # determine differences and write them
    for i in range(0, 22):
        have_changed = False
        for j in range(0, 4):
            if config_new[4 * i + j] != config[4 * i + j]:
                logging.info("Changing byte in config zone at position " + hex(4 * i + j) + ", from " + hex(
                    ord(config[4 * i + j])) + " to " + hex(ord(config_new[4 * i + j])))
                have_changed = True
        if have_changed:
            write_config_word(i, config_new[4 * i:4 * i + 4], serport)
    return config_new


def do_lock_config_area(expected_config, serport):
    logging.info("Verifying the config zone contents.")
    config_verify = read_config_area(ser_port)
    if not (expected_config == config_verify):
        logging.error(
            "The contents of the config zone is not what is expected! Problem with ATSHA or dry run? Not locking.")
        exit(1)
    else:
        logging.info("Locking the config zone!")
        crc = crc16(expected_config)
        try:
            response = do_transaction(REQUEST_SLEEP+SHA204_LOCK + b'\x00' + crc, serport)
        except TransactionError, e:
            logging.error("ERROR communicating with firmware/ATSHA: " + str(e))
            exit(1)
        else:
            logging.debug("Got response: " + ("OK" if response == chr(0) else "not OK"))
            return response


def do_lock_data_otp_area(expected_data, expected_otp, serport):
    logging.info("Locking the data and OTP zones!")
    crc = crc16(expected_data + expected_otp)
    try:
        response = do_transaction(REQUEST_SLEEP+SHA204_LOCK + b'\x01' + crc, serport)
    except TransactionError, e:
        logging.error("ERROR communicating with firmware/ATSHA: " + str(e))
        exit(1)
    else:
        logging.debug("Got response: " + ("OK" if response == chr(0) else "not OK"))
        return response


def get_random(serport):
    try:
        response = do_transaction(REQUEST_SLEEP+SHA204_RANDOM + b'\x00\x00\x00', serport)
    except TransactionError, e:
        logging.error("ERROR communicating with firmware/ATSHA: " + str(e))
        exit(1)
    else:
        return response


def write_data_slot(slot, contents, serport):
    if len(contents) != 32 or slot < 0 or slot > 15:
        logging.error("Something went wrong, the call to write_data_slot has wrong params!")
        exit(1)
    logging.debug("Writing to data zone, slot " + hex(slot) + ", data: 0x" + binascii.hexlify(contents))
    try:
        response = do_transaction(REQUEST_SLEEP+SHA204_WRITE + b'\x82' + chr(slot * 8) + b'\x00\x20' + contents, serport)
    except TransactionError, e:
        logging.error("ERROR communicating with firmware/ATSHA: " + str(e))
        exit(1)
    else:
        logging.debug("Got response: " + ("OK" if response == chr(0) else "not OK"))
        return response


def mac(challenge, slot, serport):
    if len(challenge) != 32 or slot < 0 or slot > 15:
        logging.error("Something went wrong, the call to mac has wrong params!")
        exit(1)
    logging.debug("Calling the mac command, challenge "+binascii.hexlify(challenge)+", slot "+hex(slot))
    try:
        response = do_transaction(REQUEST_SLEEP+SHA204_MAC+MAC_MODE+chr(slot)+b'\x00\x20' + challenge, serport)
    except TransactionError, e:
        logging.error("ERROR communicating with firmware/ATSHA: " + str(e))
        exit(1)
    else:
        if len(response) != 32:
            logging.error("Received an unexpected response from mac command: "+binascii.hexlify(response))
        else:
            print("data_sha256 : "+binascii.hexlify(challenge))
            print("mac         : "+binascii.hexlify(response))

def check_mac(challenge, mac, slot, serport):
    if len(challenge) != 32 or len(mac) != 32 or slot < 0 or slot > 15:
        logging.error("Something went wrong, the call to mac has wrong params!")
        exit(1)
    logging.debug("Calling the checkmac command, challenge "+binascii.hexlify(challenge)+", mac "+binascii.hexlify(mac)+" slot "+hex(slot))
    try:
        response = do_transaction(REQUEST_SLEEP+SHA204_CHECKMAC+MAC_MODE+chr(slot)+b'\x00\x20' + challenge + b'\x20' + mac + b'\x0D' + MAC_OTHER_DATA(slot), serport)
    except TransactionError, e:
        logging.error("ERROR communicating with firmware/ATSHA: " + str(e))
        exit(1)
    else:
        if response == chr(0):
            logging.info("Response: match!");
            exit(0)
        else:
            logging.info("Response: no match!");
            exit(1)

def sha(message, serport):
    if len(message) != 64:
        logging.error("Something went wrong, the call to mac has wrong params!")
        exit(1)
    logging.debug("Calling the SHA command, message "+binascii.hexlify(message))
    try:
        response = do_transaction(REQUEST_IDLE+SHA204_SHA+chr(0)+b'\x00\x00', serport)
    except TransactionError, e:
        logging.error("ERROR communicating with firmware/ATSHA: " + str(e))
        exit(1)
    if response != chr(0):
        logging.error("ERROR initialising SHA computation: " + str(e))
        exit(1)
    try:
        response = do_transaction(REQUEST_SLEEP+SHA204_SHA+chr(1)+b'\x00\x00'+b'\x40'+message,serport)
    except TransactionError, e:
        logging.error("ERROR communicating with firmware/ATSHA: " + str(e))
        exit(1)
    else:
        if len(response) != 32:
            logging.error("Received an unexpected response from SHA command: "+binascii.hexlify(response))
        else:
            print("sha256 of message: "+binascii.hexlify(response))

#######################
### Data processing ###
#######################

def process_config_area(config):
    c = {'serial': binascii.hexlify(config[0:4] + config[8:13]), 'revision': binascii.hexlify(config[4:8]),
         'I2Cenable': (ord(config[14]) & 1 == 1), 'I2Caddress': hex(ord(config[16]) >> 1)}
    if config[18] == b'\xAA':
        c['OTPmode'] = "read-only"
    elif config[18] == b'\x55':
        c['OTPmode'] = "consumption"
    elif config[18] == b'\x00':
        c['OTPmode'] = "legacy"
    else:
        c['OTPmode'] = "error!"
    if config[19] == b'\x00':
        c['SelectorMode'] = "Selector can be updated with UpdateExtra"
    else:
        c['SelectorMode'] = "Selector can be updated only when 0"
    c['UserExtra'] = binascii.hexlify(config[84])
    c['Selector'] = binascii.hexlify(config[85])
    c['DataOTPlocked'] = (config[86] != b'\x55')
    c['ConfigLocked'] = (config[87] != b'\x55')
    c['slotConfigs'] = []
    for i in range(0, 16):
        slot = {'Index': i, 'ReadKey': ord(config[20 + 2 * i]) & 0xF,
                'CheckOnly': (ord(config[20 + 2 * i]) & (1 << 4) > 0),
                'SingleUse': (ord(config[20 + 2 * i]) & (1 << 5) > 0),
                'EncryptRead': (ord(config[20 + 2 * i]) & (1 << 6) > 0),
                'IsSecret': (ord(config[20 + 2 * i]) & (1 << 7) > 0), 'WriteKey': ord(config[21 + 2 * i]) & 0xF,
                'WriteConfig': b'0b' + bin((ord(config[21 + 2 * i]) & 0xF0) >> 4)[2:].zfill(4)}
        if i < 8:
            slot['UseFlag'] = binascii.hexlify(config[52 + 2 * i])
            slot['UpdateCount'] = binascii.hexlify(config[53 + 2 * i])
        if i == 15:
            slot['KeyUse'] = binascii.hexlify(config[68:84])
        c['slotConfigs'].append(slot)
    return c


def get_ini_bool(ini, section, option):
    try:
        t = ini.getboolean(section, option)
    except ValueError:
        logging.error("[" + section + "]->" + option + " value can't be parsed correctly from the config file.")
        return None
    else:
        return t


def get_ini_int(ini, section, option, possibilites=range(0, 256)):
    try:
        t = int(ini.get(section, option), 0)
    except ValueError:
        logging.error("[" + section + "]->" + option + " value can't be parsed correctly from the config file.")
        return None
    else:
        if t not in possibilites:
            logging.error("[" + section + "]->" + option + " value can't be parsed correctly from the config file.")
            return None
        else:
            return t


def get_ini_str(ini, section, option, validate=(lambda x: True)):
    try:
        t = ini.get(section, option)
    except ValueError:
        logging.error("[" + section + "]->" + option + " value can't be parsed correctly from the config file.")
        return None
    else:
        if not validate(t):
            logging.error("[" + section + "]->" + option + " value can't be parsed correctly from the config file.")
            return None
        else:
            return t


def validate_hexlified_data(string, length):
    try:
        t = binascii.unhexlify(string)
    except TypeError:
        return False
    else:
        if len(t) != length:
            return False
        else:
            return True


def validate_key_use(key_use):
    return validate_hexlified_data(key_use, 16)


def validate_slot_contents(contents):
    return validate_hexlified_data(contents, 32)


def process_config_modifications(config):
    if len(config) != 88:
        logging.error("Something's wrong with the config (incorrect length?).")
        exit(1)
    personalize_config = ConfigParser.SafeConfigParser()
    if os.path.isfile(args.personalize_file):
        personalize_config.read(args.personalize_file)
    else:
        logging.warning("Personalize file not found. Proceeding with current ATSHA configuration.")
        return config
    config_list = list(config)
    if personalize_config.has_option('ConfigZone', 'I2Cenable'):
        t = get_ini_bool(personalize_config, 'ConfigZone', 'I2Cenable')
        if t is not None:
            config_list[14] = (chr(1) if t else chr(0))
    if personalize_config.has_option('ConfigZone', 'I2Caddress'):
        t = get_ini_int(personalize_config, 'ConfigZone', 'I2Caddress', range(0, 128))
        if t is not None:
            config_list[16] = chr(t << 1)
    if personalize_config.has_option('ConfigZone', 'OTPmode'):
        t = get_ini_int(personalize_config, 'ConfigZone', 'OTPmode', [0xAA, 0x55, 0])
        if t is not None:
            config_list[18] = chr(t)
    if personalize_config.has_option('ConfigZone', 'SelectorMode'):
        t = get_ini_int(personalize_config, 'ConfigZone', 'SelectorMode')
        if t is not None:
            config_list[19] = chr(t)
    for i in range(0, 16):
        slot_sect = 'Slot' + str(i)
        slot_addr = 20 + 2 * i
        if personalize_config.has_section(slot_sect):
            if personalize_config.has_option(slot_sect, 'ReadKey'):
                t = get_ini_int(personalize_config, slot_sect, 'ReadKey', range(0, 16))
                if t is not None:
                    config_list[slot_addr] = chr((ord(config_list[slot_addr]) & 0xF0) | t)
            if personalize_config.has_option(slot_sect, 'CheckOnly'):
                t = get_ini_bool(personalize_config, slot_sect, 'CheckOnly')
                if t is not None:
                    if t:
                        config_list[slot_addr] = chr(ord(config_list[slot_addr]) | (1 << 4))
                    else:
                        config_list[slot_addr] = chr(ord(config_list[slot_addr]) & (0xFF & ~(1 << 4)))
            if personalize_config.has_option(slot_sect, 'SingleUse'):
                t = get_ini_bool(personalize_config, slot_sect, 'SingleUse')
                if t is not None:
                    if t:
                        config_list[slot_addr] = chr(ord(config_list[slot_addr]) | (1 << 5))
                    else:
                        config_list[slot_addr] = chr(ord(config_list[slot_addr]) & (0xFF & ~(1 << 5)))
            if personalize_config.has_option(slot_sect, 'EncryptRead'):
                t = get_ini_bool(personalize_config, slot_sect, 'EncryptRead')
                if t is not None:
                    if t:
                        config_list[slot_addr] = chr(ord(config_list[slot_addr]) | (1 << 6))
                    else:
                        config_list[slot_addr] = chr(ord(config_list[slot_addr]) & (0xFF & ~(1 << 6)))
            if personalize_config.has_option(slot_sect, 'IsSecret'):
                t = get_ini_bool(personalize_config, slot_sect, 'IsSecret')
                if t is not None:
                    if t:
                        config_list[slot_addr] = chr(ord(config_list[slot_addr]) | (1 << 7))
                    else:
                        config_list[slot_addr] = chr(ord(config_list[slot_addr]) & (0xFF & ~(1 << 7)))
            if personalize_config.has_option(slot_sect, 'WriteKey'):
                t = get_ini_int(personalize_config, slot_sect, 'WriteKey', range(0, 16))
                if t is not None:
                    config_list[slot_addr + 1] = chr((ord(config_list[slot_addr + 1]) & 0xF0) | t)
            if personalize_config.has_option(slot_sect, 'WriteConfig'):
                t = get_ini_int(personalize_config, slot_sect, 'WriteConfig', range(0, 16))
                if t is not None:
                    config_list[slot_addr + 1] = chr((ord(config_list[slot_addr + 1]) & 0x0F) | (t << 4))
            if i < 8:
                useflag_addr = 52 + 2 * i
                if personalize_config.has_option(slot_sect, 'UseFlag'):
                    t = get_ini_int(personalize_config, slot_sect, 'UseFlag')
                    if t is not None:
                        config_list[useflag_addr] = chr(t)
                if personalize_config.has_option(slot_sect, 'UpdateCount'):
                    t = get_ini_int(personalize_config, slot_sect, 'UpdateCount')
                    if t is not None:
                        config_list[useflag_addr + 1] = chr(t)
            if i == 15:
                if personalize_config.has_option(slot_sect, 'KeyUse'):
                    t = get_ini_str(personalize_config, slot_sect, 'KeyUse', validate_key_use)
                    if t is not None:
                        t = binascii.unhexlify(t)
                        for j in range(0, 16):
                            config_list[68 + j] = t[j]
    return b''.join(config_list)


#########################
### Command functions ###
#########################

def lock_config(serport):
    config_bin = read_config_area(serport)
    config_area = process_config_area(config_bin)
    if config_area['ConfigLocked']:
        logging.error("Config zone already locked!")
        exit(1)
    config_bin_new = update_config_area(config_bin, serport)
    if chr(0) == do_lock_config_area(config_bin_new, serport):
        print "Config zone locked!"


def lock_data(serport):
    config_area = process_config_area(read_config_area(serport))
    if not config_area['ConfigLocked']:
        logging.error("Config zone needs to be locked before locking data and OTP!")
        exit(1)
    elif config_area['DataOTPlocked']:
        logging.error("Data and OTP zones are already locked!")
        exit(1)
    keys = {}
    keys_ini = ConfigParser.ConfigParser()
    # read keys from file if present
    if (not args.random_keys) and os.path.isfile(args.keys_file):
        keys_ini.read(args.keys_file)
        for i in range(0, 16):
            if keys_ini.has_option('Keys', 'Slot' + str(i)):
                t = get_ini_str(keys_ini, 'Keys', 'Slot' + str(i), validate_slot_contents)
                if t is not None:
                    keys[i] = binascii.unhexlify(t)
        logging.info("Read " + str(len(keys)) + " from " + args.keys_file)
    # fill in the other ones randomly
    for i in range(0, 16):
        if i not in keys:
            keys[i] = get_random(serport)
    # prepare ini for writing
    if not keys_ini.has_section('Keys'):
        keys_ini.add_section('Keys')
    for i in range(0, 16):
        keys_ini.set('Keys', 'Slot' + str(i), binascii.hexlify(keys[i]))
    # write keys_ini
    with open(args.keys_file, 'w') as f:
        keys_ini.write(f)
    # write the keys into slots
    whole_data = b''
    for i in range(0, 16):
        write_data_slot(i, keys[i], serport)
        whole_data += keys[i]  # collect all the slots into one variable, used later for locking
    # OTP stuff
    otp = b'\xFF' * 64
    if not os.path.isfile(args.personalize_file):
        logging.warning("Personalize file not found. OTP zone will be filled with 0xFF's.")
    else:
        personalize_config = ConfigParser.SafeConfigParser()
        personalize_config.read(args.personalize_file)
        if not personalize_config.has_option('OTPzone', 'OTPcontents'):
            logging.warning(
                "OTP zone contents not set in " + args.personalize_file + ". It will be filled with 0xFF's.")
        else:
            otp = get_ini_str(personalize_config, 'OTPzone', 'OTPcontents')[1:-1].replace('__DATE__', time.strftime('%Y%m%d'))[0:64].ljust(64, chr(0xFF))
    write_otp_area(otp, serport)
    # lock!
    if chr(0) == do_lock_data_otp_area(whole_data, otp, serport):
        print "Data+OTP zones locked!"


def check_mac_offline(challenge, mac, slot):
    # try to get the key
    keys = {}
    keys_ini = ConfigParser.ConfigParser()
    if not os.path.isfile(args.keys_file):
        logging.error("Need the keys_file for offline MAC checking!")
        exit(1)
    keys_ini.read(args.keys_file)
    if not keys_ini.has_option('Keys', 'Slot' + str(slot)):
        logging.error("Need the slot "+hex(slot)+" contents in "+args.keys_file+" file for offline MAC checking!")
        exit(1)
    key = get_ini_str(keys_ini, 'Keys', 'Slot' + str(slot), validate_slot_contents)
    if key is None:
        logging.error("Need the slot "+hex(slot)+" contents in "+args.keys_file+" file to be valid for offline MAC checking!")
        exit(1)
    key = binascii.unhexlify(key)
    other_data = MAC_OTHER_DATA(slot)
    full_challenge = key + challenge + other_data[0:4]+(b'\x00'*8)+other_data[4:7]+b'\xEE'+other_data[7:11]+b'\x01\x23'+other_data[11:13]
    computed_mac = SHA256.new(full_challenge)
    if computed_mac.digest() == mac:
        logging.info("Response: match!");
        exit(0)
    else:
        logging.info("Response: no match!");
        exit(1)


#################
### Main code ###
#################

# parse command-line arguments
parser = argparse.ArgumentParser(description="Talk to ATSHA204 using sha204_playground firmware.",
                                 formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument("command", choices=['status', 'show_config', 'lock_config', 'lock_data', 'personalize', 'random', 'sha',
                                        'mac', 'check_mac', 'offline_mac'], help='Command')
parser.add_argument('-n', '--dry-run', dest='dry_run', action='store_true', help="Do not do actual write or lock.")
parser.add_argument('-c', '--config-file', dest='config_file', nargs='?', default='talk_to_sha204.ini',
                    help="Path to config file.")
parser.add_argument('-s', '--serial-port', dest='serial_path', nargs='?', help='Path to serial port.')
parser.add_argument('-p', '--personalize-file', dest='personalize_file', nargs='?', default='personalize.ini',
                    help="Path to file with data for personalizing.")
parser.add_argument('-k', '--keys-file', dest='keys_file', nargs='?', default='keys.ini',
                    help="Path to file with slot contents (any new random keys will be added on personalize or lock_data).")
parser.add_argument('--random-keys', dest='random_keys', action='store_true',
                    help="Do not read keys-file even if present, generate random keys (keys-file will be overwritten!).")
parser.add_argument('-f', '--file', dest='file', nargs='?', help="Path to file to be used for MAC. Uses stdin if no file given.")
parser.add_argument('-m', '--mac', dest='mac', nargs='?', help="MAC to be checked; for check_mac or offline_mac.")
parser.add_argument('-C', '--challenge', dest='challenge', nargs='?', help="SHA256 of data (challenge) for check_mac or offline_mac.")
parser.add_argument('-S', '--sha-message', dest='sha_message', nargs='?', help="Message to be hashed with SHA, <=64 bytes (will be padded with 0).")
parser.add_argument('-V', '--verbosity', dest='verbosity', nargs='?', default='error',
                    choices=['error', 'warning', 'info', 'debug'], help="Verbosity level.")
args = parser.parse_args()  # args are used as a global variable

# set up logging
log_level = getattr(logging, args.verbosity.upper(), None)
if not isinstance(log_level, int):
    raise ValueError('Invalid log level: %s' % args.verbosity)
logging.basicConfig(level=log_level)

# parse config file
script_config = ConfigParser.ConfigParser()
if os.path.isfile(args.config_file):
    script_config.read(args.config_file)

# do offline operations before doing anything with the serial port
if args.command == 'offline_mac':
    # get params
    try:
        checkmac_challenge = binascii.unhexlify(args.challenge)
        checkmac_mac = binascii.unhexlify(args.mac)
        if len(checkmac_mac) != 32 or len(checkmac_challenge) != 32:
            raise TypeError("Incorrect length (should be 32 bytes / 64 hex chars).")
    except TypeError, e:
        logging.error("Problem processing MAC (--mac) or challenge (--challenge) parameters: "+str(e))
        exit(1)
    else:
        check_mac_offline(checkmac_challenge, checkmac_mac, MAC_SLOT)

# open serial port
serial_path = ''
if script_config.has_option('Communication', 'SerialPort'):
    serial_path = script_config.get('Communication', 'SerialPort')
if args.serial_path:
    serial_path = args.serial_path
if not os.path.exists(serial_path):
    logging.error("Specify a valid path to serial port either in the config file or on command line!")
    exit(1)
ser_port = serial.Serial(port=serial_path, baudrate=115200, timeout=2)  # timeout 2 seconds!
time.sleep(2) # putting 2 so that Arduinos have time to run the bootloader, exit from it and start the sketch
              # for avr-gcc/LUFA, 0.5 is enough
ser_port.flushInput()  # throw away the message received after connecting (DTR)

# pretty printer
pp = pprint.PrettyPrinter(indent=2)

if args.command == "show_config":
    config_area = process_config_area(read_config_area(ser_port))
    pp.pprint(config_area)
    exit(0)
elif args.command == "status":
    config_area = process_config_area(read_config_area(ser_port))
    logging.info("Data and OTP: " + ("locked" if config_area['DataOTPlocked'] else "unlocked"))
    logging.info("Config area: " + ("locked" if config_area['ConfigLocked'] else "unlocked"))
    if config_area['DataOTPlocked'] and config_area['ConfigLocked']:
        print("personalized")
    elif not config_area['DataOTPlocked'] and not config_area['ConfigLocked']:
        print("factory")
    else:
        print("neither")
    exit(0)
elif args.command == 'lock_config':
    lock_config(ser_port)
elif args.command == 'random':
    print(binascii.hexlify(get_random(ser_port)))
elif args.command == 'lock_data':
    lock_data(ser_port)
elif args.command == 'personalize':
    config_area = process_config_area(read_config_area(ser_port))
    if not config_area['ConfigLocked']:
        lock_config(ser_port)
    if not config_area['DataOTPlocked']:
        lock_data(ser_port)
    print("personalized")
elif args.command == 'mac':
    mac_challenge = SHA256.new()
    with open(args.file,'r') if args.file else sys.stdin as f:
        while True:
            chunk = f.read(8192)
            if len(chunk) == 0:
                break
            mac_challenge.update(chunk)
    mac(mac_challenge.digest(), MAC_SLOT, ser_port)
elif args.command == 'check_mac':
    try:
        checkmac_challenge = binascii.unhexlify(args.challenge)
        checkmac_mac = binascii.unhexlify(args.mac)
        if len(checkmac_mac) != 32 or len(checkmac_challenge) != 32:
            raise TypeError("Incorrect length (should be 32 bytes / 64 hex chars).")
    except TypeError, e:
        logging.error("Problem processing MAC (--mac) or challenge (--challenge) parameters: "+str(e))
        exit(1)
    else:
        check_mac(checkmac_challenge, checkmac_mac, MAC_SLOT, ser_port)
elif args.command == 'sha':
    try:
        sha_message = binascii.unhexlify(args.sha_message)
        if len(sha_message) > 64:
            raise TypeError("Incorrect length (should be at most 64 bytes / 128 hex chars).")
    except TypeError, e:
        logging.error("Problem processing --sha-message parameter: "+str(e))
        exit(1)
    else:
        sha(sha_message.ljust(64,'\x00'), ser_port)


exit(0)
