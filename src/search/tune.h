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
    PARAM(lmr_base, 96, 80, 120, 8);
    PARAM(lmr_div, 185, 150, 200, 8);

    PARAM(asp_depth, 7, 5, 9, 1);
    PARAM(asp_window, 10, 5, 30, 5);

    PARAM(rzr_depth_mult, 198, 150, 250, 15);
    PARAM(rfp_depth_mult, 84, 70, 110, 6);

    PARAM(nmp_min, 4, 3, 6, 1);
    PARAM(nmp_depth_div, 4, 3, 15, 1);
    PARAM(nmp_div, 218, 190, 235, 8);

    PARAM(probcut_margin, 168, 130, 180, 15);

    PARAM(see_cap_margin, 91, 80, 110, 6);
    PARAM(see_quiet_margin, 53, 30, 60, 6);

    PARAM(fp_base, 163, 120, 180, 8);
    PARAM(fp_mult, 93, 80, 150, 8);

    PARAM(ext_margin, 115, 45, 150, 12);

    PARAM(hp_margin, 4770, 3500, 5000, 400);
    PARAM(hp_div, 8439, 7000, 8500, 400);
    PARAM(hbonus_margin, 76, 65, 80, 5);

    PARAM(qfp_margin, 88, 60, 150, 15);

    // history parameters
    PARAM(history_mult, 148, 130, 180, 8);
    PARAM(history_minus, 49, 10, 50, 10);
    PARAM(max_history_bonus, 1594, 1100, 1750, 50);

} // namespace Astra

#endif // SEARCHPARAMS_H
