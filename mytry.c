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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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

static int gcd(int a, int b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { int t = b; b = a % b; a = t; }
    return a ? a : 1;
}

void init() {
    int cx = L/2, cy = L/2, cz = L/2;
    for(int x=0; x<L; x++)
    for(int y=0; y<L; y++)
    for(int z=0; z<L; z++) {
        int r2 = (x-cx)*(x-cx) + (y-cy)*(y-cy) + (z-cz)*(z-cz);
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
/* main() – run the CA, print radial profile, detect stability        */
/* ------------------------------------------------------------------ */

#define MAX_TICKS    2000
#define PRINT_EVERY  100

static long profile[L];       /* radial average of u at each integer r */
static long prev_profile[L];  /* previous frame for stability check    */
static int  count[L];         /* number of cells at each radius        */

/* Compute the radially averaged displacement profile */
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

/* Return max absolute change between current and previous profile */
static long profile_max_change(void) {
    long maxd = 0;
    for (int i = 0; i < RADIUS; i++) {
        long d = profile[i] - prev_profile[i];
        if (d < 0) d = -d;
        if (d > maxd) maxd = d;
    }
    return maxd;
}

/* Print a compact radial profile table to stdout */
static void print_profile(void) {
    printf("# tick=%d  radial profile (r, avg_u)\n", tick);
    for (int r = 0; r < RADIUS; r++) {
        if (count[r] > 0)
            printf("%3d  %7ld\n", r, profile[r]);
    }
    printf("\n");
}

/* Print a single-line status summary */
static void print_status(long max_change) {
    printf("[tick %4d]  u(0)=%7ld  u(shell=%d)=%7ld  max_delta=%ld\n",
           tick, profile[0], SHELL_R, profile[SHELL_R], max_change);
}

int main(void) {
    int stable_count = 0;

    printf("mytry CA — 3-D spherical sinc wave\n");
    printf("Grid: %dx%dx%d  Radius: %d  Shell: r=%d±%d  Target: %d\n",
           L, L, L, RADIUS, SHELL_R, SHELL_W, SHELL_TARGET);
    printf("Running up to %d ticks (stability: delta<%d for %d frames)\n\n",
           MAX_TICKS, STABILITY_THRESHOLD, STABILITY_FRAMES);

    init();

    for (int t = 0; t < MAX_TICKS; t++) {
        step();
        compute_profile();

        long max_change = profile_max_change();

        if (max_change < STABILITY_THRESHOLD)
            stable_count++;
        else
            stable_count = 0;

        if ((tick % PRINT_EVERY) == 0)
            print_status(max_change);

        if (stable_count >= STABILITY_FRAMES) {
            printf("\nConverged at tick %d (stable for %d frames).\n",
                   tick, STABILITY_FRAMES);
            break;
        }
    }

    if (stable_count < STABILITY_FRAMES)
        printf("\nReached max ticks (%d) without full convergence.\n",
               MAX_TICKS);

    /* Build the rational sinc in each cell from the emergent profile.
     * Normalize so that the peak displacement = 1/1. */
    long u_peak = 0;
    for (int r = 0; r < RADIUS; r++)
        if (profile[r] > u_peak) u_peak = profile[r];

    if (u_peak > 0) {
        for (int x = 0; x < L; x++)
        for (int y = 0; y < L; y++)
        for (int z = 0; z < L; z++) {
            int num = grid[x][y][z].u;
            int den = (int)u_peak;
            int g = gcd(num, den);
            grid[x][y][z].sinc_p = num / g;
            grid[x][y][z].sinc_q = den / g;
        }
    }

    printf("\n--- Final radial profile ---\n");
    print_profile();

    printf("\n--- Emergent sinc rational (r, sinc_p/sinc_q) ---\n");
    printf("# peak u = %ld\n", u_peak);
    {
        int cx = L/2, cy = L/2, cz = L/2;
        for (int r = 0; r < RADIUS; r++) {
            int sx = cx + r;
            if (sx < L)
                printf("%3d  %d/%d\n", r,
                       grid[sx][cy][cz].sinc_p,
                       grid[sx][cy][cz].sinc_q);
        }
    }

    /* --- Phase 2: run with Bresenham triggers active --- */
    #define TRIGGER_TICKS 50
    printf("\n--- Phase 2: Bresenham trigger demo (%d ticks) ---\n",
           TRIGGER_TICKS);
    printf("# Each cell triggers at rate sinc_p/sinc_q per tick\n");

    /* reset accumulators */
    for (int x = 0; x < L; x++)
    for (int y = 0; y < L; y++)
    for (int z = 0; z < L; z++)
        grid[x][y][z].acc = 0;

    /* count triggers per radius over TRIGGER_TICKS steps */
    static long triggers[L];
    for (int i = 0; i < L; i++) triggers[i] = 0;

    for (int t = 0; t < TRIGGER_TICKS; t++) {
        for (int x = 1; x < L-1; x++)
        for (int y = 1; y < L-1; y++)
        for (int z = 1; z < L-1; z++) {
            int acc = grid[x][y][z].acc + grid[x][y][z].sinc_p;
            if (acc >= grid[x][y][z].sinc_q && grid[x][y][z].sinc_q > 0) {
                acc -= grid[x][y][z].sinc_q;
                triggers[grid[x][y][z].r]++;
            }
            grid[x][y][z].acc = acc;
        }
    }

    printf("  r  triggers/%d  expected_rate(p/q)\n", TRIGGER_TICKS);
    {
        int cx = L/2, cy = L/2, cz = L/2;
        for (int r = 0; r < RADIUS; r++) {
            int sx = cx + r;
            if (sx < L && count[r] > 0)
                printf("%3d  %7ld      %d/%d\n", r, triggers[r],
                       grid[sx][cy][cz].sinc_p,
                       grid[sx][cy][cz].sinc_q);
        }
    }

    return 0;
}

#endif /* MYTRY */
