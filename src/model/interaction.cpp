/*
 * interaction.cpp
 *
 * This file contains important routines used to
 * evaluate interactions in the CA.
 */

#include <cassert>
#include <algorithm>
#include "model/simulation.h"
#include "globals.h"

namespace automaton
{
  extern unsigned EL;
  extern unsigned W_USED;

  int count = 0;

  bool ctrl = true; // debug

  /**
   * Analyzes the cases when the wavefronts cross
   * during the convolution process.
   *
   * @curr the current lattice
   * @draft the draft lattice
   * @mirror the mirrored lattice
   */
  bool convolute(Cell& curr, Cell &draft, Cell &mirror)
  {
    switch(gConfig.simulation.scenario)
    {
      case 0:
        return convolute0(curr, draft, mirror);
      case 1:
        return convolute1(curr, draft, mirror);
      case 2:
        return convolute2(curr, draft, mirror);
      case 3:
        return convolute3(curr, draft, mirror);
      case 4:
        return convolute4(curr, draft, mirror);
      case 5:
        return convolute5(curr, draft, mirror);
      case 6:
        return convolute6(curr, draft, mirror);
      case 7:
        return convolute7(curr, draft, mirror);
      default:
      {
    	exit(1);
      }
    }
    return false;
  }

  void diffuse(Cell& curr, Cell &draft, Cell &forward,
               Cell &north, Cell &west, Cell &down,
               Cell &south, Cell &east, Cell &up)
  {
	  /****** SLOT I ******/
	  if (curr.k < SLOT1)
	  {
	    /*--- Orphan propagation ---*/
	    if ((north.a == W_USED && curr.r2 >= north.r2) ||
	        (west.a  == W_USED && curr.r2 >= west.r2)  ||
	        (down.a  == W_USED && curr.r2 >= down.r2)  ||
	        (south.a == W_USED && curr.r2 >= south.r2) ||
	        (east.a  == W_USED && curr.r2 >= east.r2)  ||
	        (up.a    == W_USED && curr.r2 >= up.r2))
	    {
	      draft.a = W_USED;
	    }
	  }
	  /****** SLOT II ******/
    if (curr.k < SLOT2)
    {
      /*--- Orphan propagation ---*/
      if ((north.a == W_USED && curr.r2 >= north.r2) ||
          (west.a  == W_USED && curr.r2 >= west.r2)  ||
          (down.a  == W_USED && curr.r2 >= down.r2)  ||
          (south.a == W_USED && curr.r2 >= south.r2) ||
          (east.a  == W_USED && curr.r2 >= east.r2)  ||
          (up.a    == W_USED && curr.r2 >= up.r2))
      {
        draft.a = W_USED;
      }
      /*--- Hunting using hB ---*/
      if (curr.r2 == pulse_from_time(curr.t))
      {
        if (north.hB) { draft.c[0] = (north.c[0] + 1) % EL; curr.sB = !draft.hB; }
        else if (west.hB)  { draft.c[1] = (west.c[1] + 1) % EL; curr.sB = !draft.hB; }
        else if (down.hB)  { draft.c[2] = (down.c[2] + 1) % EL; curr.sB = !draft.hB; }
        else if (south.hB) { draft.c[1] = (south.c[1] + 1) % EL; curr.sB = !draft.hB; }
        else if (east.hB)  { draft.c[0] = (east.c[0] + 1) % EL; curr.sB = !draft.hB; }
        else if (up.hB)    { draft.c[2] = (up.c[2] + 1) % EL; curr.sB = !draft.hB; }
      }
    }
    /****** SLOT III ******/
    else if (curr.k < SLOT3)
    {
      if (!ZERO(north.c))
      {
        draft.c[0] = north.c[0];
        draft.c[1] = north.c[1];
        draft.c[2] = north.c[2];
        if (north.kB) draft.kB = north.kB;
      }
      if (!ZERO(west.c))
      {
        draft.c[0] = west.c[0];
        draft.c[1] = west.c[1];
        draft.c[2] = west.c[2];
        if (west.kB) draft.kB = west.kB;
      }
      if (!ZERO(down.c))
      {
        draft.c[0] = down.c[0];
        draft.c[1] = down.c[1];
        draft.c[2] = down.c[2];
        if (down.kB) draft.kB = down.kB;
      }
      draft.f = max(down.f, max(west.f, max(north.f,
                  max(south.f, max(east.f, up.f)))));
      // Diffuse CB toward center (r2=0)
      if (!curr.cB)
      {
        if (north.cB && north.r2 > curr.r2)
        {
          draft.cB = true;
          if (north.a != W_USED)
            draft.a = north.a;
        }
        else if (south.cB && south.r2 > curr.r2)
        {
          draft.cB = true;
          if (south.a != W_USED)
            draft.a = south.a;
        }
        else if (east.cB && east.r2 > curr.r2)
        {
          draft.cB = true;
          if (east.a != W_USED)
            draft.a = east.a;
        }
        else if (west.cB && west.r2 > curr.r2)
        {
          draft.cB = true;
          if (west.a != W_USED)
            draft.a = west.a;
        }
        else if (down.cB && down.r2 > curr.r2)
        {
          draft.cB = true;
          if (down.a != W_USED)
            draft.a = down.a;
        }
        else if (up.cB && up.r2 > curr.r2)
        {
          draft.cB = true;
          if (up.a != W_USED)
            draft.a = up.a;
        }
      }
    }
    /****** SLOT IV ******/
    else if (curr.k < SLOT4)
    {
      if (forward.kB && forward.a == curr.a)
      {
        int delta_x = (curr.x[0] - forward.x[0] + EL) % EL;
        int delta_y = (curr.x[1] - forward.x[1] + EL) % EL;
        int delta_z = (curr.x[2] - forward.x[2] + EL) % EL;

        draft.c[0] = (forward.c[0] + delta_x) % EL;
        draft.c[1] = (forward.c[1] + delta_y) % EL;
        draft.c[2] = (forward.c[2] + delta_z) % EL;
        draft.kB = forward.kB;
        draft.cB = forward.cB;
      }
      draft.f = max(forward.f, curr.f);
    }
    /****** SLOT V ******/
    else if (curr.k < SLOT5)
    {
      if (curr.a == W_USED)
      {
        if (curr.r2 < curr.t * curr.t)
        {
      	  draft.a = curr.x[3];
        }
      }
    }
  }

