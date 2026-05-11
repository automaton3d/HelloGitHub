/*
 * help.cpp (adaptado)
 */

#include <vector>
#include <string>
#include "globals.h"
#include "help.h"

namespace framework
{
  // Texto de ajuda da interface
  const std::vector<std::string> ui_help = {
    "c: Print camera Eye, Center, Up",
    "r: Reset view",
    "t: Toggle Pan / First-Person",
    "x, y, z: Snap camera to axis",
    "Left-Click: Rotate",
    "Middle-Click: Pan or First-Person",
    "Right-Click: Roll",
    "Scroll-Wheel: Dolly (zoom)",
    "G: Toggle wrapping gizmo",
    "P: Pause the simulation",
    "F1: Toggle keyboard shortcuts",
    "Escape: EXIT"
  };

  // Texto de ajuda para gravação/replay
  const std::vector<std::string> record_help = {
    "F5 - record",
    "F6 - replay",
    "F7 - save to file",
    "F8 - load from file"
  };

  // Textos de ajuda para cenários
  std::vector<std::string> scenarioHelpTexts = {
    // Scenario 0
    "Scenario 0: Wrapping Test\n\n"
    "This test checks the lattice's ability to wrap around its edges.\n"
    "Use small grid sizes (e.g., L=9) to observe boundary continuity.\n\n"
    "All layers have similar behavior.\n\n"
    "No active behavior is triggered; this is a sanity check.",

    // Scenario 1
    "Scenario 1: Relocate Test\n\n"
    "Triggered when a cell reaches midpoint (t = RMAX / 2).\n"
    "If control flag is active, the cell relocates to a random position.\n"
    "Use this to test relocation behavior, the most important\n"
    "result of an interaction.\n\n"
    "The relocation itself is shown just once. The relocated bubble\n"
    "keeps expanding until wrapping.\n\n"
    "This event only occurs at layer 0.\n\n"
    "The suggested L in this case is 15.",

    // Scenario 2
    "Scenario 2: Orphan Expansion\n\n"
    "When triggered, the cell becomes an orphan seed.\n"
    "Affinity is set to maximum layer count (W_USED). Then, the\n"
    "orphan seed spreads to the whole shell (red voxels)\n"
    "and continues to expand until wrapping, reappearing as a new\n"
    "bubble (white voxels).\n\n"
    "Useful for testing layer propagation and orphan behavior.\n\n"
    "This event only occurs at layer 0.\n\n"
    "The suggested L in this case is 19.",

    // Scenario 3
    "Scenario 3: Contraction Test\n\n"
    "Similar to orphan expansion, but also triggers contraction.\n"
    "Cell becomes an orphan and activates contraction flag.\n"
    "This event only occurs at layer 0.\n\n"
    "Observe how contraction affects surrounding cells.\n\n"
    "The suggested L in this case is 19.",

    // Scenario 4
    "Scenario 4: Hunting Behavior\n\n"
    "Triggered only if spin bit (sB) is active.\n"
    "Cell becomes a hunter (hB = true), whose goal is to calculate\n"
    "a c vector to relocate the bubble to its sB true site.\n\n"
    "Use this to visualize this diffusion operation.\n\n"
    "This event only occurs at layer 0.\n\n"
    "The suggested L in this case is 19.",

    // Scenario 5
    "Scenario 5: Reissue Test\n\n"
    "Triggered by pBit presence and affinity mismatch.\n"
    "Cell reissues from its current position and becomes an orphan.\n"
    "Contraction flag is activated to simulate collapse.\n\n"
    "This event only occurs at layer 0.\n\n"
    "The suggested L in this case is 19.",

    // Scenario 6
    "Scenario 6: Dispersion Test\n\n"
    "Triggered when two cells overlap and differ in sector.\n"
    "If pBit and spinBit are active, reissue occurs with contraction.\n"
    "Observe how dispersion leads to orphan seeding and wavefront\n"
    "splitting.\n\n"
    "This is the prototypical interaction.\n\n"
    "This event involves all layers.\n\n",

    // Scenario 7
    "Scenario 7: Full Simulation\n\n"
    "This scenario combines all previous behaviors.\n"
    "Includes photon, graviton, neutrino, and boson interactions.\n"
    "Tests superposition, annihilation, cohesion, and electroweak\n"
    "forces.\n"
    "Use this to explore complex particle dynamics and emergent\n"
    "behavior.\n\n"
    "This event involves all layers.\n\n"
    "The suggested L in this case is 21."
  };
}
