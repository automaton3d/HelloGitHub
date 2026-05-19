// cuda_automaton.cu - CUDA implementation with FULL CA logic
// Unified version: all constants and host functions are defined here.

#pragma nv_diag_suppress 177

#include "model/simulation.h"
#include "cuda_sim_optimized.h"
#include <cuda_runtime.h>
#include "cuda_constants.h" 
#include <iostream>
#include <algorithm>
#include "config.h"

// ===================================================================
// Constant memory — defined HERE so they are in the same compilation
// unit as the kernels (required without -dc separate compilation).
// ===================================================================
__constant__ unsigned dev_EL;
__constant__ unsigned dev_W_USED;
__constant__ unsigned dev_RMAX;
__constant__ unsigned dev_CENTER;
__device__   int      dev_ctrl;

// Global device pointers (defined here, used by bridge)
::CellDevice* d_lattice_curr = nullptr;
::CellDevice* d_lattice_draft = nullptr;
::CellDevice* d_lattice_mirror = nullptr;

static bool g_cuda_initialized = false;

#define CUDA_CHECK(call) \
    { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            fprintf(stderr, "CUDA error at %s:%d: %s (code %d)\n", \
                    __FILE__, __LINE__, cudaGetErrorString(err), err); \
            fflush(stderr); \
            return false; \
        } \
    }

#define CUDA_CHECK_VOID(call) \
    { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            fprintf(stderr, "CUDA error at %s:%d: %s (code %d)\n", \
                    __FILE__, __LINE__, cudaGetErrorString(err), err); \
            fflush(stderr); \
        } \
    }

// ===================================================================
// DEVICE HELPER FUNCTIONS
// ===================================================================
static __device__ inline ::CellDevice& d_getCell(::CellDevice* lattice, int x, int y, int z, int w)
{
    return lattice[(((x * dev_EL + y) * dev_EL + z) * dev_W_USED) + w];
}

// Spherical antipodal wrap for spatial coordinates — matches CPU's spherical_wrap()
static __device__ void dev_spherical_wrap(int& x, int& y, int& z)
{
    double R = dev_EL / 2.0;
    double C = (dev_EL - 1) / 2.0;

    double dx = x - C;
    double dy = y - C;
    double dz = z - C;

    double r = sqrt(dx*dx + dy*dy + dz*dz);

    // Inside or on surface: just clamp
    if (r <= R + 0.5) {
        if (x < 0) x = 0;
        if (x >= (int)dev_EL) x = (int)dev_EL - 1;
        if (y < 0) y = 0;
        if (y >= (int)dev_EL) y = (int)dev_EL - 1;
        if (z < 0) z = 0;
        if (z >= (int)dev_EL) z = (int)dev_EL - 1;
        return;
    }

    // Outside sphere: project to surface then invert (antipodal)
    double scale = R / r;

    int anx = (int)round(dx * scale);
    int any = (int)round(dy * scale);
    int anz = (int)round(dz * scale);

    // Antipodal inversion (through the center)
    anx = -anx;
    any = -any;
    anz = -anz;

    x = anx + (int)round(C);
    y = any + (int)round(C);
    z = anz + (int)round(C);

    // Final bounds clamping
    if (x < 0) x = 0;
    if (x >= (int)dev_EL) x = (int)dev_EL - 1;
    if (y < 0) y = 0;
    if (y >= (int)dev_EL) y = (int)dev_EL - 1;
    if (z < 0) z = 0;
    if (z >= (int)dev_EL) z = (int)dev_EL - 1;
}

// Neighbor with spherical antipodal wrapping for spatial + periodic for W
static __device__ ::CellDevice d_getNeighbor(::CellDevice* d_curr_lattice,
                                           unsigned x_curr, 
                                           unsigned y_curr, 
                                           unsigned z_curr, 
                                           unsigned w_curr, 
                                           int i) 
{
    static const int disp[8][4] = {
        {+1, 0, 0, 0}, {-1, 0, 0, 0},
        { 0,+1, 0, 0}, { 0,-1, 0, 0},
        { 0, 0,+1, 0}, { 0, 0,-1, 0},
        { 0, 0, 0,+1}, { 0, 0, 0,-1}
    };

    int nx = (int)x_curr + disp[i][0];
    int ny = (int)y_curr + disp[i][1];
    int nz = (int)z_curr + disp[i][2];
    int nw = (int)w_curr + disp[i][3];

    // Antipodal spherical wrapping for spatial coordinates
    dev_spherical_wrap(nx, ny, nz);

    // Additional integer bounds guarantee
    if (nx < 0) nx = 0;
    if (nx >= (int)dev_EL) nx = (int)dev_EL - 1;
    if (ny < 0) ny = 0;
    if (ny >= (int)dev_EL) ny = (int)dev_EL - 1;
    if (nz < 0) nz = 0;
    if (nz >= (int)dev_EL) nz = (int)dev_EL - 1;

    // Periodic wrapping for W dimension
    nw = nw % (int)dev_W_USED;
    if (nw < 0) nw += (int)dev_W_USED;

    return d_getCell(d_curr_lattice, nx, ny, nz, nw);
}

#define ZERO_C(c) (!(c[0] | c[1] | c[2]))

// Charge-bit access for CellDevice (mirrors CPU)
#define DEV_W1(cell)  ((cell).ch & 0x20)
#define DEV_W0(cell)  ((cell).ch & 0x10)
#define DEV_Q(cell)   ((cell).ch & 0x08)
#define DEV_C2(cell)  ((cell).ch & 0x04)
#define DEV_C1(cell)  ((cell).ch & 0x02)
#define DEV_C0(cell)  ((cell).ch & 0x01)
#define DEV_COLOR(cell)     ((cell).ch & COLOR_MASK)
#define DEV_ANTICOLOR(cell) ((~(cell).ch) & COLOR_MASK)

