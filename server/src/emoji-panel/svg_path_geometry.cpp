#include "svg_path_geometry.h"

#include <cctype>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace msimeui
{
namespace
{
class SvgPathParser
{
  public:
    SvgPathParser(std::string path, ID2D1GeometrySink *sink) : path_(std::move(path)), sink_(sink)
    {
    }

    bool Parse()
    {
        index_ = 0;
        command_ = '\0';
        while (SkipSeparators(), index_ < path_.size())
        {
            if (std::isalpha(static_cast<unsigned char>(path_[index_])))
            {
                command_ = path_[index_++];
            }
            if (command_ == '\0')
            {
                return false;
            }
            if (!DispatchCommand())
            {
                return false;
            }
        }
        if (figureOpen_)
        {
            sink_->EndFigure(D2D1_FIGURE_END_OPEN);
            figureOpen_ = false;
        }
        return true;
    }

  private:
    bool DispatchCommand()
    {
        switch (command_)
        {
        case 'M':
            return MoveTo(false);
        case 'm':
            return MoveTo(true);
        case 'L':
            return LineTo(false);
        case 'l':
            return LineTo(true);
        case 'H':
            return HorizontalLine(false);
        case 'h':
            return HorizontalLine(true);
        case 'V':
            return VerticalLine(false);
        case 'v':
            return VerticalLine(true);
        case 'Q':
            return QuadraticBezier(false);
        case 'q':
            return QuadraticBezier(true);
        case 'T':
            return SmoothQuadratic(false);
        case 't':
            return SmoothQuadratic(true);
        case 'Z':
        case 'z':
            return ClosePath();
        default:
            return false;
        }
    }

    bool MoveTo(bool relative)
    {
        const auto point = ReadPoint(relative);
        if (!point)
        {
            return false;
        }
        if (!figureOpen_)
        {
            sink_->BeginFigure(*point, D2D1_FIGURE_BEGIN_FILLED);
            figureOpen_ = true;
        }
        else
        {
            sink_->EndFigure(D2D1_FIGURE_END_OPEN);
            sink_->BeginFigure(*point, D2D1_FIGURE_BEGIN_FILLED);
        }
        startX_ = currentX_;
        startY_ = currentY_;
        command_ = relative ? 'l' : 'L';
        return true;
    }

    bool LineTo(bool relative)
    {
        while (true)
        {
            const auto point = ReadPoint(relative);
            if (!point)
            {
                break;
            }
            EnsureFigure();
            sink_->AddLine(*point);
        }
        lastControlValid_ = false;
        return true;
    }

    bool HorizontalLine(bool relative)
    {
        while (true)
        {
            const auto value = ReadNumber();
            if (!value)
            {
                break;
            }
            EnsureFigure();
            currentX_ = relative ? currentX_ + *value : *value;
            sink_->AddLine({currentX_, currentY_});
        }
        lastControlValid_ = false;
        return true;
    }

    bool VerticalLine(bool relative)
    {
        while (true)
        {
            const auto value = ReadNumber();
            if (!value)
            {
                break;
            }
            EnsureFigure();
            currentY_ = relative ? currentY_ + *value : *value;
            sink_->AddLine({currentX_, currentY_});
        }
        lastControlValid_ = false;
        return true;
    }

    bool QuadraticBezier(bool relative)
    {
        while (true)
        {
            const auto control = ReadPoint(relative);
            if (!control)
            {
                break;
            }
            const auto end = ReadPoint(relative);
            if (!end)
            {
                return false;
            }
            EnsureFigure();
            sink_->AddQuadraticBezier({*control, *end});
            lastControlX_ = control->x;
            lastControlY_ = control->y;
            lastControlValid_ = true;
        }
        command_ = relative ? 't' : 'T';
        return true;
    }

    bool SmoothQuadratic(bool relative)
    {
        while (true)
        {
            const auto end = ReadPoint(relative);
            if (!end)
            {
                break;
            }
            EnsureFigure();
            const D2D1_POINT_2F control =
                lastControlValid_ ? D2D1_POINT_2F{2.0f * currentX_ - lastControlX_, 2.0f * currentY_ - lastControlY_}
                                  : D2D1_POINT_2F{currentX_, currentY_};
            sink_->AddQuadraticBezier({control, *end});
            lastControlX_ = control.x;
            lastControlY_ = control.y;
            lastControlValid_ = true;
        }
        return true;
    }

    bool ClosePath()
    {
        if (figureOpen_)
        {
            sink_->EndFigure(D2D1_FIGURE_END_CLOSED);
            figureOpen_ = false;
        }
        currentX_ = startX_;
        currentY_ = startY_;
        lastControlValid_ = false;
        return true;
    }

    void EnsureFigure()
    {
        if (!figureOpen_)
        {
            sink_->BeginFigure({currentX_, currentY_}, D2D1_FIGURE_BEGIN_FILLED);
            figureOpen_ = true;
            startX_ = currentX_;
            startY_ = currentY_;
        }
    }

    std::optional<D2D1_POINT_2F> ReadPoint(bool relative)
    {
        const auto x = ReadNumber();
        if (!x)
        {
            return std::nullopt;
        }
        const auto y = ReadNumber();
        if (!y)
        {
            return std::nullopt;
        }
        if (relative)
        {
            currentX_ += *x;
            currentY_ += *y;
        }
        else
        {
            currentX_ = *x;
            currentY_ = *y;
        }
        return D2D1_POINT_2F{currentX_, currentY_};
    }

    std::optional<float> ReadNumber()
    {
        SkipSeparators();
        if (index_ >= path_.size())
        {
            return std::nullopt;
        }

        size_t start = index_;
        if (path_[index_] == '+' || path_[index_] == '-')
        {
            ++index_;
        }
        bool hasDigits = false;
        while (index_ < path_.size() && std::isdigit(static_cast<unsigned char>(path_[index_])))
        {
            hasDigits = true;
            ++index_;
        }
        if (index_ < path_.size() && path_[index_] == '.')
        {
            ++index_;
            while (index_ < path_.size() && std::isdigit(static_cast<unsigned char>(path_[index_])))
            {
                hasDigits = true;
                ++index_;
            }
        }
        if (index_ < path_.size() && (path_[index_] == 'e' || path_[index_] == 'E'))
        {
            ++index_;
            if (index_ < path_.size() && (path_[index_] == '+' || path_[index_] == '-'))
            {
                ++index_;
            }
            while (index_ < path_.size() && std::isdigit(static_cast<unsigned char>(path_[index_])))
            {
                hasDigits = true;
                ++index_;
            }
        }
        if (!hasDigits)
        {
            index_ = start;
            return std::nullopt;
        }
        return std::stof(path_.substr(start, index_ - start));
    }

    void SkipSeparators()
    {
        while (index_ < path_.size())
        {
            const char ch = path_[index_];
            if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == ',')
            {
                ++index_;
                continue;
            }
            break;
        }
    }

    std::string path_;
    ID2D1GeometrySink *sink_ = nullptr;
    size_t index_ = 0;
    char command_ = '\0';
    float currentX_ = 0.0f;
    float currentY_ = 0.0f;
    float startX_ = 0.0f;
    float startY_ = 0.0f;
    float lastControlX_ = 0.0f;
    float lastControlY_ = 0.0f;
    bool lastControlValid_ = false;
    bool figureOpen_ = false;
};
} // namespace

Microsoft::WRL::ComPtr<ID2D1PathGeometry> CreatePathGeometryFromSvgPath(ID2D1Factory *factory,
                                                                        const std::string &pathData)
{
    if (!factory || pathData.empty())
    {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID2D1PathGeometry> geometry;
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(factory->CreatePathGeometry(geometry.GetAddressOf())) || FAILED(geometry->Open(sink.GetAddressOf())))
    {
        return nullptr;
    }

    sink->SetFillMode(D2D1_FILL_MODE_WINDING);

    SvgPathParser parser(pathData, sink.Get());
    if (!parser.Parse())
    {
        return nullptr;
    }

    if (FAILED(sink->Close()))
    {
        return nullptr;
    }

    return geometry;
}
} // namespace msimeui
