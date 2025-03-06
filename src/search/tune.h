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
    PARAM(stability_base, 132, 100, 200);
    PARAM(stability_mult, 74, 0, 250);

    PARAM(results_base, 74, 0, 160);
    PARAM(results_mult1, 10, 0, 27);
    PARAM(results_mult2, 26, 0, 45);
    PARAM(results_min, 77, 0, 150);
    PARAM(results_max, 144, 50, 220);

    PARAM(node_mult, 199, 120, 260);
    PARAM(node_base, 54, 10, 90);

    PARAM(lmr_base, 101, 40, 200);
    PARAM(lmr_div, 217, 150, 500);

    PARAM(rfp_depth_mult, 107, 40, 200);
    PARAM(rfp_improving_mult, 94, 40, 200);

    PARAM(rzr_depth_mult, 230, 150, 350);

    PARAM(nmp_depth_mult, 27, 27, 34);
    PARAM(nmp_base, 183, 160, 220);
    PARAM(nmp_eval_div, 222, 100, 400);

    PARAM(see_cap_margin, 96, 80, 110);
    PARAM(see_quiet_margin, 50, 30, 60);

    PARAM(fp_base, 158, 40, 300);
    PARAM(fp_mult, 98, 40, 200);

    PARAM(ext_margin, 96, 10, 250);
    PARAM(zws_margin, 59, 10, 160);

    PARAM(hp_depth_mult, 5855, 2500, 12500);
    PARAM(hp_qdiv, 6147, 1, 16384);
    PARAM(hp_cdiv, 4705, 1, 16384);
    PARAM(hbonus_margin, 73, 10, 200);

    PARAM(qfp_margin, 97, 40, 280);

    // history parameters
    PARAM(history_mult, 341, 1, 1536);
    PARAM(history_minus, -14, -500, 500);
    PARAM(max_history_bonus, 1680, 1, 4096);

} // namespace Astra
