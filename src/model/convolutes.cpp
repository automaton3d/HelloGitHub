/*
 * convolutes.cpp
 *
 * Convolution rules for each scenario (Sect. 5.4 of the manuscript).
 *
 * During the CONVOL phase (k < CONVOL), the CA compares each cell against
 * its mirror counterpart (cyclically shifted in the W dimension) to detect
 * wavefront crossings and trigger interactions.
 *
 * The scenarios form a progressive test suite: each one isolates a single
 * mechanism of the CA so it can be validated independently before enabling
 * the full rule set.
 *
 * Common gate condition (scenarios 1-5):
 *   r2 == pulse_from_time(t)  — cell is on the active wavefront
 *   pulse_from_time(t) == (RMAX/2)²  — wavefront is at mid-radius
 *   x[3] == 0                 — only layer 0 (w = 0)
 *   ctrl                      — fire-once flag (prevents re-triggering)
 */

#include "model/simulation.h"
#include <algorithm>

namespace automaton
{
  extern unsigned EL;
  extern unsigned W_USED;

  extern bool ctrl;

  /*
   * Scenario 0 — Wavefront propagation test.
   * No interaction: the wavefront expands and contracts freely under
   * antipodal (spherical) wrapping. Validates the basic CA timing,
   * effective_t oscillation, and wrapping topology.
   */
  bool convolute0(Cell& curr, Cell &draft, Cell &mirror)
  {
    return false;
  }

  /*
   * Scenario 1 — Relocation test.
   * At mid-radius on layer 0, assigns a random relocation vector c[].
   * During SLOT III (diffusion), c[] propagates to all cells on the
   * wavefront. During SLOTS VI-VIII (relocation), each cell physically
   * shifts by c[], displacing the entire bubble to a new position.
   * After reissue, t resets and expansion restarts from the new center.
   */
  bool convolute1(Cell& curr, Cell &draft, Cell &mirror)
  {
    if (curr.r2 == pulse_from_time(curr.t) && pulse_from_time(curr.t) == (RMAX / 2) * (RMAX / 2) && curr.x[3] == 0 && ctrl)
    {
      draft.c[0] = getRandomUnsigned(EL);
      draft.c[1] = getRandomUnsigned(EL);
      draft.c[2] = getRandomUnsigned(EL);
      ctrl = false;
    }
    return false;
  }

  /*
   * Scenario 2 — Orphan mechanism test.
   * Sets affinity a = W_USED on a wavefront cell, marking it as an orphan.
   * Orphans represent the residual field of a bubble after reissue: they
   * continue expanding outward (t++ without modulo) but carry no momentum
   * and are dominated when they meet an active wavefront.
   * Validates SLOT I/II orphan propagation in diffusion.
   */
  bool convolute2(Cell& curr, Cell &draft, Cell &mirror)
  {
    if (curr.r2 == pulse_from_time(curr.t) && pulse_from_time(curr.t) == (RMAX / 2) * (RMAX / 2) && curr.x[3] == 0 && ctrl)
    {
      draft.a = W_USED;
      ctrl = false;
    }
    return false;
  }

  /*
   * Scenario 3 — Contraction test.
   * Sets orphan (a = W_USED) and contraction flag (cB = true).
   * cB propagates inward during SLOT III (diffusion, toward r2 = 0).
   * When cB reaches the center (r2 < 4) during reissue, t resets to 0,
   * restarting the wavefront expansion. This is the mechanism by which
   * a bubble "collapses" and re-emits from its center.
   */
  bool convolute3(Cell& curr, Cell &draft, Cell &mirror)
  {
    if (curr.r2 == pulse_from_time(curr.t) && pulse_from_time(curr.t) == (RMAX / 2) * (RMAX / 2) && curr.x[3] == 0 && ctrl)
    {
      draft.a = W_USED;
      draft.cB = true;
      ctrl = false;
    }
    return false;
  }

