/*
 * bridge_cuda.cu
 *
 * CUDA bridge — compiled by nvcc.
 *
 * Build assumptions
 * -----------------
 *   cl  (MSVC)  compiles bridge.cpp  *without*  /DUSE_CUDA
 *               → provides: isVisibleInTomogram, updateBufferCPU,
 *                           automaton::updateBuffer (CPU-only stub),
 *                           automaton::tryEnableCuda (stub), etc.
 *
 *   nvcc        compiles this file    *with*    -DUSE_CUDA
 *               → provides: CUDA management (init, step, enable/disable),
 *                           updateBufferCuda, real automaton::updateBuffer,
 *                           real automaton::tryEnableCuda, etc.
 *
 * OpenGL / GUI headers (glad, GLFW, glm, layers.h …) are NOT included
 * here because nvcc cannot reliably compile them.  Symbols that live
 * behind those headers are declared `extern` instead.
 */

#include "cuda_runtime.h"
#include "cuda_sim_optimized.h"   // isCudaAvailable, cudaSimulationStep, …
#include "cuda/cuda_common.h"     // CellDevice
#include "model/simulation.h"     // automaton::Cell, getCell (no OpenGL)
#include "config.h"

#include <cstdio>
#include <vector>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <chrono>

// Implemented in cuda_constants.cu
extern "C" void setCudaConstants(unsigned EL, unsigned W_USED, unsigned RMAX);

// -----------------------------------------------------------------
// Extern declarations for symbols defined in .cpp files that CAN
// include the full GUI headers.
// -----------------------------------------------------------------
extern std::vector<unsigned int> voxels;           // globals.cpp
extern bool isVisibleInTomogram(unsigned x,        // bridge.cpp
                                unsigned y,
                                unsigned z);
extern void updateBufferCPU();                     // bridge.cpp
extern void updateLCenter(unsigned w, unsigned x,  // bridge.cpp
                          unsigned y, unsigned z);
extern unsigned long long timer;                   // globals.cpp
extern std::mutex timerMutex;                      // globals.cpp
extern std::mutex gVoxelBufferMutex;               // core.cpp

// From voxel.h — colour packing helper (no OpenGL dependency)
inline unsigned int makeColor(unsigned char r, unsigned char g,
                              unsigned char b, unsigned char a = 255)
{
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// -----------------------------------------------------------------
// Automaton namespace externs (defined in globals.cpp / initSim.cpp)
// -----------------------------------------------------------------
namespace automaton
{
    extern unsigned EL;
    extern unsigned W_USED;
    extern unsigned FRAME;
    extern unsigned RMAX;
    extern unsigned CENTER;
    extern unsigned CONVOL;
    extern unsigned GSLOT_X, GSLOT_Y, GSLOT_Z;
    extern unsigned SLOT1, SLOT2, SLOT3, SLOT4, SLOT5;
    extern unsigned SLOT6, SLOT7, SLOT8;
    extern unsigned DIFFUSION;
    extern unsigned RELOC;
    extern unsigned REISSUE;
    extern unsigned FLOOD;
    extern std::vector<Cell> lattice_curr;
}

// -----------------------------------------------------------------
// Global selected-layer index.
// Updated by the GUI (layers / bridge.cpp).  Defaults to 0.
// Declared extern in cuda_bridge_shared.h (or wherever convenient)
// so that the GUI code can write to it.
// -----------------------------------------------------------------
unsigned g_cuda_selectedLayer = 0;

// =================================================================
//  USE_CUDA section — only compiled when -DUSE_CUDA is passed
// =================================================================
#ifdef USE_CUDA

static bool useCuda = false;

// -----------------------------------------------------------------
// Cell ↔ CellDevice conversion
// -----------------------------------------------------------------
static void convertCellToCellDevice(const automaton::Cell& src,
                                    CellDevice& dst)
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
    dst.c[3] = 0;

    dst.k   = static_cast<uint32_t>(src.k);
    dst.s2B = src.s2B ? 1 : 0;
    dst.kB  = src.kB  ? 1 : 0;
    dst.bB  = src.bB  ? 1 : 0;
    dst.hB  = src.hB  ? 1 : 0;
    dst.cB  = src.cB  ? 1 : 0;

    dst.gB  = src.gB  ? 1 : 0;
    for (int i = 0; i < 3; ++i)
        dst.g[i] = static_cast<int32_t>(src.g[i]);
}

