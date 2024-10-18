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

#include "homie_node.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace mqtt_homie {

class Proxy;
class HomieDevice;
class HomiePropertyBase;

template<class PropertyClass> class HomieNodeSimpleProperty : public HomieNodeBase {
 public:
  static constexpr auto TAG = "homie:simple_node";

  HomieNodeSimpleProperty(typename PropertyClass::TargetType *target) : property(target) {
    target->add_on_state_callback([this](auto) { notify_property_changed(&property); });
  }

  std::set<std::string> get_properties() const override { return {property.get_id()}; }
  homie::const_property_ptr get_property(const std::string &id) const override {
    if (id == property.get_id())
      return &property;
    return nullptr;
  }
  homie::property_ptr get_property(const std::string &id) override {
    if (id == property.get_id())
      return &property;
    return nullptr;
  }


 protected:
  const esphome::EntityBase *GetEntityBase() const override { return property.target; }
  const esphome::EntityBase_DeviceClass *GetEntityBaseDeviceClass() const override { return property.target; }

  PropertyClass property;
};

class HomieSensorProperty : public HomiePropertyBase {
 public:
  using TargetType = sensor::Sensor;
  TargetType *target;

  HomieSensorProperty(TargetType *target) : target(target) {}

  std::string get_id() const override { return "value"; }
  std::string get_name() const override { return "Value"; }
  bool is_settable() const override { return false; }
  homie::datatype get_datatype() const override { return homie::datatype::number; }

  std::string get_unit() const { return target->get_unit_of_measurement(); }

  std::string get_value() const override {
    int8_t accuracy = target->get_accuracy_decimals();
    return value_accuracy_to_string(target->get_state(), accuracy);
  }

  std::map<std::string, std::string> get_attributes() const override {
    return {
        {"accuracy", std::to_string(target->get_accuracy_decimals())},
        {"state_class", state_class_to_string(target->get_state_class())},
    };
  }
};

class HomieSwitchProperty : public HomiePropertyBase {
 public:
  using TargetType = switch_::Switch;
  TargetType *target;

  HomieSwitchProperty(TargetType *target) : target(target) {}

  std::string get_id() const override { return "state"; }
  std::string get_name() const override { return "State"; }
  bool is_settable() const override { return true; }
  homie::datatype get_datatype() const override { return homie::datatype::boolean; }

  std::map<std::string, std::string> get_attributes() const override {
    return {
        {"inverted", target->is_inverted() ? "true" : "false"},
    };
  }

  std::string get_value() const override { return target->state ? "true" : "false"; }
  void set_value(const std::string &value) override {
    switch (parse_on_off(value.c_str())) {
      case PARSE_ON:
        return target->turn_on();
      case PARSE_OFF:
        return target->turn_off();
      case PARSE_TOGGLE:
        return target->toggle();
      case PARSE_NONE:
      default:
        if (value == "true")
          target->turn_on();
        else
          target->turn_off();
        break;
    }
  }
};

using HomieNodeSensor = HomieNodeSimpleProperty<HomieSensorProperty>;
using HomieNodeSwitch = HomieNodeSimpleProperty<HomieSwitchProperty>;

}  // namespace mqtt_homie
}  // namespace esphome