// Color neutrality test (mirrors CPU neutralColor)
static __device__ inline bool dev_neutralColor(const ::CellDevice& a, const ::CellDevice& b)
{
    int color_a = a.ch & 0x07;
    int color_b = b.ch & 0x07;
    return (color_a ^ color_b) == 0x07;
}

// Weak neutrality test (mirrors CPU neutralWeak)
static __device__ inline bool dev_neutralWeak(const ::CellDevice& a, const ::CellDevice& b)
{
    int weak_a = (a.ch >> 3) & 0x03;
    int weak_b = (b.ch >> 3) & 0x03;
    return (weak_a ^ weak_b) == 0x03;
}

// Simple hash PRNG (for convolute1 random c[] values)
__device__ inline unsigned dev_hash_random(unsigned seed, unsigned mod)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    seed *= 2654435761u;
    return seed % mod;
}

// Device helper: effective wavefront radius (triangle wave)
static __device__ inline unsigned dev_effective_t(unsigned t) {
    unsigned period = 2 * dev_RMAX;
    unsigned raw = t % period;
    return (raw <= dev_RMAX) ? raw : (2 * dev_RMAX - raw);
}

// ===================================================================
// DEVICE CONVOLUTE FUNCTIONS (mirror convolutes.cpp)
// ===================================================================

__device__ inline void dev_convolute0(::CellDevice& /*curr*/, ::CellDevice& /*draft*/,
                                      ::CellDevice& /*mirror*/, unsigned /*w*/, unsigned /*tid*/)
{
    // Scenario 0: no interaction
}

__device__ inline void dev_convolute1(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& /*mirror*/, unsigned w, unsigned tid)
{
    if (curr.t == curr.d && curr.t == dev_RMAX / 2 && w == 0)
    {
        int old = atomicExch(&dev_ctrl, 0);
        if (old == 1)
        {
            draft.c[0] = dev_hash_random(tid * 3 + 1, dev_EL);
            draft.c[1] = dev_hash_random(tid * 3 + 2, dev_EL);
            draft.c[2] = dev_hash_random(tid * 3 + 3, dev_EL);
        }
    }
}

__device__ inline void dev_convolute2(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& /*mirror*/, unsigned w, unsigned /*tid*/)
{
    if (curr.t == curr.d && curr.t == dev_RMAX / 2 && w == 0)
    {
        int old = atomicExch(&dev_ctrl, 0);
        if (old == 1) draft.a = dev_W_USED;
    }
}

__device__ inline void dev_convolute3(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& /*mirror*/, unsigned w, unsigned /*tid*/)
{
    if (curr.t == curr.d && curr.t == dev_RMAX / 2 && w == 0)
    {
        int old = atomicExch(&dev_ctrl, 0);
        if (old == 1)
        {
            draft.a = dev_W_USED;
            draft.cB = 1;
        }
    }
}

__device__ inline void dev_convolute4(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& /*mirror*/, unsigned w, unsigned /*tid*/)
{
    if (curr.t == curr.d && curr.t == dev_RMAX / 2 && curr.sB && w == 0)
    {
        int old = atomicExch(&dev_ctrl, 0);
        if (old == 1) draft.hB = 1;
    }
}

__device__ inline void dev_convolute5(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& /*mirror*/, unsigned w, unsigned /*tid*/)
{
    if (curr.t == curr.d && curr.t == dev_RMAX / 2 && curr.pB && w == 0 &&
        !curr.cB && curr.a != dev_W_USED)
    {
        int old = atomicExch(&dev_ctrl, 0);
        if (old == 1)
        {
            draft.c[0] = curr.x[0];
            draft.c[1] = curr.x[1];
            draft.c[2] = curr.x[2];
            draft.cB = 1;
            draft.a = dev_W_USED;
        }
    }
}

__device__ inline void dev_convolute6(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& mirror, unsigned /*w*/, unsigned /*tid*/)
{
    // Cells awaken?
    if (curr.t == curr.d && mirror.t == mirror.d)
    {
        // Test superposition
        if (curr.x[0] == mirror.x[0] &&
            curr.x[1] == mirror.x[1] &&
            curr.x[2] == mirror.x[2])
        {
            // Test dispersion
            if (curr.a != dev_W_USED &&
                DEV_W1(curr) != DEV_W1(mirror) &&
                !curr.cB &&
                curr.t == dev_RMAX / 2)
            {
                if (curr.pB && mirror.sB)
                {
                    draft.c[0] = curr.x[0];
                    draft.c[1] = curr.x[1];
                    draft.c[2] = curr.x[2];
                    draft.cB = 1;
                    draft.a = dev_W_USED;
                }
                else if (curr.sB && !mirror.pB)
                {
                    draft.hB = 1;
                    draft.cB = 1;
                    draft.a = dev_W_USED;
                }
            }
        }
    }
}

