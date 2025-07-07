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
PARAM(stability_mult, 51, 0, 250);

PARAM(results_base, 57, 0, 160);
PARAM(results_mult1, 11, 0, 27);
PARAM(results_mult2, 28, 0, 45);
PARAM(results_min, 67, 0, 150);
PARAM(results_max, 139, 50, 220);

PARAM(node_mult, 197, 120, 260);
PARAM(node_base, 54, 10, 90);

PARAM(lmr_base, 107, 40, 200);
PARAM(lmr_div, 303, 150, 500);
PARAM(lmr_min_moves, 3, 1, 3);

PARAM(static_h_mult, -53, -500, -1);
PARAM(static_h_min, 72, 1, 1000);
PARAM(static_h_max, 286, 1, 1000);

PARAM(rzr_depth, 5, 2, 20);
PARAM(rzr_depth_mult, 258, 150, 350);

PARAM(rfp_depth, 11, 2, 20);
PARAM(rfp_depth_mult, 106, 40, 200);
PARAM(rfp_improving_mult, 89, 40, 200);

PARAM(nmp_depth_mult, 25, 1, 58);
PARAM(nmp_base, 161, 1, 400);
PARAM(nmp_eval_div, 217, 50, 350);
PARAM(nmp_rbase, 4, 1, 5);
PARAM(nmp_rdepth_div, 3, 1, 6);
PARAM(nmp_rmin, 4, 1, 10);

PARAM(prob_cut_margin, 237, 1, 500);

PARAM(hp_depth, 5, 1, 15);
PARAM(hp_div, 7848, 1, 16384);

PARAM(fp_depth, 9, 2, 20);
PARAM(fp_base, 98, 1, 300);
PARAM(fp_mult, 107, 5, 200);

PARAM(see_cap_margin, 97, 5, 200);
PARAM(see_quiet_margin, 17, -100, 100);

PARAM(ext_margin, 74, 10, 250);
PARAM(zws_margin, 56, 10, 160);

PARAM(hp_depth_mult, 6775, 2500, 12500);
PARAM(hp_qdiv, 7489, 1, 16384);
PARAM(hp_cdiv, 3886, 1, 16384);
PARAM(hbonus_margin, 64, 10, 200);

PARAM(qfp_margin, 114, 40, 280);

PARAM(asp_delta, 11, 1, 30);
PARAM(asp_depth, 4, 2, 6);

PARAM(history_bonus_mult, 238, 1, 1536);
PARAM(history_bonus_minus, -64, -500, 500);
PARAM(max_history_bonus, 2546, 1, 4096);

PARAM(history_malus_mult, 364, 1, 1536);
PARAM(history_malus_minus, 128, -500, 500);
PARAM(max_history_malus, 1737, 1, 4096);

} // namespace Astra