static void convertCellDeviceToCell(const CellDevice& src,
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

    dst.gB  = (src.gB != 0);
    for (int i = 0; i < 3; ++i)
        dst.g[i] = static_cast<int>(src.g[i]);
}

// -----------------------------------------------------------------
// CUDA initialization
// -----------------------------------------------------------------
bool initializeCudaSimulation()
{
    if (!isCudaAvailable()) {
        fprintf(stderr, "CUDA not available - falling back to CPU\n");
        return false;
    }

    if (!initCudaSimulation(automaton::EL, automaton::W_USED)) {
        fprintf(stderr, "Failed to initialize CUDA simulation\n");
        return false;
    }

    setCudaConstants(automaton::EL, automaton::W_USED, automaton::RMAX);

    size_t totalCells = (size_t)automaton::EL * automaton::EL *
                        automaton::EL * automaton::W_USED;
    std::vector<CellDevice> deviceCells(totalCells);

    printf("Converting %zu cells to device format...\n", totalCells);
    for (size_t i = 0; i < totalCells; i++)
        convertCellToCellDevice(automaton::lattice_curr[i], deviceCells[i]);

    printf("Uploading lattice to CUDA...\n");
    if (!uploadLatticeToCuda(deviceCells.data(), totalCells)) {
        fprintf(stderr, "Failed to upload lattice to CUDA\n");
        cudaCleanup();
        return false;
    }

    printf("CUDA simulation initialized successfully\n");
    return true;
}

// -----------------------------------------------------------------
// Helper: download GPU lattice → lattice_curr + update lcenters
// -----------------------------------------------------------------
static void downloadAndSync()
{
    size_t totalCells = (size_t)automaton::EL * automaton::EL *
                        automaton::EL * automaton::W_USED;
    std::vector<CellDevice> deviceCells(totalCells);

    if (downloadLatticeFromCuda(deviceCells.data(), totalCells)) {
        for (size_t i = 0; i < totalCells; i++)
            convertCellDeviceToCell(deviceCells[i], automaton::lattice_curr[i]);

        for (unsigned w = 0; w < automaton::W_USED; ++w)
        for (unsigned x = 0; x < automaton::EL; ++x)
        for (unsigned y = 0; y < automaton::EL; ++y)
        for (unsigned z = 0; z < automaton::EL; ++z) {
            const automaton::Cell& cell =
                automaton::getCell(automaton::lattice_curr, x, y, z, w);
            if (cell.r2 == 0)
                updateLCenter(w, x, y, z);
        }
    } else {
        fprintf(stderr, "Warning: Failed to download lattice from CUDA\n");
    }
}

