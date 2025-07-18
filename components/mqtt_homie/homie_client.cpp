#include "homie_client.h"
#include "homie_node.h"
#include "homie_device.h"
#include "esphome/core/application.h"
#include "esphome/components/network/util.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#include <deque>

#define TAG "homie:client"

namespace esphome::mqtt_homie {

class MqttProxy : public homie::mqtt_client {
 public:
  MqttProxy(esphome::mqtt::MQTTClientComponent *client) : m_client(client) {
    m_client->set_handler(&m_proxy);
  }

  void set_event_handler(homie::mqtt_event_handler *evt) override { m_proxy.handler = evt; }

  void open(const std::string &will_topic, const std::string &will_payload, int will_qos,
            bool will_retain) override {}
  void publish(std::string topic, std::string payload, int qos, bool retain) override {
    m_outbound_queue.emplace_back(esphome::mqtt::MQTTMessage{
        .topic = std::move(topic),
        .payload = std::move(payload),
        .qos = static_cast<uint8_t>(qos),
        .retain = retain,
    });
  }
  void subscribe(const std::string &topic, int qos) override {
    m_client->subscribe(
        topic,
        [this](const std::string &topic, const std::string &payload) {
          if (m_proxy.handler)
            m_proxy.handler->on_message(topic, payload);
        },
        qos);
  }
  void unsubscribe(const std::string &topic) override { m_client->unsubscribe(topic); }
  bool is_connected() const override { return m_client->is_connected(); }

  void check_outbound_queue() {
    if (m_outbound_queue.empty()) {
      return;
    }

    m_client->publish(m_outbound_queue.front());
    m_outbound_queue.pop_front();
    if (m_outbound_queue.empty()) {
      m_outbound_queue.shrink_to_fit();
    }
  }

 private:
  esphome::mqtt::MQTTClientComponent *m_client = nullptr;

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
  MqttToHomieProxy m_proxy;

  std::deque<esphome::mqtt::MQTTMessage> m_outbound_queue;
};

HomieClient::HomieClient(mqtt::MQTTClientComponent *client) {
  m_mqtt_proxy = std::make_unique<MqttProxy>(client);
}

void HomieClient::start_homie(HomieDevice *device, std::string prefix, int qos, bool retained) {
  if (m_homie_client)
    return;

  m_device = device;

  if (!prefix.empty() && prefix.back() != '/')
    prefix += "/";

  m_homie_client = std::make_unique<homie::client>(*m_mqtt_proxy, device, prefix, qos, retained);
  device->set_client(m_homie_client.get());
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

void HomieClient::loop() { m_mqtt_proxy->check_outbound_queue(); }

}  // namespace esphome::mqtt_homie
