/*
 * bridge.cpp - Unified bridge with CUDA support
 * Works both with and without USE_CUDA flag
 */

#include "GUI.h"
#include "model/simulation.h"
#include "layers.h"
#include "voxel.h"
#include "tomography.h"
#include "render_pipeline.h"

#include <vector>
#include <cstdint>
#include <memory>
#include <cstring>
#include <cmath>
#include <iostream>

#if defined(USE_CUDA) && !defined(CUDA_BRIDGE_CU)
#include "cuda_sim_optimized.h"
extern "C" void setCudaConstants(unsigned EL, unsigned W_USED, unsigned RMAX);
static bool useCuda = false;
#endif

namespace automaton
{
    extern unsigned EL;
    extern unsigned W_USED;
    extern unsigned FRAME;
    extern unsigned RMAX;
    extern unsigned CONVOL;
    extern unsigned SLOT1, SLOT2, SLOT3, SLOT4, SLOT5;
    extern unsigned SLOT6, SLOT7, SLOT8;
    extern unsigned DIFFUSION;
    extern unsigned RELOC;
    extern unsigned REISSUE;
    extern unsigned FLOOD;

    extern std::vector<Cell> lattice_curr;
}

// ============================================================
// Forward declarations
// ============================================================

void updateBufferCPU();

#ifdef CUDA_BRIDGE_CU
extern unsigned g_cuda_selectedLayer;
#endif

// ============================================================
// Helper for bridge_cuda.cu
// ============================================================

void updateLCenter(unsigned w, unsigned x, unsigned y, unsigned z)
{
    automaton::lcenters[w][0] = x;
    automaton::lcenters[w][1] = y;
    automaton::lcenters[w][2] = z;
}

// ============================================================
// Tomography visibility helper
// ============================================================

bool isVisibleInTomogram(unsigned x, unsigned y, unsigned z)
{
    using namespace tomography;

    // ========================================================
    // Pipeline override
    // ========================================================

    switch (gPipelineState)
    {
        case RenderPipelineState::FULL_VOLUME:
            return true;

        case RenderPipelineState::TOMOGRAPHY_XY:
            return z == tomo_z;

        case RenderPipelineState::TOMOGRAPHY_YZ:
            return x == tomo_x;

        case RenderPipelineState::TOMOGRAPHY_ZX:
            return y == tomo_y;

        default:
            break;
    }

    // ========================================================
    // Legacy fallback
    // ========================================================

    if (!tomoEnable)
        return true;

    if (!tomoEnable->getState())
        return true;

    if (tomoDirs.size() < 3)
        return true;

    if (tomoDirs[0].isSelected())
        return z == tomo_z;

    if (tomoDirs[1].isSelected())
        return x == tomo_x;

    if (tomoDirs[2].isSelected())
        return y == tomo_y;

    return true;
}

#if defined(USE_CUDA) && !defined(CUDA_BRIDGE_CU)

// ============================================================
// Cell conversion helpers
// ============================================================

static void convertCellToCellDevice(
    const automaton::Cell& src,
    ::CellDevice& dst)
{
    dst.ch  = static_cast<uint8_t>(src.ch);
    dst.pB  = src.pB ? 1 : 0;
    dst.sB  = src.sB ? 1 : 0;
    dst.a   = static_cast<uint32_t>(src.a);

    for (int i = 0; i < 4; ++i)
        dst.x[i] = static_cast<uint32_t>(src.x[i]);

    dst.r2   = static_cast<uint32_t>(src.r2);
    dst.phiB = src.phiB ? 1 : 0;
    dst.t    = static_cast<uint32_t>(src.t);
    dst.f    = static_cast<uint32_t>(src.f);

    for (int i = 0; i < 3; ++i)
        dst.c[i] = static_cast<uint32_t>(src.c[i]);

    dst.k   = static_cast<uint32_t>(src.k);
    dst.s2B = src.s2B ? 1 : 0;

    dst.kB  = src.kB ? 1 : 0;
    dst.bB  = src.bB ? 1 : 0;
    dst.hB  = src.hB ? 1 : 0;
    dst.cB  = src.cB ? 1 : 0;
}

