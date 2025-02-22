/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BASICCOMPOSITOR_H
#define MOZILLA_GFX_BASICCOMPOSITOR_H

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Triangle.h"
#include "mozilla/gfx/Polygon.h"

namespace mozilla {
namespace layers {

class BasicCompositingRenderTarget : public CompositingRenderTarget {
 public:
  BasicCompositingRenderTarget(gfx::DrawTarget* aDrawTarget,
                               const gfx::IntRect& aRect)
      : CompositingRenderTarget(aRect.TopLeft()),
        mDrawTarget(aDrawTarget),
        mSize(aRect.Size()) {}

  virtual const char* Name() const override {
    return "BasicCompositingRenderTarget";
  }

  virtual gfx::IntSize GetSize() const override { return mSize; }

  void BindRenderTarget();

  virtual gfx::SurfaceFormat GetFormat() const override {
    return mDrawTarget ? mDrawTarget->GetFormat()
                       : gfx::SurfaceFormat(gfx::SurfaceFormat::UNKNOWN);
  }

  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
};

class BasicCompositor : public Compositor {
 public:
  explicit BasicCompositor(CompositorBridgeParent* aParent,
                           widget::CompositorWidget* aWidget);

 protected:
  virtual ~BasicCompositor();

 public:
  virtual BasicCompositor* AsBasicCompositor() override { return this; }

  virtual bool Initialize(nsCString* const out_failureReason) override;

  virtual void DetachWidget() override;

  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() override;

  virtual already_AddRefed<CompositingRenderTarget> CreateRenderTarget(
      const gfx::IntRect& aRect, SurfaceInitMode aInit) override;

  virtual already_AddRefed<CompositingRenderTarget>
  CreateRenderTargetFromSource(const gfx::IntRect& aRect,
                               const CompositingRenderTarget* aSource,
                               const gfx::IntPoint& aSourcePoint) override;

  virtual already_AddRefed<CompositingRenderTarget> CreateRenderTargetForWindow(
      const LayoutDeviceIntRect& aRect, const LayoutDeviceIntRect& aClearRect,
      BufferMode aBufferMode);

  virtual already_AddRefed<DataTextureSource> CreateDataTextureSource(
      TextureFlags aFlags = TextureFlags::NO_FLAGS) override;

  virtual already_AddRefed<DataTextureSource> CreateDataTextureSourceAround(
      gfx::DataSourceSurface* aSurface) override;

  virtual already_AddRefed<DataTextureSource>
  CreateDataTextureSourceAroundYCbCr(TextureHost* aTexture) override;

  virtual bool SupportsEffect(EffectTypes aEffect) override;

  bool SupportsLayerGeometry() const override;

  virtual bool ReadbackRenderTarget(CompositingRenderTarget* aSource,
                                    AsyncReadbackBuffer* aDest) override;

  virtual already_AddRefed<AsyncReadbackBuffer> CreateAsyncReadbackBuffer(
      const gfx::IntSize& aSize) override;

  virtual bool BlitRenderTarget(CompositingRenderTarget* aSource,
                                const gfx::IntSize& aSourceSize,
                                const gfx::IntSize& aDestSize) override;

  virtual void SetRenderTarget(CompositingRenderTarget* aSource) override {
    mRenderTarget = static_cast<BasicCompositingRenderTarget*>(aSource);
    mRenderTarget->BindRenderTarget();
  }

  virtual already_AddRefed<CompositingRenderTarget> GetWindowRenderTarget()
      const override {
    return do_AddRef(mFullWindowRenderTarget);
  }

  virtual already_AddRefed<CompositingRenderTarget> GetCurrentRenderTarget()
      const override {
    return do_AddRef(mRenderTarget);
  }

  virtual void DrawQuad(const gfx::Rect& aRect, const gfx::IntRect& aClipRect,
                        const EffectChain& aEffectChain, gfx::Float aOpacity,
                        const gfx::Matrix4x4& aTransform,
                        const gfx::Rect& aVisibleRect) override;

  virtual void ClearRect(const gfx::Rect& aRect) override;

  virtual void BeginFrame(const nsIntRegion& aInvalidRegion,
                          const gfx::IntRect* aClipRectIn,
                          const gfx::IntRect& aRenderBounds,
                          const nsIntRegion& aOpaqueRegion,
                          gfx::IntRect* aClipRectOut = nullptr,
                          gfx::IntRect* aRenderBoundsOut = nullptr) override;
  virtual void EndFrame() override;

  virtual bool SupportsPartialTextureUpdate() override { return true; }
  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize& aSize) override {
    return true;
  }
  virtual int32_t GetMaxTextureSize() const override;
  virtual void SetDestinationSurfaceSize(const gfx::IntSize& aSize) override {}

  virtual void SetScreenRenderOffset(const ScreenPoint& aOffset) override {}

  virtual void MakeCurrent(MakeCurrentFlags aFlags = 0) override {}

#ifdef MOZ_DUMP_PAINTING
  virtual const char* Name() const override { return "Basic"; }
#endif  // MOZ_DUMP_PAINTING

  virtual LayersBackend GetBackendType() const override {
    return LayersBackend::LAYERS_BASIC;
  }

  gfx::DrawTarget* GetDrawTarget() { return mDrawTarget; }

  virtual bool IsPendingComposite() override {
    return mIsPendingEndRemoteDrawing;
  }

  virtual void FinishPendingComposite() override;

 private:
  template <typename Geometry>
  void DrawGeometry(const Geometry& aGeometry, const gfx::Rect& aRect,
                    const gfx::IntRect& aClipRect,
                    const EffectChain& aEffectChain, gfx::Float aOpacity,
                    const gfx::Matrix4x4& aTransform,
                    const gfx::Rect& aVisibleRect, const bool aEnableAA);

  virtual void DrawPolygon(const gfx::Polygon& aPolygon, const gfx::Rect& aRect,
                           const gfx::IntRect& aClipRect,
                           const EffectChain& aEffectChain, gfx::Float aOpacity,
                           const gfx::Matrix4x4& aTransform,
                           const gfx::Rect& aVisibleRect) override;

  void TryToEndRemoteDrawing(bool aForceToEnd = false);

  bool NeedsToDeferEndRemoteDrawing();

  // The final destination surface
  RefPtr<gfx::DrawTarget> mDrawTarget;
  // The current render target for drawing
  RefPtr<BasicCompositingRenderTarget> mRenderTarget;

  LayoutDeviceIntRect mInvalidRect;
  LayoutDeviceIntRegion mInvalidRegion;

  uint32_t mMaxTextureSize;
  bool mIsPendingEndRemoteDrawing;

  // mDrawTarget will not be the full window on all platforms. We therefore need
  // to keep a full window render target around when we are capturing
  // screenshots on those platforms.
  RefPtr<BasicCompositingRenderTarget> mFullWindowRenderTarget;
};

BasicCompositor* AssertBasicCompositor(Compositor* aCompositor);

}  // namespace layers
}  // namespace mozilla

#endif /* MOZILLA_GFX_BASICCOMPOSITOR_H */
