/*
 * simulation.h
 */

#ifndef SIMULATION_H_
#define SIMULATION_H_

#include <vector>
#include <iostream>
#include <cstdint>

// Simulation symbols
#define NORTH     0
#define EAST      1
#define SOUTH     2
#define WEST      3
#define UP        4
#define DOWN      5
#define FORWARD   6
#define BACKWARD  7

// Enable simulation IDE
#define GRAPH

// Macros
#define ZERO(v)   (!(v[0] | v[1] | v[2]))

// Charge masks
#define C0_MASK     0x01
#define C1_MASK     0x02
#define C2_MASK     0x04
#define Q_MASK      0x08
#define W0_MASK     0x10
#define W1_MASK     0x20
#define COLOR_MASK  (C0_MASK | C1_MASK | C2_MASK)
#define WEAK_MASK   (W0_MASK | W1_MASK)
#define CHARGE_MASK (W0_MASK | W1_MASK | C0_MASK | C1_MASK | C2_MASK | Q_MASK)

// Platform-independent color type (RGBA)
struct Color {
  uint8_t r, g, b, a;
  
  Color() : r(0), g(0), b(0), a(255) {}
  Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
    : r(red), g(green), b(blue), a(alpha) {}
  
  // Convert to 32-bit integer (RGBA)
  uint32_t toUInt32() const {
    return (static_cast<uint32_t>(r) << 24) |
           (static_cast<uint32_t>(g) << 16) |
           (static_cast<uint32_t>(b) << 8) |
           static_cast<uint32_t>(a);
  }
  
  // Create from 32-bit integer (RGBA)
  static Color fromUInt32(uint32_t color) {
    return Color(
      (color >> 24) & 0xFF,
      (color >> 16) & 0xFF,
      (color >> 8) & 0xFF,
      color & 0xFF
    );
  }
};

namespace framework
{
  extern void sound(bool loop);
}

namespace automaton
{
  using namespace std;

  extern unsigned EL;
  extern unsigned W_USED;
  extern bool convol_delay;
  extern bool diffuse_delay;
  extern bool reloc_delay;
  extern std::vector<std::array<unsigned, 3>> lcenters;

  struct Point
  {
    unsigned x, y, z;
    bool operator==(const Point& other) const
    {
      return x == other.x && y == other.y && z == other.z;
    }
  };

  // Define the outer structure
  struct WPoint
  {
    Point p;
  };


struct NeighborResult
{
    int x, y, z, w;

    bool wrapped;
    bool antipodal;
};

  extern string lastAllocationError;

  // Cell Class
  class Cell
  {
    public:
      // Physical properties
      unsigned char ch;   // Charge bits q, w1, w0, c2, c1, c0
      bool pB;            // Linear motion direction bit
      bool sB;            // Rotation spiral bit
      unsigned a;         // Affinity
      unsigned x[4];      // Relative position
      // Wavefront
      unsigned d;         // Euclidean distance
      bool phiB;          // Fixed period mask bit
      unsigned t;         // Light frame counter
      unsigned f;         // Sine phase parameter
      // Operational variables
      unsigned c[3] = { 0, 0, 0 }; // Relocation offset
      unsigned k;         // Tick counter
      bool s2B;           // Sieve test result
      // Interaction control
      bool kB;            // Collapse flag
      bool bB;            // Blob flag
      bool hB;            // Hunt flag
      bool cB;            // Contraction flag
      // Glider (antipodal transport)
      bool gB;            // Glider active flag
      int  g[3] = {0,0,0}; // Signed displacement to antipodal
      // Pulsating sphere
      unsigned int r2;    // Squared distance from center (BFS-propagated)
      // Default constructor
      Cell()
        : ch(0), pB(false), sB(false), a(0),
          d(0), phiB(false), t(0), f(0),
          k(0), s2B(false), kB(false), bB(false), hB(false), cB(false),
          gB(false), r2(0xFFFFFFFFu)
      {
        fill(begin(x), end(x), 0);
        fill(begin(c), end(c), 0);
        fill(begin(g), end(g), 0);
      }
      // Serialization functions
      void serialize(ofstream& out) const;
      void deserialize(ifstream& in);

      bool Q() { return ch & Q_MASK; }
      bool W1() { return ch & W1_MASK; }
      bool W0() { return ch & W0_MASK; }
      bool C2() { return ch & C2_MASK; }
      bool C1() { return ch & C1_MASK; }
      bool C0() { return ch & C0_MASK; }
      unsigned char COLOR() { return ch & COLOR_MASK; }
      unsigned char ANTICOLOR() { return ~ch & COLOR_MASK; }
      Cell &getNeighbor(int i);
  };


  // Inline accessor for 4D indexing
  inline Cell& getCell(vector<Cell>& lattice, int x, int y, int z, int w)
  {
    return lattice[((x * EL + y) * EL + z) * W_USED + w];
  }

  
  inline const Cell& getCell(const vector<Cell>& lattice, int x, int y, int z, int w)
  {
    return lattice[((x * EL + y) * EL + z) * W_USED + w];
  }