  /*
   * Scenario 4 — Hunting test.
   * Sets the hunt flag (hB) on a wavefront cell that has sB (spiral bit).
   * During SLOT II (diffusion), hB propagates outward along the wavefront,
   * seeking cells with sB set. This is how a bubble locates its interaction
   * partner — the "hunter" propagates until it finds the "prey" (sB cell).
   */
  bool convolute4(Cell& curr, Cell &draft, Cell &mirror)
  {
    if (curr.r2 == pulse_from_time(curr.t) && pulse_from_time(curr.t) == (RMAX / 2) * (RMAX / 2) && curr.sB && curr.x[3] == 0 && ctrl)
    {
      draft.hB = true;
      ctrl = false;
    }
    return false;
  }

  /*
   * Scenario 5 — Reissue (affinity propagation) test.
   * Triggers a full reissue cycle on a pB (propeller) cell: sets c[] to
   * the cell's own position, cB = true, and a = W_USED (orphan).
   * During diffusion, c[] and cB propagate inward; during relocation,
   * the bubble physically moves to the target position; during reissue,
   * t resets and the bubble re-expands from the relocated center.
   * This tests the complete relocate-and-reissue pipeline.
   */
  bool convolute5(Cell& curr, Cell &draft, Cell &mirror)
  {
    if (curr.r2 == pulse_from_time(curr.t) && pulse_from_time(curr.t) == (RMAX / 2) * (RMAX / 2) && curr.pB && curr.x[3] == 0 &&
       !curr.cB && curr.a != W_USED && ctrl)
    {
      draft.c[0] = curr.x[0];
      draft.c[1] = curr.x[1];
      draft.c[2] = curr.x[2];
      draft.cB = true;
      draft.a = W_USED;
      ctrl = false;
    }
    return false;
  }

  /*
   * Scenario 6 — Dispersion (Orbis/Umbra separation) test.
   * Detects inter-sector interaction: curr and mirror have different W1
   * (weak isospin), meaning they belong to different sectors (Orbis vs Umbra).
   *
   * Two outcomes at the same spatial position (superposing wavefronts):
   *   pB && mirror.sB  →  reissue: the propeller cell relocates to its own
   *                        position (c[] = x[]), becomes orphan, and re-emits.
   *   sB && !mirror.pB →  hunting + contraction: the spiral cell hunts for
   *                        a new partner while contracting.
   *
   * This models how bubbles from different sectors interact without
   * annihilating — they separate (disperse) rather than merge.
   */
  bool convolute6(Cell& curr, Cell &draft, Cell &mirror)
  {
    if (curr.r2 == pulse_from_time(curr.t) && mirror.r2 == pulse_from_time(mirror.t))
    {
      if (curr.x[0] == mirror.x[0] &&
          curr.x[1] == mirror.x[1] &&
          curr.x[2] == mirror.x[2])
      {
        if (curr.a != W_USED &&
            curr.W1() != mirror.W1() &&
            !curr.cB &&
            pulse_from_time(curr.t) == (RMAX / 2) * (RMAX / 2))
        {
          if (curr.pB && mirror.sB)
          {
            draft.c[0] = curr.x[0];
            draft.c[1] = curr.x[1];
            draft.c[2] = curr.x[2];
            draft.cB = true;
            draft.a = W_USED;
          }
          else if (curr.sB && !mirror.pB)
          {
            draft.hB = true;
            draft.cB = true;
            draft.a = W_USED;
          }
        }
      }
    }
    return false;
  }

