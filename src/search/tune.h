#pragma once

#include <string>

 #define TUNE

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
    PARAM(stability_mult, 73, 0, 250);

    PARAM(results_base, 66, 0, 160);
    PARAM(results_mult1, 10, 0, 27);
    PARAM(results_mult2, 26, 0, 45);
    PARAM(results_min, 68, 0, 150);
    PARAM(results_max, 136, 50, 220);

    PARAM(node_mult, 188, 120, 260);
    PARAM(node_base, 48, 10, 90);

    PARAM(lmr_base, 105, 40, 200);
    PARAM(lmr_div, 287, 150, 500);

    PARAM(static_h_mult, -60, -500, -1);
    PARAM(static_h_min, 57, 1, 1000);
    PARAM(static_h_max, 224, 1, 1000);

    PARAM(rfp_depth_mult, 102, 40, 200);
    PARAM(rfp_improving_mult, 89, 40, 200);

    PARAM(rzr_depth_mult, 237, 150, 350);

    PARAM(nmp_depth_mult, 29, 1, 58);
    PARAM(nmp_base, 155, 1, 400);
    PARAM(nmp_eval_div, 216, 50, 350);

    PARAM(hp_div, 7825, 1, 16384);

    PARAM(fp_base, 139, 1, 300);
    PARAM(fp_mult, 92, 5, 200);

    PARAM(see_cap_margin, 97, 5, 200);
    PARAM(see_quiet_margin, 77, -50, 100);

    PARAM(ext_margin, 91, 10, 250);
    PARAM(zws_margin, 59, 10, 160);

    PARAM(hp_depth_mult, 6094, 2500, 12500);
    PARAM(hp_qdiv, 5645, 1, 16384);
    PARAM(hp_cdiv, 3892, 1, 16384);
    PARAM(hbonus_margin, 64, 10, 200);

    PARAM(qfp_margin, 108, 40, 280);

    PARAM(history_bonus_mult, 416, 1, 1536);
    PARAM(history_bonus_minus, -22, -500, 500);
    PARAM(max_history_bonus, 2106, 1, 4096);

    PARAM(history_malus_mult, 342, 1, 1536);
    PARAM(history_malus_minus, -3, -500, 500);
    PARAM(max_history_malus, 1874, 1, 4096);

} // namespace Astra