  /**
   * Grid relocation.
   */
  void relocate(Cell& curr, Cell &draft, Cell &north, Cell &west, Cell &down)
  {
    // Save the 3D address
    unsigned x, y, z;
    x = curr.x[0];
    y = curr.x[1];
    z = curr.x[2];
    /****** SLOT VI ******/
    if (curr.k < SLOT6)
    {
      if (north.c[0] > 0)
      {
        draft = north;
        draft.c[0]--;
      }
    }
    /****** SLOT VII ******/
    else if (curr.k < SLOT7)
    {
      if (west.c[1] > 0)
      {
        draft = west;
        draft.c[1]--;
      }
    }
    /****** SLOT VIII ******/
    else if (curr.k < SLOT8)
    {
      if (down.c[2] > 0)
      {
        draft = down;
        draft.c[2]--;
      }
    }
    // Recover 3D address
    draft.x[0] = x;
    draft.x[1] = y;
    draft.x[2] = z;
  }

  /**
   * Prepares new wavefront.
   *
   * @curr the current lattice
   * @draft the draft lattice
   * @mirror the mirrored lattice
   */
  void reissue(Cell& curr, Cell &draft, Cell &forward,
               Cell &north, Cell &west, Cell &down,
               Cell &south, Cell &east, Cell &up)
  {
      // Reset propagation status
      draft.kB = false;
      draft.hB = false;
      draft.bB = false;
      // Propagate normal affinity outward, overwriting normal or orphan
      if (curr.r2 == pulse_from_time(curr.t))
      {
          if (north.r2 > curr.r2)
          {
              // Copy a from inner to outer cell
              draft.a = north.a;
          }
          if (south.r2 > curr.r2)
          {
              draft.a = south.a;
          }
          if (east.r2 > curr.r2)
          {
              draft.a = east.a;
          }
          if (west.r2 > curr.r2)
          {
              draft.a = west.a;
          }
          if (up.r2 > curr.r2)
          {
              draft.a = up.a;
          }
          if (down.r2 > curr.r2)
          {
              draft.a = down.a;
          }
      }
      if (curr.cB)
      {
        // Consume cB
        draft.cB = false;
        if (curr.a != W_USED && curr.r2 < 4)
        {
          draft.t = 0;
        }
      }
  }

  void flood(Cell& curr, Cell &draft, Cell &forward,
               Cell &north, Cell &west, Cell &down,
               Cell &south, Cell &east, Cell &up)
  {
	if (curr.a != W_USED)
      draft.t = min({ north.t, south.t, east.t, west.t, down.t, up.t });
  }
}
