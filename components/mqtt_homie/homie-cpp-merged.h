/*

ORIGINAL SOURCE FROM: https://github.com/Thalhammer/homie-cpp
ALL CREDITS TO ORIGINAL CREATOR
ORIGINAL LICENSE: MIT
BASED ON COMMIT: cd9276b2656701cf48c41de1bf4b4cfd5dcf0b0e

Modified for arduino needs

*/
#pragma once

#include "esphome/core/log.h"

#include <chrono>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace homie {
	struct mqtt_event_handler {
		// Called after connection is initiated
		virtual void on_connect() = 0;
		// Clean disconnect started
		virtual void on_closing() = 0;
		// Clean disconnect
		virtual void on_closed() = 0;
		// Unexpected connection loss
		virtual void on_offline() = 0;
		virtual void on_message(const std::string& topic, const std::string& payload) = 0;
	};

	struct mqtt_client {
		virtual void set_event_handler(mqtt_event_handler* evt) = 0;
		virtual void open(const std::string& will_topic, const std::string& will_payload, int will_qos, bool will_retain) = 0;
		virtual void publish(const std::string& topic, const std::string& payload, int qos, bool retain) = 0;
		virtual void subscribe(const std::string& topic, int qos) = 0;
		virtual void unsubscribe(const std::string& topic) = 0;
		virtual bool is_connected() const = 0;
	};

	template<typename T>
	inline T enum_from_string(const std::string& s);

	enum class device_state {
		init,
		ready,
		disconnected,
		sleeping,
		alert,
		lost,
	};

	inline std::string enum_to_string(device_state s) {
		switch (s)
		{
		case device_state::init: return "init";
		case device_state::ready: return "ready";
		case device_state::disconnected: return "disconnected";
		case device_state::sleeping: return "sleeping";
		case device_state::lost: return "lost";
		case device_state::alert: return "alert";
		}
		return "lost";
	}

	template<>
	inline device_state enum_from_string<device_state>(const std::string& s) {
		if (s == "init") return device_state::init;
		if (s == "ready") return device_state::ready;
		if (s == "disconnected") return device_state::disconnected;
		if (s == "sleeping") return device_state::sleeping;
		if (s == "lost") return device_state::lost;
		if (s == "alert") return device_state::alert;
		return device_state::lost;
	}

	template<typename T>
	inline T enum_from_string(const std::string& s);

	enum class datatype {
		integer,
		number,
		boolean,
		string,
		enumeration,
		color
	};

	inline std::string enum_to_string(datatype s) {
		switch (s)
		{
		case datatype::integer: return "integer";
		case datatype::number: return "float";
		case datatype::boolean: return "boolean";
		case datatype::string: return "string";
		case datatype::enumeration: return "enum";
		case datatype::color: return "color";
		}
		return "unknown";
	}

	template<>
	inline datatype enum_from_string<datatype>(const std::string& s) {
		if (s == "integer") return datatype::integer;
		if (s == "float") return datatype::number;
		if (s == "boolean") return datatype::boolean;
		if (s == "string") return datatype::string;
		if (s == "enum") return datatype::enumeration;
		if (s == "color") return datatype::color;
		return datatype::string;
	}

	struct node;
	typedef node* node_ptr;
	typedef const node *const_node_ptr;

	struct property {
		virtual std::string get_id() const = 0;
		virtual std::string get_name() const = 0;
		virtual bool is_settable() const = 0;
		virtual std::string get_unit() const = 0;
		virtual datatype get_datatype() const = 0;
		virtual std::string get_format() const = 0;

		virtual std::string get_value(int64_t node_idx) const = 0;
		virtual void set_value(int64_t node_idx, const std::string& value) = 0;
		virtual std::string get_value() const = 0;
		virtual void set_value(const std::string& value) = 0;

		virtual std::map<std::string, std::string> get_attributes() const = 0;
	};

	typedef property *property_ptr;
	typedef const property *const_property_ptr;

	struct device;
	typedef device *device_ptr;
	typedef const device *const_device_ptr;

	struct node {
		virtual std::string get_id() const = 0;
		virtual std::string get_name() const = 0;
		virtual std::string get_name(int64_t node_idx) const = 0;
		virtual std::string get_type() const = 0;
		virtual bool is_array() const = 0;
		virtual std::pair<int64_t, int64_t> array_range() const = 0;
		virtual std::set<std::string> get_properties() const = 0;
		virtual const_property_ptr get_property(const std::string& id) const = 0;
		virtual property_ptr get_property(const std::string& id) = 0;

		virtual std::map<std::string, std::string> get_attributes() const = 0;
		virtual std::map<std::string, std::string> get_attributes(int64_t node_idx) const { return get_attributes(); }
	};


	typedef node *node_ptr;
	typedef const node *const_node_ptr;

	namespace utils {
		// split string
		template<typename StringType = std::string>
		inline std::vector<StringType> split(const StringType& s, const StringType& delim, size_t offset = 0, size_t max = std::numeric_limits<size_t>::max()) {
			std::vector<StringType> res;
			do {
				auto pos = s.find(delim, offset);
				if (res.size() < max - 1 && pos != std::string::npos)
					res.push_back(s.substr(offset, pos - offset));
				else {
					res.push_back(s.substr(offset));
					break;
				}
				offset = pos + delim.size();
			} while (true);
			return res;
		}
	}

	struct device {
		virtual const std::string& get_id() const = 0;
		virtual const std::string& get_name() const = 0;

		virtual std::set<std::string> get_nodes() const = 0;
		virtual node_ptr get_node(const std::string& id) = 0;
		virtual const_node_ptr get_node(const std::string& id) const = 0;

		virtual std::map<std::string, std::string> get_attributes() const = 0;
		virtual device_state get_state() const = 0;
	};

	typedef device *device_ptr;
	typedef const device *const_device_ptr;

	struct client_event_handler {
		virtual void on_broadcast(const std::string& level, const std::string& payload) = 0;
	};

	class client : private mqtt_event_handler {
	  static constexpr auto TAG = "homie:client";

		mqtt_client& mqtt;
		std::string base_topic;
		device_ptr dev;
		client_event_handler* handler;
		bool device_info_published = false;

		// Inherited by mqtt_event_handler
		virtual void on_connect() override {
			if(!device_info_published){
				publish_device_info();
				device_info_published = true;
			}
		}
		virtual void on_closing() override {
			mqtt.publish(base_topic + dev->get_id() + "/$state", enum_to_string(device_state::disconnected), 1, true);
			device_info_published = false;
		}
		virtual void on_closed() override {device_info_published = false;}
		virtual void on_offline() override {device_info_published = false;}
		virtual void on_message(const std::string & topic, const std::string & payload) override {
			// Check basetopic
			if (topic.size() < base_topic.size())
				return;
			if (topic.compare(0, base_topic.size(), base_topic) != 0)
				return;

			auto parts = utils::split<std::string>(topic, "/", base_topic.size());
			if (parts.size() < 2)
				return;
			for (auto& e : parts) if (e.empty()) return;
			if (parts[0][0] == '$') {
				if (parts[0] == "$broadcast") {
					this->handle_broadcast(parts[1], payload);
				}
			}
			else if(parts[0] == dev->get_id()) {
				if (parts.size() != 4
					|| parts[3] != "set"
					|| parts[2][0] == '$')
					return;
				this->handle_property_set(parts[1], parts[2], payload);
			}
		}

		void handle_property_set(const std::string& snode, const std::string& sproperty, const std::string& payload) {
			if (snode.empty() || sproperty.empty())
				return;

			int64_t id = 0;
			bool is_array_node = false;
			auto node = dev->get_node(snode);

			if(!node) {
				std::string rnode = snode;
				auto pos = rnode.find('_');
				if (pos != std::string::npos) {
					std::size_t digits = 0;
					id = std::stoll(rnode.substr(pos + 1), &digits);
					if(digits > 0) {
						is_array_node = true;
						rnode.resize(pos);
					}
				}

				auto node = dev->get_node(rnode);
				if (node == nullptr || node->is_array() != is_array_node) return;
			}

			auto prop = node->get_property(sproperty);
			if (prop == nullptr) return;
			if (!prop->is_settable()) return;

			if (is_array_node)
				prop->set_value(id, payload);
			else prop->set_value(payload);
		}

		void handle_broadcast(const std::string& level, const std::string& payload) {
			if(handler)
				handler->on_broadcast(level, payload);
		}

		void publish_device_info() {
			using namespace esphome;
    		ESP_LOGI(TAG, "Publishing device info");

			// Signal initialisation phase
			this->publish_device_attribute("$state", enum_to_string(device_state::init));

			// Public device properties
			this->publish_device_attribute("$homie", "3.0.0");
			this->publish_device_attribute("$name", dev->get_name());

			for(const auto &[key, value]: dev->get_attributes()) {
				this->publish_device_attribute("$" + key, value);
			}

			// Publish nodes
			std::string nodes = "";
			for (auto& nodename : dev->get_nodes()) {
				auto node = dev->get_node(nodename);

				if (node->is_array()) {
					nodes += node->get_id() + "[],";
					this->publish_node_attribute(node, "$array", std::to_string(node->array_range().first) + "-" + std::to_string(node->array_range().second));
					for (int64_t i = node->array_range().first; i <= node->array_range().second; i++) {
						auto n = node->get_name(i);
						if(n != "")
						this->publish_device_attribute(node->get_id() + "_" + std::to_string(i) + "/$name", n);
					}
				}
				else {
					nodes += node->get_id() + ",";
				}
				this->publish_node_attribute(node, "$name", node->get_name());
				this->publish_node_attribute(node, "$type", node->get_type());

				for(const auto &[key, value]: node->get_attributes()) {
					this->publish_node_attribute(node, "$" + key, value);
				}

				// Publish node properties
				std::string properties = "";
				for (auto& propertyname : node->get_properties()) {
					auto property = node->get_property(propertyname);
					properties += property->get_id() + ",";
					this->publish_property_attribute(node, property, "$name", property->get_name());
					this->publish_property_attribute(node, property, "$settable", property->is_settable() ? "true" : "false");
					this->publish_property_attribute(node, property, "$unit", property->get_unit());
					this->publish_property_attribute(node, property, "$datatype", enum_to_string(property->get_datatype()));

					for(const auto &[key, value]: property->get_attributes()) {
						this->publish_property_attribute(node, property, "$" + key, value);
					}

					this->publish_device_attribute(node->get_id() + "/" + property->get_id() + "/$format", property->get_format());
					if (!node->is_array()) {
						auto val = property->get_value();
						if (!val.empty())
							this->publish_node_attribute(node, property->get_id(), val);
					}
					else {
						for (int64_t i = node->array_range().first; i <= node->array_range().second; i++) {
							auto val = property->get_value(i);
							if(!val.empty())
								this->publish_device_attribute(node->get_id() + "_" + std::to_string(i) + "/" + property->get_id(), val);
						}
					}
				}
				if (!properties.empty())
					properties.resize(properties.size() - 1);
				this->publish_node_attribute(node, "$properties", properties);
			}
			if (!nodes.empty())
				nodes.resize(nodes.size() - 1);
			this->publish_device_attribute("$nodes", nodes);

			// Everything done, set device to real state
			this->publish_device_attribute("$state", enum_to_string(dev->get_state()));
		}

		void publish_device_attribute(const std::string& attribute, const std::string& value) {
			mqtt.publish(base_topic + dev->get_id() + "/" + attribute, value, 1, true);
		}

		void publish_node_attribute(const_node_ptr node, const std::string& attribute, const std::string& value) {
			publish_device_attribute(node->get_id() + "/" + attribute, value);
		}

		void publish_property_attribute(const_node_ptr node, const_property_ptr prop, const std::string& attribute, const std::string& value) {
			publish_node_attribute(node, prop->get_id() + "/" + attribute, value);
		}

		void notify_property_changed_impl(const std::string& snode, const std::string& sproperty, const int64_t* idx) {
			if (snode.empty() || sproperty.empty())
				return;

			auto node = dev->get_node(snode);
			if (!node) return;
			auto prop = node->get_property(sproperty);
			if (!prop) return;
			notify_property_changed_impl(node, prop, idx);
		}

		void notify_property_changed_impl(node *node, property *prop, const int64_t* idx) {
			if(!node || !prop)
				return;

			if (node->is_array()) {
				if (idx != nullptr) {
					this->publish_device_attribute(node->get_id() + "_" + std::to_string(*idx) + "/" + prop->get_id(), prop->get_value(*idx));
				}
				else {
					auto range = node->array_range();
					for (auto i = range.first; i <= range.second; i++) {
						this->publish_device_attribute(node->get_id() + "_" + std::to_string(i) + "/" + prop->get_id(), prop->get_value(i));
					}
				}
			}
			else {
				this->publish_device_attribute(node->get_id() + "/" + prop->get_id(), prop->get_value());
			}
		}
	public:
		client(mqtt_client& con, device_ptr pdev, std::string basetopic = "homie/")
			: mqtt(con), base_topic(basetopic), dev(pdev), handler(nullptr)
		{
			mqtt.set_event_handler(this);

			mqtt.open(base_topic + dev->get_id() + "/$state", enum_to_string(device_state::lost), 1, true);
			mqtt.subscribe(base_topic + dev->get_id() + "/+/+/set", 1);
		}

		~client() {
			this->publish_device_attribute("$state", enum_to_string(device_state::disconnected));
			this->mqtt.unsubscribe(base_topic + dev->get_id() + "/+/+/set");
			mqtt.set_event_handler(nullptr);
		}

		void notify_property_changed(const std::string& snode, const std::string& sproperty) {
			notify_property_changed_impl(snode, sproperty, nullptr);
		}

		void notify_property_changed(node *node, property *prop) {
			notify_property_changed_impl(node, prop, nullptr);
		}

		void notify_property_changed(const std::string& snode, const std::string& sproperty, int64_t idx) {
			notify_property_changed_impl(snode, sproperty, &idx);
		}

		void notify_device_state_changed() {
			this->publish_device_attribute("$state", enum_to_string(dev->get_state()));
		}

		void set_event_handler(client_event_handler* hdl) {
			handler = hdl;
		}

		bool is_connected() const { return mqtt.is_connected() && device_info_published; }
	};
}
