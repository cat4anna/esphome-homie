#include "homie_client.h"
#include "homie_node.h"
#include "homie_device.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"

#define TAG "homie:client"

namespace esphome {
namespace mqtt_homie {

class MqttProxy : public homie::mqtt_client {
 public:
  MqttProxy(esphome::mqtt_client::MQTTClientComponent *client) : client(client) { client->set_handler(&proxy); }

  void set_event_handler(homie::mqtt_event_handler *evt) override { proxy.handler = evt; }

  void open(const std::string &will_topic, const std::string &will_payload, int will_qos, bool will_retain) override {
    client->set_last_will(esphome::mqtt_client::MQTTMessage{
        .topic = will_topic,
        .payload = will_payload,
        .qos = static_cast<uint8_t>(will_qos),
        .retain = will_retain,
    });
  }
  void publish(const std::string &topic, const std::string &payload, int qos, bool retain) override {
    client->publish(esphome::mqtt_client::MQTTMessage{
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
  esphome::mqtt_client::MQTTClientComponent *client = nullptr;

  class MqttToHomieProxy : public esphome::mqtt_client::MqttStateHandler {
   public:
    homie::mqtt_event_handler *handler = nullptr;
    void on_connect() {
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

HomieClient::HomieClient(mqtt_client::MQTTClientComponent *client) {
  mqtt_proxy = std::make_unique<MqttProxy>(client);
}

void HomieClient::start_homie(HomieDevice* device, std::string prefix, int qos, bool retained) {
  if (homie_client)
    return;

  if(!prefix.empty() && prefix.back() != '/')
    prefix += "/";

  homie_client = std::make_unique<homie::client>(*mqtt_proxy, device, prefix, qos, retained);
  device->set_client(homie_client.get());
}

}  // namespace mqtt_homie
}  // namespace esphome
