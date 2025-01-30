#ifndef SEARCHPARAMS_H
#define SEARCHPARAMS_H

#include <string>

// #define TUNE

namespace Astra
{
    struct Param;

    inline std::vector<Param *> params;

    struct Param
    {
        std::string name;
        int value;
        int min, max;
        int step;

        Param(std::string name, int value, int min, int max, int step);

        operator int() const;
    };

    void paramsToUCI();

    void setParam(const std::string &name, int value);

    void paramsToSpsa();

    void paramsToJSON();

#ifdef TUNE
#define PARAM(name, value, min, max, step) inline Param name(#name, value, min, max, step)
#else
#define PARAM(name, value, min, max, step) constexpr int name = value
#endif

    // search parameters
    PARAM(lmr_base, 119, 80, 120, 10);
    PARAM(lmr_div, 195, 150, 300, 25);

    PARAM(asp_depth, 6, 5, 9, 1);
    PARAM(asp_window, 6, 5, 30, 5);

    PARAM(rfp_depth, 7, 7, 9, 1);
    PARAM(rfp_depth_mult, 94, 70, 120, 8);

    PARAM(rzr_depth_mult, 235, 150, 300, 20);

    PARAM(nmp_depth_mult, 31, 27, 34, 3);
    PARAM(nmp_base, 202, 160, 220, 10);
    PARAM(nmp_min, 5, 3, 6, 1);
    PARAM(nmp_depth_div, 3, 3, 15, 1);
    PARAM(nmp_div, 218, 190, 235, 8);

    PARAM(probcut_margin, 165, 130, 180, 15);

    PARAM(see_cap_margin, 95, 80, 110, 10);
    PARAM(see_quiet_margin, 51, 30, 60, 10);

    PARAM(hp_depth, 7, 5, 8, 1);

    PARAM(fp_depth, 11, 7, 11, 1);
    PARAM(fp_base, 164, 130, 200, 15);
    PARAM(fp_mult, 109, 80, 150, 10);

    PARAM(ext_margin, 89, 65, 150, 12);

    PARAM(zws_margin, 76, 60, 95, 8);

    PARAM(hp_margin, 4848, 3500, 6000, 400);
    PARAM(hp_qdiv, 7870, 7500, 9000, 600);
    PARAM(hp_cdiv, 6247, 5500, 7500, 600);
    PARAM(hbonus_margin, 79, 65, 85, 8);

    PARAM(qfp_margin, 94, 60, 150, 15);

    // history parameters
    PARAM(history_mult, 132, 130, 180, 15);
    PARAM(history_minus, 25, 10, 30, 8);
    PARAM(max_history_bonus, 1511, 1100, 1750, 100);

} // namespace Astra

#endif // SEARCHPARAMS_H