static void convertCellDeviceToCell(
    const ::CellDevice& src,
    automaton::Cell& dst)
{
    dst.ch  = static_cast<unsigned char>(src.ch);
    dst.pB  = (src.pB != 0);
    dst.sB  = (src.sB != 0);
    dst.a   = static_cast<unsigned>(src.a);

    for (int i = 0; i < 4; ++i)
        dst.x[i] = static_cast<unsigned>(src.x[i]);

    dst.r2   = static_cast<unsigned>(src.r2);
    dst.phiB = (src.phiB != 0);
    dst.t    = static_cast<unsigned>(src.t);
    dst.f    = static_cast<unsigned>(src.f);

    for (int i = 0; i < 3; ++i)
        dst.c[i] = static_cast<unsigned>(src.c[i]);

    dst.k   = static_cast<unsigned>(src.k);
    dst.s2B = (src.s2B != 0);

    dst.kB  = (src.kB != 0);
    dst.bB  = (src.bB != 0);
    dst.hB  = (src.hB != 0);
    dst.cB  = (src.cB != 0);
}

// ============================================================
// CUDA initialization
// ============================================================

bool initializeCudaSimulation()
{
    if (!isCudaAvailable())
    {
        fprintf(stderr, "CUDA not available\n");
        return false;
    }

    if (!initCudaSimulation(
            automaton::EL,
            automaton::W_USED))
    {
        fprintf(stderr, "Failed to initialize CUDA simulation\n");
        return false;
    }

    setCudaConstants(
        automaton::EL,
        automaton::W_USED,
        automaton::RMAX);

    size_t totalCells =
        static_cast<size_t>(automaton::EL) *
        automaton::EL *
        automaton::EL *
        automaton::W_USED;

    std::vector<::CellDevice> deviceCells(totalCells);

    for (size_t i = 0; i < totalCells; ++i)
    {
        convertCellToCellDevice(
            automaton::lattice_curr[i],
            deviceCells[i]);
    }

    if (!uploadLatticeToCuda(deviceCells.data(), totalCells))
    {
        fprintf(stderr, "Failed to upload lattice\n");
        cudaCleanup();
        return false;
    }

    printf("CUDA simulation initialized\n");
    return true;
}

// ============================================================
// CUDA simulation step
// ============================================================

void cudaSimulationStepWrapper()
{
    cudaSimulationStep(
        automaton::CONVOL,
        automaton::SLOT1,
        automaton::SLOT2,
        automaton::SLOT3,
        automaton::SLOT4,
        automaton::DIFFUSION,
        automaton::SLOT5,
        automaton::SLOT6,
        automaton::SLOT7,
        automaton::SLOT8,
        automaton::RELOC,
        automaton::REISSUE,
        automaton::FLOOD,
        automaton::FRAME,
        automaton::RMAX,
        scenario
    );

    size_t totalCells =
        static_cast<size_t>(automaton::EL) *
        automaton::EL *
        automaton::EL *
        automaton::W_USED;

    std::vector<::CellDevice> deviceCells(totalCells);

    if (downloadLatticeFromCuda(deviceCells.data(), totalCells))
    {
        for (size_t i = 0; i < totalCells; ++i)
        {
            convertCellDeviceToCell(
                deviceCells[i],
                automaton::lattice_curr[i]);
        }
    }
}

// ============================================================
// CUDA voxel update
// ============================================================

