/*
 modbus_base.cpp - Modbus functions
 Copyright (C) 2020 Germain Masse

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "Arduino.h"
#include <ModbusMaster.h>
#include <ArduinoJson.h>
#include <modbus_registers.h>

#include "modbus_base.h"

static const char __attribute__((__unused__)) *TAG = "Modbus_base";

/* The following symbols are passed via BUILD parameters
#define RXD 27 // aka R0
#define TXD 26 // aka DI
#define RTS 25 // aka DE and /RE
//if there is no flow control PIN
//#define RTS            NOT_A_PIN

#define MODBUS_BAUDRATE 9600
#define MODBUS_UNIT 10
#define MODBUS_RETRIES 2
#define MODBUS_SCANRATE 30 // in seconds
*/

// instantiate ModbusMaster object
ModbusMaster modbus_client;

void preTransmission() {
  digitalWrite(RTS, 1);
}

void postTransmission() {
  digitalWrite(RTS, 0);
}

void initModbus() {
  Serial2.begin(MODBUS_BAUDRATE, SERIAL_8N1, RXD, TXD);  // Using ESP32 UART2 for Modbus
  modbus_client.begin(MODBUS_UNIT, Serial2);

  // do we have a flow control pin?
  if (RTS != NOT_A_PIN) {
    // Init in receive mode
    pinMode(RTS, OUTPUT);
    digitalWrite(RTS, 0);

    // Callbacks allow us to configure the RS485 transceiver correctly
    modbus_client.preTransmission(preTransmission);
    modbus_client.postTransmission(postTransmission);
  }
}

bool getModbusResultMsg(ModbusMaster *node, uint8_t result) {
  String tmpstr2 = "";
  switch (result) {
    case node->ku8MBSuccess:
      return true;
      break;
      case node->ku8MBIllegalFunction:
      tmpstr2 += "Illegal Function";
      break;
    case node->ku8MBIllegalDataAddress:
      tmpstr2 += "Illegal Data Address";
      break;
    case node->ku8MBIllegalDataValue:
      tmpstr2 += "Illegal Data Value";
      break;
    case node->ku8MBSlaveDeviceFailure:
      tmpstr2 += "Slave Device Failure";
      break;
    case node->ku8MBInvalidSlaveID:
      tmpstr2 += "Invalid Slave ID";
      break;
    case node->ku8MBInvalidFunction:
      tmpstr2 += "Invalid Function";
      break;
    case node->ku8MBResponseTimedOut:
      tmpstr2 += "Response Timed Out";
      break;
    case node->ku8MBInvalidCRC:
      tmpstr2 += "Invalid CRC";
      break;
    default:
      tmpstr2 += "Unknown error: " + String(result);
      break;
  }
  ESP_LOGV(TAG, "%s", tmpstr2.c_str());
  return false;
}

bool getModbusValue(uint16_t register_id, modbus_entity_t modbus_entity, uint16_t *value_ptr) {
  ESP_LOGD(TAG, "Requesting data");
  for (uint8_t i = 1; i <= MODBUS_RETRIES + 1; ++i) {
    ESP_LOGV(TAG, "Trial %d/%d", i, MODBUS_RETRIES + 1);
    switch (modbus_entity) {
      case MODBUS_TYPE_HOLDING:
        uint8_t result;
        result = modbus_client.readHoldingRegisters(register_id, 1);
        if (getModbusResultMsg(&modbus_client, result)) {
          *value_ptr = modbus_client.getResponseBuffer(0);
          ESP_LOGV(TAG, "Data read: %x", *value_ptr);
          return true;
        }
        break;
      default:
        ESP_LOGW(TAG, "Unsupported Modbus entity type");
        value_ptr = nullptr;
        return false;
        break;
    }
  }
  // Time-out
  ESP_LOGW(TAG, "Time-out");
  value_ptr = nullptr;
  return false;
}

bool decodeDiematicDecimal(uint16_t int_input, int8_t decimals, float *value_ptr) {
  ESP_LOGV(TAG, "Decoding %#x with %d decimal(s)", int_input, decimals);
  if (int_input == 65535) {
    value_ptr = nullptr;
    return false;
  } else {
    uint16_t masked_input = int_input & 0x7FFF;
    float output = static_cast<float>(masked_input);
    if (int_input >> 15 == 1) {
      output = -output;
    }
    *value_ptr = output / pow(10, decimals);
    return true;
  }
}

void readModbusRegisterToJson(uint16_t register_id, ArduinoJson::JsonVariant variant) {
  // searchin for register matching register_id
  const uint8_t item_nb = sizeof(registers) / sizeof(modbus_register_t);
  for (uint8_t i = 0; i < item_nb; ++i) {
    if (registers[i].id != register_id) {
      // not this one
      continue;
    } else {
      // register found
      ESP_LOGD(TAG, "Register id=%d type=0x%x name=%s", registers[i].id, registers[i].type, registers[i].name);
      uint16_t raw_value;
      if (getModbusValue(registers[i].id, registers[i].modbus_entity, &raw_value)) {
        ESP_LOGV(TAG, "Raw value: %s=%#06x", registers[i].name, raw_value);
        switch (registers[i].type) {
          case REGISTER_TYPE_DIEMATIC_ONE_DECIMAL:
            float final_value;
            if (decodeDiematicDecimal(raw_value, 1, &final_value)) {
              ESP_LOGV(TAG, "VALUE: %.1f", final_value);
              variant[registers[i].name] = final_value;
            } else {
              ESP_LOGD(TAG, "VALUE: Invalid Diematic value");
            }
            break;
          case REGISTER_TYPE_BITFIELD:
            for (uint8_t j = 0; j < 16; ++j) {
              const char *bit_varname = registers[i].optional_param.bitfield[j];
              const uint8_t bit_value = raw_value >> j & 1;
              ESP_LOGV(TAG, " [bit%02d] %s=%d", j, bit_varname, bit_value);
              variant[bit_varname] = bit_value;
            }
            break;
          default:
            // Unsupported type
            ESP_LOGW(TAG, "Unsupported register type");
            break;
        }
      } else {
        ESP_LOGW(TAG, "Request failed!");
      }
      return;  // break the loop and exit the function
    }
  }
  // register not found
}

void parseModbusToJson(ArduinoJson::JsonVariant variant) {
  ESP_LOGI(TAG, "Parsing all Modbus registers (Logging Tag: %s)", TAG);
  uint8_t item_nb = sizeof(registers) / sizeof(modbus_register_t);
  for (uint8_t i = 0; i < item_nb; ++i) {
    readModbusRegisterToJson(registers[i].id, variant);
  }
}
