#include "device_info.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP32

#include <esp_sleep.h>

#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_partition.h>

#ifdef USE_ARDUINO
#include <Esp.h>
#endif

namespace esphome::mqtt_homie {

#ifdef HOMIE_DEVICE_INFO_CHIP_ID
std::string get_chip_id() { return ESP.getChipId(); }
#endif

std::string get_cpu_frequency() { return std::to_string(esphome::arch_get_cpu_freq_hz() / 1000000) + "MHz"; }

std::string get_free_heap() { return std::to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)); }

}  // namespace esphome::mqtt_homie

#endif
namespace esphome::mqtt_homie {

std::string get_framework_name() {
#ifdef USE_ARDUINO
  return "Arduino";
#elif defined(USE_ESP_IDF)
  return "ESP-IDF";
#else
  return "Unknown";
#endif
}
}  // namespace esphome::mqtt_homie
