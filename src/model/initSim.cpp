/*
 * initSim.cpp
 *
 * Gather all initialization routines.
 */

#include "model/simulation.h"
#include "model/geometry.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <random>
#include <cassert>
#include <array>
#include "globals.h"
#include "layers.h"

namespace automaton
{
  using namespace std;

  // DEBUG
  void relocateAllWRandom();

  constexpr double PI =
    3.14159265358979323846264338327950288;

  // Global variables for lattice
  extern std::vector<Cell> lattice_curr;
  extern std::vector<Cell> lattice_draft;
  extern std::vector<Cell> lattice_mirror;

  // Global dimensions
  vector<unsigned> dirs;
  vector<WPoint> wpoints;

  extern std::vector<std::array<unsigned, 3>> lcenters;

  inline size_t index(unsigned x, unsigned y, unsigned z, unsigned w)
  {
    return (((size_t)w * EL + x) * EL + y) * EL + z;
  }

  /**
   * Function to initialize the lattice with general data.
   */
void initGeneral()
{
    const double R = EL / 2.0;
    
    printf("initGeneral: EL=%u, RMAX=%u\n", EL, RMAX);
    
    for (unsigned w = 0; w < W_USED; ++w)
    {
        // Layer-specific center
        unsigned cx = lcenters[w][0];
        unsigned cy = lcenters[w][1];
        unsigned cz = lcenters[w][2];
        
        printf("  Layer %u: center=(%u,%u,%u)\n", w, cx, cy, cz);
        
        for (unsigned x = 0; x < EL; ++x)
        {
            for (unsigned y = 0; y < EL; ++y)
            {
                for (unsigned z = 0; z < EL; ++z)
                {
                    Cell& cell = getCell(lattice_curr, x, y, z, w);
                    
                    // Basic configuration
                    char w0 = w % 2;
                    char w1 = (w >> 1) % 2;
                    char q = w0 ^ w1;
                    
                    cell.ch = (w % 8) | (q << 3) | (w0 << 4) | (w1 << 5);
                    cell.x[0] = x;
                    cell.x[1] = y;
                    cell.x[2] = z;
                    cell.x[3] = w;
                    
                    // Calculate distance from layer center
                    int dx = (int)x - (int)cx;
                    int dy = (int)y - (int)cy;
                    int dz = (int)z - (int)cz;
                    int r2 = dx*dx + dy*dy + dz*dz;
                    int R2 = RMAX * RMAX;
                    
                    if (r2 <= R2) {
                        cell.d = (unsigned)sqrt((double)r2);
                        cell.a = w;
                    } else {
                        cell.d = RMAX;
                        cell.a = W_USED;  // Orphan outside sphere
                    }
                    
                    // Initialize flags
                    cell.pB = false;
                    cell.sB = false;
                    cell.phiB = false;
                    cell.t = 0;
                    cell.f = 0;
                    cell.s2B = false;
                    cell.kB = false;
                    cell.bB = false;
                    cell.hB = false;
                    cell.cB = false;
                    cell.c[0] = 0;
                    cell.c[1] = 0;
                    cell.c[2] = 0;
                }
            }
        }
    }
    
    puts("initGeneral ok.");
}

  /*
   * Initialize momentum directions
   */
  void initMomentum()
  {
    const unsigned cx = CENTER;
    const unsigned cy = CENTER;
    const unsigned cz = CENTER;
    
    printf("initMomentum: looking for shell points with R=%u\n", RMAX);

    auto tuples = generateShell(static_cast<int>(EL));
    
    printf("initMomentum: found %zu shell points\n", tuples.size());
    
    unsigned n = static_cast<unsigned>(std::floor(1 / 0.10483));
    unsigned w = 0;
    
    dirs.clear();
    
    for (unsigned i = 0; i < n; i++)
    {
      for (auto& t : tuples)
      {
        if (w >= W_USED)
          break;
          
        int xx, yy, zz;
        std::tie(xx, yy, zz) = t;
        
        unsigned ux = (unsigned)xx;
        unsigned uy = (unsigned)yy;
        unsigned uz = (unsigned)zz;
        
        // Ensure point is inside sphere
        if (!isInsideSphere((int)ux, (int)uy, (int)uz))
          continue;
          
        dirs.push_back(ux);
        dirs.push_back(uy);
        dirs.push_back(uz);
        
        unsigned p[3] = { ux, uy, uz };
        markPoints(p, static_cast<unsigned>(w));
        
        w++;
      }
    }
    
    printf("initMomentum: requested W_USED=%u, actually initialized w=%u\n", W_USED, w);
    printf("initMomentum: dirs.size()=%zu\n", dirs.size());
    
    puts("initMomentum ok.");
  }

