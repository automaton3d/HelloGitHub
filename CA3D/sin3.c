/*
 * sin3.c
 *
 * Variant of sin2.c with a spherical cavity mask.
 *
 * Design constraints (same as sin2.c):
 *   - Only integer + and -, bit shifts, boolean logic.
 *   - No multiplications, no floats, no lookup tables, no Manhattan distance.
 *   - Each cell is a simple FSM; nothing that would require a Turing machine.
 *
 * The single change versus sin2.c is that cells whose (Euclidean) r^2 exceeds
 * the cavity radius squared are clamped to 0 each step. Everything else --
 * the 18-point isotropic Laplacian, leapfrog update, radial plot -- is
 * preserved verbatim.
 */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define L                101
#define WINDOW_SIZE      1000
#define MAX_R2           (3 * (L / 2) * (L / 2))
#define R                (L / 2 - 1)   /* cavity radius = 49 */
#define REF_AMPLITUDE_PX 200           /* pixel amplitude of reference sine */

typedef struct
{
    int u;
    int u_old;
} Cell;

Cell grid[L][L][L];
Cell next[L][L][L];

/* R^2, computed once at startup using only repeated addition. */
int R2;

/* -------- r2 sem multiplicacao -------- */
int r2_of(int x, int y, int z, int cx, int cy, int cz)
{
    int dx = x - cx, dy = y - cy, dz = z - cz;

    int r2 = 0;

    int ax = (dx < 0) ? -dx : dx;
    for (int i = 0; i < ax; i++) r2 += ax;

    int ay = (dy < 0) ? -dy : dy;
    for (int i = 0; i < ay; i++) r2 += ay;

    int az = (dz < 0) ? -dz : dz;
    for (int i = 0; i < az; i++) r2 += az;

    return r2;
}

/* Compute R*R once, using only + (no multiplication). */
void init_R2(void)
{
    R2 = 0;
    for (int i = 0; i < R; i++) R2 += R;
}

/* -------- passo CA com cavidade esferica -------- */
void step(void)
{
    int cx = L / 2, cy = L / 2, cz = L / 2;

    for (int x = 1; x < L - 1; x++)
    for (int y = 1; y < L - 1; y++)
    for (int z = 1; z < L - 1; z++)
    {
        int r2 = r2_of(x, y, z, cx, cy, cz);

        /* Fora da cavidade esferica: Dirichlet u = 0.
         * Celulas vizinhas fora da esfera contribuem 0 para f e e na
         * proxima iteracao -- isso implementa a condicao de contorno
         * sem precisar tratar a casca como codigo especial. */
        if (r2 > R2)
        {
            next[x][y][z].u_old = 0;
            next[x][y][z].u     = 0;
            continue;
        }

        int u     = grid[x][y][z].u;
        int u_old = grid[x][y][z].u_old;

        /* 6 faces */
        int f =
            grid[x + 1][y][z].u + grid[x - 1][y][z].u +
            grid[x][y + 1][z].u + grid[x][y - 1][z].u +
            grid[x][y][z + 1].u + grid[x][y][z - 1].u;

        /* 12 arestas */
        int e =
            grid[x + 1][y + 1][z].u + grid[x + 1][y - 1][z].u +
            grid[x - 1][y + 1][z].u + grid[x - 1][y - 1][z].u +

            grid[x + 1][y][z + 1].u + grid[x + 1][y][z - 1].u +
            grid[x - 1][y][z + 1].u + grid[x - 1][y][z - 1].u +

            grid[x][y + 1][z + 1].u + grid[x][y + 1][z - 1].u +
            grid[x][y - 1][z + 1].u + grid[x][y - 1][z - 1].u;

        /* laplaciano 18-pontos balanceado.
         * 12*u implementado como (u<<3) + (u<<2) = 8u + 4u, preservando a
         * restricao "apenas +, -, shifts". */
        int lap = f + (e >> 1) - ((u << 3) + (u << 2));

        /* leapfrog estavel, c^2 * dt^2 / h^2 = 1/16 */
        int u_new = (u << 1) - u_old + (lap >> 4);

        next[x][y][z].u_old = u;
        next[x][y][z].u     = u_new;
    }

    /* copiar buffer */
    for (int x = 1; x < L - 1; x++)
    for (int y = 1; y < L - 1; y++)
    for (int z = 1; z < L - 1; z++)
        grid[x][y][z] = next[x][y][z];
}