__device__ inline void dev_convolute7(::CellDevice& curr, ::CellDevice& draft,
                                      ::CellDevice& mirror, unsigned /*w*/, unsigned /*tid*/)
{
    // Cells awaken?
    if (curr.t == curr.d && mirror.t == mirror.d)
    {
        // --- A) SAME POSITION (superposition) ---
        if (curr.x[0] == mirror.x[0] &&
            curr.x[1] == mirror.x[1] &&
            curr.x[2] == mirror.x[2])
        {
            // Test dispersion
            if (DEV_W1(curr) != DEV_W1(mirror) &&
                curr.t == dev_RMAX / 2 &&
                !curr.cB && curr.a != dev_W_USED)
            {
                // Who has the pB true interacts once
                if (curr.pB && !mirror.pB)
                {
                    draft.c[0] = curr.x[0];
                    draft.c[1] = curr.x[1];
                    draft.c[2] = curr.x[2];
                    draft.cB = 1;
                }
                // Who has pB false interacts with the last pB true
                if (!curr.pB && mirror.pB)
                {
                    draft.hB = 1;
                    draft.cB = 1;
                }
            }
            // Test single pair
            else if (curr.f == curr.t && mirror.f == mirror.t)
            {
                // Different sectors?
                if (DEV_W1(curr) != DEV_W1(mirror))
                {
                    // Momentum (Graviton)
                    if (curr.pB && mirror.pB)
                    {
                        draft.f += curr.t;
                        draft.s2B &= curr.phiB;
                        draft.a = min(curr.a, mirror.a);
                    }
                }
                else if ((DEV_Q(curr)  ^ DEV_Q(mirror))  &&
                         (DEV_W1(curr) == DEV_W1(mirror)) &&
                         (DEV_W0(curr) ^ DEV_W0(mirror))  &&
                         (DEV_C2(curr) == DEV_C2(mirror)) &&
                         (DEV_C1(curr) == DEV_C1(mirror)) &&
                         (DEV_C0(curr) == DEV_C0(mirror)))
                {
                    // Photon
                    draft.f += curr.t;
                    draft.s2B &= curr.phiB;
                    draft.a = min(curr.a, mirror.a);
                    draft.bB = 1;
                }
                else if ((curr.ch == 0 && mirror.ch == 0) ||
                         (curr.ch == 63 && mirror.ch == 63))
                {
                    // Neutrino
                    draft.f += curr.t;
                    draft.s2B &= curr.phiB;
                    draft.a = min(curr.a, mirror.a);
                }
                else if ((!DEV_Q(curr) && !DEV_Q(mirror)) &&
                         (!DEV_W1(curr) && !DEV_W1(mirror)) &&
                         (DEV_W0(curr) && DEV_W0(mirror)) &&
                         (DEV_COLOR(curr) == DEV_COLOR(mirror)) &&
                         (DEV_COLOR(curr) != 0 && DEV_COLOR(curr) != 7))
                {
                    // Boson W-
                    draft.f += curr.t;
                    draft.s2B &= curr.phiB;
                    draft.a = min(curr.a, mirror.a);
                    draft.bB = 1;
                }
                else if ((DEV_Q(curr) && DEV_Q(mirror)) &&
                         (DEV_W1(curr) && DEV_W1(mirror)) &&
                         (!DEV_W0(curr) && !DEV_W0(mirror)) &&
                         (DEV_COLOR(curr) == DEV_COLOR(mirror)) &&
                         (DEV_COLOR(curr) != 0 && DEV_COLOR(curr) != 7))
                {
                    // Boson W+
                    draft.f += curr.t;
                    draft.s2B &= curr.phiB;
                    draft.a = min(curr.a, mirror.a);
                    draft.bB = 1;
                }
                else if ((DEV_Q(curr) != DEV_Q(mirror)) &&
                         (DEV_W1(curr) && DEV_W1(mirror)) &&
                         (!DEV_W0(curr) && !DEV_W0(mirror)) &&
                         (DEV_COLOR(curr) == DEV_COLOR(mirror)) &&
                         (DEV_COLOR(curr) != 0 && DEV_COLOR(curr) != 7))
                {
                    // Boson Z
                    draft.f += curr.t;
                    draft.s2B &= curr.phiB;
                    draft.a = min(curr.a, mirror.a);
                    draft.bB = 1;
                }
            }
            // Blob formation
            else if (curr.f != curr.t && mirror.f != mirror.t && curr.bB)
            {
                draft.f += curr.f + mirror.f;
                draft.s2B &= curr.phiB;
                draft.a = min(curr.a, mirror.a);
            }
        }
        // --- B) DIFFERENT POSITION (distinct bubbles) ---
        else
        {
            // Same sector
            if (DEV_W1(curr) == DEV_W1(mirror))
            {
                // Annihilation?
                if (!curr.kB && !mirror.kB &&
                    DEV_Q(curr) != DEV_Q(mirror) &&
                    DEV_W0(curr) != DEV_W0(mirror) &&
                    DEV_COLOR(curr) == DEV_ANTICOLOR(mirror) &&
                    curr.f == curr.t && mirror.f == mirror.t)
                {
                    draft.c[0] = curr.x[0];
                    draft.c[1] = curr.x[1];
                    draft.c[2] = curr.x[2];
                    draft.kB = 1;
                    draft.a = curr.x[3];
                }
                // Fermion cohesion?
                else if (curr.ch == mirror.ch &&
                         curr.f == curr.t && mirror.f == mirror.t)
                {
                    if (curr.c[3] > mirror.c[3])
                    {
                        draft.c[0] = curr.x[0];
                        draft.c[1] = curr.x[1];
                        draft.c[2] = curr.x[2];
                        draft.a = min(curr.a, mirror.a);
                    }
                    else
                    {
                        draft.hB = 1;
                        draft.a = min(curr.a, mirror.a);
                    }
                }
                // Same affinity?
                else if (curr.a == mirror.a)
                {
                    if (curr.pB && !mirror.pB)
                    {
                        draft.c[0] = curr.c[0];
                        draft.c[1] = curr.c[1];
                        draft.c[2] = curr.c[2];
                    }
                    // Parallel transport?
                    else if (!curr.pB && mirror.pB)
                    {
                        draft.c[0] = dev_EL + (curr.x[0] - mirror.x[0]) % dev_EL;
                        draft.c[1] = dev_EL + (curr.x[1] - mirror.x[1]) % dev_EL;
                        draft.c[2] = dev_EL + (curr.x[2] - mirror.x[2]) % dev_EL;
                    }
                }
                // Strong interaction
                else if (dev_neutralColor(curr, mirror))
                {
                    // Gluon x gluon
                    if (curr.f > curr.t && mirror.f > mirror.t)
                    {
                        draft.c[0] = curr.x[0];
                        draft.c[1] = curr.x[1];
                        draft.c[2] = curr.x[2];
                        draft.ch = (curr.ch & ~COLOR_MASK) | (mirror.ch & COLOR_MASK);
                    }
                    // Quark x gluon
                    else if (curr.f == curr.t && mirror.f > mirror.t)
                    {
                        draft.c[0] = curr.x[0];
                        draft.c[1] = curr.x[1];
                        draft.c[2] = curr.x[2];
                        draft.ch = (curr.ch & ~COLOR_MASK) | (mirror.ch & COLOR_MASK);
                    }
                }
                // Electroweak interaction: Harmonic?
                else if (curr.phiB && mirror.phiB)
                {
                    // Weak interaction
                    if (dev_neutralWeak(curr, mirror))
                    {
                        if ((curr.pB && !mirror.pB) || (curr.sB && mirror.sB))
                        {
                            draft.c[0] = curr.x[0];
                            draft.c[1] = curr.x[1];
                            draft.c[2] = curr.x[2];
                            draft.kB = 1;
                        }
                    }
                    // Electric interaction
                    else if (curr.pB)
                    {
                        draft.c[0] = curr.x[0];
                        draft.c[1] = curr.x[1];
                        draft.c[2] = curr.x[2];
                        if (mirror.pB)
                        {
                            draft.kB = 1;
                        }
                        else
                        {
                            draft.a = mirror.a;
                            draft.t = mirror.t;
                            draft.c[0] = mirror.x[0];
                            draft.c[1] = mirror.x[1];
                            draft.c[2] = mirror.x[2];
                        }
                    }
                    // Magnetic interaction
                    else if (curr.sB)
                    {
                        draft.c[0] = curr.x[0];
                        draft.c[1] = curr.x[1];
                        draft.c[2] = curr.x[2];
                        if (mirror.sB)
                        {
                            draft.kB = 1;
                        }
                        else
                        {
                            draft.a = mirror.a;
                            draft.t = mirror.t;
                            draft.c[0] = mirror.x[0];
                            draft.c[1] = mirror.x[1];
                            draft.c[2] = mirror.x[2];
                        }
                    }
                }
            }
        }
    }
    // Different sectors
    else
    {
        // Singularization
        if (curr.ch == ((~mirror.ch) & CHARGE_MASK))
        {
            draft.c[0] = curr.x[0];
            draft.c[1] = curr.x[1];
            draft.c[2] = curr.x[2];
            draft.a = curr.x[3];
        }
        // Electroweak interaction: Harmonic?
        else if (curr.phiB && mirror.phiB)
        {
            // Weak interaction
            if (dev_neutralWeak(curr, mirror))
            {
                if ((curr.pB && !mirror.pB) || (curr.sB && mirror.sB))
                {
                    draft.c[0] = curr.x[0];
                    draft.c[1] = curr.x[1];
                    draft.c[2] = curr.x[2];
                    draft.kB = 1;
                }
            }
            // Electric interaction
            else if (curr.pB)
            {
                draft.c[0] = curr.x[0];
                draft.c[1] = curr.x[1];
                draft.c[2] = curr.x[2];
                if (mirror.pB)
                {
                    draft.kB = 1;
                }
            }
            // Magnetic interaction
            else if (curr.sB)
            {
                draft.c[0] = curr.x[0];
                draft.c[1] = curr.x[1];
                draft.c[2] = curr.x[2];
                if (mirror.sB)
                {
                    draft.kB = 1;
                }
            }
        }
    }
}

