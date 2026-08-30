#include "raster.h"
#include <algorithm>

#include <cstring>

namespace sb {

namespace {

// Triangles are handed to workers in batches of this size. Small enough that a
// 20k-triangle terrain spreads evenly, large enough that the atomic counter in
// the pool is not the bottleneck.
constexpr int kChunkTriangles = 384;

struct ClipVert {
    Vec4 clip;
    Vec3 col;
};

inline ClipVert lerpClip(const ClipVert& a, const ClipVert& b, float t) {
    ClipVert r;
    r.clip = a.clip + (b.clip - a.clip) * t;
    r.col = lerp(a.col, b.col, t);
    return r;
}

// Inverse transpose of the upper-left 3x3, so non-uniformly scaled parts (every
// leg segment is scaled differently along its length) still shade correctly.
Mat4 normalMatrixOf(const Mat4& m) {
    const float a = m.at(0, 0), b = m.at(0, 1), c = m.at(0, 2);
    const float d = m.at(1, 0), e = m.at(1, 1), f = m.at(1, 2);
    const float g = m.at(2, 0), h = m.at(2, 1), i = m.at(2, 2);

    const float A =  (e * i - f * h), B = -(d * i - f * g), C =  (d * h - e * g);
    const float det = a * A + b * B + c * C;
    Mat4 n;
    if (std::fabs(det) < 1e-12f) {
        n.at(0, 0) = a; n.at(0, 1) = b; n.at(0, 2) = c;
        n.at(1, 0) = d; n.at(1, 1) = e; n.at(1, 2) = f;
        n.at(2, 0) = g; n.at(2, 1) = h; n.at(2, 2) = i;
        return n;
    }
    const float inv = 1.0f / det;
    const float D = -(b * i - c * h), E =  (a * i - c * g), F = -(a * h - b * g);
    const float G =  (b * f - c * e), H = -(a * f - c * d), I =  (a * e - b * d);
    n.at(0, 0) = A * inv; n.at(0, 1) = B * inv; n.at(0, 2) = C * inv;
    n.at(1, 0) = D * inv; n.at(1, 1) = E * inv; n.at(1, 2) = F * inv;
    n.at(2, 0) = G * inv; n.at(2, 1) = H * inv; n.at(2, 2) = I * inv;
    return n;
}

} // namespace

// ------------------------------------------------------------------- Camera

void Camera::set(const Vec3& eye, const Vec3& target, float fovYRad, float aspectRatio,
                 float nearZ, float farZ, const Vec3& upHint) {
    pos = eye;
    fovY = fovYRad;
    aspect = aspectRatio;
    zNear = nearZ;
    zFar = farZ;
    forward = normalize(target - eye);
    Vec3 hint = normalize(upHint);
    if (std::fabs(dot(forward, hint)) > 0.999f) hint = Vec3(0.0f, 0.0f, 1.0f);
    right = normalize(cross(forward, hint));
    up = cross(right, forward);
    view = lookAt(eye, target, up);
    proj = perspective(fovY, aspect, zNear, zFar);
    viewProj = proj * view;
}

// --------------------------------------------------------------- Rasterizer

Rasterizer::Rasterizer() {
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    setThreadCount(static_cast<int>(hw));
}

Rasterizer::~Rasterizer() { pool_.shutdown(); }

void Rasterizer::setThreadCount(int threads) {
    if (threads < 1) threads = 1;
    pool_.start(threads - 1);
    // A few bands per thread keeps the load balanced when the geometry is all
    // clustered in one part of the screen.
    bandCount_ = threads * 4;
    if (height_ > 0) {
        bandRows_ = std::max(1, (height_ + bandCount_ - 1) / bandCount_);
        bandCount_ = (height_ + bandRows_ - 1) / bandRows_;
    }
}

void Rasterizer::resize(int cellsX, int cellsY, int supersample) {
    cellsX_ = std::max(1, cellsX);
    cellsY_ = std::max(1, cellsY);
    ss_ = std::max(1, std::min(3, supersample));
    width_ = cellsX_ * ss_;
    height_ = cellsY_ * ss_;
    color_.assign(static_cast<size_t>(width_) * height_, Vec3(0.0f));
    depth_.assign(static_cast<size_t>(width_) * height_, 0.0f);

    const int threads = pool_.threadCount();
    bandCount_ = threads * 4;
    bandRows_ = std::max(1, (height_ + bandCount_ - 1) / bandCount_);
    bandCount_ = (height_ + bandRows_ - 1) / bandRows_;
}

void Rasterizer::beginFrame(const Camera& cam, const RenderSettings& settings) {
    cam_ = cam;
    settings_ = settings;
    items_.clear();
    trianglesDrawn_ = 0;
    drawsSubmitted_ = 0;

    // Frustum planes straight out of the view-projection matrix (Gribb/Hartmann).
    const Mat4& m = cam_.viewProj;
    auto row = [&](int r) { return Vec4(m.at(r, 0), m.at(r, 1), m.at(r, 2), m.at(r, 3)); };
    const Vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
    frustum_[0] = r3 + r0;   // left
    frustum_[1] = r3 - r0;   // right
    frustum_[2] = r3 + r1;   // bottom
    frustum_[3] = r3 - r1;   // top
    frustum_[4] = r3 + r2;   // near
    frustum_[5] = r3 - r2;   // far
    for (Vec4& p : frustum_) {
        const float len = length(p.xyz());
        if (len > 1e-8f) p = p * (1.0f / len);
    }
}

void Rasterizer::submit(const DrawItem& item) {
    if (!item.mesh || item.mesh->idx.empty()) return;

    // Bounding-sphere frustum cull. Uniform-ish scale is assumed; the largest
    // column length is used as a conservative scale factor.
    const Mat4& mm = item.model;
    const float sx = length(Vec3(mm.at(0, 0), mm.at(1, 0), mm.at(2, 0)));
    const float sy = length(Vec3(mm.at(0, 1), mm.at(1, 1), mm.at(2, 1)));
    const float sz = length(Vec3(mm.at(0, 2), mm.at(1, 2), mm.at(2, 2)));
    const float scale = std::max(sx, std::max(sy, sz));
    const Vec3 centre = transformPoint(mm, item.mesh->boundsCenter);
    const float radius = item.mesh->boundsRadius * scale;
    for (const Vec4& p : frustum_) {
        if (dot(p.xyz(), centre) + p.w < -radius) return;
    }

    items_.push_back(item);
    ++drawsSubmitted_;
}

void Rasterizer::endFrame() {
    // ---- 1. split all submitted geometry into equal-sized triangle batches ----
    struct Batch { int item; uint32_t first; uint32_t count; };
    std::vector<Batch> batches;
    for (size_t i = 0; i < items_.size(); ++i) {
        const uint32_t triCount = static_cast<uint32_t>(items_[i].mesh->triangleCount());
        for (uint32_t t = 0; t < triCount; t += kChunkTriangles) {
            batches.push_back({static_cast<int>(i), t,
                               std::min<uint32_t>(kChunkTriangles, triCount - t)});
        }
    }
    if (perThreadTris_.size() < batches.size()) perThreadTris_.resize(batches.size());
    // Clear every bucket, not just the ones this frame will use: leftovers from
    // a busier frame would otherwise be rasterized again as ghost geometry.
    for (std::vector<ScreenTri>& bucket : perThreadTris_) bucket.clear();

    const Vec3 L = normalize(settings_.lightDir);
    const Vec3 fillL = normalize(settings_.fillDir);
    const float rimStrength = settings_.rimStrength;

    // ---- 2. vertex transform, shading, near clip and projection, in parallel ----
    pool_.parallelFor(static_cast<int>(batches.size()), [&](int bi) {
        const Batch& batch = batches[static_cast<size_t>(bi)];
        const DrawItem& item = items_[static_cast<size_t>(batch.item)];
        const Mesh& mesh = *item.mesh;
        std::vector<ScreenTri>& out = perThreadTris_[static_cast<size_t>(bi)];

        const Mat4 mvp = cam_.viewProj * item.model;
        const Mat4 nrmMat = normalMatrixOf(item.model);
        const float emissive = item.emissive;
        const float itemRim = rimStrength * item.rim;

        ClipVert poly[8], tmp[8];
        for (uint32_t t = 0; t < batch.count; ++t) {
            const uint32_t base = (batch.first + t) * 3;
            int nPoly = 3;
            for (int k = 0; k < 3; ++k) {
                const Vertex& vin = mesh.verts[mesh.idx[base + k]];
                ClipVert& cv = poly[k];
                cv.clip = transform(mvp, Vec4(vin.pos, 1.0f));

                const Vec3 worldPos = transformPoint(item.model, vin.pos);
                const Vec3 n = normalize(transformDir(nrmMat, vin.nrm));
                const Vec3 albedo = vin.col * item.tint;
                const float ndl = std::max(dot(n, L), 0.0f);
                // Hemisphere ambient: sky from above, dim bounce from below.
                const float hemi = n.y * 0.5f + 0.5f;
                const Vec3 ambient = lerp(settings_.groundAmbient, settings_.skyAmbient, hemi);
                const float ndf = std::max(dot(n, fillL), 0.0f);
                Vec3 lit = albedo * (settings_.lightColor * ndl + settings_.fillColor * ndf + ambient);

                if (itemRim > 0.0f) {
                    const Vec3 toEye = normalize(cam_.pos - worldPos);
                    const float facing = 1.0f - std::fabs(dot(n, toEye));
                    const float rim = std::pow(clampf(facing, 0.0f, 1.0f), settings_.rimPower);
                    // Only lit surfaces get a rim, so shadowed sides stay dark.
                    lit += albedo * (rim * itemRim * (0.35f + 0.65f * ndl));
                }

                const float viewDist = std::max(cv.clip.w, 0.0f);
                const float fog = 1.0f - std::exp(-viewDist * settings_.fogDensity);
                lit = lerp(lit, settings_.fogColor, fog);
                if (emissive > 0.0f) lit = lerp(lit, albedo * 2.2f, emissive);
                cv.col = lit * settings_.exposure;
            }

            // ---- near-plane clip (w >= zNear) ----
            const float nearW = cam_.zNear;
            bool needClip = false;
            for (int k = 0; k < 3; ++k) if (poly[k].clip.w < nearW) needClip = true;
            if (needClip) {
                int outN = 0;
                for (int k = 0; k < nPoly; ++k) {
                    const ClipVert& cur = poly[k];
                    const ClipVert& nxt = poly[(k + 1) % nPoly];
                    const float dCur = cur.clip.w - nearW;
                    const float dNxt = nxt.clip.w - nearW;
                    if (dCur >= 0.0f) tmp[outN++] = cur;
                    if ((dCur >= 0.0f) != (dNxt >= 0.0f)) {
                        const float denom = dCur - dNxt;
                        if (std::fabs(denom) > 1e-9f)
                            tmp[outN++] = lerpClip(cur, nxt, dCur / denom);
                    }
                    if (outN >= 7) break;
                }
                if (outN < 3) continue;
                nPoly = outN;
                for (int k = 0; k < nPoly; ++k) poly[k] = tmp[k];
            }

            // ---- project and fan-triangulate ----
            ScreenVert sv[8];
            for (int k = 0; k < nPoly; ++k) {
                const float invW = 1.0f / poly[k].clip.w;
                sv[k].x = (poly[k].clip.x * invW * 0.5f + 0.5f) * width_;
                sv[k].y = (0.5f - poly[k].clip.y * invW * 0.5f) * height_;
                sv[k].invW = invW;
                sv[k].cw = poly[k].col * invW;
            }
            for (int k = 1; k + 1 < nPoly; ++k) {
                ScreenTri tri;
                tri.v[0] = sv[0];
                tri.v[1] = sv[k];
                tri.v[2] = sv[k + 1];

                // Screen Y points down, so front faces (CCW in world) come out
                // with a negative signed area. Two-sided items keep their back
                // faces, rewound so the raster's inside test still works.
                float area = (tri.v[1].x - tri.v[0].x) * (tri.v[2].y - tri.v[0].y)
                           - (tri.v[2].x - tri.v[0].x) * (tri.v[1].y - tri.v[0].y);
                if (item.twoSided) {
                    if (area > 1e-7f) {
                        std::swap(tri.v[1], tri.v[2]);
                        area = -area;
                    }
                    if (area >= -1e-7f) continue;
                } else if (area >= -1e-7f) {
                    continue;
                }

                float minY = tri.v[0].y, maxY = tri.v[0].y;
                float minX = tri.v[0].x, maxX = tri.v[0].x;
                for (int j = 1; j < 3; ++j) {
                    minY = std::min(minY, tri.v[j].y); maxY = std::max(maxY, tri.v[j].y);
                    minX = std::min(minX, tri.v[j].x); maxX = std::max(maxX, tri.v[j].x);
                }
                if (maxX < 0.0f || minX > static_cast<float>(width_)) continue;
                if (maxY < 0.0f || minY > static_cast<float>(height_)) continue;
                tri.yMin = std::max(0, static_cast<int>(std::floor(minY)));
                tri.yMax = std::min(height_ - 1, static_cast<int>(std::ceil(maxY)));
                if (tri.yMin > tri.yMax) continue;
                out.push_back(tri);
            }
        }
    });

    for (const std::vector<ScreenTri>& v : perThreadTris_) trianglesDrawn_ += static_cast<int>(v.size());
    batchCount_ = static_cast<int>(batches.size());

    // ---- 3. clear + rasterize, one job per horizontal band ----
    pool_.parallelFor(bandCount_, [&](int band) {
        clearBand(band);
        rasterizeBand(band);
    });
}

void Rasterizer::clearBand(int band) {
    const int y0 = band * bandRows_;
    const int y1 = std::min(height_, y0 + bandRows_);
    if (y0 >= y1) return;

    const float tanHalf = std::tan(cam_.fovY * 0.5f);
    for (int y = y0; y < y1; ++y) {
        // Sky colour from the world-space direction of this scanline, so the
        // horizon stays put when the camera pitches.
        const float ndcY = 1.0f - 2.0f * ((y + 0.5f) / height_);
        const Vec3 dir = normalize(cam_.forward + cam_.up * (ndcY * tanHalf));
        Vec3 sky;
        if (dir.y >= 0.0f) {
            sky = lerp(settings_.skyHorizon, settings_.skyTop, smoothstep01(dir.y * 1.7f));
        } else {
            sky = lerp(settings_.skyHorizon, settings_.fogColor, smoothstep01(-dir.y * 4.0f));
        }
        sky = sky * settings_.exposure;
        Vec3* c = color_.data() + static_cast<size_t>(y) * width_;
        float* d = depth_.data() + static_cast<size_t>(y) * width_;
        for (int x = 0; x < width_; ++x) { c[x] = sky; d[x] = 0.0f; }
    }
}

void Rasterizer::rasterizeBand(int band) {
    const int bandY0 = band * bandRows_;
    const int bandY1 = std::min(height_, bandY0 + bandRows_);
    if (bandY0 >= bandY1) return;

    for (const std::vector<ScreenTri>& list : perThreadTris_) {
        for (const ScreenTri& tri : list) {
            if (tri.yMax < bandY0 || tri.yMin >= bandY1) continue;

            const ScreenVert& a = tri.v[0];
            const ScreenVert& b = tri.v[1];
            const ScreenVert& c = tri.v[2];

            const float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
            const float invArea = 1.0f / area;

            // Edge functions: w0 + w1 + w2 == area, each linear in x and y.
            const float A0 = b.y - c.y, B0 = c.x - b.x, C0 = b.x * c.y - c.x * b.y;
            const float A1 = c.y - a.y, B1 = a.x - c.x, C1 = c.x * a.y - a.x * c.y;
            const float A2 = a.y - b.y, B2 = b.x - a.x, C2 = a.x * b.y - b.x * a.y;

            float minXf = std::min(a.x, std::min(b.x, c.x));
            float maxXf = std::max(a.x, std::max(b.x, c.x));
            int x0 = std::max(0, static_cast<int>(std::floor(minXf)));
            int x1 = std::min(width_ - 1, static_cast<int>(std::ceil(maxXf)));
            int y0 = std::max(bandY0, tri.yMin);
            int y1 = std::min(bandY1 - 1, tri.yMax);
            if (x0 > x1 || y0 > y1) continue;

            for (int y = y0; y <= y1; ++y) {
                const float py = y + 0.5f;
                float px = x0 + 0.5f;
                float w0 = A0 * px + B0 * py + C0;
                float w1 = A1 * px + B1 * py + C1;
                float w2 = A2 * px + B2 * py + C2;

                Vec3* crow = color_.data() + static_cast<size_t>(y) * width_;
                float* drow = depth_.data() + static_cast<size_t>(y) * width_;

                for (int x = x0; x <= x1; ++x, w0 += A0, w1 += A1, w2 += A2) {
                    // area is negative for front faces, so inside means all
                    // three edge values share that sign.
                    if (w0 > 0.0f || w1 > 0.0f || w2 > 0.0f) continue;
                    const float l0 = w0 * invArea;
                    const float l1 = w1 * invArea;
                    const float l2 = w2 * invArea;
                    const float invW = l0 * a.invW + l1 * b.invW + l2 * c.invW;
                    if (invW <= drow[x]) continue;
                    drow[x] = invW;
                    const Vec3 cw = a.cw * l0 + b.cw * l1 + c.cw * l2;
                    crow[x] = cw * (1.0f / invW);
                }
            }
        }
    }
}

} // namespace sb
