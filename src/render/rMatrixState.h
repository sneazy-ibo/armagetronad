#ifndef AT_MATRIX_STATE_H
#define AT_MATRIX_STATE_H

#ifndef DEDICATED

// Software fixed-function matrix stack: modelview / projection / texture, with
// push/pop, the classic glTranslate/glScale/glMultMatrix builders, and
// glFrustum/glOrtho. Salvaged from the RCL Metal experiment (see
// docs/metal-port-review.md). It carries no GL/Metal dependency of its own — it
// is the matrix math you need for *any* renderer that lacks the legacy
// fixed-function stack (Metal, or OpenGL core profile / VBO work, task #1).
//
// Matrices are column-major (OpenGL layout): element[col*4 + row].

// ponytail: RMATRIXSTATE_NO_GL lets the standalone self-check
// (rMatrixState_test.cpp) compile this without SDL/GL headers. Production builds
// take the rGL.h path so GLenum/GL_* match the rest of the engine.
#ifdef RMATRIXSTATE_NO_GL
typedef unsigned int GLenum;
#ifndef GL_MODELVIEW
#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_TEXTURE 0x1702
#endif
#else
#include "rGL.h"
#endif

class rMatrixState
{
public:
    static void ResetAll();
    static void MatrixMode(GLenum mode);
    static void LoadIdentity();
    static void MultMatrixf(const float* m);
    static void PushMatrix();
    static void PopMatrix();
    static void Translatef(float x, float y, float z);
    static void Scalef(float x, float y, float z);
    static void Frustum(float left, float right, float bottom, float top, float nearVal, float farVal);
    static void Ortho(float left, float right, float bottom, float top, float nearVal, float farVal);

    static void GetModelViewProjection(float out[16]);
    static void GetTextureMatrix(float out[16]);

private:
    enum
    {
        MATRIX_STACK_DEPTH = 32
    };

    static GLenum activeMode_;
    static int projSP_, mvSP_, texSP_;
    static float proj_[MATRIX_STACK_DEPTH][16];
    static float modelview_[MATRIX_STACK_DEPTH][16];
    static float texture_[MATRIX_STACK_DEPTH][16];

    static float* Current();
    static void LoadIdentityStack(float m[16]);
    static void Multiply(float m[16], const float* a, const float* b);
};

#endif

#endif
