#include "homie_client.h"
#include "homie_device.h"
#include "homie_node.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"

namespace esphome {
namespace mqtt_homie {

std::string HomieNodeBase::get_id() const { return GetEntityBase()->get_object_id(); }

std::string HomieNodeBase::get_name() const { return GetEntityBase()->get_name(); }

std::string HomieNodeBase::get_name(int64_t node_idx) const { return ""; }

std::string HomieNodeBase::get_type() const { return ""; }

bool HomieNodeBase::is_array() const { return false; }

std::pair<int64_t, int64_t> HomieNodeBase::array_range() const { return {0, 0}; }

std::map<std::string, std::string> HomieNodeBase::get_attributes() const {
  auto base = GetEntityBase();
  auto base_class = const_cast<EntityBase_DeviceClass *>(GetEntityBaseDeviceClass());
  return {
      {"icon", base->get_icon()},
      {"class", base_class ? base_class->get_device_class() : std::string()},
  };
}

void HomieNodeBase::notify_property_changed(HomiePropertyBase *property) {
  if (device) {
    this->defer("send", [this, property]() {  //
      device->notify_node_changed(this, property);
    });
  }
}

void HomieNodeBase::notify_property_changed(const std::string &name) {
  notify_property_changed(static_cast<HomiePropertyBase *>(get_property(name)));
}
void HomieNodeBase::notify_all_properties_changed() { notify_property_changed(nullptr); }

void HomieNodeMultiProperty::attach_property(std::unique_ptr<HomiePropertyBase> property) {
  property->set_parent(this);
  m_properties_map[property->get_id()] = std::move(property);
}

void HomieNodeMultiProperty::create_properties(std::initializer_list<PropertyDescriptor> descriptors) {
  for (auto &item : descriptors) {
    attach_property(std::make_unique<HomiePropertyFunctor>(std::move(item)));
  }
}

std::set<std::string> HomieNodeMultiProperty::get_properties() const {
  std::set<std::string> r;
  std::transform(m_properties_map.begin(), m_properties_map.end(), std::inserter(r, r.end()),
                 [](const auto &item) { return item.first; });
  return r;
}

homie::const_property_ptr HomieNodeMultiProperty::get_property(const std::string &id) const {
  auto it = m_properties_map.find(id);
  if (it != m_properties_map.end())
    return it->second.get();
  return nullptr;
}

homie::property_ptr HomieNodeMultiProperty::get_property(const std::string &id) {
  auto it = m_properties_map.find(id);
  if (it != m_properties_map.end())
    return it->second.get();
  return nullptr;
}

void HomiePropertyBase::notify_changed() {
  if (m_parent) {
    m_parent->notify_property_changed(this);
  } else {
    ESP_LOGW(TAG, "Cannot notify about change: no parent set");
  }
}

HomiePropertyFunctor::HomiePropertyFunctor(PropertyDescriptor descriptor) : m_descriptor(std::move(descriptor)) {}

std::string HomiePropertyFunctor::get_id() const { return m_descriptor.id; }
std::string HomiePropertyFunctor::get_name() const { return m_descriptor.name; }
homie::datatype HomiePropertyFunctor::get_datatype() const { return m_descriptor.datatype; }
std::string HomiePropertyFunctor::get_format() const { return m_descriptor.format; }
std::string HomiePropertyFunctor::get_unit() const { return m_descriptor.unit; }
bool HomiePropertyFunctor::is_settable() const { return static_cast<bool>(m_descriptor.setter); }
bool HomiePropertyFunctor::is_retained() const { return m_descriptor.retained; }

std::string HomiePropertyFunctor::get_value() const {
  if (m_descriptor.getter)
    return m_descriptor.getter();
  return "";
}
std::string HomiePropertyFunctor::get_value(int64_t node_idx) const { return get_value(); }
void HomiePropertyFunctor::set_value(int64_t node_idx, const std::string &value) { set_value(value); }
void HomiePropertyFunctor::set_value(const std::string &value) {
  if (m_descriptor.setter) {
    m_descriptor.setter(value);
    notify_changed();
  }
}

std::map<std::string, std::string> HomiePropertyFunctor::get_attributes() const { return {}; }

}  // namespace mqtt_homie
}  // namespace esphome
