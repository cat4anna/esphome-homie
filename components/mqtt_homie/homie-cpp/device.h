#pragma once
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <chrono>
#include "device_state.h"
#include "node.h"
#include "utils.h"

namespace homie {
struct device {
  static constexpr auto TAG = "homie:device";
  virtual const std::string &get_id() const = 0;
  virtual const std::string &get_name() const = 0;

  virtual std::set<std::string> get_nodes() const = 0;
  virtual node_ptr get_node(const std::string &id) = 0;
  virtual const_node_ptr get_node(const std::string &id) const = 0;

  virtual std::map<std::string, std::string> get_attributes() const = 0;
  virtual std::map<std::string, std::string> get_stats() const = 0;
  virtual device_state get_state() const = 0;
};

typedef device *device_ptr;
typedef const device *const_device_ptr;
}  // namespace homie
