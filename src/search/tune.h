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
    PARAM(lmr_base, 102, 80, 120, 8);
    PARAM(lmr_div, 188, 150, 200, 8);

    PARAM(asp_depth, 7, 5, 9, 1);
    PARAM(asp_window, 7, 5, 30, 5);

    PARAM(rzr_depth_mult, 195, 150, 250, 15);
    PARAM(rfp_depth_mult, 84, 70, 110, 6);

    PARAM(nmp_min, 4, 3, 6, 1);
    PARAM(nmp_depth_div, 5, 3, 15, 1);
    PARAM(nmp_div, 228, 190, 235, 8);

    PARAM(probcut_margin, 168, 130, 180, 15);

    PARAM(see_cap_margin, 93, 80, 110, 6);
    PARAM(see_quiet_margin, 49, 30, 60, 6);

    PARAM(fp_base, 160, 120, 180, 8);
    PARAM(fp_mult, 90, 80, 150, 8);

    PARAM(ext_margin, 120, 65, 150, 12);

    PARAM(hp_margin, 4825, 3500, 5000, 400);
    PARAM(hp_div, 8421, 7000, 8500, 400);
    PARAM(hbonus_margin, 75, 65, 80, 5);

    PARAM(qfp_margin, 87, 60, 150, 15);

    // history parameters
    PARAM(history_mult, 144, 130, 180, 8);
    PARAM(history_minus, 48, 10, 50, 10);
    PARAM(max_history_bonus, 1636, 1100, 1750, 50);

} // namespace Astra

#endif // SEARCHPARAMS_H
