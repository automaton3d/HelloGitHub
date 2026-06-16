/*
 * spiral.c — emergent spiral CA on pulsating wavefront
 *
 * The spiral pattern emerges from purely local BFS propagation:
 *   - Each cell carries a CORDIC direction vector (spiral_x, spiral_y)
 *   - On z-transitions during BFS, the vector rotates via shift+add
 *   - A Bresenham accumulator distributes rotations evenly over z-levels
 *   - Spin is marked by tracing Bresenham lines per z-level
 *
 * Runtime CA operations: addition, subtraction, shift, comparison.
 * No multiplication, no division, no lookup tables, no floats.
 * (pulse_from_time and rendering use multiplication — host-only)
 */

#include "spiral.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* --- Grid allocation (heap, too large for stack) --- */
Cell (*grid)[L][L]      = NULL;
Cell (*grid_next)[L][L] = NULL;

int tick = 0;



/* ==========================================================
 * Initialization
 * ========================================================== */
void init(void) {
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        Cell *c = &grid[x][y][z];
        c->r        = 0;
        c->r2       = INF_R2;
        c->active   = 0;
        c->spin     = 0;
        c->spiral_x = 0;
        c->spiral_y = 0;
        c->spiral_acc = 0;

        Cell *cn = &grid_next[x][y][z];
        cn->r        = 0;
        cn->r2       = INF_R2;
        cn->active   = 0;
        cn->spin     = 0;
        cn->spiral_x = 0;
        cn->spiral_y = 0;
        cn->spiral_acc = 0;
    }

    /* seed center */
    grid[MID][MID][MID].r2 = 0;
    grid[MID][MID][MID].r  = 0;
    grid[MID][MID][MID].spiral_x = RADIUS;
    grid[MID][MID][MID].spiral_y = 0;
    grid[MID][MID][MID].spiral_acc = 0;


}

/* ==========================================================
 * BFS wavefront propagation (sum-of-odds for r2)
 *
 * Also propagates CORDIC spiral state:
 *   - same z: inherit (spiral_x, spiral_y, spiral_acc)
 *   - different z: advance Bresenham acc, apply CORDIC if triggered
 *
 * CORDIC rotation (shift+add only):
 *   new_sx = sx - (sy >> CORDIC_SHIFT)
 *   new_sy = sy + (sx >> CORDIC_SHIFT)
 *
 * Direction: going UP (nz > z) rotates CCW,
 *            going DOWN (nz < z) rotates CW.
 * This creates a continuous spiral from pole to pole.
 * ========================================================== */

/* 6-neighbor offsets */
static const int ddx[6] = {1, -1, 0,  0, 0,  0};
static const int ddy[6] = {0,  0, 1, -1, 0,  0};
static const int ddz[6] = {0,  0, 0,  0, 1, -1};

static void pulse_update_wavefront(void) {
    /* snapshot current r2 + spiral state into grid_next */
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        grid_next[x][y][z].r2        = grid[x][y][z].r2;
        grid_next[x][y][z].spiral_x  = grid[x][y][z].spiral_x;
        grid_next[x][y][z].spiral_y  = grid[x][y][z].spiral_y;
        grid_next[x][y][z].spiral_acc = grid[x][y][z].spiral_acc;
    }

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

        int src_sx  = grid[x][y][z].spiral_x;
        int src_sy  = grid[x][y][z].spiral_y;
        int src_acc = grid[x][y][z].spiral_acc;

        for (int d = 0; d < 6; d++) {
            int nx = x + ddx[d];
            int ny = y + ddy[d];
            int nz = z + ddz[d];
            if (nx < 0 || nx >= L || ny < 0 || ny >= L ||
                nz < 0 || nz >= L)
                continue;

            /* sum-of-odds distance update */
            unsigned int diff;
            if (d < 2)      diff = (ax << 1) + 1;
            else if (d < 4) diff = (ay << 1) + 1;
            else            diff = (az << 1) + 1;

            unsigned int new_r2 = grid[x][y][z].r2 + diff;
            if (new_r2 < grid_next[nx][ny][nz].r2) {
                grid_next[nx][ny][nz].r2 = new_r2;

                /* propagate CORDIC spiral state */
                if (nz != z) {
                    /* z-transition: advance Bresenham accumulator */
                    int new_acc = src_acc + CORDIC_N;
                    int new_sx  = src_sx;
                    int new_sy  = src_sy;

                    if (new_acc >= R_MAX) {
                        new_acc -= R_MAX;
                        /* apply CORDIC micro-rotation */
                        int old_sx = new_sx;
                        if (nz > z) {
                            /* going UP → CCW */
                            new_sx = old_sx - (new_sy >> CORDIC_SHIFT);
                            new_sy = new_sy + (old_sx >> CORDIC_SHIFT);
                        } else {
                            /* going DOWN → CW */
                            new_sx = old_sx + (new_sy >> CORDIC_SHIFT);
                            new_sy = new_sy - (old_sx >> CORDIC_SHIFT);
                        }
                    }

                    grid_next[nx][ny][nz].spiral_x   = new_sx;
                    grid_next[nx][ny][nz].spiral_y   = new_sy;
                    grid_next[nx][ny][nz].spiral_acc  = new_acc;
                } else {
                    /* same z: inherit unchanged */
                    grid_next[nx][ny][nz].spiral_x   = src_sx;
                    grid_next[nx][ny][nz].spiral_y   = src_sy;
                    grid_next[nx][ny][nz].spiral_acc  = src_acc;
                }
            }
        }
    }
}

