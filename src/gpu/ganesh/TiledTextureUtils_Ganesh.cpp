/*
 * Copyright 2023 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/TiledTextureUtils.h"

#include "include/core/SkBitmap.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkRect.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkSize.h"
#include "include/gpu/GrDirectContext.h"
#include "src/core/SkDevice.h"
#include "src/core/SkImagePriv.h"
#include "src/core/SkSamplingPriv.h"
#include "src/gpu/ganesh/Device.h"
#include "src/image/SkImage_Base.h"

#if GR_TEST_UTILS
extern int gOverrideMaxTextureSize;
extern std::atomic<int>  gNumTilesDrawn;
#endif

namespace {

void draw_tiled_bitmap_ganesh(skgpu::ganesh::Device* device,
                              const SkBitmap& bitmap,
                              int tileSize,
                              const SkMatrix& srcToDst,
                              const SkRect& srcRect,
                              const SkIRect& clippedSrcIRect,
                              const SkPaint& paint,
                              SkCanvas::QuadAAFlags origAAFlags,
                              SkCanvas::SrcRectConstraint constraint,
                              SkSamplingOptions sampling) {
    if (sampling.isAniso()) {
        sampling = SkSamplingPriv::AnisoFallback(/* imageIsMipped= */ false);
    }
    SkRect clippedSrcRect = SkRect::Make(clippedSrcIRect);

    int nx = bitmap.width() / tileSize;
    int ny = bitmap.height() / tileSize;

#if GR_TEST_UTILS
    gNumTilesDrawn.store(0, std::memory_order_relaxed);
#endif

    skia_private::TArray<SkCanvas::ImageSetEntry> imgSet(nx * ny);

    for (int x = 0; x <= nx; x++) {
        for (int y = 0; y <= ny; y++) {
            SkRect tileR;
            tileR.setLTRB(SkIntToScalar(x * tileSize),       SkIntToScalar(y * tileSize),
                          SkIntToScalar((x + 1) * tileSize), SkIntToScalar((y + 1) * tileSize));

            if (!SkRect::Intersects(tileR, clippedSrcRect)) {
                continue;
            }

            if (!tileR.intersect(srcRect)) {
                continue;
            }

            SkIRect iTileR;
            tileR.roundOut(&iTileR);
            SkVector offset = SkPoint::Make(SkIntToScalar(iTileR.fLeft),
                                            SkIntToScalar(iTileR.fTop));
            SkRect rectToDraw = tileR;
            if (!srcToDst.mapRect(&rectToDraw)) {
                continue;
            }

            if (sampling.filter != SkFilterMode::kNearest || sampling.useCubic) {
                SkIRect iClampRect;

                if (SkCanvas::kFast_SrcRectConstraint == constraint) {
                    // In bleed mode we want to always expand the tile on all edges
                    // but stay within the bitmap bounds
                    iClampRect = SkIRect::MakeWH(bitmap.width(), bitmap.height());
                } else {
                    // In texture-domain/clamp mode we only want to expand the
                    // tile on edges interior to "srcRect" (i.e., we want to
                    // not bleed across the original clamped edges)
                    srcRect.roundOut(&iClampRect);
                }
                int outset = sampling.useCubic ? kBicubicFilterTexelPad : 1;
                skgpu::TiledTextureUtils::ClampedOutsetWithOffset(&iTileR, outset, &offset,
                                                                  iClampRect);
            }

            // We must subset as a bitmap and then turn it into an SkImage if we want caching to
            // work. Image subsets always make a copy of the pixels and lose the association with
            // the original's SkPixelRef.
            if (SkBitmap subsetBmp; bitmap.extractSubset(&subsetBmp, iTileR)) {
                sk_sp<SkImage> image = SkMakeImageFromRasterBitmap(subsetBmp,
                                                                   kNever_SkCopyPixelsMode);
                if (!image) {
                    continue;
                }

                unsigned aaFlags = SkCanvas::kNone_QuadAAFlags;
                // Preserve the original edge AA flags for the exterior tile edges.
                if (tileR.fLeft <= srcRect.fLeft && (origAAFlags & SkCanvas::kLeft_QuadAAFlag)) {
                    aaFlags |= SkCanvas::kLeft_QuadAAFlag;
                }
                if (tileR.fRight >= srcRect.fRight && (origAAFlags & SkCanvas::kRight_QuadAAFlag)) {
                    aaFlags |= SkCanvas::kRight_QuadAAFlag;
                }
                if (tileR.fTop <= srcRect.fTop && (origAAFlags & SkCanvas::kTop_QuadAAFlag)) {
                    aaFlags |= SkCanvas::kTop_QuadAAFlag;
                }
                if (tileR.fBottom >= srcRect.fBottom &&
                    (origAAFlags & SkCanvas::kBottom_QuadAAFlag)) {
                    aaFlags |= SkCanvas::kBottom_QuadAAFlag;
                }

                // Offset the source rect to make it "local" to our tmp bitmap
                tileR.offset(-offset.fX, -offset.fY);

                imgSet.push_back(SkCanvas::ImageSetEntry(std::move(image),
                                                         tileR,
                                                         rectToDraw,
                                                         /* matrixIndex= */ -1,
                                                         /* alpha= */ 1.0f,
                                                         aaFlags,
                                                         /* hasClip= */ false));

#if GR_TEST_UTILS
                (void)gNumTilesDrawn.fetch_add(+1, std::memory_order_relaxed);
#endif
            }
        }
    }

    device->drawEdgeAAImageSet(imgSet.data(),
                               imgSet.size(),
                               /* dstClips= */ nullptr,
                               /* preViewMatrices= */ nullptr,
                               sampling,
                               paint,
                               constraint);
}

