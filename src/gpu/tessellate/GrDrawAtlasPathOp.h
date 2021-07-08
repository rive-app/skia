/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrDrawAtlasPathOp_DEFINED
#define GrDrawAtlasPathOp_DEFINED

#include "src/core/SkIPoint16.h"
#include "src/gpu/ops/GrDrawOp.h"
#include "src/gpu/tessellate/GrAtlasInstancedHelper.h"

// Fills a rectangle of pixels with a clip against coverage values from an atlas.
class GrDrawAtlasPathOp : public GrDrawOp {
public:
    DEFINE_OP_CLASS_ID

    GrDrawAtlasPathOp(SkArenaAlloc* arena, const SkIRect& fillBounds, const SkMatrix& localToDevice,
                      GrPaint&& paint, SkIPoint16 locationInAtlas, const SkIRect& pathDevIBounds,
                      bool transposedInAtlas, GrSurfaceProxyView atlasView, bool isInverseFill,
                      int numRenderTargetSamples)
            : GrDrawOp(ClassID())
            , fHeadInstance(arena->make<Instance>(fillBounds, localToDevice, paint.getColor4f(),
                                                  locationInAtlas, pathDevIBounds,
                                                  transposedInAtlas))
            , fTailInstance(&fHeadInstance->fNext)
            , fAtlasHelper(std::move(atlasView),
                           isInverseFill ? GrAtlasInstancedHelper::ShaderFlags::kCheckBounds |
                                           GrAtlasInstancedHelper::ShaderFlags::kInvertCoverage
                                         : GrAtlasInstancedHelper::ShaderFlags::kNone)
            , fEnableHWAA(numRenderTargetSamples > 1)
            , fProcessors(std::move(paint)) {
        this->setBounds(SkRect::Make(fillBounds), HasAABloat::kYes, IsHairline::kNo);
    }

    const char* name() const override { return "GrDrawAtlasPathOp"; }
    FixedFunctionFlags fixedFunctionFlags() const override {
        return fEnableHWAA ? FixedFunctionFlags::kUsesHWAA : FixedFunctionFlags::kNone;
    }
    void visitProxies(const GrVisitProxyFunc& func) const override {
        func(fAtlasHelper.proxy(), GrMipmapped::kNo);
        fProcessors.visitProxies(func);
    }
    GrProcessorSet::Analysis finalize(const GrCaps&, const GrAppliedClip*, GrClampType) override;
    CombineResult onCombineIfPossible(GrOp*, SkArenaAlloc*, const GrCaps&) override;

    void onPrePrepare(GrRecordingContext*, const GrSurfaceProxyView& writeView, GrAppliedClip*,
                      const GrDstProxyView&, GrXferBarrierFlags, GrLoadOp colorLoadOp) override;
    void onPrepare(GrOpFlushState*) override;
    void onExecute(GrOpFlushState*, const SkRect& chainBounds) override;

private:
    void prepareProgram(const GrCaps&, SkArenaAlloc*, const GrSurfaceProxyView& writeView,
                        GrAppliedClip&&, const GrDstProxyView&, GrXferBarrierFlags,
                        GrLoadOp colorLoadOp);

    struct Instance {
        Instance(const SkIRect& fillIBounds, const SkMatrix& m,
                 const SkPMColor4f& color, SkIPoint16 locationInAtlas,
                 const SkIRect& pathDevIBounds, bool transposedInAtlas)
                : fFillBounds(fillIBounds)
                , fLocalToDeviceIfUsingLocalCoords{m.getScaleX(), m.getSkewY(),
                                                   m.getSkewX(), m.getScaleY(),
                                                   m.getTranslateX(), m.getTranslateY()}
                , fColor(color)
                , fAtlasInstance(locationInAtlas, pathDevIBounds, transposedInAtlas) {
        }
        SkIRect fFillBounds;
        std::array<float, 6> fLocalToDeviceIfUsingLocalCoords;
        SkPMColor4f fColor;
        GrAtlasInstancedHelper::Instance fAtlasInstance;
        Instance* fNext = nullptr;
    };

    Instance* fHeadInstance;
    Instance** fTailInstance;

    GrAtlasInstancedHelper fAtlasHelper;
    const bool fEnableHWAA;
    bool fUsesLocalCoords = false;

    int fInstanceCount = 1;

    GrProgramInfo* fProgram = nullptr;

    sk_sp<const GrBuffer> fInstanceBuffer;
    int fBaseInstance;

    // Only used if sk_VertexID is not supported.
    sk_sp<const GrGpuBuffer> fVertexBufferIfNoIDSupport;

    GrProcessorSet fProcessors;
};

#endif
