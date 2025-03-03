#include <iostream>
#include <sstream>
#include <vector>
#include "tune.h"

namespace Astra
{
    Param::Param(std::string name, int value, int min, int max)
        : name(std::move(name)), value(value), min(min), max(max)
    {
        if (value < min || value > max)
        {
            std::cerr << "Error: value out of range for parameter " << name << std::endl;
            return;
        }

        params.push_back(this);
    }

    Param::operator int() const { return value; }

    void paramsToUCI()
    {
        for (const auto &param : params)
            std::cout << "option name " << param->name
                      << " type spin default " << param->value
                      << " min " << param->min
                      << " max " << param->max << std::endl;
    }

    void setParam(const std::string &name, int value)
    {
        for (auto *param : params)
            if (param->name == name)
            {
                if (value < param->min || value > param->max)
                {
                    std::cerr << "Error: value out of range for parameter " << name << std::endl;
                    return;
                }

                param->value = value;
                return;
            }
    }

    void paramsToSpsa()
    {
        for (auto &param : params)
            std::cout << param->name
                      << ", " << "int"
                      << ", " << param->value
                      << ", " << param->min
                      << ", " << param->max
                      << ", " << std::max(0.5, double(param->max - param->min) / 20.0)
                      << ", " << 0.002 << "\n";
    }

} // namespace Astra
