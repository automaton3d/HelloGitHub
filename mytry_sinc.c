/*
 * mytry.c — unified sinc wave CA + pulsating wavefront CA
 *
 * Two independent CAs share the same grid structure:
 *   1) Sinc wave: 3-D integer-only wave equation → emergent sin(r)/r
 *      with Bresenham accumulator triggers.
 *   2) Pulsating wavefront: BFS distance propagation with pulsing shell.
 *
 * No interaction between the two CAs (for now).
 *
 * Constraints inside sinc_step():
 *   - no floating-point
 *   - no multiplication (*)
 *   - no division (/)
 *   - no lookup tables
 */

#include "pulsating.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* --- Grid allocation (heap, too large for stack) --- */
Cell (*grid)[L][L]      = NULL;
Cell (*grid_next)[L][L] = NULL;

int tick = 0;

static int and_count[L];          /* accumulated AND hits per shell radius */

/* ==========================================================
 * Unified initialization (sinc wave + pulsating wavefront)
 *
 * r2 is filled dynamically by the wavefront BFS (sum-of-odds)
 * inside pulse_step(); r = isqrt(r2) is cached on first visit.
 * Until the wavefront reaches a cell, r2 = INF_R2 and r = 0,
 * which is harmless because the sinc wave has not arrived
 * there yet either.
 * ========================================================== */
void init(void) {
    int cx = L/2, cy = L/2, cz = L/2;

    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        Cell *c = &grid[x][y][z];
        c->u       = 0;
        c->v       = 0;
        c->acc     = 0;
        c->sinc_p  = 0;
        c->sinc_q  = 1;
        c->r       = 0;
        c->r2      = INF_R2;
        c->active  = 0;
        c->ttl     = 0;
        c->trig    = 0;
    }
    grid[cx][cy][cz].u       = 2048;
    grid[cx][cy][cz].r2 = 0;
}

/* ==========================================================
 * Sinc wave CA — one tick (integer-only in the hot loop)
 * ========================================================== */
#ifndef USE_CUDA
void sinc_step(void)
{
    int cur_sweep_r = isqrt((int)pulse_from_time((unsigned int)tick));

    for (int x = 1; x < L-1; x++)
    for (int y = 1; y < L-1; y++)
    for (int z = 1; z < L-1; z++)
    {
        int u = grid[x][y][z].u;
        int v = grid[x][y][z].v;

        int neighbors =
            grid[x+1][y][z].u + grid[x-1][y][z].u +
            grid[x][y+1][z].u + grid[x][y-1][z].u +
            grid[x][y][z+1].u + grid[x][y][z-1].u;

        int lap = neighbors - (u << 2) - (u << 1);

        int r = grid[x][y][z].r;

        int diff_shift = DIFF_SHIFT + 1 - (r >> DIFF_DIV_SHIFT);
        if (diff_shift < DIFF_SHIFT - 1)
            diff_shift = DIFF_SHIFT - 1;

        int v_new = v + (lap >> diff_shift);
        int u_new = u + v_new;

        v_new -= (v_new >> VEL_DAMP_SHIFT);

        /* shell forcing */
        int dr = r - SHELL_R;
        if (dr < 0) dr = -dr;

        if (dr <= SHELL_W)
        {
            if (u > SHELL_TARGET)
            {
                int excess = u - SHELL_TARGET;
                v_new -= (excess >> 4);
            }
            else if ((tick & 3) == 0)
            {
                int deficit = SHELL_TARGET - u;
                v_new += (deficit >> 10) + 1;
            }
        }

        /* boundary absorption (ABSORB_W scales with RADIUS) */
        if (r > RADIUS - ABSORB_W)
        {
            int dist = r - (RADIUS - ABSORB_W);

            if (dist >= ABSORB_W)
                u_new = 0;
            else
                u_new >>= dist;  /* dist=1→50%, dist=2→25%, … */
        }

        if (r >= RADIUS)
            u_new = 0;

        if (u_new < 0)
            u_new = 0;

        /* Bresenham trigger */
        int acc = grid[x][y][z].acc + grid[x][y][z].sinc_p;

        int triggered = 0;

        if (acc >= grid[x][y][z].sinc_q &&
            grid[x][y][z].sinc_q > 0)
        {
            acc -= grid[x][y][z].sinc_q;
            triggered = 1;
        }

        /* ---------------------------
           TTL persistence
           --------------------------- */

        unsigned char ttl = grid[x][y][z].ttl;

        if ((tick & TTL_DECAY_MASK) == 0 && ttl > 0)
            ttl--;

        /* AND interaction: Bresenham trigger × pulsating active */
        if (triggered && grid[x][y][z].active)
        {
            ttl = 32 + ((223 * grid[x][y][z].sinc_p) /
                        grid[x][y][z].sinc_q);
            int rr = grid[x][y][z].r;
            if (rr >= 0 && rr < L && rr == cur_sweep_r)
                and_count[rr]++;
        }

        grid_next[x][y][z].u      = u_new;
        grid_next[x][y][z].v      = v_new;
        grid_next[x][y][z].acc    = acc;
        grid_next[x][y][z].sinc_p = grid[x][y][z].sinc_p;
        grid_next[x][y][z].sinc_q = grid[x][y][z].sinc_q;
        grid_next[x][y][z].ttl    = ttl;
        grid_next[x][y][z].trig   = (unsigned char)triggered;
    }

    /* copy back with global damping */

    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++)
    {
        grid[x][y][z].u =
            grid_next[x][y][z].u -
            (grid_next[x][y][z].u >> 12);

        grid[x][y][z].v =
            grid_next[x][y][z].v -
            (grid_next[x][y][z].v >> 12);

        grid[x][y][z].acc    = grid_next[x][y][z].acc;
        grid[x][y][z].sinc_p = grid_next[x][y][z].sinc_p;
        grid[x][y][z].sinc_q = grid_next[x][y][z].sinc_q;
        grid[x][y][z].ttl    = grid_next[x][y][z].ttl;
        grid[x][y][z].trig   = grid_next[x][y][z].trig;
    }
}

