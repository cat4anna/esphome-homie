#pragma once

#include "esphome.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"

#include <vector>
#include <memory>
#include <map>
#include <stdexcept>

#include "homie-cpp.h"
#include "homie_node.h"

namespace esphome {
namespace mqtt_homie {

class Proxy;
class HomieDevice;

template<class PropertyClass> class HomieNodeSingleProperty : public HomieNodeBase {
 public:
  static constexpr auto TAG = "homie:simple_node";

  HomieNodeSingleProperty(typename PropertyClass::TargetType *target) : property(target) {
    target->add_on_state_callback([this](auto) { notify_property_changed(&property); });
    property.set_parent(this);
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
  const esphome::EntityBase_DeviceClass *GetEntityBaseDeviceClass() const override {
    return property.target;
  }

  PropertyClass property;
};

#ifdef USE_SENSOR
class HomieSensorProperty : public HomiePropertyBase {
 public:
  using TargetType = sensor::Sensor;
  TargetType *target;

  HomieSensorProperty(TargetType *target) : target(target) {}

  std::string get_id() const override { return "value"; }
  std::string get_name() const override { return "Value"; }
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
using HomieNodeSensor = HomieNodeSingleProperty<HomieSensorProperty>;
#endif

#ifdef USE_SWITCH
class HomieSwitchProperty : public HomiePropertyBase {
 public:
  static constexpr auto TAG = "homie:SwitchProperty";
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
using HomieNodeSwitch = HomieNodeSingleProperty<HomieSwitchProperty>;
#endif

#ifdef USE_BINARY_SENSOR
class HomieBinarySensorProperty : public HomiePropertyBase {
 public:
  using TargetType = esphome::binary_sensor::BinarySensor;
  TargetType *target;

  HomieBinarySensorProperty(TargetType *target) : target(target) {}

  std::string get_id() const override { return "state"; }
  std::string get_name() const override { return "State"; }
  homie::datatype get_datatype() const override { return homie::datatype::boolean; }

  std::string get_value() const override { return target->state ? "true" : "false"; }
};
using HomieNodeBinarySensor = HomieNodeSingleProperty<HomieBinarySensorProperty>;
#endif

}  // namespace mqtt_homie
}  // namespace esphome
