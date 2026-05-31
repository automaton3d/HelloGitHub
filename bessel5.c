/*
 * bessel5.c  –  3-D integer-only cellular automaton
 *
 * Target radial profile:  sin²(r) / r   (spherical Bessel j0² envelope)
 *
 * Constraints kept inside step():
 *   • no floating-point arithmetic
 *   • no multiplication operator  (*)
 *   • no division operator         (/)
 *   • no lookup tables
 *
 * All intensity scaling is done with bit-shifts and additions only.
 *
 * Based on seno5.c (sin² version) by Alexandre Neto.
 * Adapted to let the natural 1/r radial decay of the 3-D wave equation
 * show through, producing a sin²(r)/r envelope instead of flat sin²(r).
 */

#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* ── grid and window ──────────────────────────────────────────────── */
#define L           101
#define WINDOW_W    1000
#define WINDOW_H    700
#define GRAPH_HEIGHT 500

/* ── physics ──────────────────────────────────────────────────────── */
#define RADIUS      (L / 2 - 2)          /* 48 */
#define DIFF_SHIFT  4
#define SHELL_R     22
#define SHELL_W     (L / 10)             /* 10 */
#define CORE_R      3
#define SHELL_TARGET 8192                /* lower than sin² (was 16384) */

/* ── visualisation ────────────────────────────────────────────────── */
#define GRAPH_SCALE_X 16
#define STABILITY_THRESHOLD 35
#define STABILITY_FRAMES    180

/* ── cell ─────────────────────────────────────────────────────────── */
typedef struct
{
    int u;   /* displacement  */
    int v;   /* velocity      */
    int r2;  /* squared dist  */
    int r;   /* integer dist (precomputed) */
} Cell;

Cell grid[L][L][L];
Cell next[L][L][L];

int tick = 0;

/* ── integer square-root (Newton) ─────────────────────────────────── */
int isqrt(int n)
{
    if (n <= 0) return 0;
    int x = n;
    int y = (x + 1) >> 1;
    while (y < x)
    {
        x = y;
        y = (x + n / x) >> 1;
    }
    return x;
}

/* ── initialise grid ──────────────────────────────────────────────── */
void init(void)
{
    int cx = L / 2, cy = L / 2, cz = L / 2;
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++)
    {
        int dx = x - cx;
        int dy = y - cy;
        int dz = z - cz;
        int r2 = dx * dx + dy * dy + dz * dz;
        grid[x][y][z].r2 = r2;
        grid[x][y][z].r  = isqrt(r2);
        grid[x][y][z].u  = 0;
        grid[x][y][z].v  = 0;
    }
    grid[cx][cy][cz].u = 2048;
}

/* ────────────────────────────────────────────────────────────────────
 * step()  –  ONE CA tick.  Integer-only, no *, no /, no floats.
 * ──────────────────────────────────────────────────────────────────── */
