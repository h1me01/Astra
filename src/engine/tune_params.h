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

PARAM(pawn_value_see, 98, 50, 150);
PARAM(knight_value_see, 318, 200, 400);
PARAM(bishop_value_see, 335, 200, 400);
PARAM(rook_value_see, 504, 400, 675);
PARAM(queen_value_see, 997, 700, 1500);

PARAM(tm_stability_base, 157, 100, 200);
PARAM(tm_stability_mult, 35, 0, 250);
PARAM(tm_stability_max, 4, 1, 20);

PARAM(tm_results_base, 70, 0, 160);
PARAM(tm_results_mult1, 15, 0, 27);
PARAM(tm_results_mult2, 29, 0, 45);
PARAM(tm_results_min, 101, 0, 150);
PARAM(tm_results_max, 147, 50, 220);

PARAM(tm_node_mult, 219, 120, 260);
PARAM(tm_node_base, 58, 10, 90);

PARAM(lmr_base, 111, 40, 200);
PARAM(lmr_div, 297, 150, 500);

PARAM(tt_hist_bonus_mult, 308, 1, 1536);
PARAM(tt_hist_bonus_minus, 4, -500, 500);
PARAM(max_tt_hist_bonus, 2445, 1, 4096);

PARAM(tt_hist_malus_mult, 311, 1, 1536);
PARAM(tt_hist_malus_minus, 104, -500, 500);
PARAM(max_tt_hist_malus, 1646, 1, 4096);

PARAM(static_h_mult, -51, -500, -1);
PARAM(static_h_min, -108, -1000, -1);
PARAM(static_h_max, 226, 1, 1000);

PARAM(rzr_base, 318, 1, 750);
PARAM(rzr_mult, 234, 1, 500);

PARAM(rfp_depth, 11, 2, 20);
PARAM(rfp_depth_mult, 103, 40, 200);
PARAM(rfp_improving_mult, 85, 40, 200);

PARAM(nmp_depth_mult, 26, 1, 58);
PARAM(nmp_base, 192, 1, 400);
PARAM(nmp_eval_div, 193, 50, 350);
PARAM(nmp_rbase, 4, 1, 5);
PARAM(nmp_rdepth_div, 3, 1, 6);
PARAM(nmp_rmin, 4, 1, 10);

PARAM(probcut_margin, 217, 1, 500);

PARAM(hist_div, 7778, 1, 16384);

PARAM(hp_depth, 6, 1, 15);
PARAM(hp_depth_mult, -5429, -12500, -2500);

PARAM(fp_depth, 10, 2, 20);
PARAM(fp_base, 88, 1, 300);
PARAM(fp_mult, 104, 5, 200);

PARAM(see_cap_margin, -103, -200, -20);
PARAM(see_quiet_margin, -15, -100, -1);

PARAM(double_ext_margin, 13, 1, 30);
PARAM(tripple_ext_margin, 93, 10, 250);
PARAM(zws_margin, 39, 10, 160);

PARAM(quiet_hist_div, 8026, 1, 16384);
PARAM(noisy_hist_div, 4566, 1, 16384);

PARAM(qfp_margin, 111, 40, 280);
PARAM(qsee_margin, -20, -200, 50);

PARAM(asp_delta, 11, 1, 30);
PARAM(asp_depth, 4, 2, 6);

PARAM(hist_bonus_margin, 52, 10, 200);

PARAM(hist_bonus_mult, 308, 1, 1536);
PARAM(hist_bonus_minus, 4, -500, 500);
PARAM(max_hist_bonus, 2445, 1, 4096);

PARAM(hist_malus_mult, 311, 1, 1536);
PARAM(hist_malus_minus, 104, -500, 500);
PARAM(max_hist_malus, 1646, 1, 4096);

PARAM(quiet_hist_bonus_mult, 1024, 1, 2048);
PARAM(quiet_hist_malus_mult, 1024, 1, 2048);

PARAM(noisy_hist_bonus_mult, 1024, 1, 2048);
PARAM(noisy_hist_malus_mult, 1024, 1, 2048);

} // namespace Engine
