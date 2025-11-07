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

bool set_param(const std::string &name, int value);
void params_to_spsa();

#ifdef TUNE
#define PARAM(name, value, min, max) inline Param name(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

PARAM(pawn_value_see, 102, 50, 150);
PARAM(knight_value_see, 313, 200, 400);
PARAM(bishop_value_see, 337, 200, 400);
PARAM(rook_value_see, 492, 400, 675);
PARAM(queen_value_see, 992, 700, 1500);

PARAM(tm_stability_base, 149, 100, 200);
PARAM(tm_stability_mult, 42, 0, 250);
PARAM(tm_stability_max, 6, 1, 20);

PARAM(tm_results_base, 64, 0, 160);
PARAM(tm_results_mult1, 15, 0, 27);
PARAM(tm_results_mult2, 26, 0, 45);
PARAM(tm_results_min, 89, 0, 150);
PARAM(tm_results_max, 139, 50, 220);

PARAM(tm_node_mult, 204, 120, 260);
PARAM(tm_node_base, 56, 10, 90);

PARAM(lmr_base, 115, 40, 200);
PARAM(lmr_div, 295, 150, 500);

PARAM(static_h_mult, -47, -500, -1);
PARAM(static_h_min, -79, -1000, -1);
PARAM(static_h_max, 248, 1, 1000);

PARAM(rzr_depth_mult, 267, 150, 350);

PARAM(rfp_depth, 11, 2, 20);
PARAM(rfp_depth_mult, 108, 40, 200);
PARAM(rfp_improving_mult, 90, 40, 200);

PARAM(nmp_depth_mult, 28, 1, 58);
PARAM(nmp_base, 175, 1, 400);
PARAM(nmp_eval_div, 195, 50, 350);
PARAM(nmp_rbase, 4, 1, 5);
PARAM(nmp_rdepth_div, 3, 1, 6);
PARAM(nmp_rmin, 4, 1, 10);

PARAM(probcut_margin, 242, 1, 500);

PARAM(history_div, 8070, 1, 16384);

PARAM(hp_depth, 6, 1, 15);
PARAM(hp_depth_mult, -6112, -12500, -2500);

PARAM(fp_depth, 9, 2, 20);
PARAM(fp_base, 104, 1, 300);
PARAM(fp_mult, 110, 5, 200);

PARAM(see_cap_margin, -103, -200, -20);
PARAM(see_quiet_margin, -18, -100, -1);

PARAM(double_ext_margin, 14, 1, 30);
PARAM(tripple_ext_margin, 96, 10, 250);
PARAM(zws_margin, 49, 10, 160);

PARAM(quiet_history_div, 7726, 1, 16384);
PARAM(noisy_history_div, 4588, 1, 16384);

PARAM(qfp_margin, 117, 40, 280);
PARAM(qsee_margin, -20, -200, 50);

PARAM(asp_delta, 11, 1, 30);
PARAM(asp_depth, 4, 2, 6);

PARAM(history_bonus_margin, 54, 10, 200);
PARAM(history_bonus_mult, 311, 1, 1536);
PARAM(history_bonus_minus, -28, -500, 500);
PARAM(max_history_bonus, 2273, 1, 4096);

PARAM(history_malus_mult, 281, 1, 1536);
PARAM(history_malus_minus, 131, -500, 500);
PARAM(max_history_malus, 1861, 1, 4096);

} // namespace Engine
