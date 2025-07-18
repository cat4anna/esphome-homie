#pragma once
#include "mqtt_client.h"
#include "device.h"
#include "utils.h"
#include "client_event_handler.h"
#include <set>

namespace homie {

inline const char *bool2str(bool v) { return v ? "true" : "false"; }

class client : private mqtt_event_handler {
  static constexpr auto TAG = "homie:client";

  mqtt_client &mqtt;
  std::string base_topic;
  device_ptr dev;
  client_event_handler *handler;
  int qos;
  bool retained;

  // Inherited by mqtt_event_handler
  virtual void on_connect() override {}
  virtual void on_closing() override {
    publish_device_attribute("$state", enum_to_string(device_state::disconnected));
  }
  virtual void on_closed() override {}
  virtual void on_offline() override {}
  virtual void on_message(const std::string &topic, const std::string &payload) override {
    // Check base topic
    if (topic.size() < base_topic.size())
      return;
    if (topic.compare(0, base_topic.size(), base_topic) != 0)
      return;

    auto parts = utils::split<std::string>(topic, "/", base_topic.size());
    if (parts.size() < 2)
      return;
    for (auto &e : parts)
      if (e.empty())
        return;
    if (parts[0][0] == '$') {
      if (parts[0] == "$broadcast") {
        handle_broadcast(parts[1], payload);
      }
    } else if (parts[0] == dev->get_id()) {
      if (parts.size() != 4 || parts[3] != "set" || parts[2][0] == '$')
        return;
      handle_property_set(parts[1], parts[2], payload);
    }
  }

  void handle_property_set(const std::string &snode, const std::string &sproperty,
                           const std::string &payload) {
    if (snode.empty() || sproperty.empty())
      return;

    int64_t id = 0;
    bool is_array_node = false;
    auto node = dev->get_node(snode);

    if (!node) {
      // std::string rnode = snode;
      // auto pos = rnode.find('_');
      // if (pos != std::string::npos) {
      //   std::size_t digits = 0;
      //   id = std::stoll(rnode.substr(pos + 1), &digits);
      //   if (digits > 0) {
      //     is_array_node = true;
      //     rnode.resize(pos);
      //   }
      // }

      // auto node = dev->get_node(rnode);
      // if (node == nullptr || node->is_array() != is_array_node)
        return;
    }

    auto prop = node->get_property(sproperty);
    if (prop == nullptr)
      return;
    if (!prop->is_settable())
      return;

    if (is_array_node)
      if (prop->get_value(id) != payload) {
        prop->set_value(id, payload);
      } else {
        if (prop->get_value() != payload) {
          prop->set_value(payload);
        }
      }
  }

  void handle_broadcast(const std::string &level, const std::string &payload) {
    if (handler)
      handler->on_broadcast(level, payload);
  }

  void publish_device_attribute(const std::string &attribute, const std::string &value,
                                bool wants_retained = true) const {
    std::string topic = base_topic + dev->get_id() + "/";
    if (attribute.front() != '$')
      topic += '$';
    topic += attribute;
    mqtt.publish(topic, value, qos, retained && wants_retained);
  }

  void publish_node_attribute(const_node_ptr node, const std::string &attribute,
                              const std::string &value, bool wants_retained = true) const {
    std::string topic = base_topic + dev->get_id() + "/" + node->get_id() + "/";
    if (attribute.front() != '$')
      topic += '$';
    topic += attribute;
    mqtt.publish(topic, value, qos, retained && wants_retained);
  }

  void publish_property_attribute(const_node_ptr node, const_property_ptr prop,
                                  const std::string &attribute, const std::string &value,
                                  bool wants_retained = true) const {
    std::string topic =
        base_topic + dev->get_id() + "/" + node->get_id() + "/" + prop->get_id() + "/";
    if (attribute.front() != '$')
      topic += '$';
    topic += attribute;
    mqtt.publish(topic, value, qos, retained && wants_retained);
  }

  void publish_property_value(const_node_ptr node, const_property_ptr prop,
                              const std::string &value, bool wants_retained = true) const {
    std::string topic = base_topic + dev->get_id() + "/" + node->get_id() + "/" + prop->get_id();
    mqtt.publish(topic, value, qos, retained && wants_retained);
  }

  void notify_property_changed_impl(const std::string &snode, const std::string &sproperty,
                                    const int64_t *idx) const {
    if (snode.empty() || sproperty.empty())
      return;

    auto node = dev->get_node(snode);
    if (!node)
      return;
    auto prop = node->get_property(sproperty);
    if (!prop)
      return;
    notify_property_changed_impl(node, prop, idx);
  }

