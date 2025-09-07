#pragma once

#include <string>
#include <vector>

// #define TUNE

namespace Engine {

struct Param {
    std::string name;
    int value, min, max;

    Param(std::string name, int value, int min, int max);

    operator int() const {
        return value;
    }
};

inline std::vector<Param *> params;

void set_param(const std::string &name, int value);
void params_to_spsa();

#ifdef TUNE
#define PARAM(name, value, min, max) inline Param name(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

PARAM(pawn_value_see, 101, 50, 150);
PARAM(knight_value_see, 315, 200, 400);
PARAM(bishop_value_see, 334, 200, 400);
PARAM(rook_value_see, 493, 400, 675);
PARAM(queen_value_see, 987, 700, 1500);

PARAM(tm_stability_base, 145, 100, 200);
PARAM(tm_stability_mult, 57, 0, 250);
PARAM(tm_stability_max, 7, 1, 20);

PARAM(tm_results_base, 62, 0, 160);
PARAM(tm_results_mult1, 14, 0, 27);
PARAM(tm_results_mult2, 26, 0, 45);
PARAM(tm_results_min, 80, 0, 150);
PARAM(tm_results_max, 134, 50, 220);

PARAM(tm_node_mult, 204, 120, 260);
PARAM(tm_node_base, 51, 10, 90);

PARAM(lmr_base, 117, 40, 200);
PARAM(lmr_div, 292, 150, 500);
PARAM(lmr_min_moves, 3, 1, 3);

PARAM(static_h_mult, -45, -500, -1);
PARAM(static_h_min, -61, -1000, -1);
PARAM(static_h_max, 262, 1, 1000);

PARAM(rzr_depth, 5, 2, 20);
PARAM(rzr_depth_mult, 264, 150, 350);

PARAM(rfp_depth, 11, 2, 20);
PARAM(rfp_depth_mult, 106, 40, 200);
PARAM(rfp_improving_mult, 88, 40, 200);

PARAM(nmp_depth_mult, 28, 1, 58);
PARAM(nmp_base, 168, 1, 400);
PARAM(nmp_eval_div, 201, 50, 350);
PARAM(nmp_rbase, 4, 1, 5);
PARAM(nmp_rdepth_div, 3, 1, 6);
PARAM(nmp_rmin, 4, 1, 10);

PARAM(prob_cut_margin, 229, 1, 500);

PARAM(history_div, 8197, 1, 16384);

PARAM(hp_depth, 6, 1, 15);
PARAM(hp_depth_mult, -6285, -12500, -2500);

PARAM(fp_depth, 9, 2, 20);
PARAM(fp_base, 110, 1, 300);
PARAM(fp_mult, 108, 5, 200);

PARAM(see_cap_margin, -101, -200, -20);
PARAM(see_quiet_margin, -20, -100, -1);

PARAM(double_ext_margin, 13, 1, 30);
PARAM(tripple_ext_margin, 86, 10, 250);
PARAM(zws_margin, 49, 10, 160);

PARAM(quiet_history_div, 7259, 1, 16384);
PARAM(noisy_history_div, 4537, 1, 16384);

PARAM(qfp_margin, 116, 40, 280);
PARAM(qsee_margin, -16, -200, 50);

PARAM(asp_delta, 11, 1, 30);
PARAM(asp_depth, 4, 2, 6);

PARAM(history_bonus_margin, 60, 10, 200);
PARAM(history_bonus_mult, 303, 1, 1536);
PARAM(history_bonus_minus, -72, -500, 500);
PARAM(max_history_bonus, 2408, 1, 4096);

PARAM(history_malus_mult, 261, 1, 1536);
PARAM(history_malus_minus, 154, -500, 500);
PARAM(max_history_malus, 1797, 1, 4096);

} // namespace Engine
