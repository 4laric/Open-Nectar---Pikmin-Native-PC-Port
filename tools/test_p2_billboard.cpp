// Standalone probe for the camera-facing billboard rotation (#429, parent #128).
//
// Build: g++ -std=c++17 -Wall -Wextra -Werror tools/test_p2_billboard.cpp -o p2_billboard.exe
//
// The renderer builds a flagged mesh's draw matrix as joint * rot where
// rot = normalized(view*model)^T. This proves that concatenation makes the
// flagged mesh screen-aligned in view space while preserving scale, and that
// degenerate bases are rejected instead of producing NaNs.
#include "../pc_port/pc_p2_billboard.h"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

bool check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", what);
        ++failures;
    }
    return ok;
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

void matmul(float out[3][3], const float a[3][3], const float b[3][3]) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
        }
    }
}

void ident(float m[3][3]) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            m[i][j] = (i == j) ? 1.f : 0.f;
        }
    }
}

void rotY(float m[3][3], float a) {
    ident(m);
    m[0][0] = std::cos(a);
    m[0][2] = std::sin(a);
    m[2][0] = -std::sin(a);
    m[2][2] = std::cos(a);
}

bool isIdentity(const float m[3][3]) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (!near(m[i][j], (i == j) ? 1.f : 0.f)) {
                return false;
            }
        }
    }
    return true;
}

void testIdentityModelView() {
    float model[3][3], view[3][3], out[3][3];
    ident(model);
    ident(view);
    check(p2billboard::facingRotation(out, model, view), "identity accepted");
    check(isIdentity(out), "identity model/view -> identity facing");
}

void testRotationCancelled() {
    float model[3][3], view[3][3], out[3][3], combined[3][3], product[3][3];
    float yaw[3][3], pitch[3][3];
    // model = yaw(0.7)
    rotY(model, 0.7f);
    // view = pitch(0.4) * yaw(-0.3)
    rotY(yaw, -0.3f);
    ident(pitch);
    pitch[1][1] = std::cos(0.4f);
    pitch[1][2] = -std::sin(0.4f);
    pitch[2][1] = std::sin(0.4f);
    pitch[2][2] = std::cos(0.4f);
    matmul(view, pitch, yaw);

    check(p2billboard::facingRotation(out, model, view), "rotated basis accepted");

    matmul(combined, view, model);
    matmul(product, combined, out);
    check(isIdentity(product), "view*model*facing is rotation-free");
}

void testScalePreserved() {
    float model[3][3], view[3][3], out[3][3];
    ident(model);
    rotY(model, 0.9f);
    // Uniform scale commutes with the rotation and must survive.
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            model[i][j] *= 2.5f;
        }
    }
    ident(view);
    check(p2billboard::facingRotation(out, model, view), "scaled basis accepted");
    // out is orthonormal (unit columns) so the draw keeps the model's scale.
    for (int j = 0; j < 3; ++j) {
        const float n = std::sqrt(out[0][j] * out[0][j] + out[1][j] * out[1][j]
                                  + out[2][j] * out[2][j]);
        check(near(n, 1.f), "facing columns are unit length");
    }
}

void testDegenerateRejected() {
    float model[3][3], view[3][3], out[3][3];
    ident(model);
    ident(view);
    model[0][1] = model[1][1] = model[2][1] = 0.f;  // collapse a model column
    // Zero the combined column by zeroing the corresponding view column too.
    view[0][1] = view[1][1] = view[2][1] = 0.f;
    check(!p2billboard::facingRotation(out, model, view), "degenerate basis rejected");
}

// --- Full 4x4 composition: joint translation/pivot handling -----------------
// Mirrors the renderer: B = joint * R, and the final draw matrix is
// view * model * B. A flagged mesh must end up screen-aligned (rotation = s*I)
// and placed at the joint's pivot transformed by view*model.

using M4 = float[4][4];

void ident4(M4 m) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            m[i][j] = (i == j) ? 1.f : 0.f;
        }
    }
}

void mul4(M4 out, const M4 a, const M4 b) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float sum = 0.f;
            for (int k = 0; k < 4; ++k) {
                sum += a[i][k] * b[k][j];
            }
            out[i][j] = sum;
        }
    }
}

void fromRot3(M4 out, const float r[3][3]) {
    ident4(out);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out[i][j] = r[i][j];
        }
    }
}

void testPivotComposition() {
    float model3[3][3], view3[3][3], facing[3][3];
    rotY(model3, 0.8f);
    rotY(view3, -0.5f);

    M4 model, view, joint, rot, billboard, vm, product;
    fromRot3(model, model3);
    fromRot3(view, view3);
    ident4(joint);
    const float s = 0.8f;
    const float px = -4.f, py = 46.f, pz = 0.f;
    // joint = T(p) * S(s)
    joint[0][0] = joint[1][1] = joint[2][2] = s;
    joint[0][3] = px;
    joint[1][3] = py;
    joint[2][3] = pz;

    check(p2billboard::facingRotation(facing, model3, view3), "composition facing");
    fromRot3(rot, facing);
    mul4(billboard, joint, rot);
    mul4(vm, view, model);
    mul4(product, vm, billboard);

    // Screen-aligned: rotation part is s*I.
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            check(near(product[i][j], (i == j) ? s : 0.f),
                  "composed draw matrix is screen-aligned with joint scale");
        }
    }
    // Translation is (view*model) applied to the pivot.
    for (int i = 0; i < 3; ++i) {
        const float want = vm[i][0] * px + vm[i][1] * py + vm[i][2] * pz + vm[i][3];
        check(near(product[i][3], want), "billboard pivot placed by view*model");
    }
}

}  // namespace

int main() {
    testIdentityModelView();
    testRotationCancelled();
    testScalePreserved();
    testDegenerateRejected();
    testPivotComposition();
    if (failures == 0) {
        std::printf("PASS p2_billboard\n");
        return 0;
    }
    std::fprintf(stderr, "FAIL p2_billboard: %d failures\n", failures);
    return 1;
}
