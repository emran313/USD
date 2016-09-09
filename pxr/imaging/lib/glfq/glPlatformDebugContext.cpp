//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "pxr/imaging/glfq/glPlatformDebugContext.h"

#include "pxr/imaging/garch/gl.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/arch/defines.h"

/* static */
bool
GlfQGLPlatformDebugContext::IsEnabledDebugOutput()
{
    static bool isEnabledDebugOutput =
        TfGetenvBool("GLF_ENABLE_DEBUG_OUTPUT", false);
    return isEnabledDebugOutput;
}

/* static */
bool
GlfQGLPlatformDebugContext::IsEnabledCoreProfile()
{
    static bool isEnabledCoreProfile =
        TfGetenvBool("GLF_ENABLE_CORE_PROFILE", false);
    return isEnabledCoreProfile;
}


////////////////////////////////////////////////////////////
#if defined(ARCH_OS_LINUX)

#include <GL/glx.h>
#include <GL/glxtokens.h>
#include <X11/Xlib.h>

class GlfQGLPlatformDebugContextPrivate {
public:
    GlfQGLPlatformDebugContextPrivate(
        int majorVersion, int minorVersion,
        bool coreProfile, bool directRendering);
    ~GlfQGLPlatformDebugContextPrivate();

    void MakeCurrent();

    Display *_dpy;
    GLXContext _ctx;
};

GlfQGLPlatformDebugContextPrivate::GlfQGLPlatformDebugContextPrivate(
        int majorVersion, int minorVersion,
        bool coreProfile, bool directRendering)
    : _dpy(NULL)
    , _ctx(NULL)
{
    Display *shareDisplay = glXGetCurrentDisplay();
    GLXContext shareContext = glXGetCurrentContext();

    int fbConfigId = 0;
    glXQueryContext(shareDisplay, shareContext, GLX_FBCONFIG_ID, &fbConfigId);
    int screen = 0;
    glXQueryContext(shareDisplay, shareContext, GLX_SCREEN, &screen);

    int configSpec[] = {
        GLX_FBCONFIG_ID, fbConfigId,
        None, 
    };
    GLXFBConfig *configs = NULL;
    int configCount = 0;
    configs = glXChooseFBConfig(shareDisplay, screen, configSpec, &configCount);
    if (not TF_VERIFY(configCount > 0)) {
        return;
    }

    const int profile =
        coreProfile
            ? GLX_CONTEXT_CORE_PROFILE_BIT_ARB
            : GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;

    int attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, majorVersion,
        GLX_CONTEXT_MINOR_VERSION_ARB, minorVersion,
        GLX_CONTEXT_PROFILE_MASK_ARB, profile,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
        0,
    };

    // Extension entry points must be resolved at run-time.
    PFNGLXCREATECONTEXTATTRIBSARBPROC createContextAttribs =
        (PFNGLXCREATECONTEXTATTRIBSARBPROC)
            glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

    // Create a GL context with the requested capabilities.
    if (createContextAttribs) {
        _ctx = (*createContextAttribs)(shareDisplay, configs[0],
                                       shareContext, directRendering,
                                       attribs);
    } else {
        TF_WARN("Unable to create GL debug context.");
        XVisualInfo *vis = glXGetVisualFromFBConfig(shareDisplay, configs[0]);
        _ctx = glXCreateContext(shareDisplay, vis,
                                shareContext, directRendering);
    }
    if (not TF_VERIFY(_ctx)) {
        return;
    }

    _dpy = shareDisplay;
}

GlfQGLPlatformDebugContextPrivate::~GlfQGLPlatformDebugContextPrivate()
{
    if (_dpy and _ctx) {
        glXDestroyContext(_dpy, _ctx);
    }
}

void
GlfQGLPlatformDebugContextPrivate::MakeCurrent()
{
    glXMakeCurrent(glXGetCurrentDisplay(), glXGetCurrentDrawable(), _ctx);
}

void *GlfqSelectCoreProfileMacVisual()
{
    return nullptr;
}

#endif

////////////////////////////////////////////////////////////

#if defined(ARCH_OS_DARWIN)

// XXX: implement debug context
class GlfQGLPlatformDebugContextPrivate {
public:
    GlfQGLPlatformDebugContextPrivate(
        int majorVersion, int minorVersion,
        bool coreProfile, bool directRendering) {}
    ~GlfQGLPlatformDebugContextPrivate() {}

  void MakeCurrent() {}
};

void *GlfqSelectCoreProfileMacVisual();  // extern obj-c

#endif


////////////////////////////////////////////////////////////

#if defined(ARCH_OS_WINDOWS)

// XXX: implement debug context
class GlfQGLPlatformDebugContextPrivate {
public:
    GlfQGLPlatformDebugContextPrivate(
        int majorVersion, int minorVersion,
        bool coreProfile, bool directRendering) {}
    ~GlfQGLPlatformDebugContextPrivate() {}

  void MakeCurrent() {}
};

void *GlfqSelectCoreProfileMacVisual()
{
    return nullptr;
}

#endif


////////////////////////////////////////////////////////////

GlfQGLPlatformDebugContext::GlfQGLPlatformDebugContext(int majorVersion,
                                                       int minorVersion,
                                                       bool coreProfile,
                                                       bool directRendering)
#if !defined(ARCH_OS_WINDOWS)
    : _private(NULL)
    , _coreProfile(coreProfile)

#endif
{
    if (not GlfQGLPlatformDebugContext::IsEnabledDebugOutput()) {
        return;
    }
#if !defined(ARCH_OS_WINDOWS)
    _private.reset(new GlfQGLPlatformDebugContextPrivate(majorVersion,
                                                         minorVersion,
                                                         coreProfile,
                                                         directRendering));
#endif
}

GlfQGLPlatformDebugContext::~GlfQGLPlatformDebugContext()
{
    // nothing
}

/* virtual */
void
GlfQGLPlatformDebugContext::makeCurrent()
{
#if !defined(ARCH_OS_WINDOWS)
    if (not GlfQGLPlatformDebugContext::IsEnabledDebugOutput()) {
        return;
    }

    if (not TF_VERIFY(_private)) {
        return;
    }

    _private->MakeCurrent();
#endif
}

#if defined(ARCH_OS_DARWIN)
void*
GlfQGLPlatformDebugContext::chooseMacVisual()
{
    if (_coreProfile or
        GlfQGLPlatformDebugContext::IsEnabledCoreProfile()) {
        return GlfqSelectCoreProfileMacVisual();
    } else {
        return nullptr;
    }
    return nullptr;
}
#endif