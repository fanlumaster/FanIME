#include "session_factory.h"

#include "config/ime_config.h"
#include "engine_input_session.h"
#include "shuangpin_input_session.h"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace
{
std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string ResolveEffectiveBackend(std::string configured_backend, SchemeType scheme)
{
    if (configured_backend.empty() || configured_backend == "legacy")
    {
        switch (scheme)
        {
        case SchemeType::Shuangpin:
            return "legacy-shuangpin";
        case SchemeType::Quanpin:
            return "engine-quanpin";
        case SchemeType::Wubi:
            return "engine-wubi";
        default:
            throw std::runtime_error("Unknown input scheme.");
        }
    }

    if (configured_backend == "engine")
    {
        switch (scheme)
        {
        case SchemeType::Shuangpin:
            return "engine-shuangpin";
        case SchemeType::Quanpin:
            return "engine-quanpin";
        case SchemeType::Wubi:
            return "engine-wubi";
        default:
            throw std::runtime_error("Unknown input scheme.");
        }
    }

    throw std::runtime_error("Unsupported input session backend: " + configured_backend);
}
} // namespace

std::shared_ptr<IInputSession> CreateInputSessionFromConfig()
{
    const std::string backend = DescribeEffectiveInputSessionBackendFromConfig();
    if (backend == "engine-shuangpin")
    {
        return std::make_shared<EngineInputSession>(SchemeType::Shuangpin);
    }
    if (backend == "engine-quanpin")
    {
        return std::make_shared<EngineInputSession>(SchemeType::Quanpin);
    }
    if (backend == "engine-wubi")
    {
        return std::make_shared<EngineInputSession>(SchemeType::Wubi);
    }

    if (backend == "legacy-shuangpin")
    {
        return std::make_shared<ShuangpinInputSession>();
    }

    throw std::runtime_error("Unsupported effective input session backend: " + backend);
}

std::string DescribeConfiguredInputSessionBackendFromConfig()
{
    return ToLowerAscii(GetConfiguredSessionBackend());
}

std::string DescribeEffectiveInputSessionBackendFromConfig()
{
    return ResolveEffectiveBackend(DescribeConfiguredInputSessionBackendFromConfig(), GetConfiguredInputScheme());
}
