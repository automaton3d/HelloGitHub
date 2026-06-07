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

/* --- Grid allocation (heap, too large for stack) --- */
Cell (*grid)[L][L]      = NULL;
Cell (*grid_next)[L][L] = NULL;

const int MID   = L / 2;
const int R_MAX = L / 2;
int tick = 0;

/* ==========================================================
 * Integer square root — shift-only, no division, no multiply
 * ========================================================== */
static int isqrt(int n) {
    if (n <= 0) return 0;
    int result = 0;
    int bit = 1 << 30;
    while (bit > n) bit >>= 2;
    while (bit != 0) {
        if (n >= result + bit) {
            n -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

/* ==========================================================
 * Sinc wave CA — initialization
 * ========================================================== */
void sinc_init(void) {
    int cx = L/2, cy = L/2, cz = L/2;

    /* Incremental squares: d² via sum of odds (no multiply) */
    int dx2_table[L];
    for (int i = 0; i < L; i++) {
        int d = i - cx;
        int absd = d < 0 ? -d : d;
        int sq = 0, odd = 1;
        for (int k = 0; k < absd; k++) {
            sq += odd;
            odd += 2;
        }
        dx2_table[i] = sq;
    }

    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        Cell *c = &grid[x][y][z];
        c->r2   = dx2_table[x] + dx2_table[y] + dx2_table[z];
        c->r    = isqrt(c->r2);
        c->u    = 0;
        c->v    = 0;
        c->acc  = 0;
        c->sinc_p = 0;
        c->sinc_q = 1;
    }
    grid[cx][cy][cz].u = 2048;
}

/* ==========================================================
 * Sinc wave CA — one tick (integer-only in the hot loop)
 * ========================================================== */
void sinc_step(void) {
    for (int x = 1; x < L-1; x++)
    for (int y = 1; y < L-1; y++)
    for (int z = 1; z < L-1; z++) {
        int u = grid[x][y][z].u;
        int v = grid[x][y][z].v;

        int neighbors =
            grid[x+1][y][z].u + grid[x-1][y][z].u +
            grid[x][y+1][z].u + grid[x][y-1][z].u +
            grid[x][y][z+1].u + grid[x][y][z-1].u;

        /* 6*u via shifts */
        int lap = neighbors - (u << 2) - (u << 1);
        int r = grid[x][y][z].r;

        /* r / 16 via shift */
        int diff_shift = DIFF_SHIFT + 1 - (r >> 4);
        if (diff_shift < DIFF_SHIFT - 1) diff_shift = DIFF_SHIFT - 1;

        int v_new = v + (lap >> diff_shift);
        int u_new = u + v_new;

        v_new -= (v_new >> 5);

        /* shell forcing */
        int dr = r - SHELL_R;
        if (dr < 0) dr = -dr;
        if (dr <= SHELL_W) {
            if (u > SHELL_TARGET) {
                int excess = u - SHELL_TARGET;
                v_new -= (excess >> 4);
            }
            else if ((tick & 3) == 0) {
                int deficit = SHELL_TARGET - u;
                v_new += (deficit >> 10) + 1;
            }
        }

        /* boundary absorption — shift-only */
        if (r > RADIUS - 4) {
            int dist = r - (RADIUS - 4);
            if (dist >= 4)      u_new = 0;
            else if (dist == 3) u_new = u_new >> 2;
            else if (dist == 2) u_new = (u_new >> 2) + (u_new >> 3);
            else                u_new = u_new >> 1;
        }
        if (r >= RADIUS) u_new = 0;
        if (u_new < 0) u_new = 0;

        /* Bresenham-style accumulator trigger */
        int acc = grid[x][y][z].acc + grid[x][y][z].sinc_p;
        int triggered = 0;
        if (acc >= grid[x][y][z].sinc_q && grid[x][y][z].sinc_q > 0) {
            acc -= grid[x][y][z].sinc_q;
            triggered = 1;
        }

        /* Persist: if trigger fires while wavefront shell is present */
        int fired = grid[x][y][z].fired;
        if (triggered && grid[x][y][z].wave_r2 != INF_R2) {
            unsigned int pulse_thr = pulse_from_time((unsigned int)tick);
            if (grid[x][y][z].wave_r2 == pulse_thr) {
                fired = 1;
            }
        }

        grid_next[x][y][z].u      = u_new;
        grid_next[x][y][z].v      = v_new;
        grid_next[x][y][z].r2     = grid[x][y][z].r2;
        grid_next[x][y][z].r      = r;
        grid_next[x][y][z].acc    = acc;
        grid_next[x][y][z].sinc_p = grid[x][y][z].sinc_p;
        grid_next[x][y][z].sinc_q = grid[x][y][z].sinc_q;
        grid_next[x][y][z].fired  = fired;
    }

    /* copy back with global damping */
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        grid[x][y][z].u      = grid_next[x][y][z].u - (grid_next[x][y][z].u >> 12);
        grid[x][y][z].v      = grid_next[x][y][z].v - (grid_next[x][y][z].v >> 12);
        grid[x][y][z].r2     = grid_next[x][y][z].r2;
        grid[x][y][z].r      = grid_next[x][y][z].r;
        grid[x][y][z].acc    = grid_next[x][y][z].acc;
        grid[x][y][z].sinc_p = grid_next[x][y][z].sinc_p;
        grid[x][y][z].sinc_q = grid_next[x][y][z].sinc_q;
        grid[x][y][z].fired  = grid_next[x][y][z].fired;
    }
}

/* ==========================================================
 * Pulsating wavefront CA — initialization
 * ========================================================== */
void pulse_init(void) {
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        grid[x][y][z].wave_r2 = INF_R2;
    }
    grid[MID][MID][MID].wave_r2 = 0;
}

