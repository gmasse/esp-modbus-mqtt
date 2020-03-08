/*
 main.cpp - esp-modbus-mqtt
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

#include "main.h"

#include <esp_log.h>
#include <math.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
// #include "esp32-hal-log.h"
#elif defined(ARDUINO_ARCH_ESP8266)
// TODO(gmasse): test ESP8266 #include <ESP8266WiFi.h>
// TODO(gmasse): test ESP8266 #include <ESP8266mDNS.h>
#endif
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"
  #include "freertos/task.h"
}

#include <ArduinoJson.h>
#include <AsyncMqttClient.h>

#include <Url.h>
#include "esp_base.h"
#ifndef MODBUS_DISABLED
#include <modbus_base.h>
#endif  // MODBUS_DISABLED


/* The following symbols are passed via BUILD parameters
#define MONITOR_SPEED 115200

#define MQTT_HOST IPAddress(192, 168, 0, 2)
#define MQTT_PORT 1883
#define MQTT_TOPIC "diematic"
*/

static const char *HOSTNAME = "esp-mm-a2kycmFlbXU1cGFn";
static const char __attribute__((__unused__)) *TAG = "Main";

// static const char *FIRMWARE_URL = "https://domain.com/path/file.bin";
static const char *FIRMWARE_VERSION = "000.000.023";

// instanciate AsyncMqttClient object
AsyncMqttClient mqtt_client;

// instanciate timers
TimerHandle_t mqtt_reconnect_timer;
TimerHandle_t wifi_reconnect_timer;
TimerHandle_t modbus_poller_timer;
bool modbus_poller_inprogress = false;

// instanciate task handlers
TaskHandle_t modbus_poller_task_handler = NULL;
TaskHandle_t ota_update_task_handler = NULL;


void resetWiFi() {
  // Set WiFi to station mode
  // and disconnect from an AP if it was previously connected
  ESP_LOGD(TAG, "Resetting Wifi");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
}

