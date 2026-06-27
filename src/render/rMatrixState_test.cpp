// Standalone self-check for rMatrixState — no framework, no engine, no GL.
//   clang++ -std=c++17 src/render/rMatrixState_test.cpp -o /tmp/mtest && /tmp/mtest
// Pulls the implementation in directly so it links with nothing.

#define RMATRIXSTATE_NO_GL
#include "rMatrixState.cpp"

#include <cassert>
#include <cmath>
#include <cstdio>

// Transform a point by a column-major 4x4: out[row] = sum_col M[col*4+row] * p[col].
static void Transform(const float m[16], const float p[4], float out[4])
{
    for (int row = 0; row < 4; ++row)
        out[row] = m[0 * 4 + row] * p[0] + m[1 * 4 + row] * p[1] +
                   m[2 * 4 + row] * p[2] + m[3 * 4 + row] * p[3];
}

static bool Near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

static void ExpectPoint(const float m[16], float px, float py, float pz,
                        float ex, float ey, float ez)
{
    const float p[4] = {px, py, pz, 1.f};
    float o[4];
    Transform(m, p, o);
    assert(Near(o[0], ex) && Near(o[1], ey) && Near(o[2], ez) && Near(o[3], 1.f));
}

int main()
{
    float mvp[16];

    // 1. Reset gives identity modelview*projection.
    rMatrixState::ResetAll();
    rMatrixState::GetModelViewProjection(mvp);
    ExpectPoint(mvp, 2, 3, 4, 2, 3, 4);

    // 2. Translate moves a point (proj stays identity).
    rMatrixState::ResetAll();
    rMatrixState::Translatef(5, -2, 1);
    rMatrixState::GetModelViewProjection(mvp);
    ExpectPoint(mvp, 0, 0, 0, 5, -2, 1);

    // 3. Multiply order: glTranslate then glScale = T*S, so the point is scaled
    //    first, then translated. (1,0,0) -> scale*2 -> (2,0,0) -> +1 -> (3,0,0).
    rMatrixState::ResetAll();
    rMatrixState::Translatef(1, 0, 0);
    rMatrixState::Scalef(2, 2, 2);
    rMatrixState::GetModelViewProjection(mvp);
    ExpectPoint(mvp, 1, 0, 0, 3, 0, 0);

    // 4. Push/Pop restores the matrix.
    rMatrixState::ResetAll();
    rMatrixState::PushMatrix();
    rMatrixState::Translatef(9, 9, 9);
    rMatrixState::PopMatrix();
    rMatrixState::GetModelViewProjection(mvp);
    ExpectPoint(mvp, 0, 0, 0, 0, 0, 0);

    // 5. Ortho(-1,1,-1,1,-1,1) is identity except z is flipped.
    rMatrixState::ResetAll();
    rMatrixState::MatrixMode(GL_PROJECTION);
    rMatrixState::Ortho(-1, 1, -1, 1, -1, 1);
    rMatrixState::GetModelViewProjection(mvp);
    ExpectPoint(mvp, 0.5f, 0.25f, 0.5f, 0.5f, 0.25f, -0.5f);

    // 6. Ortho maps its box to NDC. GL z convention: eye-space -near -> -1,
    //    -far -> +1. With near=0,far=100: z=0 -> -1, z=-100 -> +1.
    rMatrixState::ResetAll();
    rMatrixState::MatrixMode(GL_PROJECTION);
    rMatrixState::Ortho(0, 10, 0, 20, 0, 100);
    rMatrixState::GetModelViewProjection(mvp);
    ExpectPoint(mvp, 10, 20, -100, 1, 1, 1);
    ExpectPoint(mvp, 0, 0, 0, -1, -1, -1);

    // 7. Texture matrix is an independent stack.
    rMatrixState::ResetAll();
    rMatrixState::MatrixMode(GL_TEXTURE);
    rMatrixState::Translatef(0.5f, 0.5f, 0);
    float tex[16];
    rMatrixState::GetTextureMatrix(tex);
    ExpectPoint(tex, 0, 0, 0, 0.5f, 0.5f, 0);
    rMatrixState::GetModelViewProjection(mvp); // modelview untouched
    ExpectPoint(mvp, 1, 2, 3, 1, 2, 3);

    std::printf("rMatrixState self-check: all passed\n");
    return 0;
}
