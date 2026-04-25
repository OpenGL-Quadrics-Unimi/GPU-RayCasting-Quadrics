/* Check which atoms are close enough to form bonds (the distance between them is less than the sum of their covalent radii, which is defined in the getCovalentRadius function).
    Loop over every unique pair of atoms and compute their distance. If the distance is less than the threshold, add a bond Bond{i, j} to the list. 
    This is an O(n²) operation! Check if it can be optimized later.
*/
#include "Geometry/Bonds.h"

#include <glm/glm.hpp>

// Covalent radii for common elements (Angstroms).
// Source: Alvarez, S. (2008). Dalton Trans., 2832.
//         doi: 10.1039/b801115j

float Bonds::getCovalentRadius(const char element[3]) {
    char c0 = element[0], c1 = element[1];
    auto is1 = [&](char a)         { return c0 == a && c1 == '\0'; };
    auto is2 = [&](char a, char b) { return c0 == a && c1 == b; };

    if (is1('H'))     return 0.31f;
    if (is1('C'))     return 0.76f;
    if (is1('N'))     return 0.71f;
    if (is1('O'))     return 0.66f;
    if (is1('S'))     return 1.05f;
    if (is1('P'))     return 1.07f;
    if (is1('F'))     return 0.57f;
    if (is2('C','l')) return 1.02f;
    if (is2('B','r')) return 1.20f;
    if (is1('I'))     return 1.39f;
    if (is2('F','e')) return 1.32f;
    if (is2('M','g')) return 1.41f;
    if (is2('C','a')) return 1.76f;
    if (is2('Z','n')) return 1.22f;
    if (is2('N','a')) return 1.66f;
    if (is1('K'))     return 2.03f;

    return 0.75f; // unknown element: safe fallback close to carbon
}

void Bonds::build(const std::vector<Atom>& atoms, float tolerance) {
    List.clear();

    // Check every unique pair of atoms.
    // Two atoms are bonded if their distance is within the sum of their
    // covalent radii plus the tolerance. 
    for (size_t i = 0; i < atoms.size(); ++i) {
        for (size_t j = i + 1; j < atoms.size(); ++j) {
            float maxDist = getCovalentRadius(atoms[i].Element)
                          + getCovalentRadius(atoms[j].Element)
                          + tolerance;

            float dist = glm::length(atoms[i].Position - atoms[j].Position);

            if (dist < maxDist)
                List.push_back({i, j});
        }
    }
}