void connectToWifi() {
  ESP_LOGD(TAG, "Connecting to '%s'", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  ESP_LOGD(TAG, "Connecting to MQTT");
  mqtt_client.connect();
}

void wiFiEvent(WiFiEvent_t event) {
  ESP_LOGD(TAG, "WiFi event: %d", event);
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      ESP_LOGI(TAG, "WiFi connected with IP address: %s", WiFi.localIP().toString().c_str());
      if (!MDNS.begin(HOSTNAME)) {  // init mdns
        ESP_LOGW(TAG, "Error setting up MDNS responder");
      }
      configTime(0, 0, "pool.ntp.org");  // init UTC time
      struct tm now;
      if (getLocalTime(&now)) {
        ESP_LOGI(TAG, "Time: %4d-%02d-%02d %02d:%02d:%02d",
          now.tm_year+1900, now.tm_mon+1, now.tm_mday,
          now.tm_hour, now.tm_min, now.tm_sec);
      } else {
        ESP_LOGW(TAG, "Failed to obtain time");
      }
      connectToMqtt();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      ESP_LOGW(TAG, "WiFi lost connection");
      xTimerStop(mqtt_reconnect_timer, 0);  // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifi_reconnect_timer, 0);
      break;
    default:
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  ESP_LOGI(TAG, "Connected to MQTT");
  ESP_LOGD(TAG, "Session present: %s", sessionPresent ? "true" : "false");

  String mqtt_topic = MQTT_TOPIC;
  mqtt_topic += "/" + String(HOSTNAME) + "/action/#";
  ESP_LOGI(TAG, "Subscribing at %s", mqtt_topic.c_str());
  // uint16_t packetIdSub = mqtt_client.subscribe(mqtt_topic.c_str(), 1);
  mqtt_client.subscribe(mqtt_topic.c_str(), 1);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  ESP_LOGW(TAG, "Disconnected from MQTT");

  if (WiFi.isConnected()) {
    xTimerStart(mqtt_reconnect_timer, 0);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  ESP_LOGD(TAG, "Subscribe acknowledged for packetId: %d qos: %d", packetId, qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  ESP_LOGD(TAG, "Unsubscribe acknowledged for packetId: %d", packetId);
}

void onMqttMessage(char *topic, char *payload,
  AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  ESP_LOGV(TAG, "Message received (topic=%s, qos=%d, dup=%d, retain=%d, len=%d, index=%d, total=%d): %s",
    topic, properties.qos, properties.dup, properties.retain, len, index, total, payload);

  String suffix = String(topic).substring(strlen(MQTT_TOPIC) + strlen(HOSTNAME) + 9);
          // substring(1 + strlen(MQTT_TOPIC) + strlen("/") + strlen(HOSTNAME) + strlen("/") + strlen("action"))
  ESP_LOGV(TAG, "MQTT topic suffix=%s", suffix.c_str());

  if (suffix == "upgrade") {
    ESP_LOGD(TAG, "MQTT OTA update requested");
    vTaskResume(ota_update_task_handler);
    return;
/*
// TODO(gmasse): fix esp_log_level_set
  } else if (suffix == "loglevel") {
    ESP_LOGD(TAG, "MQTT log level update requested");
    uint8_t log_level_nb;
    if (sscanf(payload, "%hhu", &log_level_nb) == 1) {
      switch (log_level_nb) {
        case 0:
          esp_log_level_set("*", ESP_LOG_NONE);
          ESP_LOGI(TAG, "Log level changed to %#x", ESP_LOG_NONE);
          break;
        case 1:
          esp_log_level_set("*", ESP_LOG_ERROR);
          ESP_LOGI(TAG, "Log level changed to %#x", ESP_LOG_ERROR);
          break;
        case 2:
          esp_log_level_set("*", ESP_LOG_WARN);
          ESP_LOGI(TAG, "Log level changed to %#x", ESP_LOG_WARN);
          break;
        case 3:
          esp_log_level_set("*", ESP_LOG_INFO);
          ESP_LOGI(TAG, "Log level changed to %#x", ESP_LOG_INFO);
          break;
        case 4:
          esp_log_level_set("*", ESP_LOG_DEBUG);
          ESP_LOGI(TAG, "Log level changed to %#x", ESP_LOG_DEBUG);
          break;
        case 5:
          esp_log_level_set("*", ESP_LOG_VERBOSE);
          ESP_LOGI(TAG, "Log level changed to %#x", ESP_LOG_VERBOSE);
          break;
        default:
          ESP_LOGE(TAG, "MQTT Invalid requested log level: %s (expected 0 to 5)", payload);
          break;
      }
    }
*/
  } else {
    ESP_LOGW(TAG, "Unknow MQTT topic received: %s", topic);
  }
}

void onMqttPublish(uint16_t packetId) {
  ESP_LOGD(TAG, "Publish acknowledged for packetId: %d", packetId);
}

void runOtaUpdateTask(void * pvParameters) {
  UBaseType_t __attribute__((__unused__)) uxHighWaterMark;
  /* Inspect our own high water mark on entering the task. */
  uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  ESP_LOGV(TAG, "Entering OTA task. Unused stack size: %d", uxHighWaterMark);

  for (;;) {
    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGV(TAG, "Suspending OTA task. Unused stack size: %d", uxHighWaterMark);
    vTaskSuspend(NULL);  // Task is suspended by default

    ESP_LOGV(TAG, "Resuming OTA task...");
    ESP_LOGI(TAG, "Checking if new firmware is available");
    if (checkFirmwareUpdate(FIRMWARE_URL, FIRMWARE_VERSION)) {
      ESP_LOGI(TAG, "New firmware found");
      ESP_LOGV(TAG, "Suspending modbus poller");
      if (xTimerStop(modbus_poller_timer, 10) == pdFAIL) {
        ESP_LOGW(TAG, "Unable to stop Modbus Poller timer for OTA update");
      }
      vTaskSuspend(modbus_poller_task_handler);

      if (updateOTA(FIRMWARE_URL)) {
        // Update is done. Rebooting...
        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGV(TAG, "Rebooting. Unused stack size: %d", uxHighWaterMark);
        ESP_LOGI(TAG, "************************ REBOOT IN PROGRESS *************************");
        ESP.restart();
      } else {
        ESP_LOGV(TAG, "OTA update failed. Restarting Modbus Poller");
// TODO(gmasse): retry?
        vTaskResume(modbus_poller_task_handler);
        if (xTimerStart(modbus_poller_timer, 10) == pdFAIL) {
          ESP_LOGE(TAG, "Unable to restart Modbus Poller timer after OTA update failure.");
          uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
          ESP_LOGV(TAG, "Rebooting. Unused stack size: %d", uxHighWaterMark);
          ESP_LOGI(TAG, "************************ REBOOT IN PROGRESS *************************");
          ESP.restart();
        }
      }
    }
  }
}

void runModbusPollerTask(void * pvParameters) {
#ifndef MODBUS_DISABLED
  UBaseType_t __attribute__((__unused__)) uxHighWaterMark;
  /* Inspect our own high water mark on entering the task. */
  uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  ESP_LOGV(TAG, "Entering Modbus Poller task. Unused stack size: %d", uxHighWaterMark);

  for (;;) {
    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGV(TAG, "Suspending Modbus Poller task. Unused stack size: %d", uxHighWaterMark);
    vTaskSuspend(NULL);  // Task is suspended by default

    ESP_LOGV(TAG, "Resuming Modbus Poller task");
    if (modbus_poller_inprogress) {  // not sure if needed
      ESP_LOGV(TAG, "Modbus polling in already progress. Waiting for next cycle.");
      return;
    }

    StaticJsonDocument<2000> json_doc;  // instanciate JSON storage
    parseModbusToJson(json_doc.to<JsonVariant>());

    char buffer[1600];
    size_t n = serializeJson(json_doc, buffer);
    ESP_LOGD(TAG, "JSON serialized: %s", buffer);
    if (mqtt_client.connected()) {
      String mqtt_topic = MQTT_TOPIC;
      mqtt_topic += "/" + String(HOSTNAME) + "/data";
      ESP_LOGI(TAG, "MQTT Publishing data to topic: %s", mqtt_topic.c_str());
      mqtt_client.publish(mqtt_topic.c_str(), 0, true, buffer, n);
    }
  }
#endif  // MODBUS_DISABLED
}

void runModbusPollerTimer() {
  ESP_LOGV(TAG, "Time to resume Modbus Poller");
  vTaskResume(modbus_poller_task_handler);
  ESP_LOGV(TAG, "Modbus Poller resume done");
}

void setup() {
  // debug comm
  Serial.begin(MONITOR_SPEED);
  while (!Serial) continue;
  Serial.setDebugOutput(true);
/*
// TODO(gmasse): fix esp_log_level_set
  esp_log_level_set("*", ESP_LOG_VERBOSE);
  esp_log_level_set("ESP_base", ESP_LOG_INFO);
  esp_log_level_set("Modbus_base", ESP_LOG_INFO);
  esp_log_level_set("Main", ESP_LOG_INFO);
  ESP_LOGE("Main", "Error");
  ESP_LOGW("Main", "Warning");
  ESP_LOGI("Main", "Info");
  ESP_LOGD("Main", "Debug");
  ESP_LOGV("Main", "Verbose");
  esp_log_level_set(TAG, ESP_LOG_INFO);
  ESP_LOGE(TAG, "Error");
  ESP_LOGW(TAG, "Warning");
  ESP_LOGI(TAG, "Info");
  ESP_LOGD(TAG, "Debug");
  ESP_LOGV(TAG, "Verbose");
*/

  ESP_LOGI(TAG, "*********************************************************************");
  ESP_LOGI(TAG, "Firmware version %s (compiled at %s %s)", FIRMWARE_VERSION, __DATE__, __TIME__);
  ESP_LOGV(TAG, "Watchdog time-out: %ds", CONFIG_TASK_WDT_TIMEOUT_S);

  mqtt_reconnect_timer = xTimerCreate("mqtt_timer", pdMS_TO_TICKS(2000), pdFALSE,
    NULL, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifi_reconnect_timer = xTimerCreate("wifi_timer", pdMS_TO_TICKS(2000), pdFALSE,
    NULL, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(wiFiEvent);

  mqtt_client.onConnect(onMqttConnect);
  mqtt_client.onDisconnect(onMqttDisconnect);
  mqtt_client.onSubscribe(onMqttSubscribe);
  mqtt_client.onUnsubscribe(onMqttUnsubscribe);
  mqtt_client.onMessage(onMqttMessage);
  mqtt_client.onPublish(onMqttPublish);

  IPAddress mqtt_ip;
  mqtt_ip.fromString(MQTT_HOST_IP);
  mqtt_client.setServer(mqtt_ip, MQTT_PORT);

  connectToWifi();

#ifndef MODBUS_DISABLED
  initModbus();

  xTaskCreate(runModbusPollerTask, "modbus_poller", 5900, NULL, 1, &modbus_poller_task_handler);
  configASSERT(modbus_poller_task_handler);

  modbus_poller_timer = xTimerCreate("modbus_poller_timer", pdMS_TO_TICKS(MODBUS_SCANRATE*1000), pdTRUE, NULL,
    reinterpret_cast<TimerCallbackFunction_t>(runModbusPollerTimer));
  if (modbus_poller_timer == NULL) {
    // The timer was not created
  } else {
    if (xTimerStart(modbus_poller_timer, 0) != pdPASS) {
      // The timer could not be set into the Active state
    }
  }
#endif  // MODBUS_DISABLED

  xTaskCreate(runOtaUpdateTask, "ota_update", 4500, NULL, 2, &ota_update_task_handler);
  configASSERT(ota_update_task_handler);
}

void loop() {
}