  /*
   * Initialize sine² density (phiB)
   */
  void initSine2()
  {
    const double cx = (EL - 1) / 2.0;
    const double cy = cx;
    const double cz = cx;
    const double R = EL / 2.0;
    
    std::mt19937 gen(42);
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    int phiB_count = 0;
    
    for (unsigned w = 0; w < W_USED; w++)
    {
      for (unsigned i = 0; i < EL; ++i)
      {
        double dx = i - cx;
        
        for (unsigned j = 0; j < EL; ++j)
        {
          double dy = j - cy;
          
          for (unsigned k = 0; k < EL; ++k)
          {
            // Skip cells outside sphere
            if (!isInsideSphere((int)i, (int)j, (int)k))
              continue;
              
            double dz = k - cz;
            double r = sqrt(dx*dx + dy*dy + dz*dz);
            
            if (r <= R) {
              double theta = PI * r / R;
              double prob = sin(theta) * sin(theta);
              
              if (dis(gen) < prob)
              {
                getCell(lattice_curr, i, j, k, w).phiB = true;
                phiB_count++;
              }
            }
          }
        }
      }
    }
    
    printf("initSine2: %d cells with phiB=true\n", phiB_count);
    puts("initSine2 ok.");
  }

  /*
   * Rodrigues rotation
   */
  void rotateAroundAxis(
      const double p[3],
      const double k[3],
      double theta,
      double result[3])
  {
    double cosT = cos(theta);
    double sinT = sin(theta);
    
    double cross[3] =
    {
      k[1]*p[2] - k[2]*p[1],
      k[2]*p[0] - k[0]*p[2],
      k[0]*p[1] - k[1]*p[0]
    };
    
    double dot = k[0]*p[0] + k[1]*p[1] + k[2]*p[2];
    
    for (int i = 0; i < 3; ++i)
    {
      result[i] = p[i]*cosT + cross[i]*sinT + k[i]*dot*(1 - cosT);
    }
  }

  /*
   * Initialize spirals (sB)
   */
  void initSpirals()
  {
    if (dirs.size() < 3 * W_USED)
    {
      fprintf(stderr, "FATAL: dirs[] has only %zu elements\n", dirs.size());
      return;
    }
    
    const int num_points = 10 * EL;
    
    std::vector<double> theta(num_points);
    std::vector<double> r(num_points);
    std::vector<double> x_curve(num_points);
    std::vector<double> y_curve(num_points);
    std::vector<double> z_curve(num_points);
    
    // Generate 3D spiral (helix)
    for (int i = 0; i < num_points; ++i)
    {
      theta[i] = 2 * PI * i / num_points;
      r[i] = (double)EL / (4 * PI) * theta[i];
      x_curve[i] = r[i] * cos(theta[i]);
      y_curve[i] = r[i] * sin(theta[i]);
      z_curve[i] = r[i];
    }
    
    const double cx = EL / 2.0;
    const double cy = EL / 2.0;
    const double cz = EL / 2.0;
    
    int sB_count = 0;
    
    for (unsigned w = 0; w < W_USED; ++w)
    {
      size_t base = w * 3;
      
      double k[3] =
      {
        static_cast<double>(dirs[base]) - cx,
        static_cast<double>(dirs[base + 1]) - cy,
        static_cast<double>(dirs[base + 2]) - cz
      };
      
      normalize(k);
      
      double z_axis[3] = { 0.0, 0.0, 1.0 };
      
      double axis[3] =
      {
        z_axis[1]*k[2] - z_axis[2]*k[1],
        z_axis[2]*k[0] - z_axis[0]*k[2],
        z_axis[0]*k[1] - z_axis[1]*k[0]
      };
      
      double axis_len = sqrt(axis[0]*axis[0] + axis[1]*axis[1] + axis[2]*axis[2]);
      
      if (axis_len < 1e-12)
      {
        axis[0] = 1.0;
        axis[1] = 0.0;
        axis[2] = 0.0;
        axis_len = 1.0;
      }
      
      axis[0] /= axis_len;
      axis[1] /= axis_len;
      axis[2] /= axis_len;
      
      double dot = z_axis[0]*k[0] + z_axis[1]*k[1] + z_axis[2]*k[2];
      dot = max(-1.0, min(1.0, dot));
      double angle = acos(dot);
      
      for (int i = 0; i < num_points; ++i)
      {
        double p[3] = { x_curve[i], y_curve[i], z_curve[i] };
        double pr[3];
        
        rotateAroundAxis(p, axis, angle, pr);
        
        int x = (int)round(pr[0] + cx);
        int y = (int)round(pr[1] + cy);
        int z = (int)round(pr[2] + cz);
        
        if (x >= 0 && x < (int)EL &&
            y >= 0 && y < (int)EL &&
            z >= 0 && z < (int)EL &&
            isInsideSphere(x, y, z))
        {
          getCell(lattice_curr, (unsigned)x, (unsigned)y, (unsigned)z, w).sB = true;
          sB_count++;
        }
      }
    }
    
    printf("initSpirals: %d cells with sB=true\n", sB_count);
    printf("initSpirals ok - %u spirals mapped\n", W_USED);
  }

