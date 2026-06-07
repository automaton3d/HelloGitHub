/*
 * mytry.c
 *
 *  * Spherical distribution of a sinusoidal function: sin(r)/r
 * (the natural radial solution of the 3-D wave equation).
 *
 * Constraints inside step():
 *   - no floating-point
 *   - no multiplication (*)
 *   - no division (/)
 *   - no lookup tables
 */
#define MYTRY
#ifdef MYTRY

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#define L 101
#define WINDOW_W 1850
#define WINDOW_H 700
#define GRAPH_HEIGHT 500
#define RADIUS      (L/2 - 2)
#define DIFF_SHIFT  4
#define SHELL_R (L * 12 / 100)
#define SHELL_W (L/10)
#define CORE_R 3
#define SHELL_TARGET 16384
#define GRAPH_SCALE_X 16
#define STABILITY_THRESHOLD 35
#define STABILITY_FRAMES    180

typedef struct {
    int u;   // displacement
    int v;   // velocity
    int r2;
    int r;   // precomputed integer radius
    int acc; // accumulator
    int sinc_p; // numerator   of emergent normalized sinc(r)
    int sinc_q; // denominator of emergent normalized sinc(r)
} Cell;

Cell grid[L][L][L];
Cell next[L][L][L];

int tick = 0;

/* Integer square root — shift-only, no division, no multiplication */
int isqrt(int n) {
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

void init() {
    int cx = L/2, cy = L/2, cz = L/2;
    /* Incremental squares: dx² computed without multiply.
     * (d+1)² = d² + (d << 1) + 1  */
    int dx2_table[L];
    for (int i = 0; i < L; i++) {
        int d = i - cx;
        /* compute d² by repeated addition */
        int absd = d < 0 ? -d : d;
        int sq = 0;
        int odd = 1;
        for (int k = 0; k < absd; k++) {
            sq += odd;
            odd += 2;
        }
        dx2_table[i] = sq;
    }

    for(int x=0; x<L; x++)
    for(int y=0; y<L; y++)
    for(int z=0; z<L; z++) {
        int r2 = dx2_table[x] + dx2_table[y] + dx2_table[z];
        int r  = isqrt(r2);
        grid[x][y][z].r2 = r2;
        grid[x][y][z].r  = r;
        grid[x][y][z].u  = 0;
        grid[x][y][z].v  = 0;
        grid[x][y][z].sinc_p = 0;
        grid[x][y][z].sinc_q = 1;
    }
    grid[cx][cy][cz].u = 2048;
}

/*
 * step() – ONE CA tick.  Integer-only: no *, no /, no floats, no tables.
 *
 *   1) 6*u        → (u << 2) + (u << 1)
 *   2) r / 16     → r >> 4
 *   3) CORE_R * 2 → CORE_R << 1
 *   4) Radial boost REMOVED (sinc² emerges from natural 3-D convergence)
 *   5) Boundary multiply replaced with shift cascade
 *   6) r precomputed in init(), no isqrt() call here
 */
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

        /* r / 16 via shift */
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
                v_new -= (excess >> 4);
            }
            else if ((tick & 3) == 0) {
                int deficit = SHELL_TARGET - u;
                v_new += (deficit >> 10) + 1;
            }
        }

        /* NO radial boost — the 3-D wave equation naturally
         * produces sin(kr)/r; removing the boost (seno5.c used
         * base 10) preserves the 1/r envelope and makes the
         * profile scalable across different L values. */

        /* NO core damping — spherical convergence from the
         * shell builds the natural sinc peak at r=0. */

        /* boundary absorption — shift-only replacement for:
         *   u_new = (u_new * (5 - dist)) >> 3               */
        if(r > RADIUS - 4) {
            int dist = r - (RADIUS - 4);
            if (dist >= 4)      u_new = 0;
            else if (dist == 3) u_new = u_new >> 2;
            else if (dist == 2) u_new = (u_new >> 2) + (u_new >> 3);
            else                u_new = u_new >> 1;
        }
        if(r >= RADIUS) u_new = 0;

        if(u_new < 0) u_new = 0;

        /* Bresenham-style accumulator: triggers when acc >= sinc_q */
        int acc = grid[x][y][z].acc + grid[x][y][z].sinc_p;
        if (acc >= grid[x][y][z].sinc_q && grid[x][y][z].sinc_q > 0) {
            acc -= grid[x][y][z].sinc_q;
            /* TRIGGERED — action to be defined later */
        }

        next[x][y][z].u = u_new;
        next[x][y][z].v = v_new;
        next[x][y][z].r2 = grid[x][y][z].r2;
        next[x][y][z].r  = r;
        next[x][y][z].acc = acc;
        next[x][y][z].sinc_p = grid[x][y][z].sinc_p;
        next[x][y][z].sinc_q = grid[x][y][z].sinc_q;
    }

    for(int x=0; x<L; x++)
    for(int y=0; y<L; y++)
    for(int z=0; z<L; z++) {
        grid[x][y][z].u = next[x][y][z].u - (next[x][y][z].u >> 12);
        grid[x][y][z].v = next[x][y][z].v - (next[x][y][z].v >> 12);
        grid[x][y][z].r2 = next[x][y][z].r2;
        grid[x][y][z].r  = next[x][y][z].r;
        grid[x][y][z].acc = next[x][y][z].acc;
        grid[x][y][z].sinc_p = next[x][y][z].sinc_p;
        grid[x][y][z].sinc_q = next[x][y][z].sinc_q;
    }

    tick++;
}