/* ==========================================================
 * Pulsating wavefront CA — deterministic pulse threshold
 * ========================================================== */
unsigned int pulse_from_time(unsigned int t) {
    const unsigned int min_r2 = 25;
    const unsigned int max_r2 = (unsigned int)(R_MAX * R_MAX * 0.92);
    const unsigned int step = 7;
    unsigned int span = max_r2 - min_r2;
    if (span == 0) return min_r2;
    unsigned int period = 2 * span;
    unsigned int phase = (t * step) % period;
    if (phase < span)
        return min_r2 + phase;
    else
        return max_r2 - (phase - span);
}

/* ==========================================================
 * Pulsating wavefront CA — one tick
 * ========================================================== */
static void pulse_update_wavefront(void) {
    /* copy current wave_r2 into grid_next */
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++)
        grid_next[x][y][z].wave_r2 = grid[x][y][z].wave_r2;

    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        if (grid[x][y][z].wave_r2 == INF_R2) continue;

        unsigned ax = (x > MID) ? (unsigned)(x - MID) : (unsigned)(MID - x);
        unsigned ay = (y > MID) ? (unsigned)(y - MID) : (unsigned)(MID - y);
        unsigned az = (z > MID) ? (unsigned)(z - MID) : (unsigned)(MID - z);

        /* 6 neighbors */
        int dx[6] = {1, -1, 0,  0, 0,  0};
        int dy[6] = {0,  0, 1, -1, 0,  0};
        int dz[6] = {0,  0, 0,  0, 1, -1};

        for (int d = 0; d < 6; d++) {
            int nx = x + dx[d];
            int ny = y + dy[d];
            int nz = z + dz[d];
            if (nx < 0 || nx >= L || ny < 0 || ny >= L || nz < 0 || nz >= L)
                continue;

            unsigned diff;
            if (d < 2) diff = 2 * ax + 1;
            else if (d < 4) diff = 2 * ay + 1;
            else diff = 2 * az + 1;

            unsigned int new_r2 = grid[x][y][z].wave_r2 + diff;
            if (new_r2 < grid_next[nx][ny][nz].wave_r2) {
                grid_next[nx][ny][nz].wave_r2 = new_r2;
            }
        }
    }
}

