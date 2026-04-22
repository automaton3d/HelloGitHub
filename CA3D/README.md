# CA3D — 3D cellular-automaton wave experiments

Small C programs that simulate the scalar wave equation on a 101³ integer lattice
and render a slice + radial plot via SDL3. The CA is written under a strict
"simple FSM" discipline:

- integer `+`, `-`, and bit shifts;
- boolean logic;
- no multiplication, no floats, no lookup tables, no Manhattan distance.

## Programs

| File | Cavity | Notes |
|------|--------|-------|
| [`sin2.c`](sin2.c) | cubic (Dirichlet on the `[1..L-2]³` box) | original impulse experiment |
| [`sin3.c`](sin3.c) | **spherical** (Dirichlet on `r > R`) | adds a spherical mask; `R = L/2 - 1 = 49`, `R² = 2401` |

Both share:

- 18-point isotropic Laplacian (6 faces at weight 1, 12 edges at weight 1/2).
- Leapfrog integrator with `c²·Δt²/h² = 1/16` — comfortably inside the CFL bound.
- Point impulse at the center: `u = u_old = 2000`.
- Render: `z = L/2` grayscale slice + green radial plot of the per-`r²` average of `|u|`.

## Build

SDL3 is required (https://libsdl.org/).

```sh
cc -std=c11 -O2 -Wall sin2.c -o sin2 $(pkg-config --cflags --libs sdl3)
cc -std=c11 -O2 -Wall sin3.c -o sin3 $(pkg-config --cflags --libs sdl3)
```

## The spherical-mask trick in `sin3.c`

To keep the "simple FSM" discipline, we cannot call `sqrt()`, so we compare
squared distances. We also avoid `*`:

- `R²` is computed once at startup by repeated addition: `for i<R: R2 += R`.
- Per cell we reuse `r2_of(...)` from `sin2.c`, which computes `dx² + dy² + dz²`
  with repeated addition.
- `u·12` in the Laplacian is rewritten as `(u<<3) + (u<<2)`.
- Cells with `r² > R²` are clamped each step to `{u = 0, u_old = 0}`. Their
  neighbours see those zeros as Dirichlet, so the cavity boundary is spherical.

### Smoke test

A headless harness (`test_headless` in the staging workspace) runs 60 steps and
checks:

- `R2 == 2401` (= 49·49)
- every cell with `r² > R²` is zero
- full octahedral symmetry across the three coordinate axes

All pass. Axial profile after 60 steps:
```
u[c+k][c][c] for k = 0..15:
  -434 -373 -363 -383 -393 -368 -367 -383 -360 -364 -359 -348 -348 -339 -336 -334
```
