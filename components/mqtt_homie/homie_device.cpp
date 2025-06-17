
#include "esphome/core/defines.h"

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"

#include <vector>
#include <memory>

#include "homie_device.h"
#include "homie_node.h"
#include "esphome/core/application.h"
#include "esphome/core/version.h"
#include "esphome/components/network/util.h"

namespace esphome {
namespace mqtt_homie {

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
  return {
      {"mac", get_mac_address_pretty()},

      {"fw/name", "esphome"},
      {"fw/version", ESPHOME_VERSION},
      {"fw/implementation", "esphome/homie-cpp"},
      {"fw/date", App.get_compilation_time()},

      {"comment", App.get_comment()},
      {"area", App.get_area()},

      {"localip", network::get_ip_addresses()[0].str()},
      {"domain", network::get_use_address()},
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
  if (new_state != device_state) {
    ESP_LOGI(TAG, "State changed %s->%s", homie::enum_to_string(device_state).c_str(),
             homie::enum_to_string(new_state).c_str());
    device_state = new_state;
    client->notify_device_state_changed();
  }
}

void HomieDevice::setup() {
  // nothing here
}

void HomieDevice::update() {
  if (!client->is_connected()) {
    goto_state(homie::device_state::init);
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

}  // namespace mqtt_homie
}  // namespace esphome