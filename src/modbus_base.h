/*
 modbus_base.h - Modbus functions headers
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

#ifndef SRC_MODBUS_BASE_H_
#define SRC_MODBUS_BASE_H_

#include <ModbusMaster.h>
#include <ArduinoJson.h>
#include <modbus_registers.h>

void preTransmission();
void postTransmission();
void initModbus();
bool getModbusResultMsg(ModbusMaster *node, uint8_t result);
bool getModbusValue(uint16_t register_id, modbus_entity_t modbus_entity, uint16_t *value_ptr);
bool decodeDiematicDecimal(uint16_t int_input, int8_t decimals, float *value_ptr);
void parseModbusToJson(ArduinoJson::JsonVariant variant);

#endif  // SRC_MODBUS_BASE_H_
