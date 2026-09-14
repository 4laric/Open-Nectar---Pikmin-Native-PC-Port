#pragma once
// Camera-facing billboard rotation for flagged MOD meshes (#429, parent #128).
//
// The converted MOD format bakes all geometry through its joints, so a
// billboard shape is emitted pivot-centred and re-placed by the joint's
// translation. At draw time the draw matrix for a flagged mesh keeps the
// joint's translation/scale but replaces its rotation with the inverse of the
// combined model*view rotation, so the mesh faces the camera in view space.
//
// This header is engine-independent (plain 3x3 floats) so the standalone probe
// shares the exact same math as the renderer.
#include <cmath>

namespace p2billboard {

// out = normalized((model * view).rotation)^T.
//
// `model` and `view` are row-major 3x3 rotation extracts of the active model
// matrix and the camera view matrix. Returns false (leaving `out` untouched)
// when a basis column degenerates, in which case the caller keeps the plain
// joint matrix.
inline bool facingRotation(float out[3][3], const float model[3][3], const float view[3][3]) {
    // combined = view * model
    float combined[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            combined[i][j] = view[i][0] * model[0][j]
                           + view[i][1] * model[1][j]
                           + view[i][2] * model[2][j];
        }
    }
    // Normalize each basis column so any model/joint scale survives while the
    // orientation is orthonormalised.
    for (int j = 0; j < 3; ++j) {
        const float norm = combined[0][j] * combined[0][j]
                         + combined[1][j] * combined[1][j]
                         + combined[2][j] * combined[2][j];
        if (norm < 1e-12f) {
            return false;
        }
        const float inv = 1.0f / std::sqrt(norm);
        for (int i = 0; i < 3; ++i) {
            combined[i][j] *= inv;
        }
    }
    // Inverse of an orthonormal rotation is its transpose.
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out[i][j] = combined[j][i];
        }
    }
    return true;
}

}  // namespace p2billboard