size_t get_cache_size(SkBaseDevice* device) {
    if (auto dContext = GrAsDirectContext(device->recordingContext())) {
        // NOTE: if the context is not a direct context, it doesn't have access to the resource
        // cache, and theoretically, the resource cache's limits could be being changed on
        // another thread, so even having access to just the limit wouldn't be a reliable
        // test during recording here.
        return dContext->getResourceCacheLimit();
    }

    return 0;
}

} // anonymous namespace

namespace skgpu {

bool TiledTextureUtils::DrawImageRect_Ganesh(SkCanvas*,
                                             skgpu::ganesh::Device* device,
                                             const SkImage* image,
                                             const SkRect& srcRect,
                                             const SkRect& dstRect,
                                             SkCanvas::QuadAAFlags aaFlags,
                                             const SkSamplingOptions& origSampling,
                                             const SkPaint& paint,
                                             SkCanvas::SrcRectConstraint constraint) {
    if (!image->isTextureBacked()) {
        SkRect src;
        SkRect dst;
        SkMatrix srcToDst;
        ImageDrawMode mode = OptimizeSampleArea(SkISize::Make(image->width(), image->height()),
                                                srcRect, dstRect, /* dstClip= */ nullptr,
                                                &src, &dst, &srcToDst);
        if (mode == ImageDrawMode::kSkip) {
            return true;
        }

        SkASSERT(mode != ImageDrawMode::kDecal); // only happens if there is a 'dstClip'

        if (src.contains(image->bounds())) {
            constraint = SkCanvas::kFast_SrcRectConstraint;
        }

        const SkMatrix& localToDevice = device->localToDevice();

        SkSamplingOptions sampling = origSampling;
        if (sampling.mipmap != SkMipmapMode::kNone && CanDisableMipmap(localToDevice, srcToDst)) {
            sampling = SkSamplingOptions(sampling.filter);
        }
        const GrClip* clip = device->clip();
        SkIRect clipRect = clip ? clip->getConservativeBounds() : device->bounds();

        int tileFilterPad;
        if (sampling.useCubic) {
            tileFilterPad = kBicubicFilterTexelPad;
        } else if (sampling.filter == SkFilterMode::kLinear || sampling.isAniso()) {
            // Aniso will fallback to linear filtering in the tiling case.
            tileFilterPad = 1;
        } else {
            tileFilterPad = 0;
        }

        GrRecordingContext* rContext = device->recordingContext();
        int maxTileSize = rContext->maxTextureSize() - 2*tileFilterPad;
#if GR_TEST_UTILS
        if (gOverrideMaxTextureSize) {
            maxTileSize = gOverrideMaxTextureSize - 2 * tileFilterPad;
        }
#endif
        size_t cacheSize = get_cache_size(device);

        int tileSize;
        SkIRect clippedSubset;
        if (ShouldTileImage(clipRect,
                            image->dimensions(),
                            localToDevice,
                            srcToDst,
                            &src,
                            maxTileSize,
                            cacheSize,
                            &tileSize,
                            &clippedSubset)) {
            // Extract pixels on the CPU, since we have to split into separate textures before
            // sending to the GPU if tiling.
            if (SkBitmap bm; as_IB(image)->getROPixels(nullptr, &bm)) {
                draw_tiled_bitmap_ganesh(device,
                                         bm,
                                         tileSize,
                                         srcToDst,
                                         src,
                                         clippedSubset,
                                         paint,
                                         aaFlags,
                                         constraint,
                                         sampling);
                return true;
            }
        }
    }

    return false;
}

} // namespace skgpu
