#pragma once

#include "esphome/core/defines.h"

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"

#include <vector>
#include <memory>

#include <stdexcept>
#include "homie-cpp-merged.h"

namespace esphome {
namespace mqtt_homie {

class Proxy;
class HomieDevice;
class HomiePropertyBase;

class HomieNodeBase : public Component, public homie::node {
 public:
  std::string get_id() const override;
  std::string get_name() const override;
  std::string get_name(int64_t node_idx) const override;
  std::string get_type() const override;
  bool is_array() const override;
  std::pair<int64_t, int64_t> array_range() const override;


	std::map<std::string, std::string> get_attributes() const override;

  void attach_device(HomieDevice *device) { this->device = device; }
  void notify_property_changed(HomiePropertyBase *property);

 protected:
  HomieDevice *device = nullptr;
  virtual const esphome::EntityBase *GetEntityBase() const = 0;
  virtual const esphome::EntityBase_DeviceClass *GetEntityBaseDeviceClass() const { return nullptr; }
};

class HomiePropertyBase : public homie::property {
 public:
  // virtual std::string get_id() const = 0;
  // virtual std::string get_name() const = 0;

  homie::datatype get_datatype() const override { return homie::datatype::string; };
  std::string get_format() const override { return ""; }
  std::string get_value() const override { return ""; }
  std::string get_unit() const override { return ""; }
  std::string get_value(int64_t node_idx) const override { return ""; }
  void set_value(int64_t node_idx, const std::string &value) override {};
  void set_value(const std::string &value) override {}

  bool is_settable() const override { return false; }
  bool is_retained() const override { return false; }

	std::map<std::string, std::string> get_attributes() const override { return {}; }
};

}  // namespace mqtt_homie
}  // namespace esphome