  void notify_property_changed_impl(node *node, property *prop, const int64_t *idx) const {
    if (!node || !prop)
      return;

    if (node->is_array()) {
      // if (idx != nullptr) {
      //   publish_device_attribute(
      //       node->get_id() + "_" + std::to_string(*idx) + "/" + prop->get_id(),
      //       prop->get_value(*idx));
      // } else {
      //   auto range = node->array_range();
      //   for (auto i = range.first; i <= range.second; i++) {
      //     publish_device_attribute(
      //         node->get_id() + "_" + std::to_string(i) + "/" + prop->get_id(),
      //         prop->get_value(i));
      //   }
      // }
    } else {
      publish_property_value(node, prop, prop->get_value(), prop->is_retained());
    }
  }

 public:
  client(mqtt_client &con, device_ptr pdev, std::string base_topic = "homie/", int qos = 1,
         bool retained = true)
      : mqtt(con),
        base_topic(base_topic),
        dev(pdev),
        handler(nullptr),
        qos(qos),
        retained(retained) {
    mqtt.set_event_handler(this);
    mqtt.open(base_topic + dev->get_id() + "/$state", enum_to_string(device_state::lost), qos,
              retained);
  }

  ~client() { mqtt.set_event_handler(nullptr); }

  void stop_subscription() { mqtt.unsubscribe(base_topic + dev->get_id() + "/+/+/set"); }
  void start_subscription() { mqtt.subscribe(base_topic + dev->get_id() + "/+/+/set", 1); }

  void notify_property_changed(const std::string &snode, const std::string &sproperty) const {
    notify_property_changed_impl(snode, sproperty, nullptr);
  }

  void notify_property_changed(node *node, property *prop) const {
    notify_property_changed_impl(node, prop, nullptr);
  }

  void notify_property_changed(const std::string &snode, const std::string &sproperty,
                               int64_t idx) const {
    notify_property_changed_impl(snode, sproperty, &idx);
  }

  void notify_device_state_changed() const {
    publish_device_attribute("$state", enum_to_string(dev->get_state()));
  }

  void update_device_stats() const {
    for (const auto &[key, value] : dev->get_stats()) {
      publish_device_attribute("$stats/" + key, value, false);
    }
  }

  void publish_log_message(const std::string &message) const {
    publish_device_attribute("$log", message, false);
  }

  void publish_device_info() const {
    publish_device_attribute("$homie", "3.0.1");
    publish_device_attribute("$name", dev->get_name());

    for (const auto &[key, value] : dev->get_attributes()) {
      publish_device_attribute(key, value);
    }

    // Publish nodes
    std::string nodes = "";
    for (auto &nodename : dev->get_nodes()) {
      auto node = dev->get_node(nodename);

      if (node->is_array()) {
        // nodes += node->get_id() + "[],";
        // publish_node_attribute(node, "$array",
        //                              std::to_string(node->array_range().first) + "-" +
        //                                  std::to_string(node->array_range().second));
        // for (int64_t i = node->array_range().first; i <= node->array_range().second; i++) {
        //   auto n = node->get_name(i);
        //   if (n != "")
        //     publish_device_attribute(node->get_id() + "_" + std::to_string(i) + "/$name",
        //     n);
        // }
      } else {
        if (!nodes.empty()) {
          nodes += ',';
        }
        nodes += node->get_id();
      }
      publish_node_attribute(node, "$name", node->get_name());
      publish_node_attribute(node, "$type", node->get_type());

      for (const auto &[key, value] : node->get_attributes()) {
        publish_node_attribute(node, key, value);
      }

      // Publish node properties
      std::string properties = "";
      for (auto &property_name : node->get_properties()) {
        auto property = node->get_property(property_name);
        if (!properties.empty()) {
          properties += ',';
        }
        properties += property->get_id();

        publish_property_attribute(node, property, "$name", property->get_name());
        publish_property_attribute(node, property, "$settable", bool2str(property->is_settable()));
        publish_property_attribute(node, property, "$retained",
                                   bool2str(retained && property->is_retained()));
        publish_property_attribute(node, property, "$unit", property->get_unit());
        publish_property_attribute(node, property, "$datatype",
                                   enum_to_string(property->get_datatype()));

        for (const auto &[key, value] : property->get_attributes()) {
          publish_property_attribute(node, property, key, value);
        }

        publish_property_attribute(node, property, "$format", property->get_format());

        if (!node->is_array()) {
          auto val = property->get_value();
          publish_property_value(node, property, val, property->is_retained());
        } else {
          // for (int64_t i = node->array_range().first; i <= node->array_range().second; i++) {
          //   auto val = property->get_value(i);
          //   if (!val.empty())
          //     publish_device_attribute(
          //         node->get_id() + "_" + std::to_string(i) + "/" + property->get_id(), val,
          //         property->is_retained());
          // }
        }
      }

      publish_node_attribute(node, "$properties", properties);
    }
    publish_device_attribute("$nodes", nodes);
  }

  void set_event_handler(client_event_handler *hdl) { handler = hdl; }

  bool is_connected() const { return mqtt.is_connected(); }
};

}  // namespace homie