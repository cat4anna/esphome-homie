#pragma once
#include <string>
#include <memory>
#include <set>
#include <map>
#include "property.h"

namespace homie {

struct node {
  static constexpr auto TAG = "homie:node";
  virtual std::string get_id() const = 0;
  virtual std::string get_name() const = 0;
  virtual std::string get_name(int64_t node_idx) const = 0;
  virtual std::string get_type() const = 0;
  virtual bool is_array() const = 0;
  virtual std::pair<int64_t, int64_t> array_range() const = 0;
  virtual std::set<std::string> get_properties() const = 0;
  virtual const_property_ptr get_property(const std::string &id) const = 0;
  virtual property_ptr get_property(const std::string &id) = 0;

  virtual std::map<std::string, std::string> get_attributes() const = 0;
  virtual std::map<std::string, std::string> get_attributes(int64_t node_idx) const { return get_attributes(); }
};

typedef node *node_ptr;
typedef const node *const_node_ptr;
}  // namespace homie
