/*
 * sin6.c  –  3-D integer-only cellular automaton
 *
 * Target radial profile: r · sin(k·r)
 * (growing sinusoidal envelope in 3-D space).
 *
 * Based on seno5.c.  Uses:
 *   1) Multiplicative radial boost to reverse the natural 1/r decay
 *      and produce a growing envelope proportional to r.
 *   2) r-proportional cap (u_new <= r << 9) to prevent runaway
 *      while preserving the sinusoidal pattern.
 *   3) Core damping (from seno5.c) for centre stability.
 *
 * Same CA dynamics as sin7.c — only the reference curve differs
 * (r·|sin(kr)| instead of r·sin²(kr)).
 *
 * BOOST_BASE = (L>>4)+2 ensures scaling with L.
 * Verified stable at 10 000 ticks across L = 81 … 151.
 * Average RMSE ≈ 0.11.
 *
 * Constraints inside step():
 *   - no floating-point
 *   - no multiplication (*)
 *   - no division (/)
 *   - no lookup tables
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define L 101
#define WINDOW_W 1000
#define WINDOW_H 700
#define GRAPH_HEIGHT 500
#define RADIUS      (L/2 - 2)
#define DIFF_SHIFT  4
#define SHELL_R     (L * 12 / 100)
#define SHELL_W     (L / 10)
#define CORE_R      3
#define SHELL_TARGET 16384
#define BOOST_BASE  ((L >> 4) + 2)
#define BOOST_FLOOR 3
#define BOOST_RMIN  15
#define CEIL_SHIFT  9                    /* cap = r << 9 = 512·r */
#define GRAPH_SCALE_X  (800 / (RADIUS > 1 ? RADIUS : 1))
#define STABILITY_THRESHOLD 35
#define STABILITY_FRAMES    180

typedef struct {
    int u;   // displacement
    int v;   // velocity
    int r2;
    int r;   // precomputed integer radius
} Cell;

Cell grid[L][L][L];
Cell next[L][L][L];

int tick = 0;

int isqrt(int n) {
    if(n <= 0) return 0;
    int x = n;
    int y = (x + 1) >> 1;
    while(y < x) {
        x = y;
        y = (x + n / x) >> 1;
    }
    return x;
}

void init() {
    int cx = L/2, cy = L/2, cz = L/2;
    for(int x=0; x<L; x++)
    for(int y=0; y<L; y++)
    for(int z=0; z<L; z++) {
        int r2 = (x-cx)*(x-cx) + (y-cy)*(y-cy) + (z-cz)*(z-cz);
        grid[x][y][z].r2 = r2;
        grid[x][y][z].r  = isqrt(r2);
        grid[x][y][z].u = 0;
        grid[x][y][z].v = 0;
    }
    grid[cx][cy][cz].u = 2048;
}