// ===================================================================
// MAIN UPDATE KERNEL - COMPLETE CA LOGIC
// ===================================================================

__global__ void ca_update_kernel(::CellDevice* d_curr, ::CellDevice* d_draft, ::CellDevice* d_mirror,
                                 unsigned CONVOL, unsigned GSLOT_Z,
                                 unsigned SLOT1, unsigned SLOT2, unsigned SLOT3,
                                 unsigned SLOT4, unsigned DIFFUSION, unsigned SLOT5, unsigned SLOT6,
                                 unsigned SLOT7, unsigned SLOT8, unsigned RELOC, unsigned REISSUE,
                                 unsigned FLOOD, unsigned FRAME, int scenario)
{
    unsigned int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (dev_EL == 0 || dev_W_USED == 0) return;

    unsigned int total_cells = dev_EL * dev_EL * dev_EL * dev_W_USED;
    if (idx >= total_cells) return;

    // Map 1D -> 4D
    unsigned w = idx % dev_W_USED;
    unsigned idx_3d = idx / dev_W_USED;
    unsigned z = idx_3d % dev_EL;
    unsigned y = (idx_3d / dev_EL) % dev_EL;
    unsigned x = idx_3d / (dev_EL * dev_EL);
    
    ::CellDevice curr = d_getCell(d_curr, x, y, z, w);
    ::CellDevice draft = curr;
    ::CellDevice mirror = d_getCell(d_mirror, x, y, z, w);

    // Neighbors (spherical antipodal wrap for spatial, periodic for W)
    ::CellDevice forward = d_getNeighbor(d_curr, x, y, z, w, 6); // FORWARD
    ::CellDevice north   = d_getNeighbor(d_curr, x, y, z, w, 0); // NORTH
    ::CellDevice east    = d_getNeighbor(d_curr, x, y, z, w, 1); // EAST
    ::CellDevice south   = d_getNeighbor(d_curr, x, y, z, w, 2); // SOUTH
    ::CellDevice west    = d_getNeighbor(d_curr, x, y, z, w, 3); // WEST
    ::CellDevice up      = d_getNeighbor(d_curr, x, y, z, w, 4); // UP
    ::CellDevice down    = d_getNeighbor(d_curr, x, y, z, w, 5); // DOWN

    // ===================================================================
    // CONVOLUTION PHASE (k < CONVOL)
    // ===================================================================
    if (curr.k < CONVOL) {
        switch (scenario) {
            case 0: dev_convolute0(curr, draft, mirror, w, idx); break;
            case 1: dev_convolute1(curr, draft, mirror, w, idx); break;
            case 2: dev_convolute2(curr, draft, mirror, w, idx); break;
            case 3: dev_convolute3(curr, draft, mirror, w, idx); break;
            case 4: dev_convolute4(curr, draft, mirror, w, idx); break;
            case 5: dev_convolute5(curr, draft, mirror, w, idx); break;
            case 6: dev_convolute6(curr, draft, mirror, w, idx); break;
            case 7: dev_convolute7(curr, draft, mirror, w, idx); break;
            default: break;
        }
    }
    // ===================================================================
    // GSLOT PHASES (not yet implemented)
    // ===================================================================
    else if (curr.k < GSLOT_Z) {
        // timing slots – reserved for glider transport
    }
    // ===================================================================
    // DIFFUSION PHASE (k < DIFFUSION)
    // ===================================================================
    else if (curr.k < DIFFUSION) {
        // SLOT I
        if (curr.k < SLOT1) {
            if ((north.a == dev_W_USED && curr.d >= north.d) ||
                (west.a  == dev_W_USED && curr.d >= west.d)  ||
                (down.a  == dev_W_USED && curr.d >= down.d)  ||
                (south.a == dev_W_USED && curr.d >= south.d) ||
                (east.a  == dev_W_USED && curr.d >= east.d)  ||
                (up.a    == dev_W_USED && curr.d >= up.d)) {
                draft.a = dev_W_USED;
            }
        }
        // SLOT II
        if (curr.k < SLOT2) {
            if ((north.a == dev_W_USED && curr.d >= north.d) ||
                (west.a  == dev_W_USED && curr.d >= west.d)  ||
                (down.a  == dev_W_USED && curr.d >= down.d)  ||
                (south.a == dev_W_USED && curr.d >= south.d) ||
                (east.a  == dev_W_USED && curr.d >= east.d)  ||
                (up.a    == dev_W_USED && curr.d >= up.d)) {
                draft.a = dev_W_USED;
            }
            // Hunting using hB (matches CPU: effective_t condition, no modulo on c[])
            if (curr.d == dev_effective_t(curr.t)) {
                if (north.hB) { draft.c[0] = north.c[0] + 1; curr.sB = !draft.hB; }
                else if (west.hB)  { draft.c[1] = west.c[1] + 1; curr.sB = !draft.hB; }
                else if (down.hB)  { draft.c[2] = down.c[2] + 1; curr.sB = !draft.hB; }
                else if (south.hB) { draft.c[1] = south.c[1] + 1; curr.sB = !draft.hB; }
                else if (east.hB)  { draft.c[0] = east.c[0] + 1; curr.sB = !draft.hB; }
                else if (up.hB)    { draft.c[2] = up.c[2] + 1; curr.sB = !draft.hB; }
            }
        }
        // SLOT III
        else if (curr.k < SLOT3) {
            // Propagate c[] to all cells in layer 0 (matches CPU)
            if (curr.x[3] == 0) {
                if (!ZERO_C(north.c)) {
                    draft.c[0] = north.c[0]; draft.c[1] = north.c[1]; draft.c[2] = north.c[2];
                    if (north.kB) draft.kB = north.kB;
                } else if (!ZERO_C(south.c)) {
                    draft.c[0] = south.c[0]; draft.c[1] = south.c[1]; draft.c[2] = south.c[2];
                    if (south.kB) draft.kB = south.kB;
                } else if (!ZERO_C(east.c)) {
                    draft.c[0] = east.c[0]; draft.c[1] = east.c[1]; draft.c[2] = east.c[2];
                    if (east.kB) draft.kB = east.kB;
                } else if (!ZERO_C(west.c)) {
                    draft.c[0] = west.c[0]; draft.c[1] = west.c[1]; draft.c[2] = west.c[2];
                    if (west.kB) draft.kB = west.kB;
                } else if (!ZERO_C(up.c)) {
                    draft.c[0] = up.c[0]; draft.c[1] = up.c[1]; draft.c[2] = up.c[2];
                    if (up.kB) draft.kB = up.kB;
                } else if (!ZERO_C(down.c)) {
                    draft.c[0] = down.c[0]; draft.c[1] = down.c[1]; draft.c[2] = down.c[2];
                    if (down.kB) draft.kB = down.kB;
                }
            }

            draft.f = max(down.f, max(west.f, max(north.f,
                        max(south.f, max(east.f, up.f)))));

            if (!curr.cB) {
                if (north.cB && north.d > curr.d) {
                    draft.cB = 1;
                    if (north.a != dev_W_USED) draft.a = north.a;
                } else if (south.cB && south.d > curr.d) {
                    draft.cB = 1;
                    if (south.a != dev_W_USED) draft.a = south.a;
                } else if (east.cB && east.d > curr.d) {
                    draft.cB = 1;
                    if (east.a != dev_W_USED) draft.a = east.a;
                } else if (west.cB && west.d > curr.d) {
                    draft.cB = 1;
                    if (west.a != dev_W_USED) draft.a = west.a;
                } else if (down.cB && down.d > curr.d) {
                    draft.cB = 1;
                    if (down.a != dev_W_USED) draft.a = down.a;
                } else if (up.cB && up.d > curr.d) {
                    draft.cB = 1;
                    if (up.a != dev_W_USED) draft.a = up.a;
                }
            }
        }
        // SLOT IV (matches CPU: centered coordinates + clamp)
        else if (curr.k < SLOT4) {
            if (forward.kB && forward.a == curr.a) {
                int half = (int)dev_EL / 2;
                int cx = (int)curr.x[0] - half;
                int cy = (int)curr.x[1] - half;
                int cz = (int)curr.x[2] - half;
                int fx = (int)forward.x[0] - half;
                int fy = (int)forward.x[1] - half;
                int fz = (int)forward.x[2] - half;
                int delta_x = cx - fx;
                int delta_y = cy - fy;
                int delta_z = cz - fz;
                int ncx = (int)forward.c[0] + delta_x;
                int ncy = (int)forward.c[1] + delta_y;
                int ncz = (int)forward.c[2] + delta_z;
                ncx = max(0, min((int)dev_EL - 1, ncx));
                ncy = max(0, min((int)dev_EL - 1, ncy));
                ncz = max(0, min((int)dev_EL - 1, ncz));
                draft.c[0] = ncx;
                draft.c[1] = ncy;
                draft.c[2] = ncz;
                draft.kB = forward.kB;
                draft.cB = forward.cB;
            }
            draft.f = max(forward.f, curr.f);
        }
        // SLOT V
        else if (curr.k < SLOT5) {
            if (curr.a == dev_W_USED && curr.d < curr.t) {
                draft.a = curr.x[3];
            }
        }
    }
    // ===================================================================
    // RELOCATION PHASE (k < RELOC) — restore original coordinates (matches CPU)
    // ===================================================================
    else if (curr.k < RELOC) {
        // Save 3D address (CPU restores after relocation)
        unsigned save_x = curr.x[0];
        unsigned save_y = curr.x[1];
        unsigned save_z = curr.x[2];
        // SLOT VI (x direction)
        if (curr.k < SLOT6) {
            if (north.c[0] > 0) {
                draft = north;
                draft.c[0]--;
            }
        }
        // SLOT VII (y direction)
        else if (curr.k < SLOT7) {
            if (west.c[1] > 0) {
                draft = west;
                draft.c[1]--;
            }
        }
        // SLOT VIII (z direction)
        else if (curr.k < SLOT8) {
            if (down.c[2] > 0) {
                draft = down;
                draft.c[2]--;
            }
        }
        // Restore original 3D address (matches CPU relocate)
        draft.x[0] = save_x;
        draft.x[1] = save_y;
        draft.x[2] = save_z;
    }
    // ===================================================================
    // REISSUE PHASE (k < REISSUE)
    // ===================================================================
    else if (curr.k < REISSUE) {
        draft.kB = 0;
        draft.hB = 0;
        draft.bB = 0;
        if (curr.d == dev_effective_t(curr.t)) {
            if (north.d == curr.d + 1) draft.a = north.a;
            if (south.d == curr.d + 1) draft.a = south.a;
            if (east.d  == curr.d + 1) draft.a = east.a;
            if (west.d  == curr.d + 1) draft.a = west.a;
            if (up.d    == curr.d + 1) draft.a = up.a;
            if (down.d  == curr.d + 1) draft.a = down.a;
        }
        if (curr.cB) {
            draft.cB = 0;
            if (curr.a != dev_W_USED && curr.d < 2) {
                draft.t = 0;
            }
        }
    }
    // ===================================================================
    // FLOOD PHASE (k < FLOOD)
    // ===================================================================
    else if (curr.k < FLOOD) {
        if (curr.a != dev_W_USED) {
            draft.t = min(north.t, min(south.t, min(east.t,
                      min(west.t, min(down.t, up.t)))));
        }
    }

    // Update frame counters
    draft.k = (curr.k + 1) % FRAME;
    if (draft.k == 0) {
        if (curr.a == dev_W_USED && curr.t <= dev_RMAX) {
            draft.t++;
        } else {
            draft.t = (curr.t + 1) % (dev_RMAX + 1);
        }
    }

    d_getCell(d_draft, x, y, z, w) = draft;
}

