#include "mesh.h"

namespace sb {

void Mesh::computeBounds() {
    if (verts.empty()) { boundsCenter = Vec3(0.0f); boundsRadius = 0.0f; return; }
    Vec3 lo = verts[0].pos, hi = verts[0].pos;
    for (const Vertex& v : verts) { lo = minv(lo, v.pos); hi = maxv(hi, v.pos); }
    boundsCenter = (lo + hi) * 0.5f;
    float r2 = 0.0f;
    for (const Vertex& v : verts) r2 = std::max(r2, lengthSq(v.pos - boundsCenter));
    boundsRadius = std::sqrt(r2);
}

void recomputeNormals(Mesh& mesh) {
    for (Vertex& v : mesh.verts) v.nrm = Vec3(0.0f);
    for (size_t i = 0; i + 2 < mesh.idx.size(); i += 3) {
        Vertex& a = mesh.verts[mesh.idx[i + 0]];
        Vertex& b = mesh.verts[mesh.idx[i + 1]];
        Vertex& c = mesh.verts[mesh.idx[i + 2]];
        // Not normalized: the magnitude is twice the triangle area, which gives
        // larger faces proportionally more influence.
        const Vec3 faceN = cross(b.pos - a.pos, c.pos - a.pos);
        a.nrm += faceN; b.nrm += faceN; c.nrm += faceN;
    }
    for (Vertex& v : mesh.verts) v.nrm = normalize(v.nrm);
}

void transformMesh(Mesh& mesh, const Mat4& xform) {
    for (Vertex& v : mesh.verts) {
        v.pos = transformPoint(xform, v.pos);
        v.nrm = normalize(transformDir(xform, v.nrm));
    }
    mesh.computeBounds();
}

void appendMesh(Mesh& dst, const Mesh& src, const Mat4& xform) {
    const uint32_t base = static_cast<uint32_t>(dst.verts.size());
    dst.verts.reserve(dst.verts.size() + src.verts.size());
    for (const Vertex& v : src.verts) {
        Vertex nv;
        nv.pos = transformPoint(xform, v.pos);
        nv.nrm = normalize(transformDir(xform, v.nrm));
        nv.col = v.col;
        dst.verts.push_back(nv);
    }
    dst.idx.reserve(dst.idx.size() + src.idx.size());
    for (uint32_t i : src.idx) dst.idx.push_back(base + i);
}

void appendMesh(Mesh& dst, const Mesh& src, const Mat4& xform, const Vec3& color) {
    const size_t first = dst.verts.size();
    appendMesh(dst, src, xform);
    for (size_t i = first; i < dst.verts.size(); ++i) dst.verts[i].col = color;
}

void setMeshColor(Mesh& mesh, const Vec3& color) {
    for (Vertex& v : mesh.verts) v.col = color;
}

// ------------------------------------------------------------------ primitives

Mesh makeBox(const Vec3& h, const Vec3& color) {
    Mesh m;
    // Six independent faces so each gets a hard, flat normal.
    const Vec3 n[6] = {
        { 1, 0, 0}, {-1, 0, 0}, {0,  1, 0},
        {0, -1, 0}, {0, 0,  1}, {0, 0, -1}
    };
    // Corner layout per face: (u, v) axes chosen so winding stays counter-clockwise.
    const Vec3 uAxis[6] = {
        {0, 0, -1}, {0, 0, 1}, {1, 0, 0},
        {-1, 0, 0}, {1, 0, 0}, {-1, 0, 0}
    };
    const Vec3 vAxis[6] = {
        {0, 1, 0}, {0, 1, 0}, {0, 0, 1},
        {0, 0, 1}, {0, 1, 0}, {0, 1, 0}
    };
    for (int f = 0; f < 6; ++f) {
        const Vec3 c = n[f] * Vec3(h.x, h.y, h.z);
        const Vec3 u = uAxis[f] * Vec3(h.x, h.y, h.z);
        const Vec3 v = vAxis[f] * Vec3(h.x, h.y, h.z);
        const uint32_t base = static_cast<uint32_t>(m.verts.size());
        m.verts.push_back({c - u - v, n[f], color});
        m.verts.push_back({c + u - v, n[f], color});
        m.verts.push_back({c + u + v, n[f], color});
        m.verts.push_back({c - u + v, n[f], color});
        m.idx.insert(m.idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    m.computeBounds();
    return m;
}

Mesh makeChamferBox(const Vec3& h, float chamfer, const Vec3& color) {
    Mesh m;
    const float c = clampf(chamfer, 0.0f, std::min(h.x, h.z) * 0.9f);
    // Octagonal cross-section in XZ, traced counter-clockwise seen from +Y.
    const float ox[8] = { h.x,  h.x,  c,   -c,   -h.x, -h.x, -c,   c   };
    const float oz[8] = { -c,   c,    h.z,  h.z,  c,   -c,   -h.z, -h.z };

    // Side walls.
    for (int i = 0; i < 8; ++i) {
        const int j = (i + 1) % 8;
        const Vec3 a(ox[i], -h.y, oz[i]);
        const Vec3 b(ox[j], -h.y, oz[j]);
        const Vec3 n = normalize(cross(Vec3(0.0f, 1.0f, 0.0f), b - a));
        const uint32_t base = static_cast<uint32_t>(m.verts.size());
        m.verts.push_back({a, n, color});
        m.verts.push_back({b, n, color});
        m.verts.push_back({Vec3(b.x, h.y, b.z), n, color});
        m.verts.push_back({Vec3(a.x, h.y, a.z), n, color});
        m.idx.insert(m.idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    // Caps as fans.
    const uint32_t topC = static_cast<uint32_t>(m.verts.size());
    m.verts.push_back({Vec3(0.0f, h.y, 0.0f), Vec3(0.0f, 1.0f, 0.0f), color});
    for (int i = 0; i < 8; ++i)
        m.verts.push_back({Vec3(ox[i], h.y, oz[i]), Vec3(0.0f, 1.0f, 0.0f), color});
    for (int i = 0; i < 8; ++i)
        m.idx.insert(m.idx.end(), {topC, topC + 1 + static_cast<uint32_t>(i),
                                   topC + 1 + static_cast<uint32_t>((i + 1) % 8)});

    const uint32_t botC = static_cast<uint32_t>(m.verts.size());
    m.verts.push_back({Vec3(0.0f, -h.y, 0.0f), Vec3(0.0f, -1.0f, 0.0f), color});
    for (int i = 0; i < 8; ++i)
        m.verts.push_back({Vec3(ox[i], -h.y, oz[i]), Vec3(0.0f, -1.0f, 0.0f), color});
    for (int i = 0; i < 8; ++i)
        m.idx.insert(m.idx.end(), {botC, botC + 1 + static_cast<uint32_t>((i + 1) % 8),
                                   botC + 1 + static_cast<uint32_t>(i)});
    m.computeBounds();
    return m;
}

Mesh makeSlopedBox(const Vec3& bottomHalf, const Vec3& topHalf, float height, const Vec3& color) {
    Mesh m;
    const Vec3 b[4] = {
        {-bottomHalf.x, 0.0f, -bottomHalf.z}, { bottomHalf.x, 0.0f, -bottomHalf.z},
        { bottomHalf.x, 0.0f,  bottomHalf.z}, {-bottomHalf.x, 0.0f,  bottomHalf.z}
    };
    const Vec3 t[4] = {
        {-topHalf.x, height, -topHalf.z}, { topHalf.x, height, -topHalf.z},
        { topHalf.x, height,  topHalf.z}, {-topHalf.x, height,  topHalf.z}
    };
    auto quad = [&](const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3) {
        const Vec3 n = normalize(cross(p1 - p0, p2 - p0));
        const uint32_t base = static_cast<uint32_t>(m.verts.size());
        m.verts.push_back({p0, n, color});
        m.verts.push_back({p1, n, color});
        m.verts.push_back({p2, n, color});
        m.verts.push_back({p3, n, color});
        m.idx.insert(m.idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    };
    quad(b[0], b[1], t[1], t[0]);   // -Z
    quad(b[1], b[2], t[2], t[1]);   // +X
    quad(b[2], b[3], t[3], t[2]);   // +Z
    quad(b[3], b[0], t[0], t[3]);   // -X
    quad(t[0], t[1], t[2], t[3]);   // top
    quad(b[3], b[2], b[1], b[0]);   // bottom
    m.computeBounds();
    return m;
}

Mesh makeCylinder(float rBottom, float rTop, float height, int segments,
                  bool capBottom, bool capTop, const Vec3& color) {
    Mesh m;
    if (segments < 3) segments = 3;

    // Side wall. The slant is baked into the normal so cones shade correctly.
    const float slant = (rBottom - rTop) / std::max(height, 1e-5f);
    for (int i = 0; i <= segments; ++i) {
        const float a = (static_cast<float>(i) / segments) * TAU;
        const float ca = std::cos(a), sa = std::sin(a);
        const Vec3 nrm = normalize(Vec3(ca, slant, sa));
        m.verts.push_back({Vec3(ca * rBottom, 0.0f, sa * rBottom), nrm, color});
        m.verts.push_back({Vec3(ca * rTop, height, sa * rTop), nrm, color});
    }
    for (int i = 0; i < segments; ++i) {
        const uint32_t b = static_cast<uint32_t>(i) * 2;
        m.idx.insert(m.idx.end(), {b, b + 2, b + 3, b, b + 3, b + 1});
    }

    if (capBottom && rBottom > 1e-5f) {
        const uint32_t centre = static_cast<uint32_t>(m.verts.size());
        m.verts.push_back({Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), color});
        for (int i = 0; i <= segments; ++i) {
            const float a = (static_cast<float>(i) / segments) * TAU;
            m.verts.push_back({Vec3(std::cos(a) * rBottom, 0.0f, std::sin(a) * rBottom),
                               Vec3(0.0f, -1.0f, 0.0f), color});
        }
        // Ring vertices run counter-clockwise seen from +Y, so a face that must
        // point -Y takes them in ascending order. (These two cap windings were
        // swapped originally, which back-face culled every top cap - the reason
        // the bot looked solid black from above.)
        for (int i = 0; i < segments; ++i)
            m.idx.insert(m.idx.end(), {centre, centre + 1 + static_cast<uint32_t>(i),
                                       centre + 1 + static_cast<uint32_t>(i) + 1});
    }
    if (capTop && rTop > 1e-5f) {
        const uint32_t centre = static_cast<uint32_t>(m.verts.size());
        m.verts.push_back({Vec3(0.0f, height, 0.0f), Vec3(0.0f, 1.0f, 0.0f), color});
        for (int i = 0; i <= segments; ++i) {
            const float a = (static_cast<float>(i) / segments) * TAU;
            m.verts.push_back({Vec3(std::cos(a) * rTop, height, std::sin(a) * rTop),
                               Vec3(0.0f, 1.0f, 0.0f), color});
        }
        for (int i = 0; i < segments; ++i)
            m.idx.insert(m.idx.end(), {centre, centre + 1 + static_cast<uint32_t>(i) + 1,
                                       centre + 1 + static_cast<uint32_t>(i)});
    }
    m.computeBounds();
    return m;
}

Mesh makeSphere(float radius, int rings, int segments, const Vec3& color) {
    Mesh m;
    if (rings < 2) rings = 2;
    if (segments < 3) segments = 3;
    for (int r = 0; r <= rings; ++r) {
        const float phi = (static_cast<float>(r) / rings) * PI;   // 0 at +Y pole
        const float sp = std::sin(phi), cp = std::cos(phi);
        for (int s = 0; s <= segments; ++s) {
            const float theta = (static_cast<float>(s) / segments) * TAU;
            const Vec3 n(sp * std::cos(theta), cp, sp * std::sin(theta));
            m.verts.push_back({n * radius, n, color});
        }
    }
    const int stride = segments + 1;
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            const uint32_t a = static_cast<uint32_t>(r * stride + s);
            const uint32_t b = static_cast<uint32_t>((r + 1) * stride + s);
            m.idx.insert(m.idx.end(), {a, b, b + 1, a, b + 1, a + 1});
        }
    }
    m.computeBounds();
    return m;
}

Mesh makeRock(float radius, uint32_t seed, const Vec3& color) {
    Mesh m = makeSphere(radius, 5, 7, color);
    Rng rng(seed * 2654435761u + 17u);
    // Push each ring/segment outwards by a per-direction amount so the silhouette
    // stays coherent (a per-vertex jitter would tear the shared seam apart).
    const int rings = 5, segments = 7;
    std::vector<float> scale((rings + 1) * (segments + 1), 1.0f);
    for (int r = 0; r <= rings; ++r) {
        for (int s = 0; s <= segments; ++s) {
            const int sw = (s == segments) ? 0 : s;   // seam shares the first column
            if (s == segments) { scale[r * (segments + 1) + s] = scale[r * (segments + 1) + sw]; continue; }
            scale[r * (segments + 1) + s] = rng.range(0.68f, 1.24f);
        }
    }
    for (int r = 0; r <= rings; ++r)
        for (int s = 0; s <= segments; ++s)
            m.verts[static_cast<size_t>(r) * (segments + 1) + s].pos *= scale[r * (segments + 1) + s];
    // Flatten the bottom so rocks sit on the ground instead of floating.
    for (Vertex& v : m.verts) v.pos.y = std::max(v.pos.y, -radius * 0.35f);
    recomputeNormals(m);
    m.computeBounds();
    return m;
}

} // namespace sb