// -----------------------------------------------------------------
// Simulation step wrapper
// -----------------------------------------------------------------
void cudaSimulationStepWrapper()
{
    for (unsigned tick = 0; tick < automaton::FRAME; tick++) {
        // Check if a delay is active for the current phase.
        // tick == input k (we always start a batch at k=0).
        bool wantDelay = false;
        int  delay_ms  = 0;

        if (automaton::convol_delay && tick < automaton::CONVOL) {
            wantDelay = true;  delay_ms = 120;
        } else if (automaton::diffuse_delay &&
                   tick >= automaton::CONVOL && tick < automaton::DIFFUSION) {
            wantDelay = true;  delay_ms = 80;
        } else if (automaton::reloc_delay &&
                   tick >= automaton::DIFFUSION && tick < automaton::RELOC) {
            wantDelay = true;  delay_ms = 120;
        }

        cudaSimulationStep(
            automaton::CONVOL,
            automaton::GSLOT_Z,
            automaton::SLOT1, automaton::SLOT2,
            automaton::SLOT3, automaton::SLOT4,
            automaton::DIFFUSION,
            automaton::SLOT5, automaton::SLOT6,
            automaton::SLOT7, automaton::SLOT8,
            automaton::RELOC,
            automaton::REISSUE,
            automaton::FLOOD,
            automaton::FRAME,
            automaton::RMAX,
            gConfig.simulation.scenario
        );

        if (wantDelay) {
            // Download so the 3D scene reflects the current state
            downloadAndSync();
            {
                std::lock_guard<std::mutex> lock(gVoxelBufferMutex);
                updateBufferCPU();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }

        // Advance timer each tick (except the last — core.cpp adds 1)
        if (tick < automaton::FRAME - 1) {
            std::lock_guard<std::mutex> lock(timerMutex);
            timer++;
        }
    }

    // Advance pulsation tick (mirrors CPU's pulse_tick++ in update_lattice_cpu)
    automaton::pulse_tick++;

    // Final download (always needed)
    downloadAndSync();

    // Diagnostic: print t and effective_t after each light frame
    {
        const automaton::Cell& center =
            automaton::getCell(automaton::lattice_curr,
                               automaton::CENTER, automaton::CENTER,
                               automaton::CENTER, 0);
        unsigned eff = automaton::effective_t(center.t);
        printf("[GPU] t=%u  eff_t=%u  RMAX=%u  center.gB=%d\n",
               center.t, eff, automaton::RMAX, (int)center.gB);
    }
}

// -----------------------------------------------------------------
// Voxel buffer — CUDA-accelerated version
// Uses g_cuda_selectedLayer instead of framework::layerList (no GUI dep)
// -----------------------------------------------------------------
void updateBufferCuda()
{
    unsigned selectedW = g_cuda_selectedLayer;

    // Try GPU-generated voxels first
    cudaUpdateVoxelsLayer(selectedW);
    uint32_t* gpuVoxels = getMappedVoxels();

    if (gpuVoxels != nullptr) {
        size_t idx = 0;
        for (unsigned x = 0; x < automaton::EL; ++x)
        for (unsigned y = 0; y < automaton::EL; ++y)
        for (unsigned z = 0; z < automaton::EL; ++z)
        {
            if (!isVisibleInTomogram(x, y, z))
                voxels[idx++] = 0x00000000u;
            else
                voxels[idx++] = gpuVoxels[(x * automaton::EL + y) * automaton::EL + z];
        }
    } else {
        // Fallback to CPU rendering
        updateBufferCPU();
    }
}

// -----------------------------------------------------------------
// Public API — dispatch to CPU or CUDA
// -----------------------------------------------------------------
void automaton::updateBuffer()
{
    if (useCuda)
        updateBufferCuda();
    else
        updateBufferCPU();
}

// -----------------------------------------------------------------
// Enable / disable CUDA at runtime
// -----------------------------------------------------------------
namespace automaton
{
    bool tryEnableCuda()
    {
        if (useCuda) {
            printf("CUDA already enabled\n");
            return true;
        }

        bool success = initializeCudaSimulation();
        if (success) {
            useCuda = true;
            printf("CUDA acceleration ENABLED\n");
        } else {
            printf("CUDA acceleration not available - using CPU\n");
        }
        return success;
    }

    void disableCuda()
    {
        if (!useCuda) {
            printf("CUDA already disabled\n");
            return;
        }

        size_t totalCells = (size_t)EL * EL * EL * W_USED;
        std::vector<CellDevice> deviceCells(totalCells);

        printf("Downloading final state from GPU...\n");
        if (downloadLatticeFromCuda(deviceCells.data(), totalCells)) {
            for (size_t i = 0; i < totalCells; i++)
                convertCellDeviceToCell(deviceCells[i], lattice_curr[i]);
            printf("State downloaded successfully\n");
        } else {
            fprintf(stderr, "Warning: Failed to download final state\n");
        }

        cudaCleanup();
        useCuda = false;
        printf("CUDA acceleration DISABLED\n");
    }

    bool isCudaEnabled()
    {
        return useCuda;
    }
}

#endif // USE_CUDA