// ===================================================================
// MIRROR UPDATE KERNEL (only when k == 0)
// ===================================================================
__global__ void updateMirrorKernel(CellDevice* lattice_curr,
                                   CellDevice* lattice_mirror,
                                   unsigned totalCells)
{
    unsigned tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= totalCells) return;
    lattice_mirror[tid] = lattice_curr[tid];
    lattice_mirror[tid].f = lattice_mirror[tid].t;
}

// ===================================================================
// SHIFT-MIRROR KERNEL (cyclic shift along w dimension)
// ===================================================================
__global__ void shiftMirrorKernel(const CellDevice* src,
                                  CellDevice* dst,
                                  unsigned totalCells)
{
    unsigned tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= totalCells) return;
    unsigned w = tid % dev_W_USED;
    unsigned idx3d = tid / dev_W_USED;
    unsigned src_w = (w + dev_W_USED - 1) % dev_W_USED;
    unsigned src_tid = idx3d * dev_W_USED + src_w;
    dst[tid] = src[src_tid];
}

// ===================================================================
// CONSTANT MEMORY SETUP (must be in same .cu as kernels)
// ===================================================================

extern "C" void setCudaConstants(unsigned EL, unsigned W_USED, unsigned RMAX)
{
    cudaError_t err;
    unsigned CENTER = (EL - 1) / 2;

    printf("Setting dev_EL = %u\n", EL);
    err = cudaMemcpyToSymbol(dev_EL, &EL, sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_EL: %s (code %d)\n",
                cudaGetErrorString(err), err);
        return;
    }

    printf("Setting dev_W_USED = %u\n", W_USED);
    err = cudaMemcpyToSymbol(dev_W_USED, &W_USED, sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_W_USED: %s (code %d)\n",
                cudaGetErrorString(err), err);
        return;
    }

    printf("Setting dev_RMAX = %u\n", RMAX);
    err = cudaMemcpyToSymbol(dev_RMAX, &RMAX, sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_RMAX: %s (code %d)\n",
                cudaGetErrorString(err), err);
        return;
    }

    printf("Setting dev_CENTER = %u\n", CENTER);
    err = cudaMemcpyToSymbol(dev_CENTER, &CENTER, sizeof(unsigned));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_CENTER: %s (code %d)\n",
                cudaGetErrorString(err), err);
        return;
    }

    int initCtrl = 1;
    err = cudaMemcpyToSymbol(dev_ctrl, &initCtrl, sizeof(int));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error setting dev_ctrl: %s (code %d)\n",
                cudaGetErrorString(err), err);
        return;
    }

    printf("All constants set successfully\n");
}

