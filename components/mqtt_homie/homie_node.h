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
  static constexpr auto TAG = "homie:node";

  std::string get_id() const override;
  std::string get_name() const override;
  std::string get_name(int64_t node_idx) const override;
  std::string get_type() const override;
  bool is_array() const override;
  std::pair<int64_t, int64_t> array_range() const override;

  std::map<std::string, std::string> get_attributes() const override;

  void attach_device(HomieDevice *device) { this->device = device; }
  void notify_property_changed(HomiePropertyBase *property);
  void notify_property_changed(const std::string &name);
  void notify_all_properties_changed();

 protected:
  HomieDevice *device = nullptr;
  virtual const esphome::EntityBase *GetEntityBase() const = 0;
  virtual const esphome::EntityBase_DeviceClass *GetEntityBaseDeviceClass() const { return nullptr; }
};

struct PropertyDescriptor {
  const char *id;
  const char *name;
  const char *format = "";
  const char *unit = "";
  bool retained = false;
  homie::datatype datatype = homie::datatype::string;
  std::function<std::string()> getter = {};
  std::function<void(const std::string &value)> setter = {};
};

class HomieNodeMultiProperty : public HomieNodeBase {
 public:
  std::set<std::string> get_properties() const final;
  homie::const_property_ptr get_property(const std::string &id) const final;
  homie::property_ptr get_property(const std::string &id) final;

 protected:
  void attach_property(std::unique_ptr<HomiePropertyBase> property);
  void create_properties(std::initializer_list<PropertyDescriptor> descriptors);

 private:
  std::map<std::string, std::unique_ptr<HomiePropertyBase>> m_properties_map;
};

class HomiePropertyBase : public homie::property {
 public:
  static constexpr auto TAG = "homie:property";

  void set_parent(HomieNodeBase *parent) { m_parent = parent; }

  std::string get_id() const override = 0;
  std::string get_name() const override = 0;

  homie::datatype get_datatype() const override { return homie::datatype::string; };
  std::string get_format() const override { return ""; }
  std::string get_value() const override { return ""; }
  std::string get_unit() const override { return ""; }

  std::string get_value(int64_t node_idx) const override { return ""; }
  void set_value(int64_t node_idx, const std::string &value) override{};
  void set_value(const std::string &value) override {}

  bool is_settable() const override { return false; }
  bool is_retained() const override { return false; }

  std::map<std::string, std::string> get_attributes() const override { return {}; }

  void notify_changed();

 protected:
  HomieNodeBase *m_parent;
};

class HomiePropertyFunctor : public HomiePropertyBase {
 public:
  HomiePropertyFunctor(PropertyDescriptor descriptor);

  std::string get_id() const override;
  std::string get_name() const override;

  homie::datatype get_datatype() const override;
  std::string get_format() const override;
  std::string get_value() const override;
  std::string get_unit() const override;

  std::string get_value(int64_t node_idx) const override;
  void set_value(int64_t node_idx, const std::string &value) override;
  void set_value(const std::string &value) override;

  bool is_settable() const override;
  bool is_retained() const override;

  std::map<std::string, std::string> get_attributes() const override;

 private:
  const PropertyDescriptor m_descriptor;
};

}  // namespace mqtt_homie
}  // namespace esphome
