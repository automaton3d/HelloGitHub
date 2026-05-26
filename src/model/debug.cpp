/*
 * debug.cpp
 *
 *  Created on: 9 de dez. de 2025
 *      Author: Alexandre
 */
#include <random>
#include <vector>
#include <algorithm>
#include <array>

#include "model/simulation.h"

namespace automaton
{
using namespace framework;

void relocateAllWRandom()
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<unsigned> dist(0, EL - 1);

    // Temporary buffer for the entire lattice
    std::vector<Cell> temp(BLOCK);

    // ✅ Copy ONCE at the start
    std::copy(lattice_curr.begin(), lattice_curr.begin() + BLOCK, temp.begin());

    for (unsigned w = 0; w < W_USED; ++w)
    {
        unsigned dx = dist(rng) % EL;
        unsigned dy = dist(rng) % EL;
        unsigned dz = dist(rng) % EL;

        for (unsigned x = 0; x < EL; ++x)
        {
            for (unsigned y = 0; y < EL; ++y)
            {
                for (unsigned z = 0; z < EL; ++z)
                {
                    const Cell& src = getCell(lattice_curr, x, y, z, w);

                    unsigned nx = (x + dx) % EL;
                    unsigned ny = (y + dy) % EL;
                    unsigned nz = (z + dz) % EL;

                    Cell& dst = getCell(temp, nx, ny, nz, w);

                    dst.pB   = src.pB;
                    dst.sB   = src.sB;
                    dst.a    = src.a;
                    dst.phiB = src.phiB;
                    dst.t    = src.t;
                    dst.f    = src.f;
                    dst.r2   = src.r2;
                    dst.s2B  = src.s2B;
                    dst.kB   = src.kB;
                    dst.bB   = src.bB;
                    dst.hB   = src.hB;
                    dst.cB   = src.cB;
                }
            }
        }
    }

    // ✅ Commit ONCE at the end
    std::copy(temp.begin(), temp.begin() + BLOCK, lattice_curr.begin());
}

}
