// geometry.h
#ifndef GEOMETRY_H
#define GEOMETRY_H

namespace automaton
{
    // Geodesic distance on the sphere
    unsigned geodesicDistance(int x, int y, int z);
    
    // Checks if point is inside the sphere
    bool isInsideSphere(int x, int y, int z);
    
    // Relocate all cells randomly (debug)
    void relocateAllWRandom();
    
    // Antipodal wrapping
    void spherical_wrap(int& x, int& y, int& z);
}

#endif // GEOMETRY_H