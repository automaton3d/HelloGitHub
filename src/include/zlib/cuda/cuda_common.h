#pragma once
#include <cstdint>

struct CellDevice
{
    uint8_t ch;
    uint8_t pB, sB;
    uint32_t a;

    uint32_t x[4];
    uint32_t r2;
    uint32_t phiB;
    uint32_t t;
    uint32_t f;

    uint32_t c[4];
    uint32_t k;
    uint32_t s2B;

    uint32_t kB, bB, hB, cB;

    uint32_t gB;
    int32_t  g[3];
};