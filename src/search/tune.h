#pragma once

#include <string>

// #define TUNE

namespace Astra
{

    struct Param
    {
        std::string name;
        int value, min, max;

        Param(std::string name, int value, int min, int max);

        operator int() const { return value; }
    };

    inline std::vector<Param *> params;

    void setParam(const std::string &name, int value);
    void paramsToSpsa();

#ifdef TUNE
#define PARAM(name, value, min, max) inline Param name(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

    // time management
    PARAM(stability_base, 128, 100, 200);
    PARAM(stability_mult, 74, 0, 250);

    PARAM(results_base, 69, 0, 160);
    PARAM(results_mult1, 10, 0, 27);
    PARAM(results_mult2, 25, 0, 45);
    PARAM(results_min, 65, 0, 150);
    PARAM(results_max, 134, 50, 220);

    PARAM(node_mult, 188, 120, 260);
    PARAM(node_base, 50, 10, 90);

    PARAM(lmr_base, 106, 40, 200);
    PARAM(lmr_div, 276, 150, 500);

    PARAM(static_h_mult, -44, -500, -1);
    PARAM(static_h_min, 47, 1, 1000);
    PARAM(static_h_max, 215, 1, 1000);

    PARAM(rfp_depth_mult, 99, 40, 200);
    PARAM(rfp_improving_mult, 93, 40, 200);

    PARAM(rzr_depth_mult, 235, 150, 350);

    PARAM(nmp_depth_mult, 30, 1, 58);
    PARAM(nmp_base, 167, 1, 400);
    PARAM(nmp_eval_div, 216, 50, 350);

    PARAM(hp_div, 8014, 1, 16384);

    PARAM(fp_base, 140, 1, 300);
    PARAM(fp_mult, 91, 5, 200);

    PARAM(see_cap_margin, 100, 5, 200);
    PARAM(see_quiet_margin, 73, -50, 100);

    PARAM(ext_margin, 85, 10, 250);
    PARAM(zws_margin, 55, 10, 160);

    PARAM(hp_depth_mult, 5665, 2500, 12500);
    PARAM(hp_qdiv, 5779, 1, 16384);
    PARAM(hp_cdiv, 4009, 1, 16384);
    PARAM(hbonus_margin, 72, 10, 200);

    PARAM(qfp_margin, 105, 40, 280);

    // history parameters
    PARAM(history_bonus_mult, 383, 1, 1536);
    PARAM(history_bonus_minus, -11, -500, 500);
    PARAM(max_history_bonus, 1983, 1, 4096);

    PARAM(history_malus_mult, 311, 1, 1536);
    PARAM(history_malus_minus, -8, -500, 500);
    PARAM(max_history_malus, 1857, 1, 4096);

} // namespace Astra