void step(void)
{
    for (int x = 1; x < L - 1; x++)
    for (int y = 1; y < L - 1; y++)
    for (int z = 1; z < L - 1; z++)
    {
        int u = grid[x][y][z].u;
        int v = grid[x][y][z].v;

        /* 6-neighbour sum */
        int neighbors =
            grid[x + 1][y][z].u + grid[x - 1][y][z].u +
            grid[x][y + 1][z].u + grid[x][y - 1][z].u +
            grid[x][y][z + 1].u + grid[x][y][z - 1].u;

        /* Laplacian:  neighbors - 6*u  →  shifts only */
        int lap = neighbors - (u << 2) - (u << 1);

        int r = grid[x][y][z].r;

        /* radial-dependent diffusion shift */
        int diff_shift = DIFF_SHIFT + 1 - (r >> 4);
        if (diff_shift < DIFF_SHIFT - 1)
            diff_shift = DIFF_SHIFT - 1;

        /* velocity update */
        int v_new = v + (lap >> diff_shift);

        /* displacement update */
        int u_new = u + v_new;

        /* light global damping */
        v_new -= (v_new >> 5);

        /* ── shell forcing ───────────────────────────────────────── */
        int dr = r - SHELL_R;
        if (dr < 0) dr = -dr;
        if (dr <= SHELL_W)
        {
            if (u > SHELL_TARGET)
            {
                int excess = u - SHELL_TARGET;
                v_new -= (excess >> 6);
            }
            else if ((tick & 3) == 0)
            {
                int deficit = SHELL_TARGET - u;
                v_new += (deficit >> 12) + 1;
            }
        }

        /* ── core: mild damping (not as heavy as sin² version) ─── */
        if (r < (CORE_R << 1))
        {
            v_new -= (v_new >> 4);
            u_new -= (u_new >> 5);
        }

        /* ── NO radial boost ─────────────────────────────────────
         * The sin² version added:
         *     u_new += (u_new >> (10 - (r >> 3)));
         * We omit that entirely so the natural 1/r decay of the 3-D
         * wave equation shows through, giving sin²(r)/r instead of
         * flat sin²(r).
         * ─────────────────────────────────────────────────────────── */

        /* ── boundary absorption (shift-only, no multiply) ─────── */
        if (r > RADIUS - 4)
        {
            int dist = r - (RADIUS - 4);
            /* original: u_new = (u_new * (5 - dist)) >> 3
             * replace with shift-based taper: */
            if (dist >= 4)
                u_new = 0;
            else if (dist == 3)
                u_new = u_new >> 2;          /* factor 2/8 = 0.25 */
            else if (dist == 2)
                u_new = (u_new >> 2) + (u_new >> 3); /* 3/8 */
            else /* dist == 1 */
                u_new = u_new >> 1;          /* factor 4/8 = 0.50 */
        }
        if (r >= RADIUS)
            u_new = 0;

        /* non-negative clamp */
        if (u_new < 0) u_new = 0;

        next[x][y][z].u  = u_new;
        next[x][y][z].v  = v_new;
        next[x][y][z].r2 = grid[x][y][z].r2;
        next[x][y][z].r  = r;
    }

    /* global dissipation */
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++)
    {
        grid[x][y][z].u  = next[x][y][z].u - (next[x][y][z].u >> 11);
        grid[x][y][z].v  = next[x][y][z].v - (next[x][y][z].v >> 11);
        grid[x][y][z].r2 = next[x][y][z].r2;
        grid[x][y][z].r  = next[x][y][z].r;
    }

    tick++;
}

/* ────────────────────────────────────────────────────────────────────
 *  sinc²(x) = (sin(x)/x)²   with sinc(0)=1
 * ──────────────────────────────────────────────────────────────────── */
static float sinc2f(float x)
{
    if (fabsf(x) < 1e-6f) return 1.0f;
    float s = sinf(x);
    return (s * s) / (x * x);
}

