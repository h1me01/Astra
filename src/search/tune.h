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
    PARAM(stability_base, 135, 100, 200);
    PARAM(stability_mult, 74, 0, 250);

    PARAM(results_base, 76, 0, 160);
    PARAM(results_mult1, 10, 0, 27);
    PARAM(results_mult2, 26, 0, 45);
    PARAM(results_min, 75, 0, 150);
    PARAM(results_max, 144, 50, 220);

    PARAM(node_mult, 197, 120, 260);
    PARAM(node_base, 56, 10, 90);

    PARAM(lmr_base, 106, 40, 200);
    PARAM(lmr_div, 257, 150, 500);

    PARAM(static_h_mult, -46, -500, -1);
    PARAM(static_h_min, 39, 1, 1000);
    PARAM(static_h_max, 180, 1, 1000);

    PARAM(rfp_depth_mult, 102, 40, 200);
    PARAM(rfp_improving_mult, 93, 40, 200);

    PARAM(rzr_depth_mult, 238, 150, 350);

    PARAM(nmp_depth_mult, 27, 27, 34);
    PARAM(nmp_base, 184, 160, 220);
    PARAM(nmp_eval_div, 216, 100, 400);

    PARAM(hp_div, 8266, 1, 16384);

    PARAM(fp_base, 163, 40, 300);
    PARAM(fp_mult, 95, 40, 200);

    PARAM(see_cap_margin, 94, 80, 110);
    PARAM(see_quiet_margin, 51, 30, 60);

    PARAM(ext_margin, 88, 10, 250);
    PARAM(zws_margin, 56, 10, 160);

    PARAM(hp_depth_mult, 5567, 2500, 12500);
    PARAM(hp_qdiv, 6213, 1, 16384);
    PARAM(hp_cdiv, 4421, 1, 16384);
    PARAM(hbonus_margin, 70, 10, 200);

    PARAM(qfp_margin, 103, 40, 280);

    // history parameters
    PARAM(history_mult, 385, 1, 1536);
    PARAM(history_minus, -66, -500, 500);
    PARAM(max_history_bonus, 1942, 1, 4096);

} // namespace Astra
