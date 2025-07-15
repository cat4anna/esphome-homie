#pragma once
#include <string>
#include <memory>
#include "datatype.h"

namespace homie {

struct property {
  static constexpr auto TAG = "homie:property";
  virtual std::string get_id() const = 0;
  virtual std::string get_name() const = 0;
  virtual bool is_settable() const = 0;
  virtual bool is_retained() const = 0;
  virtual std::string get_unit() const = 0;
  virtual datatype get_datatype() const = 0;
  virtual std::string get_format() const = 0;

  virtual std::string get_value(int64_t node_idx) const = 0;
  virtual void set_value(int64_t node_idx, const std::string &value) = 0;
  virtual std::string get_value() const = 0;
  virtual void set_value(const std::string &value) = 0;

  virtual std::map<std::string, std::string> get_attributes() const = 0;
};

typedef property *property_ptr;
typedef const property *const_property_ptr;

}  // namespace homie