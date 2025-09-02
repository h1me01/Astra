#include <iostream>
#include <sstream>

#include "tune_params.h"

namespace Engine {

Param::Param(std::string name, int value, int min, int max) //
    : name(std::move(name)), value(value), min(min), max(max) {
    if(value < min || value > max) {
        std::cerr << "info string Value out of range for parameter " << name << std::endl;
        return;
    }

    params.push_back(this);
}

void set_param(const std::string &name, int value) {
    for(auto *param : params) {
        if(param->name != name)
            continue;

        if(value < param->min || value > param->max) {
            std::cerr << "info string Value out of range for parameter " << name << std::endl;
            return;
        }

        param->value = value;
        break;
    }
}

void params_to_spsa() {
    for(auto &param : params) {
        std::cout << param->name << ", "                                           //
                  << "int" << ", "                                                 //
                  << param->value << ", "                                          //
                  << param->min << ", "                                            //
                  << param->max << ", "                                            //
                  << std::max(0.5, double(param->max - param->min) / 20.0) << ", " //
                  << 0.002 << "\n";
    }
}

} // namespace Engine
