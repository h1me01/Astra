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

    PARAM(stability_base, 128, 100, 200);
    PARAM(stability_mult, 71, 0, 250);

    PARAM(results_base, 64, 0, 160);
    PARAM(results_mult1, 9, 0, 27);
    PARAM(results_mult2, 26, 0, 45);
    PARAM(results_min, 68, 0, 150);
    PARAM(results_max, 139, 50, 220);

    PARAM(node_mult, 188, 120, 260);
    PARAM(node_base, 48, 10, 90);

    PARAM(lmr_base, 104, 40, 200);
    PARAM(lmr_div, 295, 150, 500);

    PARAM(static_h_mult, -58, -500, -1);
    PARAM(static_h_min, 41, 1, 1000);
    PARAM(static_h_max, 212, 1, 1000);

    PARAM(rfp_depth_mult, 99, 40, 200);
    PARAM(rfp_improving_mult, 83, 40, 200);

    PARAM(rzr_depth_mult, 243, 150, 350);

    PARAM(nmp_depth_mult, 29, 1, 58);
    PARAM(nmp_base, 143, 1, 400);
    PARAM(nmp_eval_div, 208, 50, 350);

    PARAM(hp_div, 7889, 1, 16384);

    PARAM(fp_base, 138, 1, 300);
    PARAM(fp_mult, 90, 5, 200);

    PARAM(see_cap_margin, 96, 5, 200);
    PARAM(see_quiet_margin, 79, -50, 100);

    PARAM(ext_margin, 92, 10, 250);
    PARAM(zws_margin, 55, 10, 160);

    PARAM(hp_depth_mult, 6348, 2500, 12500);
    PARAM(hp_qdiv, 6336, 1, 16384);
    PARAM(hp_cdiv, 3805, 1, 16384);
    PARAM(hbonus_margin, 55, 10, 200);

    PARAM(qfp_margin, 104, 40, 280);

    PARAM(history_bonus_mult, 405, 1, 1536);
    PARAM(history_bonus_minus, -22, -500, 500);
    PARAM(max_history_bonus, 2276, 1, 4096);

    PARAM(history_malus_mult, 387, 1, 1536);
    PARAM(history_malus_minus, 21, -500, 500);
    PARAM(max_history_malus, 1842, 1, 4096);

} // namespace Astra
