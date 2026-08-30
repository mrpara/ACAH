// mesh.h - indexed triangle meshes and the primitive builders the bot,
// the props and the terrain are assembled from.
#pragma once

#include <vector>
#include <cstdint>
#include "math3d.h"

namespace sb {

struct Vertex {
    Vec3 pos;
    Vec3 nrm;
    Vec3 col{1.0f, 1.0f, 1.0f};
};

struct Mesh {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;

    // Local-space bounding sphere, used for frustum and distance culling.
    Vec3 boundsCenter{0.0f, 0.0f, 0.0f};
    float boundsRadius = 0.0f;

    void clear() { verts.clear(); idx.clear(); boundsRadius = 0.0f; }
    size_t triangleCount() const { return idx.size() / 3; }
    void computeBounds();
};

// Recomputes vertex normals by area-weighted averaging of face normals.
void recomputeNormals(Mesh& mesh);

// Applies a transform to a mesh in place (normals use the rotation part).
void transformMesh(Mesh& mesh, const Mat4& xform);

// Appends `src` (optionally transformed and tinted) into `dst`.
void appendMesh(Mesh& dst, const Mesh& src, const Mat4& xform = Mat4::identity());
void appendMesh(Mesh& dst, const Mesh& src, const Mat4& xform, const Vec3& color);

void setMeshColor(Mesh& mesh, const Vec3& color);

// ------------------------------------------------------------------ primitives
// All primitives are authored around the origin with +Y up. Cylinders and cones
// have their base at y = 0 so segmentTransform() can stretch them along a bone.

Mesh makeBox(const Vec3& halfExtents, const Vec3& color = Vec3(1.0f, 1.0f, 1.0f));

// A box with its four vertical edges chamfered - an octagonal prism. Reads as a
// machined armour block rather than a plain cube, for very little extra cost.
Mesh makeChamferBox(const Vec3& halfExtents, float chamfer,
                    const Vec3& color = Vec3(1.0f, 1.0f, 1.0f));

// A box whose top face has different half-extents from its bottom, spanning
// y = 0..height. Sloped armour, hull glacis plates, tapered housings.
Mesh makeSlopedBox(const Vec3& bottomHalf, const Vec3& topHalf, float height,
                   const Vec3& color = Vec3(1.0f, 1.0f, 1.0f));

// Tapered cylinder: radius `rBottom` at y=0, `rTop` at y=height.
Mesh makeCylinder(float rBottom, float rTop, float height, int segments,
                  bool capBottom = true, bool capTop = true,
                  const Vec3& color = Vec3(1.0f, 1.0f, 1.0f));

Mesh makeSphere(float radius, int rings, int segments, const Vec3& color = Vec3(1.0f, 1.0f, 1.0f));

// Low-poly faceted rock built by perturbing a sphere; `seed` picks the shape.
Mesh makeRock(float radius, uint32_t seed, const Vec3& color = Vec3(1.0f, 1.0f, 1.0f));

} // namespace sb
