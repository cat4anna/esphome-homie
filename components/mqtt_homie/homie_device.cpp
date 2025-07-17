
#include "esphome/core/defines.h"

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"

#include <vector>
#include <memory>

#include "homie_device.h"
#include "homie_node.h"
#include "device_info.h"

#include "esphome/core/application.h"
#include "esphome/core/version.h"
#include "esphome/components/network/util.h"
#include "esphome/components/wifi/wifi_component.h"

namespace esphome::mqtt_homie {

const std::string &HomieDevice::get_id() const { return App.get_name(); }
const std::string &HomieDevice::get_name() const { return App.get_friendly_name(); }

std::set<std::string> HomieDevice::get_nodes() const {
  std::set<std::string> r;
  for (auto &node : nodes)
    r.insert(node.first);
  return r;
}

homie::node_ptr HomieDevice::get_node(const std::string &id) {
  if (auto it = nodes.find(id); it != nodes.end())
    return it->second;
  return nullptr;
}

homie::const_node_ptr HomieDevice::get_node(const std::string &id) const {
  if (auto it = nodes.find(id); it != nodes.end())
    return it->second;
  return nullptr;
}

homie::device_state HomieDevice::get_state() const { return device_state; }

std::map<std::string, std::string> HomieDevice::get_attributes() const {
  const char *model = ESPHOME_VARIANT;
  return {
      {"mac", get_mac_address_pretty()},
      {"localip", network::get_ip_addresses()[0].str()},

      {"fw/name", "esphome"},
      {"fw/version", ESPHOME_VERSION},
      {"implementation", "esphome/homie-cpp"},

      {"implementation/board", ESPHOME_BOARD},
      {"implementation/board_variant", ESPHOME_VARIANT},
      {"implementation/date", App.get_compilation_time()},
      {"implementation/comment", App.get_comment()},
      {"implementation/area", App.get_area()},
      {"implementation/domain", network::get_use_address()},
      {"implementation/framework", get_framework_name()},
      {"implementation/cpu_speed", get_cpu_frequency()},
#ifdef HOMIE_DEVICE_INFO_CHIP_ID
      {"implementation/chip_id", get_chip_id()},
#endif

      {"stats/stats", "uptime,signal,freeheap"},
      {"stats/interval", std::to_string(m_stat_update_interval)},
  };
}

std::map<std::string, std::string> HomieDevice::get_stats() const {
  return {
      {"uptime", std::to_string(get_uptime_seconds())},
      {"signal", std::to_string(wifi::global_wifi_component->wifi_rssi())},
      {"freeheap", get_free_heap()},
  };
}

void HomieDevice::attach_node(HomieNodeBase *node) {
  nodes[node->get_id()] = node;
  node->attach_device(this);
}

void HomieDevice::notify_node_changed(HomieNodeBase *node, HomiePropertyBase *property) {
  if (client) {
    client->notify_property_changed(node->get_id(), property->get_id());
  }
}

void HomieDevice::goto_state(homie::device_state new_state) {
  if (new_state == device_state) {
    return;
  }

  ESP_LOGI(TAG, "State changed %s->%s", homie::enum_to_string(device_state).c_str(),
           homie::enum_to_string(new_state).c_str());
  device_state = new_state;
  client->notify_device_state_changed();

  switch (new_state) {
    case homie::device_state::init:
      break;
    case homie::device_state::ready:
      client->update_device_stats();
      break;
  }

  if (new_state == homie::device_state::ready) {
    this->set_interval("homie_stats", 1000 * m_stat_update_interval, [this]() { client->update_device_stats(); });
  } else {
    this->cancel_interval("homie_stats");
  }
}

void HomieDevice::setup() {
  // nothing here
}

uint64_t HomieDevice::get_uptime_seconds() const {
  const uint32_t ms = millis();
  const uint64_t ms_mask = (1ULL << 32) - 1ULL;
  const uint32_t last_ms = this->m_uptime_ms & ms_mask;
  if (ms < last_ms) {
    this->m_uptime_ms += ms_mask + 1ULL;
    ESP_LOGI(TAG, "Detected uptime roll-over");
  }
  this->m_uptime_ms &= ~ms_mask;
  this->m_uptime_ms |= ms;

  return this->m_uptime_ms / 1000ULL;
}

void HomieDevice::update() {
  if (!client->is_connected()) {
    goto_state(homie::device_state::disconnected);
    return;
  }

  auto app_state = App.get_app_state();
  if (last_known_state != app_state) {
    last_known_state = app_state;

    homie::device_state new_device_state = homie::device_state::init;
    auto next = [&new_device_state](homie::device_state v) {
      if (v > new_device_state)
        new_device_state = v;
    };

    auto led_status = app_state & STATUS_LED_MASK;
    if (led_status == STATUS_LED_ERROR) {
      next(homie::device_state::alert);
    } else {
      next(homie::device_state::ready);
    }

    goto_state(new_device_state);
  }
}

}  // namespace esphome::mqtt_homie