/* -------- render -------- */
void render(SDL_Renderer* r)
{
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    int cx = L / 2, cy = L / 2, cz = L / 2;

    /* ---- slice ---- */
    int z = L / 2;

    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    {
        /* mascara esferica tambem no slice (pontos fora ficam pretos) */
        if (r2_of(x, y, z, cx, cy, cz) > R2) continue;

        int val = grid[x][y][z].u;
        if (val < 0) val = -val;

        int c = val >> 4;
        if (c > 255) c = 255;

        SDL_SetRenderDrawColor(r, c, c, c, 255);
        SDL_RenderPoint(r, x + 50, y + 50);
    }

    /* ---- radial bins ---- */
    static int sum[MAX_R2];
    static int count[MAX_R2];

    for (int i = 0; i < MAX_R2; i++)
    {
        sum[i]   = 0;
        count[i] = 0;
    }

    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z2 = 0; z2 < L; z2++)
    {
        int r2 = r2_of(x, y, z2, cx, cy, cz);

        /* apenas celulas dentro da cavidade esferica contribuem */
        if (r2 > R2) continue;

        if (r2 < MAX_R2)
        {
            int val = grid[x][y][z2].u;
            if (val < 0) val = -val;

            sum[r2] += val;
            count[r2]++;
        }
    }

    /* ---- escala automatica ---- */
    int maxv = 1;

    for (int i = 0; i < MAX_R2; i++)
    {
        if (count[i] == 0) continue;

        int v = sum[i] / count[i];
        if (v > maxv) maxv = v;
    }

    int scale = maxv >> 8;
    if (scale == 0) scale = 1;

    /* ---- plot radial ---- */
    int px0 = 400;
    int py0 = 900;

    SDL_SetRenderDrawColor(r, 0, 255, 0, 255);

    for (int i = 1; i < MAX_R2; i++)
    {
        if (count[i] == 0) continue;

        int v = sum[i] / count[i];

        int x = px0 + (i >> 2);
        int y = py0 - (v / scale);

        if (x < WINDOW_SIZE && y > 0) SDL_RenderPoint(r, x, y);
    }

    /* ---- reference sine profile ---- */
    /* The lowest Dirichlet eigenmode of the scalar wave equation in a
     * spherical cavity of radius R is j0(pi r / R), whose |u| envelope is
     * |sin(pi r / R)|. Overlay that pure sine (in yellow) over the averaged
     * |u| radial curve so the CA response can be visually compared against
     * the analytic reference. The overlay is a rendering-only helper and
     * deliberately lives outside the "simple FSM" CA kernel. */
    SDL_SetRenderDrawColor(r, 255, 255, 0, 255);

    for (int i = 0; i <= R2; i++)
    {
        /* same x projection as the green curve: x = px0 + (r^2 >> 2) */
        int x = px0 + (i >> 2);
        if (x >= WINDOW_SIZE) break;

        float rr = sqrtf((float) i);
        float s  = sinf((float) M_PI * rr / (float) R);
        if (s < 0.0f) s = -s;

        int y = py0 - (int) (s * (float) REF_AMPLITUDE_PX);
        if (y > 0 && y < WINDOW_SIZE) SDL_RenderPoint(r, x, y);
    }

    SDL_RenderPresent(r);
}

/* -------- main -------- */
int main(void)
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* w = SDL_CreateWindow(
        "CA onda esferica (cavidade esferica)",
        WINDOW_SIZE, WINDOW_SIZE, 0);

    SDL_Renderer* r = SDL_CreateRenderer(w, NULL);

    init_R2();

    int c = L / 2;

    grid[c][c][c].u     = 2000;
    grid[c][c][c].u_old = 2000;

    SDL_Event e;
    int run = 1;

    while (run)
    {
        while (SDL_PollEvent(&e))
            if (e.type == SDL_EVENT_QUIT) run = 0;

        for (int i = 0; i < 3; i++) step();

        render(r);

        SDL_Delay(16);
    }

    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}
