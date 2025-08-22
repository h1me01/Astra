#pragma once

#include <string>
#include <vector>

// #define TUNE

namespace Search {

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

PARAM(pawn_value, 101, 50, 150);
PARAM(knight_value, 311, 200, 400);
PARAM(bishop_value, 328, 200, 400);
PARAM(rook_value, 489, 400, 600);
PARAM(queen_value, 994, 700, 1100);

PARAM(pawn_value_see, 103, 50, 150);
PARAM(knight_value_see, 330, 200, 400);
PARAM(bishop_value_see, 314, 200, 400);
PARAM(rook_value_see, 511, 400, 600);
PARAM(queen_value_see, 997, 700, 1100);

PARAM(tm_stability_base, 141, 100, 200);
PARAM(tm_stability_mult, 57, 0, 250);
PARAM(tm_stability_max, 8, 1, 20);

PARAM(tm_results_base, 62, 0, 160);
PARAM(tm_results_mult1, 13, 0, 27);
PARAM(tm_results_mult2, 26, 0, 45);
PARAM(tm_results_min, 73, 0, 150);
PARAM(tm_results_max, 134, 50, 220);

PARAM(tm_node_mult, 200, 120, 260);
PARAM(tm_node_base, 49, 10, 90);

PARAM(lmr_base, 104, 40, 200);
PARAM(lmr_div, 325, 150, 500);
PARAM(lmr_min_moves, 3, 1, 3);

PARAM(static_h_mult, -36, -500, -1);
PARAM(static_h_min, 46, 1, 1000);
PARAM(static_h_max, 344, 1, 1000);

PARAM(rzr_depth, 6, 2, 20);
PARAM(rzr_depth_mult, 263, 150, 350);

PARAM(rfp_depth, 10, 2, 20);
PARAM(rfp_depth_mult, 103, 40, 200);
PARAM(rfp_improving_mult, 88, 40, 200);

PARAM(nmp_depth_mult, 28, 1, 58);
PARAM(nmp_base, 178, 1, 400);
PARAM(nmp_eval_div, 229, 50, 350);
PARAM(nmp_rbase, 5, 1, 5);
PARAM(nmp_rdepth_div, 4, 1, 6);
PARAM(nmp_rmin, 4, 1, 10);

PARAM(prob_cut_margin, 230, 1, 500);

PARAM(hp_div, 7445, 1, 16384);

PARAM(hp_depth, 4, 1, 15);
PARAM(hp_depth_mult, 6489, 2500, 12500);

PARAM(fp_depth, 9, 2, 20);
PARAM(fp_base, 94, 1, 300);
PARAM(fp_mult, 115, 5, 200);

PARAM(see_cap_margin, 97, 5, 200);
PARAM(see_quiet_margin, 31, -100, 100);

PARAM(double_ext_margin, 15, 1, 30);
PARAM(tripple_ext_margin, 70, 10, 250);

PARAM(zws_margin, 69, 10, 160);

PARAM(hp_qdiv, 6478, 1, 16384);
PARAM(hp_cdiv, 4932, 1, 16384);

PARAM(qfp_margin, 106, 40, 280);
PARAM(qsee_margin, -20, -200, 50);

PARAM(asp_delta, 11, 1, 30);
PARAM(asp_depth, 5, 2, 6);

PARAM(history_bonus_mult, 337, 1, 1536);
PARAM(history_bonus_minus, -59, -500, 500);
PARAM(max_history_bonus, 2377, 1, 4096);

PARAM(history_malus_mult, 325, 1, 1536);
PARAM(history_malus_minus, 98, -500, 500);
PARAM(max_history_malus, 1634, 1, 4096);

} // namespace Search
