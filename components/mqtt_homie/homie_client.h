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

  void set_update_interval(uint32_t) {}
  void setup_logging(int level) { m_log_level = level; };

  void setup() override;
  void loop() override;

  void start_homie(HomieDevice *device, std::string prefix, int qos, bool retained);

 protected:
  int m_log_level = ESPHOME_LOG_LEVEL_NONE;
  HomieDevice *m_device = nullptr;
  std::unique_ptr<homie::client> m_homie_client;
  std::unique_ptr<MqttProxy> m_mqtt_proxy;
};

}  // namespace esphome::mqtt_homie
