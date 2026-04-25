#pragma once

#include "Geometry/PDB.h"
#include <vector>

// A bond connects two atoms, stored as a pair of indices into PDB::Atoms.
struct Bond {
    size_t A, B;   // indices into the Atoms array
};

class Bonds {
public:
    std::vector<Bond> List;

    // Build the bond list from an atom array.
    // Two atoms are considered bonded when their distance is smaller than the
    // sum of their covalent radii plus a tolerance (default 0.45).
    // This threshold works well for standard covalent bonds in organic molecules.
    void build(const std::vector<Atom>& atoms, float tolerance = 0.45f);

private:
    // Returns the covalent radius (Angstroms) for a given element symbol.
    // Values from: Alvarez, S. (2008). Dalton Trans., 2832.
    //              doi: 10.1039/b801115j
    static float getCovalentRadius(const char element[3]);
};