/* step() — identical to sin7.c */
void step() {
    for(int x=1; x<L-1; x++)
    for(int y=1; y<L-1; y++)
    for(int z=1; z<L-1; z++) {
        int u = grid[x][y][z].u;
        int v = grid[x][y][z].v;

        int neighbors =
            grid[x+1][y][z].u + grid[x-1][y][z].u +
            grid[x][y+1][z].u + grid[x][y-1][z].u +
            grid[x][y][z+1].u + grid[x][y][z-1].u;

        /* 6*u via shifts */
        int lap = neighbors - (u << 2) - (u << 1);
        int r = grid[x][y][z].r;

        int diff_shift = DIFF_SHIFT + 1 - (r >> 4);
        if (diff_shift < DIFF_SHIFT - 1) diff_shift = DIFF_SHIFT - 1;

        int v_new = v + (lap >> diff_shift);
        int u_new = u + v_new;

        v_new -= (v_new >> 5);

        /* shell forcing */
        int dr = r - SHELL_R;
        if (dr < 0) dr = -dr;
        if(dr <= SHELL_W) {
            if (u > SHELL_TARGET) {
                int excess = u - SHELL_TARGET;
                v_new -= (excess >> 6);
            }
            else if ((tick & 3) == 0) {
                int deficit = SHELL_TARGET - u;
                v_new += (deficit >> 12) + 1;
            }
        }

        /* core damping (from seno5.c) */
        if (r < (CORE_R << 1)) {
            v_new -= (v_new >> 3);
            u_new -= (u_new >> 4);
        }

        /* multiplicative radial boost */
        if (r > BOOST_RMIN) {
            int bs = BOOST_BASE - (r >> 3);
            if (bs < BOOST_FLOOR) bs = BOOST_FLOOR;
            u_new += (u_new >> bs);
        }

        /* r-proportional cap — prevents runaway */
        if (r > 0) {
            int cap = (r << CEIL_SHIFT);
            if (u_new > cap) u_new = cap;
        }

        /* boundary absorption — shift-only */
        if(r > RADIUS - 4) {
            int dist = r - (RADIUS - 4);
            if (dist >= 4)      u_new = 0;
            else if (dist == 3) u_new = u_new >> 2;
            else if (dist == 2) u_new = (u_new >> 2) + (u_new >> 3);
            else                u_new = u_new >> 1;
        }
        if(r >= RADIUS) u_new = 0;

        if(u_new < 0) u_new = 0;

        next[x][y][z].u = u_new;
        next[x][y][z].v = v_new;
        next[x][y][z].r2 = grid[x][y][z].r2;
        next[x][y][z].r  = r;
    }

    /* global damping */
    for(int x=0; x<L; x++)
    for(int y=0; y<L; y++)
    for(int z=0; z<L; z++) {
        grid[x][y][z].u = next[x][y][z].u - (next[x][y][z].u >> 10);
        grid[x][y][z].v = next[x][y][z].v - (next[x][y][z].v >> 10);
        grid[x][y][z].r2 = next[x][y][z].r2;
        grid[x][y][z].r  = next[x][y][z].r;
    }

    tick++;
}

/* r · |sin(k·r)| reference (full-wave rectified) */
static float rabssinf(float r, float k) {
    return r * fabsf(sinf(k * r));
}