void updateBufferCuda()
{
    unsigned selectedW =
        (framework::layerList && framework::layerList.get())
        ? framework::layerList->getSelected()
        : 0u;

    cudaUpdateVoxelsLayer(selectedW);

    uint32_t* gpuVoxels = getMappedVoxels();

    if (!gpuVoxels)
    {
        updateBufferCPU();
        return;
    }

    size_t idx = 0;

    for (unsigned x = 0; x < automaton::EL; ++x)
    for (unsigned y = 0; y < automaton::EL; ++y)
    for (unsigned z = 0; z < automaton::EL; ++z)
    {
        if (!isVisibleInTomogram(x, y, z))
        {
            voxels[idx++] = 0x00000000u;
        }
        else
        {
            voxels[idx++] =
                gpuVoxels[
                    (x * automaton::EL + y) *
                    automaton::EL + z];
        }
    }
}

#endif // USE_CUDA && !CUDA_BRIDGE_CU

// ============================================================
// CPU voxel update
// ============================================================


void updateBufferCPU()
{
    unsigned selectedW =
        (framework::layerList && framework::layerList.get())
        ? framework::layerList->getSelected()
        : 0u;

#ifdef CUDA_BRIDGE_CU
    g_cuda_selectedLayer = selectedW;
#endif

    size_t idx = 0;

    for (unsigned x = 0; x < automaton::EL; ++x)
    for (unsigned y = 0; y < automaton::EL; ++y)
    for (unsigned z = 0; z < automaton::EL; ++z)
    {
        if (!isVisibleInTomogram(x, y, z))
        {
            voxels[idx++] = 0x00000000u;
            continue;
        }

        const automaton::Cell& cell =
            automaton::getCell(
                automaton::lattice_curr,
                x, y, z,
                selectedW);

        uint32_t color = 0x00000000u;

        // Pulsating sphere visualization
        unsigned int pulse_r2 =
            automaton::pulse_from_time(automaton::pulse_tick);

        // Current radius marker on X axis (red dot)
        unsigned int pulse_r = (unsigned int)sqrt((double)pulse_r2);
        unsigned int markerX = automaton::CENTER + pulse_r;
        if (x == markerX &&
            y == automaton::CENTER &&
            z == automaton::CENTER)
        {
            color = makeColor(255, 80, 80, 255);   // Red
        }
        else if (cell.r2 != INF_R2 &&
            cell.r2 == pulse_r2)
        {
            // Shell at current pulsation threshold
            color = makeColor(255, 255, 80, 255);  // Yellow
        }
        else if (cell.r2 == 0)
        {
            // Center cell
            color = makeColor(80, 255, 80, 255);   // Green
        }

        voxels[idx++] = color;
    }

}

// ============================================================
// Public API
// ============================================================

#if !defined(CUDA_BRIDGE_CU)

#ifndef USE_CUDA

void automaton::updateBuffer()
{
    updateBufferCPU();
}

#else

void automaton::updateBuffer()
{
    if (useCuda)
        updateBufferCuda();
    else
        updateBufferCPU();
}

#endif

#ifdef USE_CUDA

namespace automaton
{
    bool tryEnableCuda()
    {
        if (useCuda)
            return true;

        bool ok = initializeCudaSimulation();

        if (ok)
        {
            useCuda = true;
            printf("CUDA ENABLED\n");
        }

        return ok;
    }

    void disableCuda()
    {
        if (!useCuda)
            return;

        size_t totalCells =
            static_cast<size_t>(EL) *
            EL * EL * W_USED;

        std::vector<::CellDevice> deviceCells(totalCells);

        if (downloadLatticeFromCuda(
                deviceCells.data(),
                totalCells))
        {
            for (size_t i = 0; i < totalCells; ++i)
            {
                convertCellDeviceToCell(
                    deviceCells[i],
                    lattice_curr[i]);
            }
        }

        cudaCleanup();

        useCuda = false;

        printf("CUDA DISABLED\n");
    }

    bool isCudaEnabled()
    {
        return useCuda;
    }
}

#else

namespace automaton
{
    bool tryEnableCuda()
    {
        std::cerr
            << "CUDA support not compiled."
            << std::endl;

        return false;
    }

    void disableCuda()
    {
    }

    bool isCudaEnabled()
    {
        return false;
    }
}

#endif // USE_CUDA

#endif // !CUDA_BRIDGE_CU