  /*
   * Replicate data to draft and mirror
   */
  void replicate()
  {
    std::copy(lattice_curr.begin(), lattice_curr.begin() + BLOCK, lattice_draft.begin());
    std::copy(lattice_curr.begin(), lattice_curr.begin() + BLOCK, lattice_mirror.begin());
    puts("replicate ok.");
  }

  /**
   * Executes initialization steps
   */
  bool initSimulation(int step)
  {
    switch(step)
    {
      case 0:
        initGeneral();
        break;
        
      case 1:
        initMomentum();
        break;
        
      case 2:
        initSpirals();
        break;
        
      case 3:
        initSine2();
        break;
        
      case 4:
        printParams();
        break;
        
      case 5:
        relocateAllWRandom();
        break;
        
      case 6:
        replicate();
        break;
        
      case 7:
        assert(sanityTest());
        break;
        
      default:
        return true;
    }
    
    return false;
  }

  /**
   * Tentative allocation
   */
  bool tryAllocate(int EL, int W)
  {
    try
    {
      size_t total = static_cast<size_t>(EL) * EL * EL * W;
      
      lattice_curr.resize(total);
      lattice_draft.resize(total);
      lattice_mirror.resize(total);
      
      const size_t totalVoxels = static_cast<size_t>(EL) * EL * EL;
      
      if (voxels.size() != totalVoxels)
        voxels.resize(totalVoxels);
      
      printf("tryAllocate: allocated %zu cells (%u^3 * %u)\n", total, EL, W);
      
      return true;
    }
    catch (const std::bad_alloc& e)
    {
      lastAllocationError = "Memory allocation failed: " + std::string(e.what());
      std::cerr << lastAllocationError << std::endl;
      return false;
    }
    catch (...)
    {
      lastAllocationError = "Unknown error during memory allocation";
      std::cerr << lastAllocationError << std::endl;
      return false;
    }
  }

void initCenters(unsigned wDim)
{
    lcenters.resize(wDim);
    
    // Distribute centers in a spherical pattern around the global center
    int R_center = EL / 4;  // Center distribution radius
    
    for (unsigned w = 0; w < wDim; ++w)
    {
        // Angles to distribute centers uniformly
        double theta = 2.0 * PI * w / wDim;
        double phi = acos(1.0 - 2.0 * w / wDim);
        
        int cx = CENTER + (int)round(R_center * sin(phi) * cos(theta));
        int cy = CENTER + (int)round(R_center * sin(phi) * sin(theta));
        int cz = CENTER + (int)round(R_center * cos(phi));
        
        // Clamp to valid range
        cx = std::max(0, std::min((int)EL - 1, cx));
        cy = std::max(0, std::min((int)EL - 1, cy));
        cz = std::max(0, std::min((int)EL - 1, cz));
        
        lcenters[w][0] = cx;
        lcenters[w][1] = cy;
        lcenters[w][2] = cz;
        
        printf("initCenters: w=%u, center=(%u,%u,%u)\n", w, cx, cy, cz);
    }
}

  /**
   * Calculates parameters
   */
  void calculateParameters(unsigned L, unsigned W)
  {
    EL        = L;
    L2        = (EL * EL);
    W_DIM     = (3 * L2 + 1);
    W_USED    = W;
    L3        = L2 * EL;
    ORDER     = ((int)round(log2(EL)));
    CENTER    = ((EL - 1) / 2);
    FCENTER   = (EL / 2.0);
    BLOCK     = L3 * W_USED;
    DIAG      = (unsigned)EL * (unsigned)sqrt(3);
    
    RMAX      = L / 2;
    
    CONTRACT  = static_cast<int>(floor(sqrt(3.0) * CENTER));
    CONVOL    = W_USED;
    
    GSLOT_X   = CONVOL + 2 * RMAX;
    GSLOT_Y   = GSLOT_X + 2 * RMAX;
    GSLOT_Z   = GSLOT_Y + 2 * RMAX;
    
    SLOT1     = GSLOT_Z + RMAX;
    SLOT2     = SLOT1 + 3 * (EL - 1);
    SLOT3     = SLOT2 + 3 * (EL - 1);
    SLOT4     = SLOT3 + 2 * W_USED;
    SLOT5     = SLOT4 + 3 * (L - 1);
    
    DIFFUSION = SLOT5;
    
    SLOT6     = DIFFUSION + (EL - 1);
    SLOT7     = SLOT6 + (EL - 1);
    SLOT8     = SLOT7 + (EL - 1);
    
    RELOC     = SLOT8;
    REISSUE   = RELOC + 1;
    FLOOD     = REISSUE + 3 * (L - 1);
    
    FRAME     = FLOOD;
    
    printf("calculateParameters: EL=%u, W_USED=%u, RMAX=%u, CENTER=%u, FRAME=%u\n", 
           EL, W_USED, RMAX, CENTER, FRAME);
    
    initCenters(W_USED);
  }

} // namespace automaton