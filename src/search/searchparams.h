#ifndef SEARCHPARAMS_H
#define SEARCHPARAMS_H

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

namespace Astra {

    struct Param {
        std::string name;
        int max, min;
        int value;

        Param(std::string name, int value, int min, int max) :
        name(std::move(name)), value(value), min(min), max(max) {}

        explicit operator int() const {
            return value;
        }
    };

    // holds all the tuning parameters for search
    inline std::vector<Param> params;

    inline void paramsToUCI() {
        for (const auto& param : params) {
            std::cout << "option name " << param.name
                    << " type spin default " << param.value
                    << " min " << param.min
                    << " max " << param.max << std::endl;
        }
    }

    inline std::string paramsToSpsa() {
        std::ostringstream ss;

        for (auto& param : params) {
            ss << param.name
              << ", " << "int"
              << ", " << param.value
              << ", " << param.min
              << ", " << param.max
              << ", " << std::max(0.5, static_cast<double>(param.max - param.min) / 20.0)
              << ", " << 0.002 << "\n";
        }

        return ss.str();
    }

    //#define TUNE

#ifdef TUNE
#define PARAM(name, value, min, max) params.emplace_back(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

    // search parameters
    PARAM(lmr_base, 100, 50, 150);
    PARAM(lmr_div, 175, 150, 250);

    PARAM(razor_margin, 130, 60, 200);
    PARAM(razor_depth, 3, 3, 7);

    PARAM(delta_margin, 400, 400, 900);

    PARAM(rfp_depth_multiplier, 62, 35, 85);
    PARAM(rfp_improvement_bonus, 71, 50, 100);
    PARAM(rfp_depth, 7, 3, 8);

    PARAM(nmp_depth, 3, 3, 7);

    PARAM(pv_see_cap_margin, 100, 90, 100);
    PARAM(pv_see_cap_depth, 6, 1, 7);

    PARAM(pv_see_quiet_margin, 40, 40, 95);
    PARAM(pv_see_quiet_depth, 7, 1, 8);

    PARAM(lmp_depth, 5, 3, 6);
    PARAM(lmp_base, 4, 3, 6);

    PARAM(lmr_depth, 2, 1, 3);
    PARAM(lmr_moves_base, 3, 1, 5);

    PARAM(history_bonus, 155, 100, 200);

} // namespace Astra

#endif //SEARCHPARAMS_H
