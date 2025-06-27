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
PARAM(stability_mult, 45, 0, 250);

PARAM(results_base, 66, 0, 160);
PARAM(results_mult1, 9, 0, 27);
PARAM(results_mult2, 27, 0, 45);
PARAM(results_min, 64, 0, 150);
PARAM(results_max, 136, 50, 220);

PARAM(node_mult, 192, 120, 260);
PARAM(node_base, 54, 10, 90);

PARAM(lmr_base, 108, 40, 200);
PARAM(lmr_div, 319, 150, 500);

PARAM(static_h_mult, -50, -500, -1);
PARAM(static_h_min, 60, 1, 1000);
PARAM(static_h_max, 276, 1, 1000);

PARAM(rzr_depth, 5, 2, 20);
PARAM(rzr_depth_mult, 254, 150, 350);

PARAM(rfp_depth, 11, 2, 20);
PARAM(rfp_depth_mult, 98, 40, 200);
PARAM(rfp_improving_mult, 85, 40, 200);

PARAM(nmp_depth_mult, 29, 1, 58);
PARAM(nmp_base, 158, 1, 400);
PARAM(nmp_eval_div, 217, 50, 350);
PARAM(nmp_rbase, 4, 1, 5);
PARAM(nmp_rdepth_div, 3, 1, 6);

PARAM(prob_cut_margin, 184, 1, 500);

PARAM(hp_depth, 5, 1, 15);
PARAM(hp_div, 7957, 1, 16384);

PARAM(fp_depth, 10, 2, 20);
PARAM(fp_base, 111, 1, 300);
PARAM(fp_mult, 104, 5, 200);

PARAM(see_cap_margin, 93, 5, 200);
PARAM(see_quiet_margin, 26, -100, 100);

PARAM(lmr_min_moves, 3, 1, 3);

PARAM(ext_margin, 72, 10, 250);
PARAM(zws_margin, 50, 10, 160);

PARAM(hp_depth_mult, 6239, 2500, 12500);
PARAM(hp_qdiv, 6932, 1, 16384);
PARAM(hp_cdiv, 4006, 1, 16384);
PARAM(hbonus_margin, 47, 10, 200);

PARAM(qfp_margin, 119, 40, 280);

PARAM(asp_delta, 11, 1, 30);
PARAM(asp_depth, 4, 2, 6);

PARAM(history_bonus_mult, 264, 1, 1536);
PARAM(history_bonus_minus, -104, -500, 500);
PARAM(max_history_bonus, 2637, 1, 4096);

PARAM(history_malus_mult, 401, 1, 1536);
PARAM(history_malus_minus, 130, -500, 500);
PARAM(max_history_malus, 1780, 1, 4096);

} // namespace Astra