/* ==========================================================
 * Pulsating wavefront CA — one tick
 * ========================================================== */
/* 6-neighbor offsets: +x,-x,+y,-y,+z,-z */
static const int ddx[6] = {1, -1, 0,  0, 0,  0};
static const int ddy[6] = {0,  0, 1, -1, 0,  0};
static const int ddz[6] = {0,  0, 0,  0, 1, -1};

static void pulse_update_wavefront(void) {
    /* copy current r2 into grid_next */
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++)
        grid_next[x][y][z].r2 = grid[x][y][z].r2;

    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        if (grid[x][y][z].r2 == INF_R2) continue;

        unsigned int ax = (x > MID) ? (unsigned int)(x - MID) : (unsigned int)(MID - x);
        unsigned int ay = (y > MID) ? (unsigned int)(y - MID) : (unsigned int)(MID - y);
        unsigned int az = (z > MID) ? (unsigned int)(z - MID) : (unsigned int)(MID - z);

        for (int d = 0; d < 6; d++) {
            int nx = x + ddx[d];
            int ny = y + ddy[d];
            int nz = z + ddz[d];
            if (nx < 0 || nx >= L || ny < 0 || ny >= L || nz < 0 || nz >= L)
                continue;

            unsigned int diff;
            if (d < 2) diff = 2 * ax + 1;
            else if (d < 4) diff = 2 * ay + 1;
            else diff = 2 * az + 1;

            unsigned int new_r2 = grid[x][y][z].r2 + diff;
            if (new_r2 < grid_next[nx][ny][nz].r2) {
                grid_next[nx][ny][nz].r2 = new_r2;
            }
        }
    }
}

void pulse_step(void) {
    pulse_update_wavefront();
    grid_next[MID][MID][MID].r2 = 0;

    /* copy r2 back; compute r for newly visited cells */
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        unsigned int old_r2 = grid[x][y][z].r2;
        unsigned int new_r2 = grid_next[x][y][z].r2;
        grid[x][y][z].r2 = new_r2;
        if (new_r2 != INF_R2 && old_r2 == INF_R2) {
            grid[x][y][z].r = isqrt((int)new_r2);
        }
    }

    /* activation flags (product of pulsating CA only) */
    unsigned int pulse_r2 = pulse_from_time((unsigned int)tick);
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        unsigned int r2 = grid[x][y][z].r2;
        if (r2 == INF_R2) {
            grid[x][y][z].active = 0;
        } else {
            unsigned int d = (r2 > pulse_r2)
                           ? (r2 - pulse_r2)
                           : (pulse_r2 - r2);
            grid[x][y][z].active = (d <= PULSE_TOLERANCE) ? 1 : 0;
        }
    }
}
#endif /* !USE_CUDA */

