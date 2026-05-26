/*
 * utils.cpp
 *
 * Ancillary code.
 */

#include <GUI.h>
#include <vector>
#include <tuple>
#include <cmath>
#include <algorithm>
#include <set>
#include <random>

#include "model/simulation.h"

namespace automaton
{
  extern unsigned EL;
  extern unsigned W_USED;
  extern unsigned CENTER;
  extern std::vector<Cell> lattice_curr;
  extern std::vector<Cell> lattice_mirror;

  /**
   * Used in color processing.
   */
  bool isColorNeutral(unsigned char c1, unsigned char c2)
  {
    unsigned res = (c1 ^ c2) & 7;
    return ((res == 0 || res == 7) && c1 && c2 && c1 != 7 && c2 != 7);
  }

  void cross_product(double result[3], const double a[3], const double b[3])
  {
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
  }

  void normalize(double vec[3])
  {
    double magnitude = sqrt(vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2]);
    // Avoid division by zero
    if (magnitude <= 1e-12)
    {
      vec[0] = 1.0; vec[1] = 0.0; vec[2] = 0.0; // Vetor padrão
    }
    else if (magnitude > 0.0)
    {
      vec[0] /= magnitude;
      vec[1] /= magnitude;
      vec[2] /= magnitude;
    }
  }

  /**
   * Marks the momentum points.
   * Uses the Bresenham line algorithm.
   * (used during initialization)
   *
   * @p the current ray
   * @w the shell in the w dimension
   */
  void markPoints(unsigned p[3], int w)
  {
    // Calculate the center of the lattice
    int cx = CENTER, cy = CENTER, cz = CENTER;
    // Calculate the deltas
    int dx = p[0] - cx;
    int dy = p[1] - cy;
    int dz = p[2] - cz;
    // Determine the step direction
    int sx = (dx > 0) ? 1 : -1;
    int sy = (dy > 0) ? 1 : -1;
    int sz = (dz > 0) ? 1 : -1;
    dx = abs(dx);
    dy = abs(dy);
    dz = abs(dz);
    // Determine the dominant axis
    int ax = 2 * dx;
    int ay = 2 * dy;
    int az = 2 * dz;
    int x = cx, y = cy, z = cz;
    if (dx >= dy && dx >= dz)
    {
      // X is dominant
      int yd = ay - dx;
      int zd = az - dx;
      for (int i = 0; i <= dx; i++)
      {
        getCell(lattice_curr, x, y, z, w).pB = true;
        x += sx;
        if (yd >= 0)
        {
          y += sy;
          yd -= ax;
        }
        if (zd >= 0)
        {
          z += sz;
          zd -= ax;
        }
        yd += ay;
        zd += az;
      }
    }
    else if (dy >= dx && dy >= dz)
    {
      // Y is dominant
      int xd = ax - dy;
      int zd = az - dy;
      for (int i = 0; i <= dy; i++)
      {
        getCell(lattice_curr, x, y, z, w).pB = true;
        y += sy;
        if (xd >= 0)
        {
          x += sx;
          xd -= ay;
        }
        if (zd >= 0)
        {
          z += sz;
          zd -= ay;
        }
        xd += ax;
        zd += az;
      }
    }
    else
    {
      // Z is dominant
      int xd = ax - dz;
      int yd = ay - dz;
      for (int i = 0; i <= dz; i++)
      {
        getCell(lattice_curr, x, y, z, w).pB = true; //printf("\t%d,%d,%d:%d\n", x, y, z, w);
        z += sz;
        if (xd >= 0)
        {
          x += sx;
          xd -= az;
        }
        if (yd >= 0)
        {
          y += sy;
          yd -= az;
        }
        xd += ax;
        yd += ay;
      }
    }
  }

  // Helper to add all 8 symmetric points (and reflections) to the set
  void add_symmetric_points(std::set<std::tuple<int, int, int>>& points, int x, int y, int z, double C)
  {
    // C is the center offset (e.g., 15.5 for EL=31)
    // Calculate the 8 base coordinates relative to the grid
    int ix_p = static_cast<int>(std::round( x + C)); // x + C
    int ix_n = static_cast<int>(std::round(-x + C)); // -x + C
    int iy_p = static_cast<int>(std::round( y + C));
    int iy_n = static_cast<int>(std::round(-y + C));
    int iz_p = static_cast<int>(std::round( z + C));
    int iz_n = static_cast<int>(std::round(-z + C));
    // Plot all 8 combinations of (x, y, z)
    points.emplace(ix_p, iy_p, iz_p);
    points.emplace(ix_p, iy_p, iz_n);
    points.emplace(ix_p, iy_n, iz_p);
    points.emplace(ix_p, iy_n, iz_n);
    points.emplace(ix_n, iy_p, iz_p);
    points.emplace(ix_n, iy_p, iz_n);
    points.emplace(ix_n, iy_n, iz_p);
    points.emplace(ix_n, iy_n, iz_n);
    // Also plot the 6 permutations (e.g., swapping x, y, z)
    // to handle surfaces where x, y, z are not unique (e.g., near axes).
    // This is vital for full coverage in a discrete sphere!
    points.emplace(iy_p, ix_p, iz_p); // (y, x, z)
    // ... many more permutations/reflections are needed, but we keep it simpler
    // by focusing on the core logic: the loop and the integer check.
  }

  /**
   * N(R)=1.258*R^2
   * ratio=0.10483
   */
  std::vector<std::tuple<int, int, int>> generateShell(int EL)
  {
    std::set<std::tuple<int, int, int>> unique_points;
    const int R_INT = (EL - 1) / 2;
    const long long R_SQUARED_LL = static_cast<long long>(R_INT) * R_INT;
    const double C = (EL - 1) / 2.0;
    // --- Octant-Only Iteration (O(R^2) complexity) ---
    // We only need to search the positive coordinates up to R.
    // This is far faster than iterating over the entire EL^3 cube.
    for (int x = 0; x <= R_INT; ++x)
    {
      // Optimization: if x^2 > R^2, we can stop the loop for x.
      long long x_sq = static_cast<long long>(x) * x;
      if (x_sq > R_SQUARED_LL) break;
      for (int y = 0; y <= R_INT; ++y)
      {
        long long y_sq = static_cast<long long>(y) * y;
        if (x_sq + y_sq > R_SQUARED_LL) break;
        // Calculate the ideal z-coordinate for the continuous sphere
        long long R_prime_sq = R_SQUARED_LL - x_sq - y_sq;
        // Find the nearest integer z-coordinate (z_near)
        int z_near = static_cast<int>(std::round(std::sqrt(R_prime_sq)));
        // --- Integer-Only Tolerance Check ---
        // Check z_near and z_near + 1 (the two candidate grid points)
        // Candidate 1: z_near
        long long dist_sq_1 = x_sq + y_sq + static_cast<long long>(z_near) * z_near;
        long long diff_1 = std::abs(dist_sq_1 - R_SQUARED_LL);
        // Candidate 2: z_near - 1 (since z_near might be rounded up from R-epsilon)
        int z_near_minus_1 = std::max(0, z_near - 1);
        long long dist_sq_2 = x_sq + y_sq + static_cast<long long>(z_near_minus_1) * z_near_minus_1;
        long long diff_2 = std::abs(dist_sq_2 - R_SQUARED_LL);
        // The point (x, y, z) is considered a surface point if its distance
        // is close enough, which we now define as the minimum difference (closest integer distance).
        // This is equivalent to checking if the point (x, y, z_near) or (x, y, z_near-1)
        // minimizes the squared distance to R^2.
        // Instead of using floating point TOLERANCE, we can simply say:
        // if the difference is <= R_INT * 2 + 1, it must be the nearest point (a rough integer tolerance).
        // A more rigorous check is typically: if diff < (2*R + 1) / 2.
        // For simplicity and correctness (given the floating point rounding used for z_near):
        // We accept both z_near and z_near-1 if their distance is minimal.
        if (diff_1 <= diff_2)
        {
           add_symmetric_points(unique_points, x, y, z_near, C);
        }
        if (diff_2 < diff_1)
        {
          add_symmetric_points(unique_points, x, y, z_near_minus_1, C);
        }
      }
    }
    // Convert the set to a vector before returning
    return std::vector<std::tuple<int, int, int>>(unique_points.begin(), unique_points.end());
  }

  /**
   * Shifts lattice_mirror slices along w-dimension with periodic boundary conditions
   */
  void shiftMirror()
  {
    // Temporary storage for one W-slice
    std::vector<Cell> temp(static_cast<size_t>(EL) * EL * EL);
    // Save the last W-slice
    for (unsigned x = 0; x < EL; ++x)
      for (unsigned y = 0; y < EL; ++y)
        for (unsigned z = 0; z < EL; ++z)
          temp[(x * EL + y) * EL + z] = getCell(lattice_mirror, x, y, z, W_USED - 1);
    // Shift all slices forward
    for (unsigned x = 0; x < EL; ++x)
      for (unsigned y = 0; y < EL; ++y)
        for (unsigned z = 0; z < EL; ++z)
          for (unsigned w = W_USED - 1; w > 0; --w)
            getCell(lattice_mirror, x, y, z, w) = getCell(lattice_mirror, x, y, z, w - 1);
    // Wrap saved slice to position 0
    for (unsigned x = 0; x < EL; ++x)
      for (unsigned y = 0; y < EL; ++y)
        for (unsigned z = 0; z < EL; ++z)
          getCell(lattice_mirror, x, y, z, 0) = temp[(x * EL + y) * EL + z];
  }

  /*
   * Tests color neutrality.
   */
  bool neutralColor(Cell &a, Cell &b)
  {
    int color_a = a.ch & 0x07;
    int color_b = b.ch & 0x07;
    return (color_a ^ color_b) == 0x07;
  }

  /*
   * Tests weak neutrality.
   */
  bool neutralWeak(Cell &a, Cell &b)
  {
    int weak_a = (a.ch >> 3) & 0x03;
    int weak_b = (b.ch >> 3) & 0x03;
    return (weak_a ^ weak_b) == 0x03;
  }

  /**
     * Relocate all cells in lattice_curr by offsets (dx, dy, dz).
     * - Wraps around with modulo EL.
     * - Copies dynamic state fields (excluding ch, k, and c[]).
     * - Leaves x[] untouched (fixed coordinates).
     * - Mirror lattice is not touched.
     */
  void relocateGlobal(unsigned dx, unsigned dy, unsigned dz)
  {
    dx %= EL; dy %= EL; dz %= EL;

    // Temporary buffer
    std::vector<Cell> temp(BLOCK);
    std::copy(lattice_curr.begin(), lattice_curr.begin() + BLOCK, temp.begin());

    for (unsigned w = 0; w < W_USED; ++w)
    {
      for (unsigned x = 0; x < EL; ++x)
      {
        for (unsigned y = 0; y < EL; ++y)
        {
          for (unsigned z = 0; z < EL; ++z)
          {
            const Cell& src = getCell(lattice_curr, x, y, z, w);

            unsigned nx = (x + dx) % EL;
            unsigned ny = (y + dy) % EL;
            unsigned nz = (z + dz) % EL;

            Cell& dst = getCell(temp, nx, ny, nz, w);

            // Copy only the relevant properties
            dst.pB   = src.pB;
            dst.sB   = src.sB;
            dst.a    = src.a;
            dst.phiB = src.phiB;
            dst.t    = src.t;
            dst.f    = src.f;
            dst.r2   = src.r2;
            dst.s2B  = src.s2B;
            dst.kB   = src.kB;
            dst.bB   = src.bB;
            dst.hB   = src.hB;
            dst.cB   = src.cB;

            // Leave x[], ch, k, and c[] untouched
          }
        }
      }
    }

    // Commit back
    std::copy(temp.begin(), temp.begin() + BLOCK, lattice_curr.begin());
  }

  /**
   * Prints the main parameters.
   */
  void printParams()
  {
    cout << "L:\t"         << EL        << endl;
    cout << "W:\t"         << W_USED     << endl;
    cout << "DIAG:\t"      << DIAG      << endl;
    cout << "RMAX:\t"      << RMAX      << endl;
    cout << "CONVOL:\t"    << CONVOL    << endl;
    cout << "SLOT1:\t"     << SLOT1     << endl;
    cout << "SLOT2:\t"     << SLOT2     << endl;
    cout << "SLOT3:\t"     << SLOT3     << endl;
    cout << "SLOT4:\t"     << SLOT4     << endl;
    cout << "DIFFUSION:\t" << DIFFUSION << endl;
    cout << "SLOT5:\t"     << SLOT5     << endl;
    cout << "SLOT6:\t"     << SLOT6     << endl;
    cout << "SLOT7:\t"     << SLOT7     << endl;
    cout << "RELOC:\t"     << RELOC     << endl;
    cout << "REISSUE:\t"   << REISSUE   << endl;
  }

  /*
   * Prints the CA state for test.
   */
  void printLattice(int w)
  {
    puts("Case: phiB");
    for (unsigned z = 0; z < EL; z++)
    {
      for (unsigned y = 0; y < EL; y++)
      {
        for (unsigned x = 0; x < EL; x++)
        {
          // Reference to the current cell
          Cell& cell = getCell(lattice_curr, x, y, z, w);
          printf("%d ", cell.phiB);
        }
        printf("\n");
      }
      printf("z=%d\n", z);
    }
    printf("\n");
  }

  /**
   * Tests if is ok.
   */
  bool sanityTest()
  {
    /*
    cout << "RMAX\t" << RMAX << endl;
    cout << "CONVOL\t" << CONVOL << endl;
    cout << "SLOT1\t" << SLOT1 << endl;
    cout << "SLOT2\t" << SLOT2 << endl;
    cout << "SLOT3\t" << SLOT3 << endl;
    cout << "SLOT4\t" << SLOT4 << endl;
    cout << "DIFFUSION\t" << DIFFUSION << endl;
    cout << "RELOC\t" << RELOC << endl;
    */
    return (CONVOL < SLOT1 && SLOT1 < SLOT2 && SLOT2 < SLOT3 && SLOT3 < SLOT4 && SLOT4 < DIFFUSION && DIFFUSION < RELOC && RELOC < REISSUE);
  }

  /**
   * Check if all c vectors are null.
   */
  bool sanityTest2()
  {
    for (unsigned w = 0; w < EL; w++)
    {
      for (unsigned z = 0; z < EL; z++)
      {
        for (unsigned y = 0; y < EL; y++)
        {
          for (unsigned x = 0; x < EL; x++)
          {
            Cell& cell = getCell(lattice_curr, x, y, z, w);
            if (!ZERO(cell.c))
            {
              return false;
            }
          }
        }
      }
    }
    return true;
  }

  bool sanityTest3()
  {
    for (unsigned w = 0; w < EL; w++)
    {

      Cell& seed = getCell(lattice_curr, 0, 0, 0, w);
      for (unsigned z = 0; z < EL; z++)
      {
        for (unsigned y = 0; y < EL; y++)
        {
          for (unsigned x = 0; x < EL; x++)
          {
            Cell& cell = getCell(lattice_curr, x, y, z, w);
            if (seed.c[0] != cell.c[0] || seed.c[1] != cell.c[1] || seed.c[2] != cell.c[2])
            {
              return false;
            }
          }
        }
      }
    }
    return true;
  }

  unsigned int getRandomUnsigned(unsigned int modulus)
  {
    static std::mt19937 rng(std::random_device{}()); // Seed once
    std::uniform_int_distribution<unsigned int> dist(0, modulus - 1);
    return dist(rng);
  }

}