/* ==========================================================
 * pulse_step — BFS + copy-back + activation flags
 * ========================================================== */
void pulse_step(void) {
    pulse_update_wavefront();
    grid_next[MID][MID][MID].r2 = 0;

    /* copy back r2, spiral state; compute r for newly visited */
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++) {
        unsigned int old_r2 = grid[x][y][z].r2;
        unsigned int new_r2 = grid_next[x][y][z].r2;
        grid[x][y][z].r2         = new_r2;
        grid[x][y][z].spiral_x   = grid_next[x][y][z].spiral_x;
        grid[x][y][z].spiral_y   = grid_next[x][y][z].spiral_y;
        grid[x][y][z].spiral_acc  = grid_next[x][y][z].spiral_acc;
        if (new_r2 != INF_R2 && old_r2 == INF_R2) {
            grid[x][y][z].r = isqrt((int)new_r2);
        }
    }

    /* activation flags (pulsating shell) */
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
}

/* ==========================================================
 * spiral_step — mark spin=1 as a 1D spiral curve
 *
 * Marks exactly ONE cell per integer radius r (0..RADIUS),
 * forming a 3D spiral from center to surface.
 *
 * For each r, a Bresenham accumulator assigns a z-level
 * (distributing SPIRAL_Z_SPAN z-steps over RADIUS r-steps).
 * At that z-level, we walk the Bresenham line in the CORDIC
 * direction and find the cell whose radius matches r.
 *
 * Result: ~RADIUS cells total, 1 per radius shell.
 * The expanding wavefront crosses the spiral at exactly 1 point.
 *
 * Operations: addition, subtraction, comparison (Bresenham walk).
 * ========================================================== */
void spiral_step(void) {
    int z_acc = 0;
    int cur_z = MID;

    for (int r = 0; r <= RADIUS; r++) {
        /* Skip if BFS hasn't reached this z-level yet */
        if (cur_z < 0 || cur_z >= L) goto advance_z;
        if (grid[MID][MID][cur_z].r2 == INF_R2) goto advance_z;

        {
            int sx = grid[MID][MID][cur_z].spiral_x;
            int sy = grid[MID][MID][cur_z].spiral_y;

            /* Walk Bresenham line from (MID,MID) toward (MID+sx,MID+sy)
             * and find the cell closest to radius r */
            int x0 = MID, y0 = MID;
            int x1 = MID + sx, y1 = MID + sy;
            if (x1 < 0) x1 = 0;
            if (x1 >= L) x1 = L - 1;
            if (y1 < 0) y1 = 0;
            if (y1 >= L) y1 = L - 1;

            int dx = x1 > x0 ? x1 - x0 : x0 - x1;
            int dy = y1 > y0 ? y1 - y0 : y0 - y1;
            int step_x = x0 < x1 ? 1 : -1;
            int step_y = y0 < y1 ? 1 : -1;
            int err = dx - dy;

            int best_x = -1, best_y = -1;
            int best_diff = RADIUS + 1;

            for (;;) {
                if (x0 >= 0 && x0 < L && y0 >= 0 && y0 < L &&
                    grid[x0][y0][cur_z].r2 != INF_R2) {
                    int cell_r = grid[x0][y0][cur_z].r;
                    int diff = cell_r > r ? cell_r - r : r - cell_r;
                    if (diff < best_diff) {
                        best_diff = diff;
                        best_x = x0;
                        best_y = y0;
                        if (diff == 0) break;
                    }
                }
                if (x0 == x1 && y0 == y1) break;
                int e2 = err + err;
                if (e2 > -dy) { err -= dy; x0 += step_x; }
                if (e2 <  dx) { err += dx; y0 += step_y; }
            }

            if (best_x >= 0) {
                grid[best_x][best_y][cur_z].spin = 1;
            }
        }

    advance_z:
        /* Bresenham accumulator: distribute SPIRAL_Z_SPAN z-steps
         * over RADIUS r-steps */
        z_acc += SPIRAL_Z_SPAN;
        while (z_acc >= RADIUS) {
            z_acc -= RADIUS;
            cur_z++;
        }
    }
}

