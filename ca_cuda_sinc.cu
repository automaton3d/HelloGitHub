/*
 * ca_cuda.cu — CUDA kernels for the sinc + pulsating wavefront CA
 *
 * Build:
 *   nvcc -c ca_cuda.cu -o ca_cuda.o -DNO_SDL -DUSE_CUDA
 *
 * Each kernel maps one thread to one grid cell (x,y,z).
 * Block size 8x8x8 = 512 threads; grid covers L^3 cells.
 */

#ifndef NO_SDL
#define NO_SDL
#endif
#ifndef USE_CUDA
#define USE_CUDA
#endif
#include "pulsating.h"
#include "ca_cuda.h"

#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 * Error-checking macro
 * ================================================================ */
#define CUDA_CHECK(call) do {                                         \
    cudaError_t err = (call);                                         \
    if (err != cudaSuccess) {                                         \
        fprintf(stderr, "CUDA error at %s:%d: %s\n",                 \
                __FILE__, __LINE__, cudaGetErrorString(err));         \
        exit(1);                                                      \
    }                                                                 \
} while (0)

/* ================================================================
 * Device memory
 * ================================================================ */
static Cell *d_grid      = NULL;
static Cell *d_grid_next = NULL;
static int  *d_and_count = NULL;

/* ================================================================
 * Launch configuration
 * ================================================================ */
#define BX 8
#define BY 8
#define BZ 8

static dim3 gDim() { return dim3((L+BX-1)/BX, (L+BY-1)/BY, (L+BZ-1)/BZ); }
static dim3 bDim() { return dim3(BX, BY, BZ); }

/* ================================================================
 * Sinc wave kernel — wave equation + Bresenham + TTL + AND
 * One thread per interior cell (boundary cells skipped)
 * ================================================================ */
__global__ void sinc_kernel(Cell *g, Cell *gn, int *and_cnt,
                            int tick, int cur_sweep_r)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;

    if (x < 1 || x >= L-1 || y < 1 || y >= L-1 || z < 1 || z >= L-1)
        return;

    int idx = IDX(x, y, z);
    int u = g[idx].u;
    int v = g[idx].v;

    int neighbors =
        g[IDX(x+1,y,z)].u + g[IDX(x-1,y,z)].u +
        g[IDX(x,y+1,z)].u + g[IDX(x,y-1,z)].u +
        g[IDX(x,y,z+1)].u + g[IDX(x,y,z-1)].u;

    int lap = neighbors - (u << 2) - (u << 1);
    int r = g[idx].r;

    int diff_shift = DIFF_SHIFT + 1 - (r >> DIFF_DIV_SHIFT);
    if (diff_shift < DIFF_SHIFT - 1)
        diff_shift = DIFF_SHIFT - 1;

    int v_new = v + (lap >> diff_shift);
    int u_new = u + v_new;

    v_new -= (v_new >> VEL_DAMP_SHIFT);

    /* shell forcing */
    int dr = r - SHELL_R;
    if (dr < 0) dr = -dr;

    if (dr <= SHELL_W) {
        if (u > SHELL_TARGET) {
            int excess = u - SHELL_TARGET;
            v_new -= (excess >> 4);
        } else if ((tick & 3) == 0) {
            int deficit = SHELL_TARGET - u;
            v_new += (deficit >> 10) + 1;
        }
    }

    /* boundary absorption */
    if (r > RADIUS - ABSORB_W) {
        int dist = r - (RADIUS - ABSORB_W);
        if (dist >= ABSORB_W)
            u_new = 0;
        else
            u_new >>= dist;
    }
    if (r >= RADIUS) u_new = 0;
    if (u_new < 0)   u_new = 0;

    /* Bresenham trigger */
    int acc = g[idx].acc + g[idx].sinc_p;
    int triggered = 0;
    if (acc >= g[idx].sinc_q && g[idx].sinc_q > 0) {
        acc -= g[idx].sinc_q;
        triggered = 1;
    }

    /* TTL persistence */
    unsigned char ttl = g[idx].ttl;
    if ((tick & TTL_DECAY_MASK) == 0 && ttl > 0)
        ttl--;

    /* AND interaction: Bresenham trigger x pulsating active */
    if (triggered && g[idx].active) {
        ttl = (unsigned char)(32 + ((223 * g[idx].sinc_p) / g[idx].sinc_q));
        int rr = g[idx].r;
        if (rr >= 0 && rr < L && rr == cur_sweep_r)
            atomicAdd(&and_cnt[rr], 1);
    }

    gn[idx].u      = u_new;
    gn[idx].v      = v_new;
    gn[idx].acc    = acc;
    gn[idx].sinc_p = g[idx].sinc_p;
    gn[idx].sinc_q = g[idx].sinc_q;
    gn[idx].ttl    = ttl;
    gn[idx].trig   = (unsigned char)triggered;
}

