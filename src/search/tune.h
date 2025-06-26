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
PARAM(stability_mult, 48, 0, 250);

PARAM(results_base, 66, 0, 160);
PARAM(results_mult1, 9, 0, 27);
PARAM(results_mult2, 28, 0, 45);
PARAM(results_min, 63, 0, 150);
PARAM(results_max, 136, 50, 220);

PARAM(node_mult, 194, 120, 260);
PARAM(node_base, 54, 10, 90);

PARAM(lmr_base, 110, 40, 200);
PARAM(lmr_div, 314, 150, 500);

PARAM(static_h_mult, -46, -500, -1);
PARAM(static_h_min, 53, 1, 1000);
PARAM(static_h_max, 282, 1, 1000);

PARAM(rzr_depth, 5, 2, 20);
PARAM(rzr_depth_mult, 252, 150, 350);

PARAM(rfp_depth, 11, 2, 20);
PARAM(rfp_depth_mult, 100, 40, 200);
PARAM(rfp_improving_mult, 84, 40, 200);

PARAM(nmp_depth_mult, 30, 1, 58);
PARAM(nmp_base, 156, 1, 400);
PARAM(nmp_eval_div, 217, 50, 350);
PARAM(nmp_rbase, 4, 1, 5);
PARAM(nmp_rdepth_div, 3, 1, 6);

PARAM(prob_cut_margin, 186, 1, 500);

PARAM(hp_depth, 5, 1, 15);
PARAM(hp_div, 8324, 1, 16384);

PARAM(fp_depth, 10, 2, 20);
PARAM(fp_base, 112, 1, 300);
PARAM(fp_mult, 104, 5, 200);

PARAM(see_cap_margin, 92, 5, 200);
PARAM(see_quiet_margin, 26, -100, 100);

PARAM(lmr_min_moves, 3, 1, 3);

PARAM(ext_margin, 73, 10, 250);
PARAM(zws_margin, 50, 10, 160);

PARAM(hp_depth_mult, 6164, 2500, 12500);
PARAM(hp_qdiv, 6855, 1, 16384);
PARAM(hp_cdiv, 3796, 1, 16384);
PARAM(hbonus_margin, 45, 10, 200);

PARAM(qfp_margin, 121, 40, 280);

PARAM(asp_delta, 10, 1, 30);
PARAM(asp_depth, 4, 2, 6);

PARAM(history_bonus_mult, 261, 1, 1536);
PARAM(history_bonus_minus, -116, -500, 500);
PARAM(max_history_bonus, 2636, 1, 4096);

PARAM(history_malus_mult, 393, 1, 1536);
PARAM(history_malus_minus, 121, -500, 500);
PARAM(max_history_malus, 1760, 1, 4096);

} // namespace Astra