/* ==========================================================
 * Rendering (SDL3)
 * ========================================================== */

#define PEAK_HIST_W  600

static int64_t profile[L];
static int64_t prev_profile[L];
static int  rcount[L];
static int  peak_history[PEAK_HIST_W];
static int  peak_idx = 0;
static int  sinc_stable_frames = 0;
static int  sinc_converged = 0;
static int64_t u_peak = 0;

static void compute_profile(void) {
    for (int i = 0; i < L; i++) {
        prev_profile[i] = profile[i];
        profile[i] = 0;
        rcount[i] = 0;
    }
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        if (grid[x][y][z].r2 == INF_R2) continue;
        int r = grid[x][y][z].r;
        if (r < L) {
            profile[r] += grid[x][y][z].u;
            rcount[r]++;
        }
    }
    for (int i = 0; i < L; i++) {
        if (rcount[i] > 0)
            profile[i] /= rcount[i];
    }
}

static int64_t profile_max_change(void) {
    int64_t maxd = 0;
    for (int i = 0; i < RADIUS; i++) {
        int64_t d = profile[i] - prev_profile[i];
        if (d < 0) d = -d;
        if (d > maxd) maxd = d;
    }
    return maxd;
}

void render_frame(SDL_Renderer *ren) {
#ifdef USE_CUDA
    cuda_download_grid((void *)grid);
    cuda_download_and_count(and_count, L);
#endif

    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    compute_profile();
    int64_t max_change = profile_max_change();

    /* detect sinc convergence → set rationals */
    if (!sinc_converged) {
        if (max_change < STABILITY_THRESHOLD) sinc_stable_frames++;
        else sinc_stable_frames = 0;
        if (sinc_stable_frames >= STABILITY_FRAMES) {
            sinc_converged = 1;
            u_peak = 0;
            for (int r = 0; r < RADIUS; r++)
                if (profile[r] > u_peak) u_peak = profile[r];
            if (u_peak > 0) {
                for (int x = 0; x < L; x++)
                for (int y = 0; y < L; y++)
                for (int z = 0; z < L; z++) {
                    grid[x][y][z].sinc_p = grid[x][y][z].u;
                    grid[x][y][z].sinc_q = (int)u_peak;
                    grid[x][y][z].acc = 0;
                }
            }
            printf("Sinc converged at tick %d — triggers active.\n", tick);
#ifdef USE_CUDA
            cuda_upload_grid_full((void *)grid);
#endif
        }
    }

    int64_t peak = 1;
    for (int r = 0; r < RADIUS; r++) {
        if (rcount[r] > 0 && profile[r] > peak)
            peak = profile[r];
    }
    peak_history[peak_idx] = (int)(peak > INT32_MAX ? INT32_MAX : peak);
    peak_idx = (peak_idx + 1) % PEAK_HIST_W;

    /* -------------------------------------------------------
     * Top-left: sinc displacement slice (z = MID), normalized
     * ------------------------------------------------------- */
    {
        int cz = MID;
        /* find max displacement in this slice for normalization */
        int slice_max = 1;
        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++) {
            int val = grid[x][y][cz].u;
            if (val > slice_max) slice_max = val;
        }

        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++) {
            int c = (grid[x][y][cz].u * 255) / slice_max;
            if (c > 255) c = 255;
            if (c < 0) c = 0;
            SDL_SetRenderDrawColor(ren, (Uint8)c, (Uint8)c, (Uint8)c, 255);
            SDL_RenderPoint(ren, (float)(x + 30), (float)(y + 10));
        }
    }

    /* -------------------------------------------------------
     * Top-middle: trigger + wavefront combined (z = MID)
     * Shows: green=distance, yellow=shell, cyan=trigger, red=fired
     * ------------------------------------------------------- */
    {
        int ox = 30 + L + 20;
        unsigned int pulse_thr = pulse_from_time((unsigned int)tick);

        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++) {
            Cell *c = &grid[x][y][MID];
            uint32_t pix_r = 0, pix_g = 0, pix_b = 0;

            /* layer 1: wavefront distance (green gradient) */
            if (c->r2 != INF_R2) {
                int rr = isqrt((int)c->r2);
                int g = (rr < RADIUS) ? 255 - (rr * 255 / RADIUS) : 0;
                pix_g = (uint32_t)g;
            }

            /* layer 2: wavefront shell (yellow ring, tight visual band) */
            {
                int delta = (int)c->r2 - (int)pulse_thr;
                if (delta < 0) delta = -delta;
                if (delta <= YELLOW_VIS_TOL) {
                    pix_r = 255; pix_g = 255; pix_b = 0;
                }
            }

            SDL_SetRenderDrawColor(ren, (Uint8)pix_r, (Uint8)pix_g, (Uint8)pix_b, 255);
            SDL_RenderPoint(ren, (float)(x + ox), (float)(y + 10));
        }
    }

    /* -------------------------------------------------------
     * Top-right: TTL pattern
     * ------------------------------------------------------- */
    {
        int ox = 30 + L + 20 + L + 20;

        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++) {
            Cell *c = &grid[x][y][MID];

            uint32_t pix_r = c->ttl;
            uint32_t pix_g = c->ttl >> 1;
            uint32_t pix_b = 0;

            SDL_SetRenderDrawColor(
                ren,
                (Uint8)pix_r,
                (Uint8)pix_g,
                (Uint8)pix_b,
                255);

            SDL_RenderPoint(
                ren,
                (float)(x + ox),
                (float)(y + 10));
        }
    }
    /* -------------------------------------------------------
     * Top-far-right: triggering cut (z = MID)
     * Shows cyan dot where Bresenham triggered this tick
     * ------------------------------------------------------- */
    {
        int ox = 30 + L + 20 + L + 20 + L + 20;

        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++) {
            Cell *c = &grid[x][y][MID];

            uint32_t pix_r = 0, pix_g = 0, pix_b = 0;

            if (c->trig) {
                pix_r = 0; pix_g = 255; pix_b = 255;
            }

            SDL_SetRenderDrawColor(
                ren,
                (Uint8)pix_r,
                (Uint8)pix_g,
                (Uint8)pix_b,
                255);

            SDL_RenderPoint(
                ren,
                (float)(x + ox),
                (float)(y + 10));
        }
    }

    /* -------------------------------------------------------
     * Bottom: sinc(r) profile graph
     * ------------------------------------------------------- */
    {
    int px0 = 80;
    int py0 = WINDOW_H - 40;
    int graph_w = RADIUS * GRAPH_SCALE_X;

    /* axis */
    SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
    SDL_RenderLine(ren, (float)px0, (float)py0,
                   (float)(px0 + graph_w), (float)py0);

    /* green: sinc(r) profile */
    SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
    {
        float gpx = -1, gpy = -1;
        for (int r = 0; r < RADIUS; r++) {
            if (rcount[r] > 0) {
                float yf = (float)py0 - ((float)profile[r] * GRAPH_HEIGHT) / (float)PROFILE_PEAK_REF;
                float xf = (float)px0 + (float)(r * GRAPH_SCALE_X);
                if (gpx >= 0)
                    SDL_RenderLine(ren, gpx, gpy, xf, yf);
                gpx = xf;
                gpy = yf;
            }
        }
    }

    /* yellow: peak history */
    SDL_SetRenderDrawColor(ren, 255, 255, 0, 255);
    {
        int max_val = 1;
        for (int i = 0; i < PEAK_HIST_W; i++)
            if (peak_history[i] > max_val) max_val = peak_history[i];
        max_val += (max_val >> 3);

        float last_x = -1, last_y = -1;
        for (int i = 0; i < PEAK_HIST_W; i++) {
            int val = peak_history[i];
            if (val == 0) continue;
            float xf = (float)px0 + ((float)i * (float)graph_w) / (float)PEAK_HIST_W;
            float yf = (float)py0 - ((float)val * (float)GRAPH_HEIGHT) / (float)max_val;
            if (i == peak_idx) last_x = -1;
            if (last_x >= 0)
                SDL_RenderLine(ren, last_x, last_y, xf, yf);
            last_x = xf;
            last_y = yf;
        }
    }

    /* cyan: trigger rate (after convergence) */
    if (sinc_converged) {
        SDL_SetRenderDrawColor(ren, 0, 200, 255, 255);
        float cpx = -1, cpy = -1;
        for (int r = 0; r < RADIUS; r++) {
            float rate = (u_peak > 0) ? (float)profile[r] / (float)u_peak : 0;
            float yf = (float)py0 - rate * (float)GRAPH_HEIGHT;
            float xf = (float)px0 + (float)(r * GRAPH_SCALE_X);
            if (cpx >= 0)
                SDL_RenderLine(ren, cpx, cpy, xf, yf);
            cpx = xf;
            cpy = yf;
        }
    }

    /* dark gray vertical line at current pulse radius */
    {
        unsigned int pulse_r2 = pulse_from_time((unsigned int)tick);
        int cur_r = isqrt((int)pulse_r2);
        if (cur_r < RADIUS) {
            int cx = px0 + cur_r * GRAPH_SCALE_X;
            SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
            SDL_RenderLine(ren, (float)cx, (float)(py0 - GRAPH_HEIGHT),
                           (float)cx, (float)py0);
        }
    }

    /* red: AND-ed point count per shell radius (scatter plot) */
    {
        int max_count = 0;
        for (int r = 0; r < RADIUS; r++)
            if (and_count[r] > max_count)
                max_count = and_count[r];

        if (max_count < 1) max_count = 1;

        SDL_SetRenderDrawColor(ren, 255, 60, 60, 255);

        for (int r = 0; r < RADIUS; r++) {
            if (and_count[r] == 0) continue;

            float yf =
                (float)py0 -
                ((float)and_count[r] / (float)max_count) *
                (float)GRAPH_HEIGHT;

            float xf =
                (float)px0 +
                (float)(r * GRAPH_SCALE_X);

            /* 3×3 dot for visibility */
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    SDL_RenderPoint(ren, xf + dx, yf + dy);
        }
    }
    {
        unsigned int pr2 = pulse_from_time((unsigned int)tick);
        int cr = isqrt((int)pr2);
        printf("\r[tick %4d] peak=%lld stable=%d converged=%d r=%d  ",
               tick, (long long)peak, sinc_stable_frames, sinc_converged, cr);
    }
    fflush(stdout);
    } /* end graph block */

    SDL_RenderPresent(ren);
}

