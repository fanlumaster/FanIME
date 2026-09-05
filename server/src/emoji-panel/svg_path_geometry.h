#pragma once

#include <d2d1.h>
#include <string>
#include <wrl/client.h>

namespace msimeui
{
Microsoft::WRL::ComPtr<ID2D1PathGeometry> CreatePathGeometryFromSvgPath(ID2D1Factory *factory,
                                                                        const std::string &pathData);
} // namespace msimeui