/* ================================================================
 * Sinc copy-back kernel — global damping
 * ================================================================ */
__global__ void sinc_copyback_kernel(Cell *g, const Cell *gn)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;

    if (x >= L || y >= L || z >= L) return;

    int idx = IDX(x, y, z);
    g[idx].u = gn[idx].u - (gn[idx].u >> 12);
    g[idx].v = gn[idx].v - (gn[idx].v >> 12);
    g[idx].acc    = gn[idx].acc;
    g[idx].sinc_p = gn[idx].sinc_p;
    g[idx].sinc_q = gn[idx].sinc_q;
    g[idx].ttl    = gn[idx].ttl;
    g[idx].trig   = gn[idx].trig;
}

/* ================================================================
 * Pulse — copy r2 from grid to grid_next (pre-BFS snapshot)
 * ================================================================ */
__global__ void pulse_copy_r2_kernel(const Cell *g, Cell *gn)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;

    if (x >= L || y >= L || z >= L) return;

    gn[IDX(x,y,z)].r2 = g[IDX(x,y,z)].r2;
}

/* ================================================================
 * Pulse — BFS propagation using atomicMin on grid_next
 * Each visited cell tries to relax its 6 neighbours.
 * ================================================================ */
__global__ void pulse_bfs_kernel(const Cell *g, Cell *gn)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;

    if (x >= L || y >= L || z >= L) return;

    int idx = IDX(x, y, z);
    if (g[idx].r2 == INF_R2) return;

    unsigned int ax = (x > MID) ? (unsigned int)(x - MID) : (unsigned int)(MID - x);
    unsigned int ay = (y > MID) ? (unsigned int)(y - MID) : (unsigned int)(MID - y);
    unsigned int az = (z > MID) ? (unsigned int)(z - MID) : (unsigned int)(MID - z);

    /* +x */
    if (x + 1 < L) atomicMin(&gn[IDX(x+1,y,z)].r2, g[idx].r2 + 2*ax + 1);
    /* -x */
    if (x - 1 >= 0) atomicMin(&gn[IDX(x-1,y,z)].r2, g[idx].r2 + 2*ax + 1);
    /* +y */
    if (y + 1 < L) atomicMin(&gn[IDX(x,y+1,z)].r2, g[idx].r2 + 2*ay + 1);
    /* -y */
    if (y - 1 >= 0) atomicMin(&gn[IDX(x,y-1,z)].r2, g[idx].r2 + 2*ay + 1);
    /* +z */
    if (z + 1 < L) atomicMin(&gn[IDX(x,y,z+1)].r2, g[idx].r2 + 2*az + 1);
    /* -z */
    if (z - 1 >= 0) atomicMin(&gn[IDX(x,y,z-1)].r2, g[idx].r2 + 2*az + 1);
}

/* ================================================================
 * Pulse — finalize: copy r2 back, compute r, set active flags
 * Also enforces center r2 = 0.
 * ================================================================ */
