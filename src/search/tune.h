#pragma once

#include <string>
#include <vector>

// #define TUNE

namespace Astra {

struct Param {
    std::string name;
    int value, min, max;

    Param(std::string name, int value, int min, int max);

    operator int() const {
        return value;
    }
};

inline std::vector<Param *> params;

void setParam(const std::string &name, int value);
void paramsToSpsa();

#ifdef TUNE
#define PARAM(name, value, min, max) inline Param name(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

PARAM(stability_base, 130, 100, 200);
PARAM(stability_mult, 63, 0, 250);

PARAM(results_base, 62, 0, 160);
PARAM(results_mult1, 9, 0, 27);
PARAM(results_mult2, 28, 0, 45);
PARAM(results_min, 65, 0, 150);
PARAM(results_max, 139, 50, 220);

PARAM(node_mult, 187, 120, 260);
PARAM(node_base, 47, 10, 90);

PARAM(lmr_base, 104, 40, 200);
PARAM(lmr_div, 323, 150, 500);

PARAM(static_h_mult, -47, -500, -1);
PARAM(static_h_min, 51, 1, 1000);
PARAM(static_h_max, 233, 1, 1000);

PARAM(rfp_depth_mult, 101, 40, 200);
PARAM(rfp_improving_mult, 85, 40, 200);

PARAM(rzr_depth_mult, 244, 150, 350);

PARAM(nmp_depth_mult, 27, 1, 58);
PARAM(nmp_base, 153, 1, 400);
PARAM(nmp_eval_div, 212, 50, 350);

PARAM(hp_div, 7712, 1, 16384);

PARAM(fp_base, 133, 1, 300);
PARAM(fp_mult, 91, 5, 200);

PARAM(see_cap_margin, 97, 5, 200);
PARAM(see_quiet_margin, 81, -50, 100);

PARAM(ext_margin, 88, 10, 250);
PARAM(zws_margin, 51, 10, 160);

PARAM(hp_depth_mult, 6087, 2500, 12500);
PARAM(hp_qdiv, 6539, 1, 16384);
PARAM(hp_cdiv, 3506, 1, 16384);
PARAM(hbonus_margin, 41, 10, 200);

PARAM(qfp_margin, 111, 40, 280);

PARAM(history_bonus_mult, 390, 1, 1536);
PARAM(history_bonus_minus, -52, -500, 500);
PARAM(max_history_bonus, 2490, 1, 4096);

PARAM(history_malus_mult, 351, 1, 1536);
PARAM(history_malus_minus, 29, -500, 500);
PARAM(max_history_malus, 1871, 1, 4096);

} // namespace Astra
