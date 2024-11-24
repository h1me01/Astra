#ifndef PVTABLE_H
#define PVTABLE_H

#include "../chess/types.h"

namespace Astra
{
    struct PVLine
    {
        Move pv[MAX_PLY + 1];
        uint8_t length;

        Move& operator()(int depth) { return pv[depth]; }
        Move operator()(int depth) const { return pv[depth]; }
    };

    struct PVTable
    {
        PVLine pvs[MAX_PLY + 1];

        PVLine& operator()(const int depth) { return pvs[depth]; }
        PVLine operator()(int depth) const { return pvs[depth]; }

        PVTable()
        {
            for (auto& pvLine : pvs)
                pvLine.length = 0;
        }

        void updatePV(int ply, Move m)
        {
            PVLine& current_pv = pvs[ply];
            current_pv(ply) = m;

            const uint8_t next_length = pvs[ply + 1].length;
            for (int next_ply = ply + 1; next_ply < next_length; next_ply++)
                current_pv(next_ply) = pvs[ply + 1](next_ply);
            current_pv.length = next_length;
        }
    };

} // namespace Astra

#endif //PVTABLE_H