__global__ void pulse_finalize_kernel(Cell *g, const Cell *gn, int tick)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;

    if (x >= L || y >= L || z >= L) return;

    int idx = IDX(x, y, z);

    unsigned int old_r2 = g[idx].r2;
    unsigned int new_r2 = gn[idx].r2;

    /* enforce center */
    if (x == MID && y == MID && z == MID)
        new_r2 = 0;

    g[idx].r2 = new_r2;

    /* compute r on first visit */
    if (new_r2 != INF_R2 && old_r2 == INF_R2)
        g[idx].r = isqrt((int)new_r2);

    /* activation flags (product of pulsating CA only) */
    unsigned int pulse_r2 = pulse_from_time((unsigned int)tick);
    if (new_r2 == INF_R2) {
        g[idx].active = 0;
    } else {
        unsigned int d = (new_r2 > pulse_r2)
                       ? (new_r2 - pulse_r2)
                       : (pulse_r2 - new_r2);
        g[idx].active = (d <= PULSE_TOLERANCE) ? 1u : 0u;
    }
}

/* ================================================================
 * C-linkage wrapper implementations
 * ================================================================ */

extern "C" void cuda_alloc_grids(void)
{
    size_t grid_bytes = sizeof(Cell) * (size_t)L * L * L;
    CUDA_CHECK(cudaMalloc(&d_grid,      grid_bytes));
    CUDA_CHECK(cudaMalloc(&d_grid_next, grid_bytes));
    CUDA_CHECK(cudaMalloc(&d_and_count, sizeof(int) * L));
    CUDA_CHECK(cudaMemset(d_and_count, 0, sizeof(int) * L));

    /* print device info */
    int dev;
    cudaGetDevice(&dev);
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, dev);
    printf("CUDA device: %s  (%.0f MB free)\n", prop.name,
           (double)prop.totalGlobalMem / (1024.0 * 1024.0));
    printf("Grid allocation: 2 x %.1f MB = %.1f MB\n",
           (double)grid_bytes / (1024.0 * 1024.0),
           2.0 * (double)grid_bytes / (1024.0 * 1024.0));
}

extern "C" void cuda_upload_grid(void *h_grid, void *h_grid_next)
{
    size_t grid_bytes = sizeof(Cell) * (size_t)L * L * L;
    CUDA_CHECK(cudaMemcpy(d_grid,      h_grid,      grid_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_grid_next, h_grid_next, grid_bytes, cudaMemcpyHostToDevice));
}

extern "C" void cuda_pulse_step(int tick)
{
    pulse_copy_r2_kernel<<<gDim(), bDim()>>>(d_grid, d_grid_next);
    pulse_bfs_kernel<<<gDim(), bDim()>>>(d_grid, d_grid_next);
    pulse_finalize_kernel<<<gDim(), bDim()>>>(d_grid, d_grid_next, tick);
}

extern "C" void cuda_sinc_step(int tick)
{
    int cur_sweep_r = isqrt((int)pulse_from_time((unsigned int)tick));
    sinc_kernel<<<gDim(), bDim()>>>(d_grid, d_grid_next, d_and_count,
                                    tick, cur_sweep_r);
    sinc_copyback_kernel<<<gDim(), bDim()>>>(d_grid, d_grid_next);
}

extern "C" void cuda_download_grid(void *h_grid)
{
    CUDA_CHECK(cudaMemcpy(h_grid, d_grid,
                          sizeof(Cell) * (size_t)L * L * L,
                          cudaMemcpyDeviceToHost));
}

extern "C" void cuda_download_and_count(int *h_and_count, int n)
{
    CUDA_CHECK(cudaMemcpy(h_and_count, d_and_count,
                          sizeof(int) * n,
                          cudaMemcpyDeviceToHost));
}

extern "C" void cuda_upload_grid_full(void *h_grid)
{
    CUDA_CHECK(cudaMemcpy(d_grid, h_grid,
                          sizeof(Cell) * (size_t)L * L * L,
                          cudaMemcpyHostToDevice));
}

extern "C" void cuda_free(void)
{
    cudaFree(d_grid);
    cudaFree(d_grid_next);
    cudaFree(d_and_count);
    d_grid      = NULL;
    d_grid_next = NULL;
    d_and_count = NULL;
}
