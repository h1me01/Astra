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
    PARAM(lmr_base, 119, 80, 120, 8);
    PARAM(lmr_div, 191, 150, 200, 8);

    PARAM(asp_depth, 6, 5, 9, 1);
    PARAM(asp_window, 15, 5, 30, 5);

    PARAM(rfp_depth, 7, 7, 9, 1);
    PARAM(rfp_depth_mult, 93, 70, 110, 6);

    PARAM(rzr_depth_mult, 228, 150, 250, 15);

    PARAM(nmp_depth_mult, 32, 27, 34, 1);
    PARAM(nmp_base, 185, 160, 200, 8);
    PARAM(nmp_min, 5, 3, 6, 1);
    PARAM(nmp_depth_div, 3, 3, 15, 1);
    PARAM(nmp_div, 216, 190, 235, 8);

    PARAM(probcut_margin, 177, 130, 180, 15);

    PARAM(see_cap_margin, 93, 80, 110, 6);
    PARAM(see_quiet_margin, 48, 30, 60, 6);

    PARAM(hp_depth, 7, 5, 8, 1);

    PARAM(fp_depth, 10, 7, 11, 1);
    PARAM(fp_base, 166, 120, 180, 8);
    PARAM(fp_mult, 102, 80, 150, 8);

    PARAM(ext_margin, 97, 65, 150, 12);

    PARAM(zws_margin, 75, 60, 85, 5);

    PARAM(hp_margin, 4560, 3500, 6000, 400);
    PARAM(hp_qdiv, 8105, 7500, 9000, 400);
    PARAM(hp_cdiv, 6435, 5500, 7500, 400);
    PARAM(hbonus_margin, 71, 65, 80, 5);

    PARAM(qfp_margin, 92, 60, 150, 15);

    // history parameters
    PARAM(history_mult, 143, 130, 180, 8);
    PARAM(history_minus, 17, 10, 30, 10);
    PARAM(max_history_bonus, 1560, 1100, 1750, 50);

} // namespace Astra

#endif // SEARCHPARAMS_H