extern "C" void resetCudaCtrl()
{
    int val = 1;
    cudaError_t err = cudaMemcpyToSymbol(dev_ctrl, &val, sizeof(int));
    if (err != cudaSuccess) {
        fprintf(stderr, "Error resetting dev_ctrl: %s (code %d)\n",
                cudaGetErrorString(err), err);
    } else {
        printf("dev_ctrl reset to 1\n");
    }
}

// ===================================================================
// HOST API FUNCTIONS
// ===================================================================

bool isCudaAvailable() 
{
    int devCount = 0;
    cudaError_t err = cudaGetDeviceCount(&devCount); 
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA not available: %s\n", cudaGetErrorString(err));
        return false;
    }
    if (devCount == 0) {
        fprintf(stderr, "No CUDA devices found\n");
        return false;
    }
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    printf("Found CUDA device: %s (Compute %d.%d)\n", 
           prop.name, prop.major, prop.minor);
    return true;
}

bool init_cuda_memory(unsigned EL, unsigned W_USED)
{
    if (d_lattice_curr != nullptr) {
        fprintf(stderr, "CUDA memory already allocated\n");
        return true;
    }
    size_t total_cells = (size_t)EL * EL * EL * W_USED;
    size_t size = total_cells * sizeof(::CellDevice);
    printf("Allocating CUDA memory: %zu cells, %zu MB per lattice\n", 
           total_cells, size / (1024 * 1024));
    if (size > 2ULL * 1024 * 1024 * 1024) {
        fprintf(stderr, "ERROR: Requested allocation too large: %zu MB\n", size / (1024 * 1024));
        return false;
    }
    CUDA_CHECK(cudaMalloc((void**)&d_lattice_curr, size));
    CUDA_CHECK(cudaMalloc((void**)&d_lattice_draft, size));
    CUDA_CHECK(cudaMalloc((void**)&d_lattice_mirror, size));
    CUDA_CHECK(cudaMemset(d_lattice_curr, 0, size));
    CUDA_CHECK(cudaMemset(d_lattice_draft, 0, size));
    CUDA_CHECK(cudaMemset(d_lattice_mirror, 0, size));
    printf("✓ CUDA Memory Allocated successfully\n");
    return true;
}

