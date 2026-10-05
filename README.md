 # Computer Graphics 2026 - Course Portfolio

## Student Information

- **Name:** Natalie Bittar  
  **Student ID:** 212586044

- **Name:** Adan Awad  
  **Student ID:** 324302678

---

## Course Portfolio

This repository contains our coursework for the **Computer Graphics 2026** course at the University of Haifa.

The repository contains Homework Assignments 1–5, the NanoRender framework used throughout the course, and our Final Project.

The assignments build progressively on the graphics pipeline developed during the course, starting from basic framebuffer manipulation and line drawing and progressing to 3D transformations, cameras, rasterization, lighting, shading, textures, and shadows.

---

## Table of Contents

| Assignment | Topic | Link |
|---|---|---|
| **HW1** | Basic Graphics and Immediate Mode GUI | [View HW1](./hw1/) |
| **HW2** | Wireframe Viewer and Geometric Transformations | [View HW2](./hw2/) |
| **HW3** | Virtual Cameras and Projections | [View HW3](./hw3/) |
| **HW4** | Triangle Rasterization and Depth Buffering | [View HW4](./hw4/) |
| **HW5** | Lighting, Materials, and Shading | [View HW5](./hw5/) |
| **Final Project** | Real-Time Shadow Rendering in a Software Renderer | [View Final Project](./Final-Project/) |

---

## Homework Assignments

### HW1 - Basic Graphics and Immediate Mode GUI

The first assignment introduces low-level graphics programming and framebuffer manipulation.

Main topics include:

- Framebuffer manipulation
- RGB color generation
- Immediate Mode GUI
- Keyboard and mouse input
- Interactive application state
- Bresenham line drawing
- Interactive drawing tools

[View HW1](./hw1/)

---

### HW2 - Wireframe Viewer and Geometric Transformations

The second assignment extends the renderer from 2D graphics to basic 3D geometry.

Main topics include:

- GLM integration
- OBJ model loading
- Mesh normalization
- Orthographic projection
- Wireframe rendering
- Translation
- Rotation
- Scaling
- Local and world transformations
- Keyboard and mouse interaction

[View HW2](./hw2/)

---

### HW3 - Virtual Cameras and Projections

The third assignment introduces virtual cameras and the complete transformation pipeline.

Main topics include:

- Coordinate axes
- Bounding boxes
- Camera representation
- View matrix
- Perspective projection
- Orthographic projection
- Face normals
- Vertex normals
- LookAt camera
- Dolly Zoom

[View HW3](./hw3/)

---

### HW4 - Triangle Rasterization and Depth Buffering

The fourth assignment extends the renderer from wireframe rendering to filled triangle rasterization.

Main topics include:

- Triangle bounding boxes
- Barycentric coordinates
- Triangle filling
- Z-buffer
- Depth testing
- Depth visualization
- Backface culling
- Triangle rasterization rules

[View HW4](./hw4/)

---

### HW5 - Lighting, Materials, and Shading

The fifth assignment introduces lighting and shading into the software rendering pipeline.

Main topics include:

- Point lights
- Materials
- Ambient lighting
- Diffuse lighting
- Specular lighting
- Flat shading
- Gouraud shading
- Phong shading
- Texture coordinates
- Texture mapping

[View HW5](./hw5/)

---

## NanoRender Framework

The `nanorender` directory contains the graphics framework used throughout the assignments.

The framework provides the basic infrastructure required by the homework implementations, including window management, framebuffer rendering, UI integration, and supporting libraries.

[View NanoRender](./nanorender/)

---

## Final Project

### Real-Time Shadow Rendering in a Software Renderer

The Final Project extends the software renderer developed throughout the course with real-time shadow rendering and additional interactive scene functionality.

The project includes features such as:

- 3D model loading
- Interactive scene editing
- Camera controls
- Model transformations
- Z-buffering
- Lighting and materials
- Flat, Gouraud, and Phong shading
- Texture support
- Backface culling
- Shadow mapping
- Real-time shadows
- Shadow filtering
- Interactive rendering controls

The Final Project directory contains the source code, models, build configuration, report, and a script for building and running the application.

[View Final Project](./Final-Project/)

---

## Repository Structure

```text
Computer-Graphics/
│
├── hw1/
│
├── hw2/
│
├── hw3/
│
├── hw4/
│
├── hw5/
│
├── nanorender/
│
├── Final-Project/
│
└── README.md
```

---

## Technologies

The coursework was developed using:

- C++
- CMake
- GLM
- MiniFB
- MicroUI
- Git and GitHub

---

## Academic Integrity

We certify that the work and reports submitted in this repository represent our coursework.

We understand and take responsibility for the code, implementation decisions, results, and documentation included in this repository.