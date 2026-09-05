#pragma once

#include <boost/json.hpp>
#include <string>

namespace SettingsDictionary
{
boost::json::object HandleRequest(const boost::json::object &request);
}
