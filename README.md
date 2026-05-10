# GPU Ray-Casting of Quadric Surfaces

Real-time molecular visualiser built for the *Real-time Graphics Programming* course at **Università degli Studi di Milano**.

The renderer implements the method described in:

> Sigg, C., Weyrich, T., Botsch, M., & Gross, M. (2006).  
> **GPU-Based Ray-Casting of Quadratic Surfaces.**  
> Eurographics Symposium on Point-Based Graphics.  
> <http://reality.cs.ucl.ac.uk/projects/quadrics/pbg06.pdf>

Atoms are rendered as mathematically exact spheres and bonds as cylinders, both via GPU ray-casting. No tessellation is used — silhouettes and normals are pixel-perfect at any zoom level.

---

## Screenshots

| Deferred lighting pass | Full pipeline |
|---|---|
| ![First light pass](report/screenshot/quadric_first_light_pass.png) | ![Shadows and SSAO](report/screenshot/quadric_light_and_shadows.png) |

---

## Features

- **Sphere and cylinder impostors** — instanced rendering, one draw call per primitive type
- **Tight NDC bounding box** (Sigg et al., eq. 5) — exact screen-space coverage at any zoom
- **Deferred shading** — G-buffer (diffuse RGB8, normals RGB16F, depth), Blinn-Phong lighting
- **Shadow maps** — 2048×2048 depth-only FBO, PCF 3×3 soft shadows, custom silhouette clipping per impostor type
- **SSAO** — 16-sample hemisphere kernel, 4×4 noise rotation, box-blur pass
- **Composite pass** — intermediate RGB16F FBO with exposure control
- **ImGui control panel** — live controls for molecule, lighting, SSAO, camera, and post-process settings
- **HiDPI / Retina support** — framebuffer size queried from GLFW, FBOs recreated on window resize

---

## Requirements

- macOS (tested on Apple Silicon and Intel)
- Xcode Command Line Tools
- [CMake](https://cmake.org/) ≥ 3.10
- [Homebrew](https://brew.sh/) packages:

```bash
brew install cmake glfw glm
```

GLAD is vendored in `external/glad/`. ImGui is vendored in `external/imgui/`. No other external dependencies are needed.

---

## Build

```bash
git clone https://github.com/OpenGL-Quadrics-Unimi/GPU-RayCasting-Quadrics.git
cd GPU-RayCasting-Quadrics
cmake -B build -S .
cmake --build build --parallel
```

The executable is placed at `build/QuadricRaycaster`.

---

## Run

```bash
cd build
./QuadricRaycaster
```

The application loads **crambin (1CRN)** by default. Use the ImGui panel to switch molecules.

---

## Controls

| Input | Action |
|---|---|
| Left-click drag | Orbit camera |
| Scroll wheel | Zoom in / out |
| ESC | Quit |

All other parameters are controlled via the **ImGui panel** (top-left corner):

| Section | Controls |
|---|---|
| **Molecule** | Switch between 11 PDB structures |
| **Animation** | Auto-rotate toggle, Reset View button |
| **Lighting** | Light direction, ambient, specular, shininess |
| **Ambient Occlusion** | Enable/disable SSAO, AO radius |
| **Camera** | Field of view |
| **Post-Process** | Exposure multiplier |
| **Stats** | Atom count, bond count, FPS |

---

## Included Molecules

| PDB ID | Name | Description |
|---|---|---|
| 1CRN | Crambin | Small plant protein, 327 atoms |
| 1UBQ | Ubiquitin | Cell-signalling protein, 660 atoms |
| 1MBN | Myoglobin | Oxygen-binding protein, 1 260 atoms |
| 1GFL | GFP | Green Fluorescent Protein, 1 798 atoms |
| 1HHP | HIV-PR | HIV Protease, 1 628 atoms |
| 2INS | Insulin | Hormone, 806 atoms |
| 2LYZ | Lysozyme | Antibacterial enzyme, 1 102 atoms |
| 3HHB | Hemoglobin | Oxygen transport, 4 778 atoms |
| 4HHB | Hemoglobin R | Oxygenated form, 4 798 atoms |
| 1AON | GroEL | Chaperone complex, ~58 k atoms |
| 1FFK | Ribosome | Protein synthesis, ~100 k atoms |

---

## Project Structure

```
src/
    main.cpp                   entry point, render loop
    Core/                      Camera, Renderer
    Geometry/                  PDB parser, Bond detector
include/
    Core/ Geometry/            headers
shaders/
    quadric.vert / .frag       sphere impostor geometry + ray-casting
    cylinder.vert / .frag      cylinder impostor geometry + ray-casting
    ground.vert / .frag        ground plane (G-buffer pass)
    lighting.vert / .frag      deferred lighting + shadows + SSAO
    ssao.frag / ssaoblur.frag  ambient occlusion passes
    composite.frag             exposure control, final output
    shadow*.vert / .frag       depth-only shadow pass shaders
external/
    glad/                      OpenGL loader (vendored)
    imgui/                     Dear ImGui (vendored)
data/
    *.pdb                      PDB structure files
```

---

## Authors

- **Giorgia Carboni** — CPU side, foundations, geometry setup, composite pass, ImGui
- **Betul Gul** — GPU side, shaders, deferred pipeline, shadows, SSAO

*Università degli Studi di Milano — Academic Year 2025–2026*
