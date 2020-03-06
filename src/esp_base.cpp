/*
 esp_base.cpp - ESP core functions
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

#include "esp_base.h"

#include "Arduino.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>

#include <Url.h>
#include "cert.h"

static const char *VERSION_HEADER = "X-Object-Meta-Version";

static const char __attribute__((__unused__)) *TAG = "ESP_base";


bool _httpRequest(HTTPClient *http_client, const String& url_s) {
  bool ret = false;
  Url url(url_s);
  if (url.Protocol == "https") {
    const uint16_t port = (url.Port.isEmpty() ? 443 : url.Port.toInt());
    ret = http_client->begin(url.Host, port, url.Path, rootCACertificate);
  } else if (url.Protocol == "http") {
    const uint16_t port = (url.Port.isEmpty() ? 80 : url.Port.toInt());
    ret = http_client->begin(url.Host, port, url.Path);
  } else {
    ESP_LOGE(TAG, "Unsupported protocol: %s", url.Protocol.c_str());
    return false;
  }

  if (ret) {
    int httpCode = http_client->GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      ESP_LOGV(TAG, "HTTP GET... code: %d", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        return true;
      } else if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND
              || httpCode == HTTP_CODE_TEMPORARY_REDIRECT || httpCode == HTTP_CODE_PERMANENT_REDIRECT ) {
        ESP_LOGE(TAG, "HTTP redirection not supported (yet): %d", httpCode);
      } else {
        ESP_LOGE(TAG, "Unsupported HTTP response code: %d", httpCode);
      }
    } else {
      ESP_LOGE(TAG, "HTTP GET... failed, error: %s", http_client->errorToString(httpCode).c_str());
    }
  }
  return false;
}

String _requestFirmwareVersion(const String &url_s) {
  ESP_LOGD(TAG, "Requesting firmware from url=%s", url_s.c_str());
  String firmware_version = "";
  HTTPClient client;
  const char* headers_list[] = { VERSION_HEADER, "Content-Type" };
  client.collectHeaders(headers_list, sizeof(headers_list)/sizeof(headers_list[0]));
  if (_httpRequest(&client, url_s)) {
    ESP_LOGV(TAG, "Header %s=%s", VERSION_HEADER, client.header(VERSION_HEADER).c_str());
    ESP_LOGV(TAG, "Header %s=%s", "Content-Type", client.header("Content-Type").c_str());
    if (client.header("Content-Type") == "application/octet-stream") {
      firmware_version = client.header(VERSION_HEADER);
    } else {
      ESP_LOGE(TAG, "Invalid Content-Type: %s", client.header("Content-Type").c_str());
    }
  }
  client.end();
  return firmware_version;
}

bool checkFirmwareUpdate(const String& url_s, const String& current_version) {
  ESP_LOGD(TAG, "Checking firmware version=%s from url=%s", current_version.c_str(), url_s.c_str());

  const String remote_version = _requestFirmwareVersion(url_s);
  if (remote_version.isEmpty()) {
    ESP_LOGW(TAG, "Remote firmware not found");
  } else if (remote_version > current_version) {
    ESP_LOGI(TAG, "New firmware version detected: %s", remote_version.c_str());
    return true;
  } else {
    ESP_LOGD(TAG, "Firmware remote version: %s", remote_version.c_str());
    ESP_LOGI(TAG, "Firmware is already up to date (version %s)", current_version.c_str());
  }
  return false;
}

bool updateOTA(const String& url_s) {
  HTTPClient client;
  if (_httpRequest(&client, url_s)) {
    int len = client.getSize();
    WiFiClient *tcp = client.getStreamPtr();

    // check whether we have everything for OTA update
    if (len) {
      if (Update.begin(len)) {
        ESP_LOGW(TAG, "Starting Over-The-Air update. This may take some time to complete ...");
        size_t written = Update.writeStream(*tcp);

        if (written == len) {
          ESP_LOGD(TAG, "Written: %d successfully", written);
        } else {
          ESP_LOGE(TAG, "Written only: %d/%d. Retry?", written, len);
          // Retry??
        }

        if (Update.end()) {
          if (Update.isFinished()) {
            ESP_LOGW(TAG, "OTA update has successfully completed. Reboot needed...");
            client.end();
            return true;
          } else {
            ESP_LOGE(TAG, "Something went wrong! OTA update hasn't been finished properly.");
          }
        } else {
          ESP_LOGE(TAG, "An error Occurred. Error #: %d", Update.getError());
        }
      } else {
        ESP_LOGE(TAG, "There isn't enough space to start OTA update");
      }
    } else {
      ESP_LOGE(TAG, "Invalid content-length received from server");
    }
  } else {
    ESP_LOGE(TAG, "Unable to connect to server");
  }
  client.end();
  return false;
}
