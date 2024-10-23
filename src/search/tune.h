#ifndef SEARCHPARAMS_H
#define SEARCHPARAMS_H

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

//#define TUNE

namespace Astra {

    struct Param;

    // holds all the tuning parameters for search
    inline std::vector<Param*> params;

    struct Param {
        std::string name;
        int value;
        int min, max;

        Param(std::string name, int value, int min, int max) :
        name(std::move(name)), value(value), min(min), max(max)
        {
            params.push_back(this);
        }

        operator int() const {
            return value;
        }
    };

    inline void paramsToUCI() {
        for (const auto& param : params) {
            std::cout << "option name " << param->name
                    << " type spin default " << param->value
                    << " min " << param->min
                    << " max " << param->max << std::endl;
        }
    }

    inline void setParam(const std::string& name, int value) {
        for (auto* param : params) {
            if (param->name == name) {
                param->value = value;
                return;
            }
        }
    }

    inline std::string paramsToSpsa() {
        std::ostringstream ss;

        for (auto& param : params) {
            ss << param->name
              << ", " << "int"
              << ", " << param->value
              << ", " << param->min
              << ", " << param->max
              << ", " << std::max(0.5, static_cast<double>(param->max - param->min) / 20.0)
              << ", " << 0.002 << "\n";
        }

        return ss.str();
    }

#ifdef TUNE
#define PARAM(name, value, min, max) Param name(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

} // namespace Astra

#endif //SEARCHPARAMS_H
