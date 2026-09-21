#pragma once

// Force the ESP32 core networking layer to load before WiFi

extern "C" {
#include "esp_event.h"
#include "esp_wifi.h"
}

#include <Network.h>
#include <WiFi.h>