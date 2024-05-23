#pragma once

#include <roofer/common/common.hpp>

namespace roofer {

class rooferException: public std::exception
{
public:
  explicit rooferException(const std::string& message):
    msg_("Error: " + message)
    {}
  virtual const char* what() const throw (){
    return msg_.c_str();
  }

protected:
    std::string msg_;
};

}