#include <iostream>
#include <sstream>
#include <vector>
#include "tune.h"

namespace Astra
{
    Param::Param(std::string name, int value, int min, int max, int step) 
    : name(std::move(name)), value(value), min(min), max(max), step(step)
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
        {
            std::cout << "option name " << param->name
                      << " type spin default " << param->value
                      << " min " << param->min
                      << " max " << param->max << std::endl;
        }
    }

    void setParam(const std::string &name, int value)
    {
        for (auto *param : params)
        {
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
    }

    void paramsToSpsa()
    {
        for (auto &param : params)
        {
            std::cout << param->name
                      << ", " << "int"
                      << ", " << param->value
                      << ", " << param->min
                      << ", " << param->max
                      << ", " << std::max(0.5, double(param->max - param->min) / 20.0)
                      << ", " << 0.002 << "\n";
        }
    }

    void paramsToJSON()
    {
        std::stringstream ss;
        ss << "{\n";
        for (size_t i = 0; i < params.size(); i++)
        {
            ss << "  \"" << params[i]->name << "\": {\n";
            ss << "    \"value\": " << params[i]->value << ",\n";
            ss << "    \"min_value\": " << params[i]->min << ",\n";
            ss << "    \"max_value\": " << params[i]->max << ",\n";
            ss << "    \"step\": " << params[i]->step << "\n";
            ss << "  }";
            
            if (i < params.size() - 1)
                ss << ",";
            
            ss << "\n";
        }
        ss << "}\n";
        std::cout << ss.str();
    }

} // namespace Astra
