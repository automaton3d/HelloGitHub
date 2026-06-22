/*
 * spiral_core.h — pure-integer cylindrical helix + pulsating wavefront
 *
 * Drop-in library for detecting wavefront-spiral intersections.
 * No SDL, no rendering, no floats in the CA core.
 *
 * Usage:
 *   1) Allocate grids:  grid = calloc(L*L*L, sizeof(Cell));
 *                       grid_next = calloc(L*L*L, sizeof(Cell));
 *   2) Call spiral_core_init()        — seeds BFS center + paints spiral
 *   3) Each tick: call spiral_core_step()  — advances BFS + updates active
 *   4) Read grid[x][y][z].active && grid[x][y][z].spin for AND detection
 *
 * All runtime CA operations: addition, subtraction, shift, comparison.
 * No multiplication, no division, no loops in cell rules.
 * (Multiplications appear only in compile-time constants and init.)
 */

#ifndef SPIRAL_CORE_H_
#define SPIRAL_CORE_H_

/* --- Compiler portability --- */
#if defined(_MSC_VER) && !defined(__cplusplus)
#define SC_INLINE static __inline
#else
#define SC_INLINE static inline
#endif

#include <stdint.h>

/* =================================================================
 * Grid dimensions — change L here to scale the entire simulation
 * ================================================================= */
#define L 221

#define INF_R2    0xFFFFFFFFu
#define MID       (L / 2)
#define R_MAX     (L / 2)
#define RADIUS    (L / 2 - 2)

/* =================================================================
 * Pulsating sweep parameters
 *
 * PULSE_TOLERANCE = 1 → active shell is exactly 1 cell thick.
 * PULSE_STEP scales with L so the sweep period is L-invariant.
 * ================================================================= */
#define PULSE_TOLERANCE  1
#define PULSE_STEP       (((L) + 15) / 30)

/* =================================================================
 * Cylindrical helix parameters (compile-time constants)
 *
 * The XY projection of the parametric spherical spiral is
 * approximately circular (R_fit/r_max ≈ 0.18103, L-independent).
 *
 * Integer approximation: 93/512 ≈ 0.18164 (<0.4% error).
 * Cylinder axis at (MID, MID + R_CYL), parallel to z.
 * Origin (MID,MID,MID) lies exactly on the cylinder surface.
 * ================================================================= */
#define R_CYL       ((R_MAX * 93 + 256) >> 9)
#define CYL_X0      MID
#define CYL_Y0      (MID + R_CYL)
#define R_CYL_SQ    (R_CYL * R_CYL)

/* Discrete (Manhattan) circumference ≈ 8 × R_CYL.
 * On a von Neumann grid, tracing a circle of Euclidean radius R
 * requires ~8R axial steps (correction factor 4/π ≈ 1.273). */
#define CYL_CIRC    (R_CYL << 3)

/* Total walker steps = circumference (planar) + height (z) */
#define CYL_TOTAL   (CYL_CIRC + R_MAX)

/* =================================================================
 * Cell struct — minimal per-cell state for the CA
 * ================================================================= */
typedef struct {
    int           r;       /* integer radius from center */
    unsigned int  r2;      /* Euclidean distance-squared (INF_R2 = unvisited) */
    unsigned char active;  /* 1 if on pulsating shell this tick */
    unsigned char spin;    /* 1 if on the spiral curve (static after init) */
} Cell;

/* =================================================================
 * Spiral point storage
 *
 * spiral_pts[0..spiral_n-1] holds the grid coordinates of every
 * cell on the helix, in walker order (origin → pole).
 * Populated once during spiral_core_init(); read-only afterwards.
 * ================================================================= */
typedef struct { int x, y, z; } SpiralPt;

#define MAX_SPIRAL_PTS (R_MAX * 3)

extern SpiralPt spiral_pts[];
extern int      spiral_n;     /* number of points on the helix */

/* --- Grid pointers (caller allocates, library uses) --- */
extern Cell (*grid)[L][L];
extern Cell (*grid_next)[L][L];
extern int tick;

/* =================================================================
 * Integer square root (bit-by-bit, no multiplication)
 * ================================================================= */
SC_INLINE int isqrt(int n) {
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

/* =================================================================
 * Pulsating sweep: triangular wave in r2 space
 * (uses multiplication — host-level scheduling, not CA cell rule)
 * ================================================================= */
SC_INLINE unsigned int pulse_from_time(unsigned int t) {
    const unsigned int min_r2 = 0;
    const unsigned int max_r2 =
        (unsigned int)((unsigned int)R_MAX * R_MAX * 92 / 100);
    const unsigned int step = PULSE_STEP;
    unsigned int span = max_r2 - min_r2;
    if (span == 0) return min_r2;
    unsigned int period = span + span;
    unsigned int phase  = (t * step) % period;
    if (phase < span)
        return min_r2 + phase;
    else
        return max_r2 - (phase - span);
}

/* =================================================================
 * Public API
 * ================================================================= */

/* Initialize grid + paint spiral geometry (call once after allocation) */
void spiral_core_init(void);

/* Advance one tick: BFS propagation + active flag update */
void spiral_core_step(void);

#endif /* SPIRAL_CORE_H_ */