  /*
   * Scenario 7 — Full convolution (Sect. 5.4 of the manuscript).
   * Implements all interaction rules between superposing wavefronts.
   *
   * Structure: curr and mirror must both be on their active wavefronts
   * (d == effective_t(t)). Then two cases:
   *
   * A) SAME POSITION (curr.x == mirror.x) — superposing bubbles:
   *
   *    A1) Different sectors (W1 ≠ mirror.W1), at mid-radius, non-orphan:
   *        - pB && !mirror.pB  → inertial transport: propeller relocates
   *          the bubble to its own position (c[] = x[], cB).
   *        - !pB && mirror.pB  → hunting + contraction: non-propeller cell
   *          hunts for a partner while contracting.
   *
   *    A2) Phase-locked (f == effective_t), same position:
   *        - Different sectors, both pB  → pair formation: accumulate sine
   *          phase (f += t), sieve (s2B &= phiB), merge affinity (min a).
   *        - Charge-conjugate (Q ⊕, W0 ⊕, same color)  → pair formation
   *          with blob flag (bB).
   *        - Neutral (ch == 0 or ch == 63)  → pair formation (singlet).
   *
   * B) DIFFERENT POSITION — distinct bubbles:
   *
   *    B1) Same sector (W1 == mirror.W1), same charge (ch == mirror.ch),
   *        phase-locked:
   *        - a > mirror.a  → cohesion: bubble with higher affinity relocates
   *          toward the one with lower affinity (c[] = x[], a = min).
   *        - a <= mirror.a → hunting: sets hB, merges affinity (a = min).
   */
  bool convolute7(Cell& curr, Cell &draft, Cell &mirror)
  {
    if (curr.r2 == pulse_from_time(curr.t) && mirror.r2 == pulse_from_time(mirror.t))
    {
      // --- A) SAME POSITION (superposing bubbles) ---
      if (curr.x[0] == mirror.x[0] &&
          curr.x[1] == mirror.x[1] &&
          curr.x[2] == mirror.x[2])
      {
        // A1) Inter-sector interaction at mid-radius
        if (curr.W1() != mirror.W1() &&
            pulse_from_time(curr.t) == (RMAX / 2) * (RMAX / 2) &&
            !curr.cB &&
            curr.a != W_USED)
        {
          // Inertial transport: propeller relocates bubble
          if (curr.pB && !mirror.pB)
          {
            draft.c[0] = curr.x[0];
            draft.c[1] = curr.x[1];
            draft.c[2] = curr.x[2];
            draft.cB = true;
          }

          // Hunting: non-propeller seeks partner
          if (!curr.pB && mirror.pB)
          {
            draft.hB = true;
            draft.cB = true;
          }
        }
        // A2) Phase-locked superposition (f == t)
        else if (curr.f == curr.t && mirror.f == mirror.t)
        {
          // Different sectors, both propellers → pair formation
          if (curr.W1() != mirror.W1())
          {
            if (curr.pB && mirror.pB)
            {
              draft.f += curr.t;
              draft.s2B &= curr.phiB;
              draft.a = std::min(curr.a, mirror.a);
            }
          }
          // Charge-conjugate pair (Q ⊕, W0 ⊕, same color) → blob
          else if ((curr.Q() ^ mirror.Q()) &&
                   (curr.W1() == mirror.W1()) &&
                   (curr.W0() ^ mirror.W0()) &&
                   (curr.C2() == mirror.C2()) &&
                   (curr.C1() == mirror.C1()) &&
                   (curr.C0() == mirror.C0()))
          {
            draft.f += curr.t;
            draft.s2B &= curr.phiB;
            draft.a = std::min(curr.a, mirror.a);
            draft.bB = true;
          }
          // Neutral pair (ch == 0 or ch == 63) → singlet
          else if ((curr.ch == 0 && mirror.ch == 0) ||
                   (curr.ch == 63 && mirror.ch == 63))
          {
            draft.f += curr.t;
            draft.s2B &= curr.phiB;
            draft.a = std::min(curr.a, mirror.a);
          }
        }
      }
      // --- B) DIFFERENT POSITION (distinct bubbles) ---
      else
      {
        // B1) Same sector, same charge, phase-locked → cohesion or hunting
        if (curr.W1() == mirror.W1())
        {
          if (curr.ch == mirror.ch &&
              curr.f == curr.t && mirror.f == mirror.t)
          {
            if (curr.a > mirror.a)
            {
              // Cohesion: higher-affinity bubble relocates toward lower
              draft.c[0] = curr.x[0];
              draft.c[1] = curr.x[1];
              draft.c[2] = curr.x[2];
              draft.a = std::min(curr.a, mirror.a);
            }
            else
            {
              // Hunting: lower-affinity bubble seeks partner
              draft.hB = true;
              draft.a = std::min(curr.a, mirror.a);
            }
          }
        }
      }
    }

    return false;
  }

}
