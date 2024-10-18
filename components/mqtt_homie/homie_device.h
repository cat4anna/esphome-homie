
#pragma once

#include "esphome/core/defines.h"

#include <vector>
#include <memory>
#include <map>

#include <stdexcept>
#include "homie-cpp-merged.h"

namespace esphome {
namespace mqtt_homie {

class HomieNodeBase;
class HomiePropertyBase;

class HomieDevice : public homie::device, public PollingComponent {
 public:
  static constexpr auto TAG = "homie:device";

  HomieDevice() = default;

  const std::string &get_id() const override;
  const std::string &get_name() const override;

  std::set<std::string> get_nodes() const override;
  homie::node_ptr get_node(const std::string &id) override;
  homie::const_node_ptr get_node(const std::string &id) const override;

  homie::device_state get_state() const override;
  std::map<std::string, std::string> get_attributes() const override;

  void attach_node(HomieNodeBase *node);
  void notify_node_changed(HomieNodeBase *node, HomiePropertyBase *property);
  void set_client(homie::client *client) { this->client = client; }

  void setup() override;
  void update() override;

 private:
  homie::client *client;
  std::map<std::string, HomieNodeBase *> nodes;

  uint32_t last_known_state = (~0);
  homie::device_state device_state = homie::device_state::init;

  void goto_state(homie::device_state new_state);
};

}  // namespace mqtt_homie
}  // namespace esphome