/* ------------------------------------------------------------------ */
/* Rendering with SDL3                                                */
/* ------------------------------------------------------------------ */

#define MAX_TICKS    2000
#define PRINT_EVERY  100
#define PEAK_HIST_W  600

static long profile[L];
static long prev_profile[L];
static int  count[L];
static int  peak_history[PEAK_HIST_W];
static int  peak_idx = 0;
static int  stable_frames = 0;
static int  converged = 0;
static long u_peak = 0;

/* triggered[x][y] = 1 if any cell at (x,y,L/2) triggered this tick */
static int triggered_slice[L][L];

static void compute_profile(void) {
    for (int i = 0; i < L; i++) {
        prev_profile[i] = profile[i];
        profile[i] = 0;
        count[i] = 0;
    }
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        int r = grid[x][y][z].r;
        if (r < L) {
            profile[r] += grid[x][y][z].u;
            count[r]++;
        }
    }
    for (int i = 0; i < L; i++) {
        if (count[i] > 0)
            profile[i] /= count[i];
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

/* Compute trigger slice at z=L/2 for this tick */
static void compute_trigger_slice(void) {
    int cz = L/2;
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++) {
        int acc = grid[x][y][cz].acc + grid[x][y][cz].sinc_p;
        if (acc >= grid[x][y][cz].sinc_q && grid[x][y][cz].sinc_q > 0) {
            triggered_slice[x][y] = 1;
        } else {
            triggered_slice[x][y] = 0;
        }
    }
}

static void render(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    compute_profile();
    long max_change = profile_max_change();

    if (!converged) {
        if (max_change < STABILITY_THRESHOLD) stable_frames++;
        else stable_frames = 0;
        if (stable_frames >= STABILITY_FRAMES) {
            converged = 1;
            /* set rationals from emergent profile */
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
            printf("Converged at tick %d — triggers active.\n", tick);
        }
    }

    /* peak value */
    int peak = 1;
    for (int r = 0; r < RADIUS; r++) {
        if (count[r] > 0 && profile[r] > peak)
            peak = (int)profile[r];
    }
    peak_history[peak_idx] = peak;
    peak_idx = (peak_idx + 1) % PEAK_HIST_W;

    /* --- 1) 2D displacement slice (top-left) --- */
    {
        int cz = L/2;
        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++) {
            int c = grid[x][y][cz].u >> 3;
            if (c > 255) c = 255;
            if (c < 0) c = 0;
            SDL_SetRenderDrawColor(renderer, (Uint8)c, (Uint8)c, (Uint8)c, 255);
            SDL_RenderPoint(renderer, (float)(x + 50), (float)(y + 20));
        }
    }

    /* --- 2) Trigger cloud slice (top-right) --- */
    if (converged) {
        compute_trigger_slice();
        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++) {
            if (triggered_slice[x][y]) {
                SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
            } else {
                int c = grid[x][y][L/2].u >> 5;
                if (c > 80) c = 80;
                SDL_SetRenderDrawColor(renderer, 0, 0, (Uint8)c, 255);
            }
            SDL_RenderPoint(renderer, (float)(x + 200 + L), (float)(y + 20));
        }
    } else {
        /* before convergence, show "waiting" */
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++)
            SDL_RenderPoint(renderer, (float)(x + 200 + L), (float)(y + 20));
    }

    /* --- Graph area (bottom half) --- */
    int px0 = 100;
    int py0 = WINDOW_H - 50;
    int graph_w = RADIUS * GRAPH_SCALE_X;

    /* axis */
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    SDL_RenderLine(renderer, (float)px0, (float)py0,
                   (float)(px0 + graph_w), (float)py0);

    /* --- 3a) Green: emergent sinc(r) profile --- */
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    {
        float gpx = -1, gpy = -1;
        for (int r = 0; r < RADIUS; r++) {
            if (count[r] > 0) {
                int avg = (int)profile[r];
                float yf = (float)py0 - ((float)avg * GRAPH_HEIGHT) / (float)peak;
                float xf = (float)px0 + (float)(r * GRAPH_SCALE_X);
                if (gpx >= 0)
                    SDL_RenderLine(renderer, gpx, gpy, xf, yf);
                gpx = xf;
                gpy = yf;
            }
        }
    }

    /* --- 3b) Yellow: peak history --- */
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
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
                SDL_RenderLine(renderer, last_x, last_y, xf, yf);
            last_x = xf;
            last_y = yf;
        }
    }

    /* --- 3c) Cyan: trigger rate per radius (after convergence) --- */
    if (converged) {
        SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
        float cpx = -1, cpy = -1;
        for (int r = 0; r < RADIUS; r++) {
            /* trigger rate = sinc_p / sinc_q, draw normalized to 1 at peak */
            long sp = profile[r];  /* sinc_p == u at convergence */
            float rate = (u_peak > 0) ? (float)sp / (float)u_peak : 0;
            float yf = (float)py0 - rate * (float)GRAPH_HEIGHT;
            float xf = (float)px0 + (float)(r * GRAPH_SCALE_X);
            if (cpx >= 0)
                SDL_RenderLine(renderer, cpx, cpy, xf, yf);
            cpx = xf;
            cpy = yf;
        }
    }

    printf("\r[tick %4d] peak=%d stable=%d converged=%d  ",
           tick, peak, stable_frames, converged);
    fflush(stdout);

    SDL_RenderPresent(renderer);
}

int main(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "mytry CA — sinc(r) + triggers", WINDOW_W, WINDOW_H, 0);
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

    init();

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = 0;
        }

        step();
        render(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

#endif /* MYTRY */
