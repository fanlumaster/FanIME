#pragma once

#include "input_session.h"
#include <memory>
#include <string>

std::shared_ptr<IInputSession> CreateInputSessionFromConfig();
std::shared_ptr<IInputSession> CreateTemporaryJapaneseInputSession();
std::string DescribeConfiguredInputSessionBackendFromConfig();
std::string DescribeEffectiveInputSessionBackendFromConfig();
