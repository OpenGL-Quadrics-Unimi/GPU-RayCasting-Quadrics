#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

// Atom position and element, as loaded from the PDB file.
// Position becomes the sphere centre in the ray-caster (Sigg 2006, §4).
struct Atom {
    glm::vec3 Position;   // Angstroms, centred at origin after loading
    char      Element[3]; // e.g. "C", "N", "O"
    char      Chain;
};

// CPK colour and van der Waals radius for one element.
// The radius is used as the sphere radius in the quadric matrix Q (Sigg 2006, §3).
struct AtomStyle {
    glm::vec3 Color;
    float     Radius; // van der Waals radius, Angstroms
};

class PDB {
public:
    std::vector<Atom> Atoms;
    glm::vec3         Centroid;
    float             BoundingRadius;

    bool load(const std::string& path);
    static AtomStyle getAtomStyle(const char element[3]);
};