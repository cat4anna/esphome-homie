#pragma once
#include "mqtt_event_handler.h"

namespace homie {
struct mqtt_client {
  virtual void set_event_handler(mqtt_event_handler *evt) = 0;
  virtual void open(const std::string &will_topic, const std::string &will_payload, int will_qos, bool will_retain) = 0;
  virtual void publish(std::string topic, std::string payload, int qos, bool retain) = 0;
  virtual void subscribe(const std::string &topic, int qos) = 0;
  virtual void unsubscribe(const std::string &topic) = 0;
  virtual bool is_connected() const = 0;
};
}  // namespace homie
