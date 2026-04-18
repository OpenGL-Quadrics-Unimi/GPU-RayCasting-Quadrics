#include "Geometry/PDB.h"

#include <fstream>
#include <cctype>
#include <cmath>

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

static void parseElement(const std::string& line, char out[3]) {
    out[0] = out[1] = out[2] = '\0';
    if (line.size() >= 78) {
        std::string e = trim(line.substr(76, 2));
        if (!e.empty()) {
            out[0] = std::toupper((unsigned char)e[0]);
            if (e.size() > 1) out[1] = std::tolower((unsigned char)e[1]);
            return;
        }
    }
    if (line.size() >= 16) {
        for (char c : line.substr(12, 4))
            if (std::isalpha((unsigned char)c)) {
                out[0] = std::toupper((unsigned char)c);
                break;
            }
    }
}

bool PDB::load(const std::string& path) {
    std::ifstream file(path);
    if (!file) return false;

    Atoms.clear();
    glm::vec3 bboxMin(1e30f), bboxMax(-1e30f);

    std::string line;
    while (std::getline(file, line)) {
        if (line.size() < 54) continue;
        const std::string rec = line.substr(0, 6);
        if (rec != "ATOM  " && rec != "HETATM") continue;

        Atom atom;
        try {
            atom.Position.x = std::stof(line.substr(30, 8));
            atom.Position.y = std::stof(line.substr(38, 8));
            atom.Position.z = std::stof(line.substr(46, 8));
        } catch (...) { continue; }

        atom.Chain = (line.size() > 21) ? line[21] : ' ';
        parseElement(line, atom.Element);

        Atoms.push_back(atom);
        bboxMin = glm::min(bboxMin, atom.Position);
        bboxMax = glm::max(bboxMax, atom.Position);
    }

    if (Atoms.empty()) return false;

    // Centre at origin so the camera target stays at (0,0,0).
    Centroid = 0.5f * (bboxMin + bboxMax);
    for (Atom& a : Atoms) a.Position -= Centroid;

    float r2 = 0.0f;
    for (const Atom& a : Atoms)
        r2 = std::max(r2, glm::dot(a.Position, a.Position));
    BoundingRadius = std::sqrt(r2);

    return true;
}

// CPK colours and van der Waals radii (Bondi 1964).
AtomStyle PDB::getAtomStyle(const char element[3]) {
    char c0 = element[0], c1 = element[1];
    auto is1 = [&](char a)         { return c0 == a && c1 == '\0'; };
    auto is2 = [&](char a, char b) { return c0 == a && c1 == b; };

    if (is1('H'))      return {{1.00f, 1.00f, 1.00f}, 1.20f};
    if (is1('C'))      return {{0.35f, 0.35f, 0.38f}, 1.70f};
    if (is1('N'))      return {{0.19f, 0.31f, 0.97f}, 1.55f};
    if (is1('O'))      return {{0.95f, 0.15f, 0.15f}, 1.52f};
    if (is1('S'))      return {{1.00f, 0.78f, 0.20f}, 1.80f};
    if (is1('P'))      return {{1.00f, 0.50f, 0.00f}, 1.80f};
    if (is1('F'))      return {{0.56f, 0.88f, 0.31f}, 1.47f};
    if (is2('C','l'))  return {{0.12f, 0.94f, 0.12f}, 1.75f};
    if (is2('B','r'))  return {{0.65f, 0.16f, 0.16f}, 1.85f};
    if (is1('I'))      return {{0.58f, 0.00f, 0.58f}, 1.98f};
    if (is2('F','e'))  return {{0.88f, 0.40f, 0.20f}, 1.94f};
    if (is2('M','g'))  return {{0.54f, 1.00f, 0.00f}, 1.73f};
    if (is2('C','a'))  return {{0.24f, 1.00f, 0.00f}, 1.94f};
    if (is2('Z','n'))  return {{0.49f, 0.50f, 0.69f}, 1.39f};
    if (is2('N','a'))  return {{0.67f, 0.36f, 0.95f}, 2.27f};
    if (is1('K'))      return {{0.56f, 0.25f, 0.83f}, 2.75f};

    return {{0.90f, 0.50f, 0.55f}, 1.60f}; // unknown: pink
}