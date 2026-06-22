/*
 * spiral_core.c — pure-integer cylindrical helix + pulsating wavefront
 *
 * Library implementation. No SDL, no rendering, no main().
 * See spiral_core.h for usage and API documentation.
 */

#include "spiral_core.h"
#include <string.h>

/* =================================================================
 * Global state
 * ================================================================= */
Cell (*grid)[L][L]      = NULL;
Cell (*grid_next)[L][L] = NULL;
int tick = 0;

SpiralPt spiral_pts[MAX_SPIRAL_PTS];
int      spiral_n = 0;

/* =================================================================
 * Spiral geometry generator (walker)
 *
 * Traces a cylindrical helix from origin to pole, marking spin=1.
 * Runs entirely during init — NOT part of the per-tick CA cycle.
 *
 * Walker state (local to this function):
 *   tip_x/y/z  — current grid position
 *   tip_dx/dy  — displacement from cylinder axis (CYL_X0, CYL_Y0)
 *   tip_d2     — distance² to cylinder axis
 *   tip_acc    — Bresenham accumulator for pitch (planar vs z)
 *
 * Direction selection (planar steps):
 *   1) Compute candidate dist² for each of 4 von Neumann neighbors
 *      using incremental formula: dist² ± (component << 1) + 1
 *   2) Filter to CCW-only directions (tangent dot product > 0)
 *   3) Among valid CCW candidates, pick minimum |dist² - R_CYL²|
 *   4) Fallback to any valid direction if no CCW candidate exists
 *
 * Operations: addition, subtraction, shift, comparison.
 * No multiplication, no division (init-time mul in d2 seed is OK).
 * ================================================================= */
static void generate_spiral(void) {
    int tip_x  = MID;
    int tip_y  = MID;
    int tip_z  = MID;
    int tip_dx = MID - CYL_X0;              /* = 0 */
    int tip_dy = MID - CYL_Y0;              /* = -R_CYL */
    int tip_d2 = tip_dx * tip_dx            /* init-time mul OK */
               + tip_dy * tip_dy;            /* = R_CYL_SQ */
    int tip_acc = 0;

    /* mark starting cell */
    grid[MID][MID][MID].spin = 1;
    spiral_pts[0].x = MID;
    spiral_pts[0].y = MID;
    spiral_pts[0].z = MID;
    spiral_n = 1;

    for (int step = 0; step < CYL_TOTAL + 50; step++) {
        /* --- Bresenham: planar vs z-step ---
         * Each tick: acc += R_MAX.
         * If acc >= CYL_TOTAL → z-step (acc -= CYL_TOTAL).
         * Otherwise → planar step. */
        int new_acc = tip_acc + R_MAX;
        int do_z;
        if (new_acc >= CYL_TOTAL) {
            new_acc -= CYL_TOTAL;
            do_z = 1;
        } else {
            do_z = 0;
        }
        tip_acc = new_acc;

        if (do_z) {
            tip_z++;
            if (tip_z >= L) break;
        } else {
            /* ---- planar step: choose best CCW direction ---- */

            /* Candidate dist² (sum-of-odds incremental update) */
            int d2_px = tip_d2 + (tip_dx << 1) + 1;   /* +x */
            int d2_mx = tip_d2 - (tip_dx << 1) + 1;   /* -x */
            int d2_py = tip_d2 + (tip_dy << 1) + 1;   /* +y */
            int d2_my = tip_d2 - (tip_dy << 1) + 1;   /* -y */

            /* Radial error: |dist² - R_CYL²| */
            int e_px = d2_px > R_CYL_SQ ? d2_px - R_CYL_SQ
                                        : R_CYL_SQ - d2_px;
            int e_mx = d2_mx > R_CYL_SQ ? d2_mx - R_CYL_SQ
                                        : R_CYL_SQ - d2_mx;
            int e_py = d2_py > R_CYL_SQ ? d2_py - R_CYL_SQ
                                        : R_CYL_SQ - d2_py;
            int e_my = d2_my > R_CYL_SQ ? d2_my - R_CYL_SQ
                                        : R_CYL_SQ - d2_my;

            /* Validity: in bounds AND not already spin */
            int v_px = (tip_x + 1 < L)
                    && !grid[tip_x + 1][tip_y][tip_z].spin;
            int v_mx = (tip_x - 1 >= 0)
                    && !grid[tip_x - 1][tip_y][tip_z].spin;
            int v_py = (tip_y + 1 < L)
                    && !grid[tip_x][tip_y + 1][tip_z].spin;
            int v_my = (tip_y - 1 >= 0)
                    && !grid[tip_x][tip_y - 1][tip_z].spin;

            /* CCW tangent = (-dy_cyl, +dx_cyl).
             * Dot product with each direction: */
            int t_px = -tip_dy;   /* (+1,0) · tangent */
            int t_mx =  tip_dy;   /* (-1,0) · tangent */
            int t_py =  tip_dx;   /* (0,+1) · tangent */
            int t_my = -tip_dx;   /* (0,-1) · tangent */

            /* Primary: min error among CCW directions (tangent > 0) */
            int best = -1;
            int best_err = 0x7FFFFFFF;

            if (v_px && t_px > 0 && e_px < best_err)
                { best = 0; best_err = e_px; }
            if (v_mx && t_mx > 0 && e_mx < best_err)
                { best = 1; best_err = e_mx; }
            if (v_py && t_py > 0 && e_py < best_err)
                { best = 2; best_err = e_py; }
            if (v_my && t_my > 0 && e_my < best_err)
                { best = 3; best_err = e_my; }

            /* Fallback: any valid direction */
            if (best < 0) {
                if (v_px && e_px < best_err)
                    { best = 0; best_err = e_px; }
                if (v_mx && e_mx < best_err)
                    { best = 1; best_err = e_mx; }
                if (v_py && e_py < best_err)
                    { best = 2; best_err = e_py; }
                if (v_my && e_my < best_err)
                    { best = 3; best_err = e_my; }
            }

            if (best < 0) {
                /* no valid planar direction — force z-step */
                tip_z++;
                if (tip_z >= L) break;
            } else {
                switch (best) {
                case 0: /* +x */
                    tip_d2 += (tip_dx << 1) + 1;
                    tip_dx++;
                    tip_x++;
                    break;
                case 1: /* -x */
                    tip_d2 -= (tip_dx << 1) - 1;
                    tip_dx--;
                    tip_x--;
                    break;
                case 2: /* +y */
                    tip_d2 += (tip_dy << 1) + 1;
                    tip_dy++;
                    tip_y++;
                    break;
                case 3: /* -y */
                    tip_d2 -= (tip_dy << 1) - 1;
                    tip_dy--;
                    tip_y--;
                    break;
                }
            }
        }

        /* mark the new cell */
        if (tip_x >= 0 && tip_x < L &&
            tip_y >= 0 && tip_y < L &&
            tip_z >= 0 && tip_z < L) {
            grid[tip_x][tip_y][tip_z].spin = 1;
            if (spiral_n < MAX_SPIRAL_PTS) {
                spiral_pts[spiral_n].x = tip_x;
                spiral_pts[spiral_n].y = tip_y;
                spiral_pts[spiral_n].z = tip_z;
                spiral_n++;
            }
        }

        /* done when we reach the pole */
        if (tip_z >= MID + R_MAX) break;
    }
}

