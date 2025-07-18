
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

namespace {
constexpr const char *gHomeStatTimerId = "homie_stats";
}

namespace esphome::mqtt_homie {

constexpr uint8_t MakeStateTransition(homie::device_state from, homie::device_state to) {
  return static_cast<uint8_t>(from) | (static_cast<uint8_t>(to) << 4);
}

const std::string &HomieDevice::get_id() const { return App.get_name(); }
const std::string &HomieDevice::get_name() const { return App.get_friendly_name(); }

std::set<std::string> HomieDevice::get_nodes() const {
  std::set<std::string> r;
  for (auto &node : m_nodes)
    r.insert(node.first);
  return r;
}

homie::node_ptr HomieDevice::get_node(const std::string &id) {
  if (auto it = m_nodes.find(id); it != m_nodes.end())
    return it->second;
  return nullptr;
}

homie::const_node_ptr HomieDevice::get_node(const std::string &id) const {
  if (auto it = m_nodes.find(id); it != m_nodes.end())
    return it->second;
  return nullptr;
}

homie::device_state HomieDevice::get_state() const { return m_device_state; }

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
      {"stats/interval", std::to_string(m_stat_update_interval / 1000)},
  };
}

std::map<std::string, std::string> HomieDevice::get_stats() const {
  auto rssi = wifi::global_wifi_component->wifi_rssi();
  return {
      {"uptime", std::to_string(get_uptime_seconds())},
      {"signal", std::to_string(clamp(2 * (rssi + 100), 0, 100)) + "%"},
      {"signal_db", std::to_string(rssi)},
      {"freeheap", get_free_heap()},
  };
}

void HomieDevice::attach_node(HomieNodeBase *node) {
  m_nodes[node->get_id()] = node;
  node->attach_device(this);
}

void HomieDevice::notify_node_changed(HomieNodeBase *node, HomiePropertyBase *property) {
  if (m_client) {
    m_client->notify_property_changed(node->get_id(), property->get_id());
  }
}

void HomieDevice::goto_state(homie::device_state new_state) {
  if (new_state == m_device_state) {
    return;
  }

  const auto prev_state = m_device_state;
  m_device_state = new_state;

  ESP_LOGI(TAG, "State changed %s->%s", homie::enum_to_string(prev_state).c_str(),
           homie::enum_to_string(new_state).c_str());

  m_client->notify_device_state_changed();

  using device_state = homie::device_state;
  auto transition = MakeStateTransition(prev_state, new_state);
  switch (transition) {
    case MakeStateTransition(device_state::init, device_state::ready):
    case MakeStateTransition(device_state::init, device_state::alert):
      m_client->update_device_stats();
      m_client->start_subscription();
      this->set_interval(gHomeStatTimerId, m_stat_update_interval,
                         [this] { m_client->update_device_stats(); });
      break;

    case MakeStateTransition(device_state::disconnected, device_state::init):
      m_client->publish_device_info();
      break;

    case MakeStateTransition(device_state::ready, device_state::disconnected):
    case MakeStateTransition(device_state::alert, device_state::disconnected):
      m_client->stop_subscription();
      this->cancel_interval(gHomeStatTimerId);
      break;

    case MakeStateTransition(device_state::ready, device_state::alert):
    case MakeStateTransition(device_state::alert, device_state::ready):
      // nothing
      break;
    case MakeStateTransition(device_state::lost, device_state::disconnected):
      // initial setup
      break;

    default:
      ESP_LOGW(TAG, "Unhandled state transition: %02x", transition);
  }
}

void HomieDevice::check_device_state() {
  using device_state = homie::device_state;

  if (!m_client->is_connected()) {
    goto_state(device_state::disconnected);
    return;
  }

  if (m_device_state == device_state::disconnected) {
    goto_state(device_state::init);
    return;
  }

  const auto app_state = App.get_app_state();

  const auto led_status = app_state & STATUS_LED_MASK;
  if ((led_status & STATUS_LED_ERROR) != 0) {
    goto_state(homie::device_state::alert);
    return;
  }

  goto_state(homie::device_state::ready);
}

void HomieDevice::setup() { goto_state(homie::device_state::disconnected); }

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

void HomieDevice::update() { check_device_state(); }

void HomieDevice::push_log_message(int level, const char *tag, const char *message) const {
  m_client->publish_log_message(message);
}

}  // namespace esphome::mqtt_homie