/* ==========================================================
 * Unified step — pulsating wavefront, then spiral marking
 * ========================================================== */
void step_all(void) {
    pulse_step();
    spiral_step();
    tick++;
}

/* ==========================================================
 * Rendering (SDL3)
 * ========================================================== */
#ifndef NO_SDL

void render_frame(SDL_Renderer *ren) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    unsigned int pulse_r2 = pulse_from_time((unsigned int)tick);
    int cur_r = isqrt((int)pulse_r2);

    /* -------------------------------------------------------
     * Panel 1 (top-left): wavefront z=MID slice
     *   green = distance gradient, yellow = active shell
     * ------------------------------------------------------- */
    {
        int ox = 30;
        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++) {
            Cell *c = &grid[x][y][MID];
            unsigned int pr = 0, pg = 0, pb = 0;

            if (c->r2 != INF_R2) {
                int rr = isqrt((int)c->r2);
                int g = (rr < RADIUS) ? 255 - (rr * 255 / RADIUS) : 0;
                pg = (unsigned int)g;
            }

            {
                int delta = (int)c->r2 - (int)pulse_r2;
                if (delta < 0) delta = -delta;
                if (delta <= YELLOW_VIS_TOL) {
                    pr = 255; pg = 255; pb = 0;
                }
            }

            SDL_SetRenderDrawColor(ren,
                (Uint8)pr, (Uint8)pg, (Uint8)pb, 255);
            SDL_RenderPoint(ren, (float)(x + ox), (float)(y + 10));
        }
    }

    /* -------------------------------------------------------
     * Panel 2 (top-right, large): 3D isometric view of spiral
     *   Re-traces Bresenham lines per z-level, projects each
     *   point isometrically.  cyan = spin, white = spin+active.
     * ------------------------------------------------------- */
    {
        int panel_x = 30 + L + 40;
        int panel_y = 10;
        int panel_w = 480;
        int panel_h = L;
        float cx = (float)(panel_x + panel_w / 2);
        float cy = (float)(panel_y + panel_h / 2);
        /* scale to fit sphere in panel (true isometric: sphere radius = R_MAX on screen) */
        float scale = (float)panel_h / (3.2f * (float)R_MAX);

        /* isometric projection coefficients:
         * azimuth ≈ 30°, slight elevation to see z-axis */
        float ax = 0.866f;   /* cos(30°) */
        float ay = 0.5f;     /* sin(30°) */
        float ez = 1.0f;     /* vertical z scale (true isometric) */

        /* Re-trace the spiral curve (same logic as spiral_step)
         * and render each point isometrically. */
        {
            int z_acc = 0;
            int rz = MID;
            for (int r = 0; r <= RADIUS; r++) {
                if (rz >= 0 && rz < L &&
                    grid[MID][MID][rz].r2 != INF_R2) {
                    int sx = grid[MID][MID][rz].spiral_x;
                    int sy = grid[MID][MID][rz].spiral_y;

                    int x0 = MID, y0 = MID;
                    int x1 = MID + sx, y1 = MID + sy;
                    if (x1 < 0) x1 = 0;
                    if (x1 >= L) x1 = L - 1;
                    if (y1 < 0) y1 = 0;
                    if (y1 >= L) y1 = L - 1;

                    int ddx = x1 > x0 ? x1 - x0 : x0 - x1;
                    int ddy = y1 > y0 ? y1 - y0 : y0 - y1;
                    int step_x = x0 < x1 ? 1 : -1;
                    int step_y = y0 < y1 ? 1 : -1;
                    int err = ddx - ddy;
                    int best_x = -1, best_y = -1;
                    int best_diff = RADIUS + 1;

                    for (;;) {
                        if (x0 >= 0 && x0 < L && y0 >= 0 && y0 < L &&
                            grid[x0][y0][rz].r2 != INF_R2) {
                            int cell_r = grid[x0][y0][rz].r;
                            int diff = cell_r > r ? cell_r - r : r - cell_r;
                            if (diff < best_diff) {
                                best_diff = diff;
                                best_x = x0;
                                best_y = y0;
                                if (diff == 0) break;
                            }
                        }
                        if (x0 == x1 && y0 == y1) break;
                        int e2 = err + err;
                        if (e2 > -ddy) { err -= ddy; x0 += step_x; }
                        if (e2 <  ddx) { err += ddx; y0 += step_y; }
                    }

                    if (best_x >= 0) {
                        int ldx = best_x - MID;
                        int ldy = best_y - MID;
                        int dz = rz - MID;
                        float px = cx + ((float)ldx - (float)ldy) * ax * scale;
                        float py = cy - (float)dz * ez * scale
                                 + ((float)ldx + (float)ldy) * ay * scale;

                        if (grid[best_x][best_y][rz].active) {
                            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                        } else {
                            int bright = 150 + (ldx + ldy) / 4;
                            if (bright > 255) bright = 255;
                            if (bright < 80) bright = 80;
                            SDL_SetRenderDrawColor(ren,
                                0, (Uint8)bright, (Uint8)bright, 255);
                        }
                        /* 2x2 pixel for visibility */
                        SDL_FRect rc = { px - 1, py - 1, 3, 3 };
                        SDL_RenderFillRect(ren, &rc);
                    }
                }
                z_acc += SPIRAL_Z_SPAN;
                while (z_acc >= RADIUS) {
                    z_acc -= RADIUS;
                    rz++;
                }
            }
        }

        /* --- Coordinate axes (RGB = XYZ) --- */
        {
            float axis_len = (float)RADIUS * scale;

            /* X axis (red): points right-front */
            float x_end_px = cx + axis_len * ax;
            float x_end_py = cy + axis_len * ay;
            SDL_SetRenderDrawColor(ren, 180, 50, 50, 255);
            SDL_RenderLine(ren, cx, cy, x_end_px, x_end_py);

            /* Y axis (green): points left-front */
            float y_end_px = cx - axis_len * ax;
            float y_end_py = cy + axis_len * ay;
            SDL_SetRenderDrawColor(ren, 50, 180, 50, 255);
            SDL_RenderLine(ren, cx, cy, y_end_px, y_end_py);

            /* Z axis (blue): points up */
            float z_end_py = cy - (float)R_MAX * ez * scale;
            SDL_SetRenderDrawColor(ren, 80, 80, 255, 255);
            SDL_RenderLine(ren, cx, cy, cx, z_end_py);
            /* also draw negative z */
            float z_neg_py = cy + (float)R_MAX * ez * scale;
            SDL_SetRenderDrawColor(ren, 40, 40, 120, 255);
            SDL_RenderLine(ren, cx, cy, cx, z_neg_py);
        }

        /* --- Cosmetic sphere silhouette (dark gray circle in screen space) ---
         * In orthographic projection, a sphere's silhouette is always a
         * perfect circle of radius R*scale, regardless of viewing angle. */
        {
            int n_seg = 64;
            int i;
            float sil_r = (float)RADIUS * scale;
            float prev_px2 = 0.0f, prev_py2 = 0.0f;

            SDL_SetRenderDrawColor(ren, 35, 35, 35, 255);
            for (i = 0; i <= n_seg; i++) {
                float angle = (float)i * 6.2832f / (float)n_seg;
                float ppx = cx + sil_r * cosf(angle);
                float ppy = cy + sil_r * sinf(angle);
                if (i > 0)
                    SDL_RenderLine(ren, prev_px2, prev_py2, ppx, ppy);
                prev_px2 = ppx;
                prev_py2 = ppy;
            }
        }
    }

    /* -------------------------------------------------------
     * Panel 3 (bottom-right): XY top-view of spiral
     *   Shows spiral points projected onto XY plane (ignoring z).
     *   Cyan = spin, white = spin+active, gray circle = wavefront.
     * ------------------------------------------------------- */
    {
        int tp_x = 30 + L + 40;
        int tp_y = L + 30;
        int tp_sz = 200;   /* square panel */
        float tp_scale = (float)tp_sz / (2.2f * (float)RADIUS);
        float tp_cx = (float)tp_x + (float)tp_sz * 0.5f;
        float tp_cy = (float)tp_y + (float)tp_sz * 0.5f;

        /* Draw wavefront circle (current radius) */
        {
            int n_seg = 48;
            float prev_px2 = 0.0f, prev_py2 = 0.0f;
            SDL_SetRenderDrawColor(ren, 60, 60, 0, 255);
            for (int i = 0; i <= n_seg; i++) {
                float angle = (float)i * 6.2832f / (float)n_seg;
                float px2 = tp_cx + (float)cur_r * cosf(angle) * tp_scale;
                float py2 = tp_cy + (float)cur_r * sinf(angle) * tp_scale;
                if (i > 0) {
                    SDL_RenderLine(ren, prev_px2, prev_py2, px2, py2);
                }
                prev_px2 = px2;
                prev_py2 = py2;
            }
        }

        /* Draw outer boundary circle */
        {
            int n_seg = 48;
            float prev_px2 = 0.0f, prev_py2 = 0.0f;
            SDL_SetRenderDrawColor(ren, 40, 40, 40, 255);
            for (int i = 0; i <= n_seg; i++) {
                float angle = (float)i * 6.2832f / (float)n_seg;
                float px2 = tp_cx + (float)RADIUS * cosf(angle) * tp_scale;
                float py2 = tp_cy + (float)RADIUS * sinf(angle) * tp_scale;
                if (i > 0) {
                    SDL_RenderLine(ren, prev_px2, prev_py2, px2, py2);
                }
                prev_px2 = px2;
                prev_py2 = py2;
            }
        }

        /* Draw spiral points (XY projection) */
        {
            int z_acc2 = 0;
            int rz2 = MID;
            for (int r = 0; r <= RADIUS; r++) {
                if (rz2 >= 0 && rz2 < L &&
                    grid[MID][MID][rz2].r2 != INF_R2) {
                    int sx = grid[MID][MID][rz2].spiral_x;
                    int sy = grid[MID][MID][rz2].spiral_y;

                    int x0 = MID, y0 = MID;
                    int x1 = MID + sx, y1 = MID + sy;
                    if (x1 < 0) x1 = 0;
                    if (x1 >= L) x1 = L - 1;
                    if (y1 < 0) y1 = 0;
                    if (y1 >= L) y1 = L - 1;

                    int ddx = x1 > x0 ? x1 - x0 : x0 - x1;
                    int ddy = y1 > y0 ? y1 - y0 : y0 - y1;
                    int step_x = x0 < x1 ? 1 : -1;
                    int step_y = y0 < y1 ? 1 : -1;
                    int err = ddx - ddy;
                    int best_x2 = -1, best_y2 = -1;
                    int best_diff2 = RADIUS + 1;

                    for (;;) {
                        if (x0 >= 0 && x0 < L && y0 >= 0 && y0 < L &&
                            grid[x0][y0][rz2].r2 != INF_R2) {
                            int cell_r = grid[x0][y0][rz2].r;
                            int diff = cell_r > r ? cell_r - r : r - cell_r;
                            if (diff < best_diff2) {
                                best_diff2 = diff;
                                best_x2 = x0;
                                best_y2 = y0;
                                if (diff == 0) break;
                            }
                        }
                        if (x0 == x1 && y0 == y1) break;
                        int e2 = err + err;
                        if (e2 > -ddy) { err -= ddy; x0 += step_x; }
                        if (e2 <  ddx) { err += ddx; y0 += step_y; }
                    }

                    if (best_x2 >= 0) {
                        float px2 = tp_cx + (float)(best_x2 - MID) * tp_scale;
                        float py2 = tp_cy + (float)(best_y2 - MID) * tp_scale;

                        if (grid[best_x2][best_y2][rz2].active) {
                            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
                            SDL_FRect rc = { px2 - 2, py2 - 2, 5, 5 };
                            SDL_RenderFillRect(ren, &rc);
                        } else {
                            SDL_SetRenderDrawColor(ren, 0, 200, 200, 255);
                            SDL_FRect rc = { px2 - 1, py2 - 1, 3, 3 };
                            SDL_RenderFillRect(ren, &rc);
                        }
                    }
                }
                z_acc2 += SPIRAL_Z_SPAN;
                while (z_acc2 >= RADIUS) {
                    z_acc2 -= RADIUS;
                    rz2++;
                }
            }
        }

        /* Cross-hair at center */
        SDL_SetRenderDrawColor(ren, 50, 50, 50, 255);
        SDL_RenderLine(ren, tp_cx - 5, tp_cy, tp_cx + 5, tp_cy);
        SDL_RenderLine(ren, tp_cx, tp_cy - 5, tp_cx, tp_cy + 5);
    }

    /* -------------------------------------------------------
     * Bottom graph: unwrapped spiral — z on x-axis, octant on y
     *   white dots = target direction (spiral_x, spiral_y)
     *   at each z-level (from z-axis cell)
     * ------------------------------------------------------- */
    {
        int gx0 = 30;
        int gy0 = L + 50;
        int gw  = L;                /* graph width = L pixels */
        int gh  = 180;              /* graph height */

        /* axis */
        SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
        SDL_RenderLine(ren, (float)gx0, (float)(gy0 + gh),
                       (float)(gx0 + gw), (float)(gy0 + gh));
        SDL_RenderLine(ren, (float)gx0, (float)gy0,
                       (float)gx0, (float)(gy0 + gh));

        /* labels */
        /* (text rendering would need SDL_ttf; skip for now) */

        /* plot spiral angle per z-level */
        /* angle proxy: atan2(spiral_y, spiral_x) mapped to [0, gh] */
        /* Using spiral_y as a proxy for angle (it goes from 0 to
         * ±RADIUS over half a turn) */
        SDL_SetRenderDrawColor(ren, 255, 200, 0, 255);
        for (int z = 0; z < L; z++) {
            if (grid[MID][MID][z].r2 == INF_R2) continue;
            int sy = grid[MID][MID][z].spiral_y;
            /* map sy from [-RADIUS, +RADIUS] to [0, gh] */
            int py = gy0 + gh / 2 - (sy * gh / (RADIUS + RADIUS + 1));
            if (py < gy0) py = gy0;
            if (py > gy0 + gh) py = gy0 + gh;
            int px = gx0 + z;
            SDL_RenderPoint(ren, (float)px, (float)py);
        }

        /* also plot spiral_x */
        SDL_SetRenderDrawColor(ren, 100, 200, 255, 255);
        for (int z = 0; z < L; z++) {
            if (grid[MID][MID][z].r2 == INF_R2) continue;
            int sx = grid[MID][MID][z].spiral_x;
            int py = gy0 + gh / 2 - (sx * gh / (RADIUS + RADIUS + 1));
            if (py < gy0) py = gy0;
            if (py > gy0 + gh) py = gy0 + gh;
            int px = gx0 + z;
            SDL_RenderPoint(ren, (float)px, (float)py);
        }

        /* vertical marker at current sweep radius
         * (mapped from r to z: the sweep covers cells at all z-levels,
         *  so we show center±cur_r) */
        if (cur_r < R_MAX) {
            SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
            int z1 = MID + cur_r;
            int z2 = MID - cur_r;
            if (z1 < L) {
                SDL_RenderLine(ren,
                    (float)(gx0 + z1), (float)gy0,
                    (float)(gx0 + z1), (float)(gy0 + gh));
            }
            if (z2 >= 0) {
                SDL_RenderLine(ren,
                    (float)(gx0 + z2), (float)gy0,
                    (float)(gx0 + z2), (float)(gy0 + gh));
            }
        }
    }

    /* status line */
    {
        int spin_count = 0;
        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++) {
            if (grid[x][y][MID].spin) spin_count++;
        }
        printf("\r[tick %4d] r=%3d  spin@MID=%5d  CORDIC_SHIFT=%d  CORDIC_N=%d  ",
               tick, cur_r, spin_count, CORDIC_SHIFT, CORDIC_N);
        fflush(stdout);
    }

    SDL_RenderPresent(ren);
}

#endif /* !NO_SDL */

/* ==========================================================
 * Main (requires SDL)
 * ========================================================== */
#ifndef NO_SDL
int main(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "CA — emergent spiral on pulsating wavefront",
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
        printf("Out of memory (need ~%lu MB)\n",
               (unsigned long)(2 * sizeof(Cell) * L * L * L / (1024*1024)));
        return 1;
    }
    memset(grid,      0, sizeof(Cell) * L * L * L);
    memset(grid_next, 0, sizeof(Cell) * L * L * L);

    init();

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

    free(grid);
    free(grid_next);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
#endif /* !NO_SDL */
