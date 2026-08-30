// raster.h - multithreaded software rasterizer.
//
// The renderer produces a linear-RGB colour buffer plus a depth buffer at
// *sample* resolution, which is the ASCII cell grid multiplied by a
// supersampling factor. Nothing here knows about characters; ascii.h turns the
// colour buffer into glyphs.
#pragma once

#include <vector>
#include "math3d.h"
#include "mesh.h"
#include "threadpool.h"

namespace sb {

struct Camera {
    Vec3 pos{0.0f, 0.0f, 0.0f};
    Mat4 view = Mat4::identity();
    Mat4 proj = Mat4::identity();
    Mat4 viewProj = Mat4::identity();
    float fovY = deg2rad(60.0f);
    float aspect = 1.0f;
    float zNear = 0.1f;
    float zFar = 400.0f;
    Vec3 forward{0.0f, 0.0f, -1.0f};
    Vec3 right{1.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};

    // `upHint` rolls the camera. Passing the mech's own up vector is what keeps
    // the machine upright on screen while it walks up a wall, so the world
    // rotates around it rather than the other way round.
    void set(const Vec3& eye, const Vec3& target, float fovYRad, float aspectRatio,
             float nearZ, float farZ, const Vec3& upHint = Vec3(0.0f, 1.0f, 0.0f));
};

struct RenderSettings {
    // A low sun is deliberate: at a steep angle every patch of ground returns
    // nearly the same value and the character ramp has nothing to work with.
    // Around 28 degrees of elevation, hillsides swing across the whole ramp.
    Vec3 lightDir = normalize(Vec3(0.68f, 0.42f, 0.38f));   // surface -> light
    Vec3 lightColor{1.25f, 1.25f, 1.22f};
    // A dim fill from the opposite side. Without it, every surface facing away
    // from the key light collapses to the same black and the model loses its
    // shape - which matters far more here than it would in a full-colour render.
    Vec3 fillDir = normalize(Vec3(-0.55f, 0.30f, -0.62f));
    Vec3 fillColor{0.26f, 0.30f, 0.32f};
    Vec3 skyAmbient{0.13f, 0.15f, 0.18f};                   // light from above
    Vec3 groundAmbient{0.035f, 0.035f, 0.04f};              // bounce from below
    Vec3 fogColor{0.018f, 0.036f, 0.031f};
    float fogDensity = 0.0068f;
    Vec3 skyTop{0.004f, 0.010f, 0.013f};
    Vec3 skyHorizon{0.028f, 0.062f, 0.058f};
    // Grazing-angle brightening. Cheap stand-in for a specular highlight that
    // lights up silhouettes, which is exactly where ASCII needs the definition.
    float rimStrength = 0.38f;
    float rimPower = 3.0f;
    float exposure = 1.0f;
};

struct DrawItem {
    const Mesh* mesh = nullptr;
    Mat4 model = Mat4::identity();
    Vec3 tint{1.0f, 1.0f, 1.0f};
    float emissive = 0.0f;      // 0 = fully lit/fogged, 1 = self-lit, ignores fog
    // Per-item rim multiplier. The bot wants a strong silhouette highlight;
    // the terrain does not - at grazing angles a global rim washes the whole
    // ground to a uniform bright field and the scene loses all depth.
    float rim = 1.0f;
    // Draw back faces too. Structures are hollow shells - a ruin is walls
    // around an empty middle, a gallery is a roof you walk under - so from
    // inside or below a one-sided shell simply vanishes. Two-sided drawing
    // costs nothing on closed meshes (the z-test wins) and makes every shell
    // solid from every angle.
    bool twoSided = false;
};

class Rasterizer {
public:
    Rasterizer();
    ~Rasterizer();

    // `cellsX/cellsY` is the ASCII grid; `supersample` is 1 or 2 (2 = 4 samples
    // per character cell, which visibly smooths the luminance ramp).
    void resize(int cellsX, int cellsY, int supersample);
    void setThreadCount(int threads);

    int sampleWidth() const { return width_; }
    int sampleHeight() const { return height_; }
    int supersample() const { return ss_; }
    int cellsX() const { return cellsX_; }
    int cellsY() const { return cellsY_; }

    void beginFrame(const Camera& cam, const RenderSettings& settings);
    void submit(const DrawItem& item);
    void endFrame();

    const std::vector<Vec3>& colorBuffer() const { return color_; }
    const std::vector<float>& depthBuffer() const { return depth_; }   // 1/w, larger = nearer

    // Statistics for the HUD.
    int lastTriangleCount() const { return trianglesDrawn_; }
    int lastDrawCount() const { return drawsSubmitted_; }

private:
    struct ScreenVert {
        float x = 0.0f, y = 0.0f;   // pixel coordinates
        float invW = 0.0f;          // 1/w, used both for depth and for interpolation
        Vec3 cw{0.0f, 0.0f, 0.0f};  // shaded colour premultiplied by invW
    };
    struct ScreenTri {
        ScreenVert v[3];
        int yMin = 0, yMax = -1;
    };

    void rasterizeBand(int bandIndex);
    void clearBand(int bandIndex);

    int width_ = 0, height_ = 0, ss_ = 1, cellsX_ = 0, cellsY_ = 0;
    std::vector<Vec3> color_;
    std::vector<float> depth_;

    Camera cam_;
    RenderSettings settings_;
    Vec4 frustum_[6];                       // world-space planes, inward normals

    std::vector<DrawItem> items_;
    std::vector<std::vector<ScreenTri>> perThreadTris_;
    std::vector<ScreenTri> tris_;

    ThreadPool pool_;
    int bandCount_ = 1;
    int bandRows_ = 1;
    int batchCount_ = 0;
    int trianglesDrawn_ = 0;
    int drawsSubmitted_ = 0;
};

} // namespace sb
