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

PARAM(stability_base, 134, 100, 200);
PARAM(stability_mult, 48, 0, 250);

PARAM(results_base, 59, 0, 160);
PARAM(results_mult1, 10, 0, 27);
PARAM(results_mult2, 28, 0, 45);
PARAM(results_min, 68, 0, 150);
PARAM(results_max, 141, 50, 220);

PARAM(node_mult, 196, 120, 260);
PARAM(node_base, 52, 10, 90);

PARAM(lmr_base, 107, 40, 200);
PARAM(lmr_div, 299, 150, 500);
PARAM(lmr_min_moves, 3, 1, 3);

PARAM(static_h_mult, -44, -500, -1);
PARAM(static_h_min, 83, 1, 1000);
PARAM(static_h_max, 275, 1, 1000);

PARAM(rzr_depth, 5, 2, 20);
PARAM(rzr_depth_mult, 256, 150, 350);

PARAM(rfp_depth, 11, 2, 20);
PARAM(rfp_depth_mult, 108, 40, 200);
PARAM(rfp_improving_mult, 90, 40, 200);

PARAM(nmp_depth_mult, 26, 1, 58);
PARAM(nmp_base, 164, 1, 400);
PARAM(nmp_eval_div, 219, 50, 350);
PARAM(nmp_rbase, 4, 1, 5);
PARAM(nmp_rdepth_div, 3, 1, 6);
PARAM(nmp_rmin, 4, 1, 10);

PARAM(prob_cut_margin, 218, 1, 500);

PARAM(hp_depth, 5, 1, 15);
PARAM(hp_div, 7889, 1, 16384);

PARAM(fp_depth, 9, 2, 20);
PARAM(fp_base, 97, 1, 300);
PARAM(fp_mult, 102, 5, 200);

PARAM(see_cap_margin, 95, 5, 200);
PARAM(see_quiet_margin, 18, -100, 100);

PARAM(ext_margin, 72, 10, 250);
PARAM(zws_margin, 53, 10, 160);

PARAM(hp_depth_mult, 6554, 2500, 12500);
PARAM(hp_qdiv, 7313, 1, 16384);
PARAM(hp_cdiv, 3707, 1, 16384);
PARAM(hbonus_margin, 58, 10, 200);

PARAM(qfp_margin, 114, 40, 280);

PARAM(asp_delta, 11, 1, 30);
PARAM(asp_depth, 4, 2, 6);

PARAM(history_bonus_mult, 260, 1, 1536);
PARAM(history_bonus_minus, -69, -500, 500);
PARAM(max_history_bonus, 2608, 1, 4096);

PARAM(history_malus_mult, 366, 1, 1536);
PARAM(history_malus_minus, 123, -500, 500);
PARAM(max_history_malus, 1740, 1, 4096);

} // namespace Astra