  /// Function prototypes ///
  void calculateParameters(unsigned L, unsigned W);
  void* SimulationLoop();
  void DeleteAutomaton();
  bool swap_lattices();
  void update();
  bool initSimulation(int step);
  void initSpirals();
  void replicate();
  void markPoints(unsigned p[3], int w);
  bool simulation();
  bool convolute(Cell& curr, Cell &draft, Cell &mirror);
  bool convolute0(Cell& curr, Cell &draft, Cell &mirror);
  bool convolute1(Cell& curr, Cell &draft, Cell &mirror);
  bool convolute2(Cell& curr, Cell &draft, Cell &mirror);
  bool convolute3(Cell& curr, Cell &draft, Cell &mirror);
  bool convolute4(Cell& curr, Cell &draft, Cell &mirror);
  bool convolute5(Cell& curr, Cell &draft, Cell &mirror);
  bool convolute6(Cell& curr, Cell &draft, Cell &mirror);
  bool convolute7(Cell& curr, Cell &draft, Cell &mirror);
  void diffuse(Cell& curr, Cell &draft, Cell &forward, Cell &north, Cell &west, Cell &down, Cell &south, Cell &east, Cell &up);
  void relocate(Cell& curr, Cell &draft, Cell &north, Cell &west, Cell &down);
  void reissue(Cell& curr, Cell &draft, Cell &forward,
               Cell &north, Cell &west, Cell &down,
               Cell &south, Cell &east, Cell &up);
  void flood(Cell& curr, Cell &draft, Cell &forward,
               Cell &north, Cell &west, Cell &down,
               Cell &south, Cell &east, Cell &up);
  void updateBuffer();
  vector<tuple<int, int, int>> generateShell(int L);
  void normalize(double vec[3]);
  void cross_product(double result[3], const double a[3], const double b[3]);
  void printLattice(int w);
  bool neutralColor(Cell &a, Cell &b);
  bool neutralWeak(Cell &a, Cell &b);
  void shiftMirror();
  bool sanityTest3();
  bool tryAllocate(int EL, int W);
  unsigned int getRandomUnsigned(unsigned int modulus);
  void relocateGlobal(unsigned dx, unsigned dy, unsigned dz);

  // Tests

  void printParams();
  bool sanityTest();
  bool sanityTest2();

  /// Cross variables ///
  extern vector<Cell> lattice_curr;

  /// Cross constants ///
  extern unsigned ORDER;
  extern unsigned EL;
  extern unsigned L2;
  extern unsigned L3;
  extern unsigned W_DIM;
  extern unsigned W_USED;
  extern unsigned long BLOCK;
  extern unsigned CENTER;
  extern unsigned FCENTER;
  extern unsigned UPDATE;
  extern unsigned DIAG;
  extern unsigned RMAX;
  extern unsigned CONTRACT;
  extern unsigned CONVOL;
  extern unsigned GSLOT_X;
  extern unsigned GSLOT_Y;
  extern unsigned GSLOT_Z;
  extern unsigned SLOT1;
  extern unsigned SLOT2;
  extern unsigned SLOT3;
  extern unsigned SLOT4;
  extern unsigned DIFFUSION;
  extern unsigned SLOT5;
  extern unsigned SLOT6;
  extern unsigned SLOT7;
  extern unsigned SLOT8;
  extern unsigned RELOC;
  extern unsigned REISSUE;
  extern unsigned FLOOD;
  extern unsigned FRAME;
  extern unsigned int pulse_tick;

  #define INF_R2 0xFFFFFFFFu

  // Pulsating sphere threshold (triangle wave on r²)
  inline unsigned int pulse_from_time(unsigned int t)
  {
      const unsigned int min_r2 = 0;
      const unsigned int max_r2 = (unsigned int)(RMAX * RMAX * 0.92);
      const unsigned int step = 1;
      unsigned int span = max_r2 - min_r2;
      if (span == 0) return min_r2;
      unsigned int period = 2 * span;
      unsigned int phase = (t * step) % period;
      if (phase < span)
          return min_r2 + phase;
      else
          return max_r2 - (phase - span);
  }

  void update_pulsating_wavefront();

  // Effective wavefront radius (triangle wave: expands 0→RMAX, contracts RMAX→0)
  // Period = 2*RMAX (= L in physics terms), amplitude = RMAX
  inline unsigned effective_t(unsigned t)
  {
      unsigned cycle = 2 * RMAX;
      unsigned phase = t % cycle;
      if (phase <= RMAX)
          return phase;
      else
          return cycle - phase;
  }


/// Cross variables ///
extern std::vector<Cell> lattice_curr;
extern std::vector<Cell> lattice_draft; // Add or verify
extern std::vector<Cell> lattice_mirror; // Add or verify

  /**
   * Tests if two vectors are equal.
   */
  inline bool EQUAL(unsigned v1[3], unsigned v2[3])
  {
    // Compare elements
    for (size_t i = 0; i < 3; ++i)
    {
      if (v1[i] != v2[i])
        return false;
    }
    return true;
  }

// ===================================================================
  // CUDA ACCELERATION FUNCTIONS
  // ===================================================================
  
  // These functions are always declared, regardless of USE_CUDA
  // When USE_CUDA is defined: implemented in cuda_automaton.cu
  // When USE_CUDA is NOT defined: implemented in bridge.cpp as stubs
  
  bool tryEnableCuda();
  void disableCuda();
  bool isCudaEnabled();
  
#ifdef USE_CUDA
  // Internal GPU wrapper functions - only declared when CUDA is enabled
  // Implementations are in cuda_automaton.cu
  void ca_update_gpu_wrapper();
  void ca_update_gpu_wrapper(
      unsigned CONVOL, unsigned SLOT1, unsigned SLOT2, unsigned SLOT3, 
      unsigned SLOT4, unsigned DIFFUSION, unsigned SLOT5, unsigned SLOT6, 
      unsigned SLOT7, unsigned SLOT8, unsigned RELOC, unsigned REISSUE, 
      unsigned FLOOD, unsigned FRAME, unsigned RMAX
  );
  bool swap_lattices_gpu();
  
  // Pointers for Device (GPU) memory
  extern Cell* d_lattice_curr;
  extern Cell* d_lattice_draft;
  extern Cell* d_lattice_mirror;
  
#endif // USE_CUDA
  
}

#endif /* SIMULATION_H_ */
  