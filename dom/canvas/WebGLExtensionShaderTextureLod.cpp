/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionShaderTextureLod::WebGLExtensionShaderTextureLod(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");
}

WebGLExtensionShaderTextureLod::~WebGLExtensionShaderTextureLod()
{
}

bool
WebGLExtensionShaderTextureLod::IsSupported(const WebGLContext* webgl)
{
    gl::GLContext* gl = webgl->GL();
    if (gl->IsGLES() && gl->Version() >= 300) {
        // ANGLE's shader translator doesn't yet translate
        // WebGL1+EXT_shader_texture_lod to ES3. (Bug 1491221)
        return false;
    }
    return gl->IsSupported(gl::GLFeature::shader_texture_lod);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionShaderTextureLod, EXT_shader_texture_lod)

} // namespace mozilla
