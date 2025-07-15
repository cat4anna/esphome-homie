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

#if 0  // USE_LIGHT

// TODO

class HomieLightNode : public HomieNodeBase {
 public:
  static constexpr auto TAG = "homie:light";

  using LightOutput = esphome::light::LightOutput;

  HomieLightNode(LightOutput *light_output) : property(light_output) {
    // target->add_on_state_callback([this](auto) { notify_property_changed(&property); });
  }

  //   std::set<std::string> get_properties() const override { return {property.get_id()}; }
  //   homie::const_property_ptr get_property(const std::string &id) const override {
  //     if (id == property.get_id())
  //       return &property;
  //     return nullptr;
  //   }
  //   homie::property_ptr get_property(const std::string &id) override {
  //     if (id == property.get_id())
  //       return &property;
  //     return nullptr;
  //   }

 protected:
  const esphome::EntityBase *GetEntityBase() const override { return property.target; }
  const esphome::EntityBase_DeviceClass *GetEntityBaseDeviceClass() const override { return property.target; }

  //   PropertyClass property;
};

// class HomieBinarySensorProperty : public HomiePropertyBase {
//  public:
//   using TargetType = esphome::binary_sensor::BinarySensor;
//   TargetType *target;

//   HomieBinarySensorProperty(TargetType *target) : target(target) {}

//   std::string get_id() const override { return "state"; }
//   std::string get_name() const override { return "State"; }
//   homie::datatype get_datatype() const override { return homie::datatype::boolean; }

//   // std::map<std::string, std::string> get_attributes() const override { return {}; }

//   std::string get_value() const override { return target->state ? "true" : "false"; }
// };
// using HomieNodeLight = HomieNodeSingleProperty<HomieBinarySensorProperty>;

// class HomieNodeLight {}

#endif

}  // namespace mqtt_homie
}  // namespace esphome
