#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/controller.h"

#include <vector>
#include <memory>
#include <map>

#include "homie-cpp.h"

namespace esphome {
namespace mqtt_homie {

class HomieNodeBase;
class HomiePropertyBase;

class HomieDevice : public ::homie::device, public PollingComponent {
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
  std::map<std::string, std::string> get_stats() const override;

  void attach_node(HomieNodeBase *node);
  void notify_node_changed(HomieNodeBase *node, HomiePropertyBase *property);
  void set_client(homie::client *client) { m_client = client; }

  void setup() override;
  void update() override;

  void set_stats_interval(int v) { m_stat_update_interval = v; }

  void push_log_message(int level, const char *tag, const char *message) const;

 private:
  homie::client *m_client;
  std::map<std::string, HomieNodeBase *> m_nodes;

  homie::device_state m_device_state = homie::device_state::lost;
  bool m_protocol_initialised = false;

  int m_stat_update_interval = 60000;
  mutable uint64_t m_uptime_ms = 0;

  uint64_t get_uptime_seconds() const;

  void goto_state(homie::device_state new_state);
  void check_device_state();
};

}  // namespace mqtt_homie
}  // namespace esphome