/* =================================================================
 * BFS wavefront propagation (sum-of-odds for r2)
 *
 * Pure distance propagation — each cell's r2 is the minimum
 * distance-squared from center, computed via 6-neighbor relaxation.
 * This IS the CA: each cell updates based on its neighbors' r2.
 * ================================================================= */
static const int ddx[6] = {1, -1, 0,  0, 0,  0};
static const int ddy[6] = {0,  0, 1, -1, 0,  0};
static const int ddz[6] = {0,  0, 0,  0, 1, -1};

static void bfs_propagate(void) {
    /* snapshot current r2 into grid_next */
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        grid_next[x][y][z].r2 = grid[x][y][z].r2;
    }

    /* relax: for each visited cell, offer r2+diff to its 6 neighbors */
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        if (grid[x][y][z].r2 == INF_R2) continue;

        unsigned int ax = (x > MID) ? (unsigned int)(x - MID)
                                     : (unsigned int)(MID - x);
        unsigned int ay = (y > MID) ? (unsigned int)(y - MID)
                                     : (unsigned int)(MID - y);
        unsigned int az = (z > MID) ? (unsigned int)(z - MID)
                                     : (unsigned int)(MID - z);

        for (int d = 0; d < 6; d++) {
            int nx = x + ddx[d];
            int ny = y + ddy[d];
            int nz = z + ddz[d];
            if (nx < 0 || nx >= L || ny < 0 || ny >= L ||
                nz < 0 || nz >= L)
                continue;

            /* sum-of-odds distance increment */
            unsigned int diff;
            if (d < 2)      diff = (ax << 1) + 1;
            else if (d < 4) diff = (ay << 1) + 1;
            else            diff = (az << 1) + 1;

            unsigned int new_r2 = grid[x][y][z].r2 + diff;
            if (new_r2 < grid_next[nx][ny][nz].r2) {
                grid_next[nx][ny][nz].r2 = new_r2;
            }
        }
    }
}

/* =================================================================
 * spiral_core_init — initialize grid, seed BFS, paint spiral
 *
 * Caller must have allocated grid and grid_next before calling.
 * ================================================================= */
void spiral_core_init(void) {
    /* clear grids */
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        grid[x][y][z].r      = 0;
        grid[x][y][z].r2     = INF_R2;
        grid[x][y][z].active = 0;
        grid[x][y][z].spin   = 0;

        grid_next[x][y][z].r      = 0;
        grid_next[x][y][z].r2     = INF_R2;
        grid_next[x][y][z].active = 0;
        grid_next[x][y][z].spin   = 0;
    }

    /* seed BFS center */
    grid[MID][MID][MID].r2 = 0;
    grid[MID][MID][MID].r  = 0;

    /* generate spiral geometry (static, once) */
    tick = 0;
    generate_spiral();
}

/* =================================================================
 * spiral_core_step — one tick of the CA
 *
 *   1) BFS propagation (sum-of-odds relaxation)
 *   2) Copy-back r2, compute r for newly visited cells
 *   3) Update active flags (pulsating shell, PULSE_TOLERANCE = 1)
 *
 * After this call, for each cell:
 *   grid[x][y][z].active — 1 if on the current wavefront shell
 *   grid[x][y][z].spin   — 1 if on the spiral (static)
 *   AND detection:  active && spin
 * ================================================================= */
void spiral_core_step(void) {
    bfs_propagate();
    grid_next[MID][MID][MID].r2 = 0;

    /* copy back r2; compute r for newly visited cells */
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

    /* update activation flags (pulsating shell) */
    unsigned int pulse_r2 = pulse_from_time((unsigned int)tick);
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        unsigned int r2 = grid[x][y][z].r2;
        if (r2 == INF_R2) {
            grid[x][y][z].active = 0;
        } else {
            unsigned int delta = (r2 > pulse_r2)
                               ? (r2 - pulse_r2)
                               : (pulse_r2 - r2);
            grid[x][y][z].active = (delta <= PULSE_TOLERANCE) ? 1 : 0;
        }
    }

    tick++;
}
