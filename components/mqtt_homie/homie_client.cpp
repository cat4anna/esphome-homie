#include "homie_client.h"
#include "homie_node.h"
#include "homie_device.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#define TAG "homie:client"

namespace esphome::mqtt_homie {

class MqttProxy : public homie::mqtt_client {
 public:
  MqttProxy(esphome::mqtt::MQTTClientComponent *client) : client(client) {
    client->set_handler(&proxy);
  }

  void set_event_handler(homie::mqtt_event_handler *evt) override { proxy.handler = evt; }

  void open(const std::string &will_topic, const std::string &will_payload, int will_qos,
            bool will_retain) override {}
  void publish(const std::string &topic, const std::string &payload, int qos,
               bool retain) override {
    client->publish(esphome::mqtt::MQTTMessage{
        .topic = topic,
        .payload = payload,
        .qos = static_cast<uint8_t>(qos),
        .retain = retain,
    });
  }
  void subscribe(const std::string &topic, int qos) override {
    client->subscribe(
        topic,
        [this](const std::string &topic, const std::string &payload) {
          if (proxy.handler)
            proxy.handler->on_message(topic, payload);
        },
        qos);
  }
  void unsubscribe(const std::string &topic) override { client->unsubscribe(topic); }
  bool is_connected() const override { return client->is_connected(); }

 private:
  esphome::mqtt::MQTTClientComponent *client = nullptr;

  class MqttToHomieProxy : public esphome::mqtt::MqttStateHandler {
   public:
    homie::mqtt_event_handler *handler = nullptr;
    void on_connected() {
      if (handler)
        handler->on_connect();
    }
    void on_closing() {
      if (handler)
        handler->on_closing();
    }
    void on_closed() {
      if (handler)
        handler->on_closed();
    }
    void on_offline() {
      if (handler)
        handler->on_offline();
    }
  };
  MqttToHomieProxy proxy;
};

HomieClient::HomieClient(mqtt::MQTTClientComponent *client) {
  mqtt_proxy = std::make_unique<MqttProxy>(client);
}

void HomieClient::start_homie(HomieDevice *device, std::string prefix, int qos, bool retained) {
  if (homie_client)
    return;

  m_device = device;

  if (!prefix.empty() && prefix.back() != '/')
    prefix += "/";

  homie_client = std::make_unique<homie::client>(*mqtt_proxy, device, prefix, qos, retained);
  device->set_client(homie_client.get());
}

void HomieClient::setup() {
#ifdef USE_LOGGER
  logger::global_logger->add_on_log_callback(
      [this](int level, const char *tag, const char *message) {
        if (level <= this->m_log_level) {
          m_device->push_log_message(level, tag, message);
        }
      });
#endif
}

}  // namespace esphome::mqtt_homie