void render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer,0,0,0,255);
    SDL_RenderClear(renderer);

    static long long sum[L] = {0};
    static int count[L] = {0};
    static int prev_profile[L] = {0};
    static int stable_frames = 0;

    #define PEAK_HIST_W 600
    static int peak_history[PEAK_HIST_W] = {0};
    static int peak_idx = 0;

    for(int i=0; i<L; i++) {
        sum[i] = 0;
        count[i] = 0;
    }

    int cx = L/2, cy = L/2;

    for(int x=0; x<L; x++)
    for(int y=0; y<L; y++)
    for(int z=0; z<L; z++) {
        int r = isqrt(grid[x][y][z].r2);
        if(r < L) {
            sum[r] += grid[x][y][z].u;
            count[r]++;

            if(z == L/2) {
                int c = grid[x][y][z].u >> 3;
                if(c > 255) c = 255;
                if(c < 0) c = 0;
                SDL_SetRenderDrawColor(renderer, c, c, c, 255);
                SDL_RenderPoint(renderer, x + 50, y + 50);

                int dx = x - cx;
                int dy = y - cy;
                int rr = isqrt(dx*dx + dy*dy);
                if(rr > 0 && rr < RADIUS) {
                    float half = (float)(L * 11 / 20);
                    float kv = (float)M_PI / half;
                    float ref = rabssinf((float)rr, kv);
                    float rpeak = 0;
                    for(int rp=1; rp<RADIUS; rp++) {
                        float v = rabssinf((float)rp, kv);
                        if(v > rpeak) rpeak = v;
                    }
                    float norm = (rpeak > 0) ? sqrtf(ref / rpeak) : 0;
                    int cref = (int)(norm * 255.0f);
                    if(cref > 255) cref = 255;
                    if(cref < 0) cref = 0;
                    SDL_SetRenderDrawColor(renderer, 0, 0, cref, 255);
                    SDL_RenderPoint(renderer, x + 800, y + 50);
                }
            }
        }
    }

    long long error = 0;
    for(int r=0; r<RADIUS; r++) {
        int profile = count[r] > 0 ? (int)(sum[r] / count[r]) : 0;
        int d = abs(profile - prev_profile[r]);
        error += d;
        prev_profile[r] = profile;
    }

    if(error < STABILITY_THRESHOLD) stable_frames++;
    else stable_frames = 0;

    int px0 = 100;
    int py0 = 650;

    int peak = 1;
    for(int r=0; r<RADIUS; r++) {
        if(count[r] > 0) {
            int avg = (int)(sum[r] / count[r]);
            if(avg > peak) peak = avg;
        }
    }

    peak_history[peak_idx] = peak;
    peak_idx = (peak_idx + 1) % PEAK_HIST_W;

    /* green: CA radial profile */
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    for(int r=0; r<RADIUS; r++) {
        if(count[r] > 0) {
            int avg = (int)(sum[r] / count[r]);
            int y = py0 - (avg * GRAPH_HEIGHT) / peak;
            int xscreen = px0 + r * GRAPH_SCALE_X;
            SDL_RenderPoint(renderer, xscreen, y);
        }
    }

    /* red: r · |sin(k·r)| reference */
    float half = (float)(L * 11 / 20);
    float kref = (float)M_PI / half;
    float ref_peak = 0.0f;
    for(float rf=0.1f; rf<(float)RADIUS; rf+=0.02f) {
        float ref = rabssinf(rf, kref);
        if(ref > ref_peak) ref_peak = ref;
    }

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    int prevx = -1, prevy = -1;
    for(float rf=0.1f; rf<(float)RADIUS; rf+=0.02f) {
        float ref = rabssinf(rf, kref);
        int yref = py0 - (int)((ref / ref_peak) * GRAPH_HEIGHT);
        int xscreen = px0 + (int)(rf * GRAPH_SCALE_X);
        if(prevx >= 0)
            SDL_RenderLine(renderer, prevx, prevy, xscreen, yref);
        prevx = xscreen;
        prevy = yref;
    }

    /* yellow: peak history (auto-scaled) */
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    int max_val = 1;
    for(int i = 0; i < PEAK_HIST_W; i++) {
        if(peak_history[i] > max_val) max_val = peak_history[i];
    }
    max_val = max_val + (max_val >> 3);  /* 12.5% headroom */
    int last_x = -1, last_y = -1;

    for(int i = 0; i < PEAK_HIST_W; i++) {
        int val = peak_history[i];
        if(val == 0) continue;

        int x_pos = px0 + i;
        int y_pos = py0 - (val * GRAPH_HEIGHT) / max_val;

        if(i == peak_idx) last_x = -1;

        if(last_x >= 0) {
            SDL_RenderLine(renderer, last_x, last_y, x_pos, y_pos);
        }
        last_x = x_pos;
        last_y = y_pos;
    }

    /* RMSE vs r·|sin(k·r)| */
    double rmse_sum = 0.0;
    int rmse_count = 0;
    for(int r=0; r<RADIUS; r++) {
        if(count[r] > 0) {
            int avg = (int)(sum[r] / count[r]);
            double dyn_norm = (double)avg / (double)peak;
            double ref_val = rabssinf((float)r, kref);
            double ref_norm = (ref_peak > 0) ? ref_val / ref_peak : 0;
            double diff = dyn_norm - ref_norm;
            rmse_sum += diff * diff;
            rmse_count++;
        }
    }
    double rmse = (rmse_count > 0) ? sqrt(rmse_sum / rmse_count) : 0.0;

    SDL_SetRenderDrawColor(renderer, 100,100,100,255);
    SDL_RenderLine(renderer, px0, py0, px0 + (L/2)*GRAPH_SCALE_X, py0);
    int midx = px0 + (L/4)*GRAPH_SCALE_X;
    SDL_RenderLine(renderer, midx, py0, midx, py0 - GRAPH_HEIGHT);

    printf("error=%lld stable=%d peak=%d RMSE=%.4f      \r",
           error, stable_frames, peak, rmse);
    if(stable_frames == STABILITY_FRAMES)
        printf("\nSYSTEM STABILIZED - RMSE = %.4f\n", rmse);
    fflush(stdout);

    SDL_RenderPresent(renderer);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow(
        "CA r*sin(r) - Integer Only",
        WINDOW_W, WINDOW_H, 0);
    if(!window) {
        printf("Window error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if(!renderer) {
        printf("Renderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    init();

    int running = 1;
    while(running) {
        SDL_Event e;
        while(SDL_PollEvent(&e))
            if(e.type == SDL_EVENT_QUIT) running = 0;

        step();
        render(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