/* ── render ────────────────────────────────────────────────────────── */
void render(SDL_Renderer *renderer)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    static long long sum[L];
    static int       count[L];
    static int       prev_profile[L];
    static int       stable_frames = 0;

    #define PEAK_HIST_W 600
    static int peak_history[PEAK_HIST_W];
    static int peak_idx = 0;

    for (int i = 0; i < L; i++)
    {
        sum[i]   = 0;
        count[i] = 0;
    }

    int cx = L / 2, cy = L / 2;

    /* accumulate radial profile & draw 2-D slice (z = L/2) */
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++)
    {
        int r = isqrt(grid[x][y][z].r2);
        if (r < L)
        {
            sum[r]   += grid[x][y][z].u;
            count[r]++;

            if (z == L / 2)
            {
                int c = grid[x][y][z].u >> 3;
                if (c > 255) c = 255;
                if (c <   0) c = 0;
                SDL_SetRenderDrawColor(renderer, c, c, c, 255);
                SDL_RenderPoint(renderer, x + 50, y + 50);

                /* reference overlay:  sin²/r   (blue channel) */
                int dx  = x - cx;
                int dy  = y - cy;
                int rr  = isqrt(dx * dx + dy * dy);
                if (rr > 0 && rr < RADIUS)
                {
                    float half = (float)(L / 2);
                    float xval = (float)M_PI * rr / half;
                    float ref  = sinc2f(xval);       /* (sin x / x)² */
                    int cref   = (int)(sqrtf(ref) * 255.0f);
                    if (cref > 255) cref = 255;
                    if (cref <   0) cref = 0;
                    SDL_SetRenderDrawColor(renderer, 0, 0, cref, 255);
                    SDL_RenderPoint(renderer, x + 800, y + 50);
                }
            }
        }
    }

    /* stability tracking */
    long long error = 0;
    for (int r = 0; r < RADIUS; r++)
    {
        int profile = count[r] > 0 ? (int)(sum[r] / count[r]) : 0;
        int d = abs(profile - prev_profile[r]);
        error += d;
        prev_profile[r] = profile;
    }
    if (error < STABILITY_THRESHOLD) stable_frames++;
    else                              stable_frames = 0;

    int px0 = 100;
    int py0 = 650;

    /* find peak of radial average */
    int peak = 1;
    for (int r = 0; r < RADIUS; r++)
    {
        if (count[r] > 0)
        {
            int avg = (int)(sum[r] / count[r]);
            if (avg > peak) peak = avg;
        }
    }

    peak_history[peak_idx] = peak;
    peak_idx = (peak_idx + 1) % PEAK_HIST_W;

    /* ── green: CA radial profile ─────────────────────────────────── */
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    for (int r = 0; r < RADIUS; r++)
    {
        if (count[r] > 0)
        {
            int avg     = (int)(sum[r] / count[r]);
            int y       = py0 - (avg * GRAPH_HEIGHT) / peak;
            int xscreen = px0 + r * GRAPH_SCALE_X;
            SDL_RenderPoint(renderer, xscreen, y);
        }
    }

    /* ── red: sin²(x)/x reference ─────────────────────────────────── */
    float ref_peak = 0.0f;
    float half     = (float)(L / 2);
    for (float rf = 0.1f; rf < (float)RADIUS; rf += 0.02f)
    {
        float xv  = (float)M_PI * rf / half;
        float ref = sinc2f(xv);
        if (ref > ref_peak) ref_peak = ref;
    }

    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    int prevx = -1, prevy = -1;
    for (float rf = 0.1f; rf < (float)RADIUS; rf += 0.02f)
    {
        float xv  = (float)M_PI * rf / half;
        float ref = sinc2f(xv);
        int yref    = py0 - (int)((ref / ref_peak) * GRAPH_HEIGHT);
        int xscreen = px0 + (int)(rf * GRAPH_SCALE_X);
        if (prevx >= 0)
            SDL_RenderLine(renderer, prevx, prevy, xscreen, yref);
        prevx = xscreen;
        prevy = yref;
    }

    /* ── yellow: peak history ─────────────────────────────────────── */
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    int max_val = L * L;
    int last_x  = -1, last_y = -1;
    for (int i = 0; i < PEAK_HIST_W; i++)
    {
        int val = peak_history[i];
        if (val == 0) continue;
        int x_pos = px0 + i;
        int y_pos = py0 - (val * GRAPH_HEIGHT) / max_val;
        if (i == peak_idx) last_x = -1;
        if (last_x >= 0)
            SDL_RenderLine(renderer, last_x, last_y, x_pos, y_pos);
        last_x = x_pos;
        last_y = y_pos;
    }

    /* ── RMSE vs sin²(x)/x ───────────────────────────────────────── */
    double rmse_sum   = 0.0;
    int    rmse_count = 0;
    for (int r = 0; r < RADIUS; r++)
    {
        if (count[r] > 0)
        {
            int    avg      = (int)(sum[r] / count[r]);
            double dyn_norm = (double)avg / (double)peak;
            double xv       = (double)M_PI * (double)r / (double)half;
            double ref_norm;
            if (r == 0)
                ref_norm = 1.0 / ref_peak;
            else
                ref_norm = sinc2f((float)xv) / ref_peak;
            double diff = dyn_norm - ref_norm;
            rmse_sum += diff * diff;
            rmse_count++;
        }
    }
    double rmse = (rmse_count > 0) ? sqrt(rmse_sum / rmse_count) : 0.0;

    /* axes */
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderLine(renderer, px0, py0, px0 + (L / 2) * GRAPH_SCALE_X, py0);
    int midx = px0 + (L / 4) * GRAPH_SCALE_X;
    SDL_RenderLine(renderer, midx, py0, midx, py0 - GRAPH_HEIGHT);

    printf("error=%lld stable=%d peak=%d RMSE=%.4f      \r",
           error, stable_frames, peak, rmse);
    if (stable_frames == STABILITY_FRAMES)
        printf("\nSYSTEM STABILIZED - RMSE = %.4f\n", rmse);
    fflush(stdout);

    SDL_RenderPresent(renderer);
}

/* ── main ─────────────────────────────────────────────────────────── */
int main(void)
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow(
        "CA j0^2 = sin^2(r)/r  –  Integer-Only",
        WINDOW_W, WINDOW_H, 0);
    if (!window)
    {
        printf("Window error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer)
    {
        printf("Renderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    init();

    int running = 1;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_EVENT_QUIT) running = 0;

        step();
        render(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