bool initCudaSimulation(unsigned EL, unsigned W_USED)
{
    if (g_cuda_initialized) {
        printf("CUDA already initialized\n");
        return true;
    }
    printf("Initializing CUDA simulation: EL=%u, W_USED=%u\n", EL, W_USED);
    CUDA_CHECK_VOID(cudaDeviceReset());
    CUDA_CHECK(cudaSetDevice(0));
    // Constants are set by setCudaConstants (called from bridge_cuda.cu)
    if (!init_cuda_memory(EL, W_USED)) return false;
    g_cuda_initialized = true;
    printf("✓ CUDA simulation initialized successfully\n");
    return true;
}

void free_cuda_memory()
{
    if (d_lattice_curr) cudaFree(d_lattice_curr);
    if (d_lattice_draft) cudaFree(d_lattice_draft);
    if (d_lattice_mirror) cudaFree(d_lattice_mirror);
    d_lattice_curr = d_lattice_draft = d_lattice_mirror = nullptr;
    g_cuda_initialized = false;
}

void cudaCleanup()
{
    printf("Cleaning up CUDA resources...\n");
    free_cuda_memory();
    cudaDeviceReset();
    printf("✓ CUDA cleanup complete\n");
}

bool uploadLatticeToCuda(::CellDevice* hostCells, size_t totalCells)
{
    if (!g_cuda_initialized) {
        fprintf(stderr, "ERROR: CUDA not initialized\n");
        return false;
    }
    size_t size = totalCells * sizeof(::CellDevice);
    CUDA_CHECK(cudaMemcpy(d_lattice_curr, hostCells, size, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_lattice_draft, d_lattice_curr, size, cudaMemcpyDeviceToDevice));
    CUDA_CHECK(cudaMemcpy(d_lattice_mirror, d_lattice_curr, size, cudaMemcpyDeviceToDevice));
    return true;
}

bool downloadLatticeFromCuda(::CellDevice* hostCells, size_t totalCells)
{
    if (!g_cuda_initialized) return false;
    size_t size = totalCells * sizeof(::CellDevice);
    CUDA_CHECK(cudaMemcpy(hostCells, d_lattice_curr, size, cudaMemcpyDeviceToHost));
    return true;
}

