#pragma once

#include <string>
#include <vector>

 #define TUNE

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

PARAM(stability_base, 128, 100, 200);
PARAM(stability_mult, 68, 0, 250);

PARAM(results_base, 59, 0, 160);
PARAM(results_mult1, 10, 0, 27);
PARAM(results_mult2, 27, 0, 45);
PARAM(results_min, 59, 0, 150);
PARAM(results_max, 136, 50, 220);

PARAM(node_mult, 192, 120, 260);
PARAM(node_base, 50, 10, 90);

PARAM(lmr_base, 102, 40, 200);
PARAM(lmr_div, 319, 150, 500);

PARAM(static_h_mult, -39, -500, -1);
PARAM(static_h_min, 45, 1, 1000);
PARAM(static_h_max, 252, 1, 1000);

PARAM(rzr_depth, 5, 2, 20);
PARAM(rzr_depth_mult, 247, 150, 350);

PARAM(rfp_depth, 10, 2, 20);
PARAM(rfp_depth_mult, 105, 40, 200);
PARAM(rfp_improving_mult, 81, 40, 200);

PARAM(nmp_depth_mult, 29, 1, 58);
PARAM(nmp_base, 149, 1, 400);
PARAM(nmp_eval_div, 223, 50, 350);

PARAM(prob_cut_margin, 165, 1, 500);

PARAM(hp_depth, 5, 1, 15);
PARAM(hp_div, 8102, 1, 16384);

PARAM(fp_depth, 11, 2, 20);
PARAM(fp_base, 118, 1, 300);
PARAM(fp_mult, 98, 5, 200);

PARAM(see_cap_margin, 93, 5, 200);
PARAM(see_quiet_margin, 27, -100, 100);

PARAM(ext_margin, 79, 10, 250);
PARAM(zws_margin, 47, 10, 160);

PARAM(hp_depth_mult, 6062, 2500, 12500);
PARAM(hp_qdiv, 6185, 1, 16384);
PARAM(hp_cdiv, 3175, 1, 16384);
PARAM(hbonus_margin, 45, 10, 200);

PARAM(qfp_margin, 115, 40, 280);

PARAM(asp_delta, 11, 1, 30);
PARAM(asp_depth, 4, 2, 6);

PARAM(history_bonus_mult, 387, 1, 1536);
PARAM(history_bonus_minus, -81, -500, 500);
PARAM(max_history_bonus, 2631, 1, 4096);

PARAM(history_malus_mult, 320, 1, 1536);
PARAM(history_malus_minus, 59, -500, 500);
PARAM(max_history_malus, 1763, 1, 4096);

} // namespace Astra
