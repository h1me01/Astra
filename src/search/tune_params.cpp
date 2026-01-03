#include <iostream>
#include <sstream>

#include "tune_params.h"

namespace search {

Param::Param(std::string name, int value, int min, int max) //
    : name(std::move(name)), value(value), min(min), max(max) {
    if(value < min || value > max) {
        std::cerr << "Value out of range for search parameter " << name << std::endl;
        return;
    }

    if(min >= max) {
        std::cerr << "Invalid range for search parameter " << name << std::endl;
        return;
    }

    params.push_back(this);
}

bool set_param(const std::string &name, int value) {
    bool found = false;

    for(auto *param : params) {
        if(param->name != name)
            continue;

        if(value < param->min || value > param->max) {
            std::cerr << "Value out of range for search parameter " << name << std::endl;
            return true;
        }

        param->value = value;
        found = true;
        break;
    }

    if(!found)
        std::cerr << "Unknown search parameter " << name << std::endl;

    return found;
}

void params_to_spsa() {
    for(const auto &param : params) {
        std::cout << param->name << ", "                                                        //
                  << "int" << ", "                                                              //
                  << param->value << ", "                                                       //
                  << param->min << ", "                                                         //
                  << param->max << ", "                                                         //
                  << std::max(0.5, static_cast<double>(param->max - param->min) / 20.0) << ", " //
                  << 0.002 << "\n";
    }
}

} // namespace search