void cudaSimulationStep(
    unsigned CONVOL, unsigned GSLOT_Z,
    unsigned SLOT1, unsigned SLOT2, unsigned SLOT3, 
    unsigned SLOT4, unsigned DIFFUSION, unsigned SLOT5, unsigned SLOT6, 
    unsigned SLOT7, unsigned SLOT8, unsigned RELOC, unsigned REISSUE, 
    unsigned FLOOD, unsigned FRAME, unsigned RMAX, int scenario)
{
    if (!g_cuda_initialized) {
        fprintf(stderr, "ERROR: CUDA not initialized\n");
        return;
    }

    // Get dimensions from automaton namespace (they are host variables)
    unsigned L = automaton::EL;
    unsigned W = automaton::W_USED;
    if (L == 0 || W == 0) {
        fprintf(stderr, "ERROR: EL or W_USED is zero\n");
        return;
    }

    size_t total_cells = (size_t)L * L * L * W;
    if (total_cells == 0) {
        fprintf(stderr, "ERROR: total_cells = 0\n");
        return;
    }

    const int BLOCK_SIZE = 256;
    int GRID = (int)((total_cells + BLOCK_SIZE - 1) / BLOCK_SIZE);
    printf("Launching kernel: total_cells=%zu, GRID=%d, BLOCK=%d, EL=%u, W_USED=%u, RMAX=%u\n",
           total_cells, GRID, BLOCK_SIZE, L, W, RMAX);

    // Launch kernel
    ca_update_kernel<<<GRID, BLOCK_SIZE>>>(
        d_lattice_curr, d_lattice_draft, d_lattice_mirror,
        CONVOL, GSLOT_Z, SLOT1, SLOT2, SLOT3, SLOT4, DIFFUSION,
        SLOT5, SLOT6, SLOT7, SLOT8, RELOC, REISSUE,
        FLOOD, FRAME, scenario
    );

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        fprintf(stderr, "Kernel launch failed: %s\n", cudaGetErrorString(err));
        return;
    }

    err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        fprintf(stderr, "Kernel execution failed: %s\n", cudaGetErrorString(err));
        return;
    }

    // Swap curr and draft
    ::CellDevice* temp = d_lattice_curr;
    d_lattice_curr = d_lattice_draft;
    d_lattice_draft = temp;

    // Read new k from first cell (only after successful kernel execution)
    unsigned new_k = 0;
    err = cudaMemcpy(&new_k, &d_lattice_curr[0].k, sizeof(unsigned), cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        fprintf(stderr, "Failed to read k from device: %s\n", cudaGetErrorString(err));
        return;
    }

    if (new_k == 0) {
        updateMirrorKernel<<<GRID, BLOCK_SIZE>>>(d_lattice_curr, d_lattice_mirror, (unsigned)total_cells);
        cudaDeviceSynchronize();
    }

    if (new_k < CONVOL) {
        size_t size = (size_t)total_cells * sizeof(::CellDevice);
        err = cudaMemcpy(d_lattice_draft, d_lattice_mirror, size, cudaMemcpyDeviceToDevice);
        if (err != cudaSuccess) {
            fprintf(stderr, "Mirror copy for shift failed: %s\n", cudaGetErrorString(err));
            return;
        }
        shiftMirrorKernel<<<GRID, BLOCK_SIZE>>>(d_lattice_draft, d_lattice_mirror, (unsigned)total_cells);
        cudaDeviceSynchronize();
    }
}

void cudaUpdateVoxelsLayer(unsigned selectedW)
{
    // TODO: implement GPU-accelerated voxel coloring (optional)
}

uint32_t* getMappedVoxels()
{
    return nullptr;
}

// ===================================================================
// NAMESPACE WRAPPER FUNCTIONS
// ===================================================================
namespace automaton 
{
    ::CellDevice convertToDevice(const Cell& src) {
        ::CellDevice dst;
        dst.ch = src.ch;
        dst.pB = src.pB ? 1 : 0;
        dst.sB = src.sB ? 1 : 0;
        dst.a = src.a;
        for (int i = 0; i < 4; ++i) dst.x[i] = src.x[i];
        dst.d = src.d;
        dst.phiB = src.phiB ? 1 : 0;
        dst.t = src.t;
        dst.f = src.f;
        for (int i = 0; i < 3; ++i) dst.c[i] = src.c[i];
        dst.k = src.k;
        dst.s2B = src.s2B ? 1 : 0;
        dst.kB = src.kB ? 1 : 0;
        dst.bB = src.bB ? 1 : 0;
        dst.hB = src.hB ? 1 : 0;
        dst.cB = src.cB ? 1 : 0;
        dst.gB = src.gB ? 1 : 0;
        for (int i = 0; i < 3; ++i) dst.g[i] = static_cast<int32_t>(src.g[i]);
        return dst;
    }

    void convertToHost(const ::CellDevice& src, Cell& dst) {
        dst.ch = src.ch;
        dst.pB = src.pB != 0;
        dst.sB = src.sB != 0;
        dst.a = src.a;
        for (int i = 0; i < 4; ++i) dst.x[i] = src.x[i];
        dst.d = src.d;
        dst.phiB = src.phiB != 0;
        dst.t = src.t;
        dst.f = src.f;
        for (int i = 0; i < 3; ++i) dst.c[i] = src.c[i];
        dst.k = src.k;
        dst.s2B = src.s2B != 0;
        dst.kB = src.kB != 0;
        dst.bB = src.bB != 0;
        dst.hB = src.hB != 0;
        dst.cB = src.cB != 0;
        dst.gB = src.gB != 0;
        for (int i = 0; i < 3; ++i) dst.g[i] = static_cast<int>(src.g[i]);
    }

    bool swap_lattices_gpu() { return true; }

    void ca_update_gpu_wrapper(
        unsigned CONVOL, unsigned GSLOT_Z,
        unsigned SLOT1, unsigned SLOT2, unsigned SLOT3, 
        unsigned SLOT4, unsigned DIFFUSION, unsigned SLOT5, unsigned SLOT6, 
        unsigned SLOT7, unsigned SLOT8, unsigned RELOC, unsigned REISSUE, 
        unsigned FLOOD, unsigned FRAME, unsigned RMAX)
    {
        cudaSimulationStep(
            CONVOL, GSLOT_Z, SLOT1, SLOT2, SLOT3, SLOT4, DIFFUSION, 
            SLOT5, SLOT6, SLOT7, SLOT8, RELOC, REISSUE, 
            FLOOD, FRAME, RMAX, gConfig.simulation.scenario
        );
    }

    void ca_update_gpu_wrapper() {
        ca_update_gpu_wrapper(
            CONVOL, GSLOT_Z, SLOT1, SLOT2, SLOT3, SLOT4, DIFFUSION, 
            SLOT5, SLOT6, SLOT7, SLOT8, RELOC, REISSUE, 
            FLOOD, FRAME, RMAX
        );
    }

    void free_cuda_memory() { ::free_cuda_memory(); }
}