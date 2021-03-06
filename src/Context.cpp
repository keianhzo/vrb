/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vrb/Context.h"
#include "vrb/private/ResourceGLState.h"
#include "vrb/private/UpdatableState.h"
#include "vrb/GLExtensions.h"

#include "vrb/ConcreteClass.h"
#if defined(ANDROID)
#include "vrb/FileReaderAndroid.h"
#include "vrb/ClassLoaderAndroid.h"
#endif // defined(ANDROID)
#include "vrb/Logger.h"
#include "vrb/ResourceGL.h"
#include "vrb/SurfaceTextureFactory.h"
#include "vrb/TextureCache.h"
#include "vrb/Updatable.h"
#include <EGL/egl.h>

namespace vrb {

struct Context::State {
  std::weak_ptr<Context> self;
  EGLContext eglContext;
  TextureCachePtr textureCache;
  GLExtensionsPtr glExtensions;
#if defined(ANDROID)
  FileReaderAndroidPtr fileReader;
  SurfaceTextureFactoryPtr surfaceTextureFactory;
  ClassLoaderAndroidPtr classLoader;
#endif // defined(ANDROID)
  UpdatableHead updatableHead;
  UpdatableTail updatableTail;
  ResourceGLHead addedResourcesHead;
  ResourceGLTail addedResourcesTail;
  ResourceGLHead resourcesHead;
  ResourceGLTail resourcesTail;
  State();
};

Context::State::State() : eglContext(EGL_NO_CONTEXT) {
  updatableHead.BindTail(updatableTail);
  addedResourcesHead.BindTail(addedResourcesTail);
  resourcesHead.BindTail(resourcesTail);
}

ContextPtr
Context::Create() {
  ContextPtr result = std::make_shared<ConcreteClass<Context, Context::State> >();
  result->m.self = result;
  result->m.textureCache = TextureCache::Create(result->m.self);
  result->m.glExtensions = GLExtensions::Create(result->m.self);
#if defined(ANDROID)
  result->m.fileReader = FileReaderAndroid::Create(result->m.self);
  result->m.surfaceTextureFactory = SurfaceTextureFactory::Create(result->m.self);
  result->m.classLoader = ClassLoaderAndroid::Create();
#endif // defined(ANDROID)

  return result;
}

#if defined(ANDROID)
void
Context::InitializeJava(JNIEnv* aEnv, jobject & aActivity, jobject& aAssetManager) {
  if (m.classLoader) { m.classLoader->Init(aEnv, aActivity); }
  if (m.fileReader) { m.fileReader->Init(aEnv, aAssetManager, m.classLoader); }
  if (m.surfaceTextureFactory) { m.surfaceTextureFactory->InitializeJava(aEnv); }
}

void
Context::ShutdownJava() {
  if (m.fileReader) { m.fileReader->Shutdown(); }
  if (m.classLoader) { m.classLoader->Shutdown(); }
}
#endif // defined(ANDROID)

bool
Context::InitializeGL() {
  EGLContext current = eglGetCurrentContext();
  if (current == EGL_NO_CONTEXT) {
    VRB_LOG("Unable to initialize VRB context: EGLContext is not valid.");
    m.eglContext = current;
    return false;
  }
  if (current == m.eglContext) {
    VRB_LOG("EGLContext c:%p == %p",(void*)current,(void*)m.eglContext);
  } else {
    VRB_LOG("*** EGLContext NOT EQUAL %p != %p",(void*)current,(void*)m.eglContext);
  }
  m.eglContext = current;

  m.resourcesHead.InitializeGL(*this);
  m.glExtensions->Initialize();
  return true;
}


void
Context::Update() {
  if (m.addedResourcesHead.Update(*this)) {
    m.resourcesTail.PrependAndAdoptList(m.addedResourcesHead, m.addedResourcesTail);
  }
  m.updatableHead.UpdateResource(*this);
}

void
Context::ShutdownGL() {
  EGLContext current = eglGetCurrentContext();
  if (current == EGL_NO_CONTEXT) {
    VRB_LOG("Unable to shutdown VRB context: EGLContext is not valid.");
  }
  m.resourcesHead.ShutdownGL(*this);
  m.eglContext = EGL_NO_CONTEXT;
}

FileReaderPtr
Context::GetFileReader() {
#if defined(ANDROID)
  return m.fileReader;
#else
#  error "Platform not supported"
#endif // defined(ANDROID)
}

void
Context::AddUpdatable(Updatable* aUpdatable) {
  m.updatableTail.Prepend(aUpdatable);
}

void
Context::AddResourceGL(ResourceGL* aResource) {
  m.addedResourcesTail.Prepend(aResource);
}

TextureCachePtr
Context::GetTextureCache() {
  return m.textureCache;
}

GLExtensionsPtr
Context::GetGLExtensions() const {
  return m.glExtensions;
}

#if defined(ANDROID)
SurfaceTextureFactoryPtr
Context::GetSurfaceTextureFactory() {
  return m.surfaceTextureFactory;
}
#endif // defined(ANDROID)

Context::Context(State& aState) : m(aState) {}
Context::~Context() {}

} // namespace vrb
