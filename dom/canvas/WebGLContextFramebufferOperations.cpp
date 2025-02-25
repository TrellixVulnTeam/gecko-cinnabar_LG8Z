/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "WebGLFormats.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"

namespace mozilla {

void
WebGLContext::Clear(GLbitfield mask)
{
    const FuncScope funcScope(*this, "clear");
    if (IsContextLost())
        return;

    uint32_t m = mask & (LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT | LOCAL_GL_STENCIL_BUFFER_BIT);
    if (mask != m)
        return ErrorInvalidValue("Invalid mask bits.");

    if (mask == 0) {
        GenerateWarning("Calling gl.clear(0) has no effect.");
    } else if (mRasterizerDiscardEnabled) {
        GenerateWarning("Calling gl.clear() with RASTERIZER_DISCARD enabled has no effects.");
    }

    if (mask & LOCAL_GL_COLOR_BUFFER_BIT && mBoundDrawFramebuffer) {
        for (const auto& cur : mBoundDrawFramebuffer->ColorDrawBuffers()) {
            const auto imageInfo = cur->GetImageInfo();
            if (!imageInfo || !imageInfo->mFormat)
                continue;

            if (imageInfo->mFormat->format->baseType != webgl::TextureBaseType::Float) {
                ErrorInvalidOperation("Color draw buffers must be floating-point"
                                      " or fixed-point. (normalized (u)ints)");
                return;
            }
        }
    }

    if (!BindCurFBForDraw())
        return;

    auto driverMask = mask;
    if (!mBoundDrawFramebuffer) {
        if (mNeedsFakeNoDepth) {
            driverMask &= ~LOCAL_GL_DEPTH_BUFFER_BIT;
        }
        if (mNeedsFakeNoStencil) {
            driverMask &= ~LOCAL_GL_STENCIL_BUFFER_BIT;
        }
    }

    const ScopedDrawCallWrapper wrapper(*this);
    gl->fClear(driverMask);
}

static GLfloat
GLClampFloat(GLfloat val)
{
    if (val < 0.0)
        return 0.0;

    if (val > 1.0)
        return 1.0;

    return val;
}

void
WebGLContext::ClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    const FuncScope funcScope(*this, "clearColor");
    if (IsContextLost())
        return;

    const bool supportsFloatColorBuffers = (IsExtensionEnabled(WebGLExtensionID::EXT_color_buffer_float) ||
                                            IsExtensionEnabled(WebGLExtensionID::EXT_color_buffer_half_float) ||
                                            IsExtensionEnabled(WebGLExtensionID::WEBGL_color_buffer_float));
    if (!supportsFloatColorBuffers) {
        r = GLClampFloat(r);
        g = GLClampFloat(g);
        b = GLClampFloat(b);
        a = GLClampFloat(a);
    }

    gl->fClearColor(r, g, b, a);

    mColorClearValue[0] = r;
    mColorClearValue[1] = g;
    mColorClearValue[2] = b;
    mColorClearValue[3] = a;
}

void
WebGLContext::ClearDepth(GLclampf v)
{
    const FuncScope funcScope(*this, "clearDepth");
    if (IsContextLost())
        return;

    mDepthClearValue = GLClampFloat(v);
    gl->fClearDepth(mDepthClearValue);
}

void
WebGLContext::ClearStencil(GLint v)
{
    const FuncScope funcScope(*this, "clearStencil");
    if (IsContextLost())
        return;

    mStencilClearValue = v;
    gl->fClearStencil(v);
}

void
WebGLContext::ColorMask(WebGLboolean r, WebGLboolean g, WebGLboolean b, WebGLboolean a)
{
    const FuncScope funcScope(*this, "colorMask");
    if (IsContextLost())
        return;

    mColorWriteMask = uint8_t(bool(r)) << 0 |
                      uint8_t(bool(g)) << 1 |
                      uint8_t(bool(b)) << 2 |
                      uint8_t(bool(a)) << 3;
}

void
WebGLContext::DepthMask(WebGLboolean b)
{
    const FuncScope funcScope(*this, "depthMask");
    if (IsContextLost())
        return;

    mDepthWriteMask = b;
    gl->fDepthMask(b);
}

void
WebGLContext::DrawBuffers(const dom::Sequence<GLenum>& buffers)
{
    const FuncScope funcScope(*this, "drawBuffers");
    if (IsContextLost())
        return;

    if (mBoundDrawFramebuffer) {
        mBoundDrawFramebuffer->DrawBuffers(buffers);
        return;
    }

    // GLES 3.0.4 p186:
    // "If the GL is bound to the default framebuffer, then `n` must be 1 and the
    //  constant must be BACK or NONE. [...] If DrawBuffers is supplied with a
    //  constant other than BACK and NONE, or with a value of `n` other than 1, the
    //  error INVALID_OPERATION is generated."
    if (buffers.Length() != 1) {
        ErrorInvalidOperation("For the default framebuffer, `buffers` must have a"
                              " length of 1.");
        return;
    }

    switch (buffers[0]) {
    case LOCAL_GL_NONE:
    case LOCAL_GL_BACK:
        break;

    default:
        ErrorInvalidOperation("For the default framebuffer, `buffers[0]` must be"
                              " BACK or NONE.");
        return;
    }

    mDefaultFB_DrawBuffer0 = buffers[0];
    // Don't actually set it.
}

void
WebGLContext::StencilMask(GLuint mask)
{
    const FuncScope funcScope(*this, "stencilMask");
    if (IsContextLost())
        return;

    mStencilWriteMaskFront = mask;
    mStencilWriteMaskBack = mask;

    gl->fStencilMask(mask);
}

void
WebGLContext::StencilMaskSeparate(GLenum face, GLuint mask)
{
    const FuncScope funcScope(*this, "stencilMaskSeparate");
    if (IsContextLost())
        return;

    if (!ValidateFaceEnum(face))
        return;

    switch (face) {
        case LOCAL_GL_FRONT_AND_BACK:
            mStencilWriteMaskFront = mask;
            mStencilWriteMaskBack = mask;
            break;
        case LOCAL_GL_FRONT:
            mStencilWriteMaskFront = mask;
            break;
        case LOCAL_GL_BACK:
            mStencilWriteMaskBack = mask;
            break;
    }

    gl->fStencilMaskSeparate(face, mask);
}

} // namespace mozilla
