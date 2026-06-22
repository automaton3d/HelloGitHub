/*
 * spiral.h — header for emergent spiral CA on pulsating wavefront
 *
 * The spiral is a cylindrical helix traced by a walker:
 *   1) BFS wavefront propagation (sum-of-odds for r2)
 *   2) Walker advances 1 cell/tick along a cylinder of radius R_CYL
 *      centered at (CYL_X0, CYL_Y0), parallel to z
 *   3) Bresenham accumulator interleaves planar (rotation) and
 *      vertical (climb) steps for correct pitch
 *
 * Runtime operations: addition, subtraction, shift, comparison only.
 * No multiplication, no division, no lookup tables, no floats.
 */

#ifndef SPIRAL_H_
#define SPIRAL_H_

/* --- MSVC portability --- */
#if defined(_MSC_VER) && !defined(__cplusplus)
#define SINLINE static __inline
#else
#define SINLINE static inline
#endif

/* --- SDL (only for host rendering) --- */
#ifndef NO_SDL
#include <SDL3/SDL.h>
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

/* --- Pulsating sweep parameters (L-invariant) --- */
#define PULSE_TOLERANCE  1
#define PULSE_STEP       (((L) + 15) / 30)

/* --- Cylindrical helix parameters ---
 *
 * The XY projection of the parametric spherical spiral is
 * approximately circular. Best-fit circle (exact, L-independent):
 *   center = (-0.0195*r_max, +0.1810*r_max)  ≈ (0, R_CYL)
 *   radius = 0.18103*r_max                    ≈ R_CYL
 *
 * Compile-time integer approximation: 93/512 ≈ 0.18164 (<0.4% error).
 * Multiplications below are compile-time constants, not runtime.
 */
#define R_CYL       ((R_MAX * 93 + 256) >> 9)
#define CYL_X0      MID
#define CYL_Y0      (MID + R_CYL)
#define R_CYL_SQ    (R_CYL * R_CYL)

/* Discrete (Manhattan) circumference of radius-R_CYL circle.
 * On a von Neumann grid, axial steps cover 2πR Euclidean distance
 * in approximately 8R steps (correction factor 4/π ≈ 1.273). */
#define CYL_CIRC    (R_CYL << 3)
/* Total walker steps = circumference (planar) + height (z) */
#define CYL_TOTAL   (CYL_CIRC + R_MAX)

/* --- Display --- */
#define WINDOW_W  (L + 500 + 80)
#define WINDOW_H  (L + 280)

/* Yellow ring visual tolerance (thin) */
#define YELLOW_VIS_TOL  ((RADIUS / 35) > 1 ? (RADIUS / 35) : 1)

/* =================================================================
 * Cell struct — minimal for spiral CA
 * ================================================================= */
typedef struct {
    int r;                /* integer radius from center */
    unsigned int r2;      /* Euclidean distance-squared (INF_R2 = unvisited) */
    unsigned char active; /* 1 if on pulsating shell */
    unsigned char spin;   /* 1 if on the spiral arm */
} Cell;

/* --- Spiral point storage (for walker + rendering) --- */
typedef struct { int x, y, z; } SpiralPt;

#define MAX_SPIRAL_PTS (R_MAX * 3)
extern SpiralPt spiral_pts[];
extern int spiral_n;
extern int spiral_done;

/* --- Grid pointers (heap-allocated) --- */
extern Cell (*grid)[L][L];
extern Cell (*grid_next)[L][L];
extern int tick;

/* =================================================================
 * Integer square root (bit-by-bit, no multiplication)
 * ================================================================= */
SINLINE int isqrt(int n) {
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
 * (uses multiplication — host-only, not part of the CA FSM)
 * ================================================================= */
SINLINE unsigned int pulse_from_time(unsigned int t) {
    const unsigned int min_r2 = 0;
    const unsigned int max_r2 = (unsigned int)((unsigned int)R_MAX * R_MAX * 92 / 100);
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

/* --- Host-side functions (spiral.c) --- */
void init(void);
void step_all(void);
void pulse_step(void);
void spiral_init(void);
void spiral_step(void);

#ifndef NO_SDL
void render_frame(SDL_Renderer *ren);
#endif

#endif /* SPIRAL_H_ */
