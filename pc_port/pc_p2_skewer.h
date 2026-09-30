#pragma once
// Shared "skewered Pikmin" orientation helper (#1020; Bloyster tongue, reusable by the Armored Maw #1014).
//
// A swallowed Pikmin is drawn from its mouth part's view-space matrix (viewPiki.cpp:671-682: the part
// matrix times a +90 degree Z turn), which makes the Pikmin's own up axis the part matrix's -X axis. A
// captor that only sets a yaw therefore leaves every held Pikmin standing upright at its joint. To look
// impaled, a captor aims the part matrix's X column along the spear/tongue instead: the Pikmin then lies
// along it with its head toward the tip.
//
// Engine-free: world-space basis only. The caller multiplies it by its camera rotation exactly as it
// already does for the yaw (see pc_p2_umimushi.cpp updateColl).
#include <cmath>

namespace p2skewer {

struct Basis {
    float x[3]; // part matrix X column (the Pikmin's up axis is -x)
    float y[3];
    float z[3];
};

// Basis whose -X axis is `dir` (the Pikmin's head points along dir). `dir` need not be normalised; a
// near-zero or vertical-degenerate direction falls back to `fallbackYaw` facing with a horizontal pose.
inline Basis along(const float dir[3], float fallbackYaw)
{
    float d[3] = {dir[0], dir[1], dir[2]};
    float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (!(len > 1.0e-3f)) {
        d[0] = std::sin(fallbackYaw);
        d[1] = 0.0f;
        d[2] = std::cos(fallbackYaw);
        len = 1.0f;
    }
    for (float& v : d) v /= len;
    Basis b;
    for (int i = 0; i < 3; ++i) b.x[i] = -d[i];
    // Y: world up made orthogonal to x (a sideways reference when the spear is vertical).
    float up[3] = {0.0f, 1.0f, 0.0f};
    if (std::fabs(b.x[1]) > 0.98f) { up[0] = 1.0f; up[1] = 0.0f; }
    const float dot = up[0] * b.x[0] + up[1] * b.x[1] + up[2] * b.x[2];
    for (int i = 0; i < 3; ++i) b.y[i] = up[i] - dot * b.x[i];
    const float yl = std::sqrt(b.y[0] * b.y[0] + b.y[1] * b.y[1] + b.y[2] * b.y[2]);
    for (float& v : b.y) v /= yl;
    // Z = X x Y
    b.z[0] = b.x[1] * b.y[2] - b.x[2] * b.y[1];
    b.z[1] = b.x[2] * b.y[0] - b.x[0] * b.y[2];
    b.z[2] = b.x[0] * b.y[1] - b.x[1] * b.y[0];
    return b;
}

// Direction of the spear at slot `i` of `n` slot positions p[i][3]: the central difference of its
// neighbours (forward/backward at the ends), i.e. the local tangent of the tongue.
inline void tangent(const float (*p)[3], int n, int i, float out[3])
{
    const int a = i > 0 ? i - 1 : i;
    const int c = i + 1 < n ? i + 1 : i;
    for (int k = 0; k < 3; ++k) out[k] = p[c][k] - p[a][k];
}

} // namespace p2skewer