void pulse_step(void) {
    pulse_update_wavefront();
    grid_next[MID][MID][MID].wave_r2 = 0;

    /* copy wave_r2 back; clear fired for cells at the new shell position
     * (will be recalculated by the next sinc_step trigger coincidence) */
    unsigned int new_thr = pulse_from_time((unsigned int)(tick + 1));
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        grid[x][y][z].wave_r2 = grid_next[x][y][z].wave_r2;
        if (grid[x][y][z].wave_r2 == new_thr)
            grid[x][y][z].fired = 0;  /* reset — will be recalculated */
    }
}

/* ==========================================================
 * Rendering (SDL3)
 * ========================================================== */

#define PEAK_HIST_W  600

static long profile[L];
static long prev_profile[L];
static int  rcount[L];
static int  peak_history[PEAK_HIST_W];
static int  peak_idx = 0;
static int  sinc_stable_frames = 0;
static int  sinc_converged = 0;
static long u_peak = 0;

static void compute_profile(void) {
    for (int i = 0; i < L; i++) {
        prev_profile[i] = profile[i];
        profile[i] = 0;
        rcount[i] = 0;
    }
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
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

static long profile_max_change(void) {
    long maxd = 0;
    for (int i = 0; i < RADIUS; i++) {
        long d = profile[i] - prev_profile[i];
        if (d < 0) d = -d;
        if (d > maxd) maxd = d;
    }
    return maxd;
}

void render_frame(SDL_Renderer *ren) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    compute_profile();
    long max_change = profile_max_change();

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
        }
    }

    int peak = 1;
    for (int r = 0; r < RADIUS; r++) {
        if (rcount[r] > 0 && profile[r] > peak)
            peak = (int)profile[r];
    }
    peak_history[peak_idx] = peak;
    peak_idx = (peak_idx + 1) % PEAK_HIST_W;

    /* -------------------------------------------------------
     * Top-left: sinc displacement slice (z = MID)
     * ------------------------------------------------------- */
    {
        int cz = MID;
        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++) {
            int c = grid[x][y][cz].u >> 3;
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
            if (c->wave_r2 != INF_R2) {
                int g = 255 - (isqrt((int)c->wave_r2) << 2);
                if (g < 0) g = 0;
                pix_g = (uint32_t)g;
            }

            /* layer 2: wavefront shell (yellow ring) */
            if (c->wave_r2 == pulse_thr) {
                pix_r = 255; pix_g = 255; pix_b = 0;
            }

            /* layer 3: AND result — persistent fired (trigger AND shell coincided) */
            if (c->fired) {
                pix_r = 255; pix_g = 0; pix_b = 0;
            }

            SDL_SetRenderDrawColor(ren, (Uint8)pix_r, (Uint8)pix_g, (Uint8)pix_b, 255);
            SDL_RenderPoint(ren, (float)(x + ox), (float)(y + 10));
        }
    }

    /* -------------------------------------------------------
     * Top-right: fired pattern (initially empty, fills as
     * trigger+wavefront coincidences accumulate)
     * ------------------------------------------------------- */
    {
        int ox = 30 + L + 20 + L + 20;

        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++) {
            Cell *c = &grid[x][y][MID];
            uint32_t pix_r = 0, pix_g = 0, pix_b = 0;

            /* only show cells that have fired (trigger + shell coincidence) */
            if (c->fired) {
                pix_r = 255; pix_g = 0; pix_b = 0;
            }

            SDL_SetRenderDrawColor(ren, (Uint8)pix_r, (Uint8)pix_g, (Uint8)pix_b, 255);
            SDL_RenderPoint(ren, (float)(x + ox), (float)(y + 10));
        }
    }

    /* -------------------------------------------------------
     * Bottom: sinc(r) profile graph
     * ------------------------------------------------------- */
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
                float yf = (float)py0 - ((float)profile[r] * GRAPH_HEIGHT) / (float)peak;
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

    printf("\r[tick %4d] peak=%d stable=%d converged=%d  ",
           tick, peak, sinc_stable_frames, sinc_converged);
    fflush(stdout);

    SDL_RenderPresent(ren);
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

    /* initialize both CAs independently */
    sinc_init();
    pulse_init();

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = 0;
        }

        /* step both CAs */
        sinc_step();
        pulse_step();
        tick++;

        render_frame(renderer);
        SDL_Delay(16);
    }

    free(grid);
    free(grid_next);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