/* ==========================================================
 * Unified step — pulsating wavefront first, then sinc wave
 * ========================================================== */
void step_all(void) {
#ifdef USE_CUDA
    cuda_pulse_step(tick);
    cuda_sinc_step(tick);
#else
    pulse_step();
    sinc_step();
#endif
    tick++;
}

/* ==========================================================
 * Main — runs both CAs in lockstep
 * ========================================================== */
int main(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "CA — sinc(r) + pulsating wavefront",
        WINDOW_W, WINDOW_H, 0);
    if (!window) {
        printf("Window error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        printf("Renderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    /* allocate grids on heap */
    grid      = malloc(sizeof(Cell) * L * L * L);
    grid_next = malloc(sizeof(Cell) * L * L * L);
    if (!grid || !grid_next) {
        printf("Out of memory\n");
        return 1;
    }
    memset(grid,      0, sizeof(Cell) * L * L * L);
    memset(grid_next, 0, sizeof(Cell) * L * L * L);

    /* unified initialization (sinc wave + pulsating wavefront) */
    init();

#ifdef USE_CUDA
    cuda_alloc_grids();
    cuda_upload_grid((void *)grid, (void *)grid_next);
#endif

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = 0;
        }

        step_all();

        render_frame(renderer);
        SDL_Delay(16);
    }

#ifdef USE_CUDA
    cuda_free();
#endif
    free(grid);
    free(grid_next);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
