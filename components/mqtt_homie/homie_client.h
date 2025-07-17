#pragma once

#include "esphome/core/defines.h"

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"

#include <vector>
#include <memory>

#include <stdexcept>
#include "homie-cpp.h"

#include "esphome/components/mqtt/mqtt_client.h"

namespace esphome::mqtt_homie {

class MqttProxy;

class HomieDevice;
class HomieNodeBase;

class HomieClient : public Component {
 public:
  HomieClient(mqtt::MQTTClientComponent *client);

  void start_homie(HomieDevice *device, std::string prefix, int qos, bool retained);

  void set_update_interval(uint32_t) { /* nothing */
  }

 protected:
  std::unique_ptr<homie::client> homie_client;
  std::unique_ptr<MqttProxy> mqtt_proxy;
};

}  // namespace esphome::mqtt_homie
