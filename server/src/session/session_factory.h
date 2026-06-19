#pragma once

#include "input_session.h"
#include <memory>
#include <string>

std::shared_ptr<IInputSession> CreateInputSessionFromConfig();
std::string DescribeConfiguredInputSessionBackendFromConfig();
std::string DescribeEffectiveInputSessionBackendFromConfig();
