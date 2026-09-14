#pragma once
// Engine-side billboard draw helpers (#429, parent #128).
//
// The PC port renders shapes through the OGL/DGX draw backends, not
// Joint::render. A flagged mesh's per-dependency draw matrix is replaced by
// `joint * facing` where `facing` cancels the combined model*view rotation, so
// the pivot-centred billboard geometry faces the camera. The alignment helpers
// also feed a small counter the private GL fixture reads to prove the path ran.
#include "Matrix4f.h"
#include "pc_p2_billboard.h"

#include <cmath>

namespace p2billboard {

// out = joint * normalized((model*view).rotation)^T. False leaves `out`
// untouched and the caller keeps the plain joint matrix.
inline bool facingMatrix(Matrix4f& out, const Matrix4f& joint, const Matrix4f& model,
                         const Matrix4f& view) {
    float m3[3][3];
    float v3[3][3];
    float r3[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            m3[i][j] = model.mMtx[i][j];
            v3[i][j] = view.mMtx[i][j];
        }
    }
    if (!facingRotation(r3, m3, v3)) {
        return false;
    }
    Matrix4f rot;
    rot.makeIdentity();
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            rot.mMtx[i][j] = r3[i][j];
        }
    }
    joint.multiplyTo(rot, out);
    return true;
}

// Max off-diagonal magnitude of the column-normalised rotation of `view*mesh`.
// ~0 means the mesh is screen-aligned; used as the GL-fixture assertion.
inline float offDiagonal(const Matrix4f& view, const Matrix4f& mesh) {
    Matrix4f final;
    view.multiplyTo(mesh, final);
    float column[3][3];
    for (int j = 0; j < 3; ++j) {
        const float norm = std::sqrt(final.mMtx[0][j] * final.mMtx[0][j]
                                     + final.mMtx[1][j] * final.mMtx[1][j]
                                     + final.mMtx[2][j] * final.mMtx[2][j]);
        if (norm < 1e-9f) {
            return 1e9f;
        }
        for (int i = 0; i < 3; ++i) {
            column[i][j] = final.mMtx[i][j] / norm;
        }
    }
    float worst = 0.f;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (i != j && std::fabs(column[i][j]) > worst) {
                worst = std::fabs(column[i][j]);
            }
        }
    }
    return worst;
}

struct Stats {
    unsigned long long draws = 0;
    float max_offdiagonal = 0.f;
};

inline Stats& stats() {
    static Stats value;
    return value;
}

}  // namespace p2billboard
