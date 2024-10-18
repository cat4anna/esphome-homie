#pragma once

#include "esphome/core/defines.h"

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"
// #include "esphome/components/json/json_util.h"
// #include "esphome/components/network/ip_address.h"
// #include "lwip/ip_addr.h"

#include <vector>
#include <memory>

#include <stdexcept>
#include "homie-cpp-merged.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/mqtt_client/mqtt_client.h"

namespace esphome {
namespace mqtt_homie {

class MqttProxy;

class HomieDevice;
class HomieNodeBase;

class HomieClient : public Component {
 public:
  HomieClient(mqtt_client::MQTTClientComponent *client);

  void start_homie(HomieDevice* device, std::string prefix);

  void set_update_interval(uint32_t) { /* nothing */ }

 protected:
  std::unique_ptr<homie::client> homie_client;
  std::unique_ptr<MqttProxy> mqtt_proxy;
};

}  // namespace mqtt_homie
}  // namespace esphome
