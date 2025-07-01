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

PARAM(stability_base, 132, 100, 200);
PARAM(stability_mult, 49, 0, 250);

PARAM(results_base, 60, 0, 160);
PARAM(results_mult1, 10, 0, 27);
PARAM(results_mult2, 27, 0, 45);
PARAM(results_min, 70, 0, 150);
PARAM(results_max, 142, 50, 220);

PARAM(node_mult, 195, 120, 260);
PARAM(node_base, 52, 10, 90);

PARAM(lmr_base, 105, 40, 200);
PARAM(lmr_div, 300, 150, 500);
PARAM(lmr_min_moves, 3, 1, 3);

PARAM(static_h_mult, -45, -500, -1);
PARAM(static_h_min, 90, 1, 1000);
PARAM(static_h_max, 278, 1, 1000);

PARAM(rzr_depth, 5, 2, 20);
PARAM(rzr_depth_mult, 255, 150, 350);

PARAM(rfp_depth, 11, 2, 20);
PARAM(rfp_depth_mult, 105, 40, 200);
PARAM(rfp_improving_mult, 89, 40, 200);

PARAM(nmp_depth_mult, 26, 1, 58);
PARAM(nmp_base, 155, 1, 400);
PARAM(nmp_eval_div, 218, 50, 350);
PARAM(nmp_rbase, 4, 1, 5);
PARAM(nmp_rdepth_div, 3, 1, 6);
PARAM(nmp_rmin, 4, 1, 10);

PARAM(prob_cut_margin, 203, 1, 500);

PARAM(hp_depth, 5, 1, 15);
PARAM(hp_div, 7901, 1, 16384);

PARAM(fp_depth, 10, 2, 20);
PARAM(fp_base, 102, 1, 300);
PARAM(fp_mult, 101, 5, 200);

PARAM(see_cap_margin, 94, 5, 200);
PARAM(see_quiet_margin, 23, -100, 100);

PARAM(ext_margin, 73, 10, 250);
PARAM(zws_margin, 52, 10, 160);

PARAM(hp_depth_mult, 6624, 2500, 12500);
PARAM(hp_qdiv, 7066, 1, 16384);
PARAM(hp_cdiv, 3767, 1, 16384);
PARAM(hbonus_margin, 57, 10, 200);

PARAM(qfp_margin, 118, 40, 280);

PARAM(asp_delta, 11, 1, 30);
PARAM(asp_depth, 4, 2, 6);

PARAM(history_bonus_mult, 245, 1, 1536);
PARAM(history_bonus_minus, -72, -500, 500);
PARAM(max_history_bonus, 2637, 1, 4096);

PARAM(history_malus_mult, 361, 1, 1536);
PARAM(history_malus_minus, 127, -500, 500);
PARAM(max_history_malus, 1776, 1, 4096);

} // namespace Astra
