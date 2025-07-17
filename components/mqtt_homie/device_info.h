
#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/controller.h"

#include <vector>
#include <memory>
#include <map>

namespace esphome::mqtt_homie {

#if defined(USE_ESP32) && defined(USE_ARDUINO)
std::string get_chip_id();
#define HOMIE_DEVICE_INFO_CHIP_ID
#endif

std::string get_framework_name();
std::string get_cpu_frequency();
std::string get_free_heap();

}  // namespace esphome::mqtt_homie
