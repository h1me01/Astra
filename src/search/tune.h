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

PARAM(stability_base, 131, 100, 200);
PARAM(stability_mult, 65, 0, 250);

PARAM(results_base, 62, 0, 160);
PARAM(results_mult1, 9, 0, 27);
PARAM(results_mult2, 28, 0, 45);
PARAM(results_min, 64, 0, 150);
PARAM(results_max, 139, 50, 220);

PARAM(node_mult, 187, 120, 260);
PARAM(node_base, 47, 10, 90);

PARAM(lmr_base, 106, 40, 200);
PARAM(lmr_div, 325, 150, 500);

PARAM(static_h_mult, -48, -500, -1);
PARAM(static_h_min, 62, 1, 1000);
PARAM(static_h_max, 230, 1, 1000);

PARAM(rzr_depth, 5, 2, 20);
PARAM(rzr_depth_mult, 242, 150, 350);

PARAM(rfp_depth, 10, 2, 20);
PARAM(rfp_depth_mult, 101, 40, 200);
PARAM(rfp_improving_mult, 85, 40, 200);

PARAM(nmp_depth_mult, 27, 1, 58);
PARAM(nmp_base, 159, 1, 400);
PARAM(nmp_eval_div, 215, 50, 350);

PARAM(prob_cut_margin, 174, 1, 500);

PARAM(hp_depth, 5, 1, 15);
PARAM(hp_div, 7725, 1, 16384);

PARAM(fp_depth, 11, 2, 20);
PARAM(fp_base, 129, 1, 300);
PARAM(fp_mult, 91, 5, 200);

PARAM(see_cap_margin, 97, 5, 200);
PARAM(see_quiet_margin, 80, -100, 100);

PARAM(ext_margin, 85, 10, 250);
PARAM(zws_margin, 50, 10, 160);

PARAM(hp_depth_mult, 5976, 2500, 12500);
PARAM(hp_qdiv, 6606, 1, 16384);
PARAM(hp_cdiv, 3526, 1, 16384);
PARAM(hbonus_margin, 41, 10, 200);

PARAM(qfp_margin, 113, 40, 280);

PARAM(asp_delta, 12, 1, 30);
PARAM(asp_depth, 4, 2, 6);

PARAM(history_bonus_mult, 371, 1, 1536);
PARAM(history_bonus_minus, -56, -500, 500);
PARAM(max_history_bonus, 2527, 1, 4096);

PARAM(history_malus_mult, 363, 1, 1536);
PARAM(history_malus_minus, 34, -500, 500);
PARAM(max_history_malus, 1866, 1, 4096);

} // namespace Astra
