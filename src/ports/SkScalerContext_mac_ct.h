/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkScalerContext_mac_ct_DEFINED
#define SkScalerContext_mac_ct_DEFINED

#include "include/core/SkTypes.h"
#if defined(SK_BUILD_FOR_MAC) || defined(SK_BUILD_FOR_IOS)

#include "include/core/SkRefCnt.h"
#include "include/core/SkSize.h"
#include "src/core/SkAutoMalloc.h"
#include "src/core/SkScalerContext.h"
#include "src/utils/mac/SkUniqueCFRef.h"

#ifdef SK_BUILD_FOR_MAC
#import <ApplicationServices/ApplicationServices.h>
#endif

#ifdef SK_BUILD_FOR_IOS
#include <CoreText/CoreText.h>
#include <CoreText/CTFontManager.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <memory>

class SkDescriptor;
class SkGlyph;
class SkPath;
class SkTypeface_Mac;
struct SkFontMetrics;


typedef uint32_t CGRGBPixel;

///////////////////////////////////////////////////////////////////////////////
/** GlyphRect is in FUnits (em space, y up). */
struct GlyphRect {
    int16_t fMinX;
    int16_t fMinY;
    int16_t fMaxX;
    int16_t fMaxY;
};

class SkScalerContext_Mac : public SkScalerContext {
public:
    SkScalerContext_Mac(sk_sp<SkTypeface_Mac>, const SkScalerContextEffects&, const SkDescriptor*);

protected:
    unsigned generateGlyphCount(void) override;
    bool generateAdvance(SkGlyph* glyph) override;
    void generateMetrics(SkGlyph* glyph) override;
    void generateImage(const SkGlyph& glyph) override;
    bool generatePath(SkGlyphID glyph, SkPath* path) override;
    void generateFontMetrics(SkFontMetrics*) override;

private:
    class Offscreen {
    public:
        Offscreen()
            : fRGBSpace(nullptr)
            , fCG(nullptr)
            , fDoAA(false)
            , fDoLCD(false)
        {
            fSize.set(0, 0);
        }

        CGRGBPixel* getCG(const SkScalerContext_Mac& context, const SkGlyph& glyph,
                          CGGlyph glyphID, size_t* rowBytesPtr, bool generateA8FromLCD);

    private:
        enum {
            kSize = 32 * 32 * sizeof(CGRGBPixel)
        };
        SkAutoSMalloc<kSize> fImageStorage;
        SkUniqueCFRef<CGColorSpaceRef> fRGBSpace;

        // cached state
        SkUniqueCFRef<CGContextRef> fCG;
        SkISize fSize;
        bool fDoAA;
        bool fDoLCD;
    };
    /** Initializes and returns the value of fFBoundingBoxesGlyphOffset.
     *
     *  For use with (and must be called before) generateBBoxes.
     */
    uint16_t getFBoundingBoxesGlyphOffset();
    /** Initializes fFBoundingBoxes and returns true on success.
     *
     *  On Lion and Mountain Lion, CTFontGetBoundingRectsForGlyphs has a bug which causes it to
     *  return a bad value in bounds.origin.x for SFNT fonts whose hhea::numberOfHMetrics is
     *  less than its maxp::numGlyphs. When this is the case we try to read the bounds from the
     *  font directly.
     *
     *  This routine initializes fFBoundingBoxes to an array of
     *  fGlyphCount - fFBoundingBoxesGlyphOffset GlyphRects which contain the bounds in FUnits
     *  (em space, y up) of glyphs with ids in the range [fFBoundingBoxesGlyphOffset, fGlyphCount).
     *
     *  Returns true if fFBoundingBoxes is properly initialized. The table can only be properly
     *  initialized for a TrueType font with 'head', 'loca', and 'glyf' tables.
     *
     *  TODO: A future optimization will compute fFBoundingBoxes once per fCTFont.
     */
    bool generateBBoxes();
    /** Converts from FUnits (em space, y up) to SkGlyph units (pixels, y down).
     *
     *  Used on Snow Leopard to correct CTFontGetVerticalTranslationsForGlyphs.
     *  Used on Lion to correct CTFontGetBoundingRectsForGlyphs.
     */
    SkMatrix fFUnitMatrix;
    Offscreen fOffscreen;

    /** Unrotated variant of fCTFont.
     *
     *  In 10.10.1 CTFontGetAdvancesForGlyphs applies the font transform to the width of the
     *  advances, but always sets the height to 0. This font is used to get the advances of the
     *  unrotated glyph, and then the rotation is applied separately.
     *
     *  CT vertical metrics are pre-rotated (in em space, before transform) 90deg clock-wise.
     *  This makes kCTFontOrientationDefault dangerous, because the metrics from
     *  kCTFontOrientationHorizontal are in a different space from kCTFontOrientationVertical.
     *  With kCTFontOrientationVertical the advances must be unrotated.
     *
     *  Sometimes, creating a copy of a CTFont with the same size but different trasform will select
     *  different underlying font data. As a result, avoid ever creating more than one CTFont per
     *  SkScalerContext to ensure that only one CTFont is used.
     *
     *  As a result of the above (and other constraints) this font contains the size, but not the
     *  transform. The transform must always be applied separately.
     */
    SkUniqueCFRef<CTFontRef> fCTFont;

    /** The transform without the font size. */
    CGAffineTransform fTransform;
    CGAffineTransform fInvTransform;

    SkUniqueCFRef<CGFontRef> fCGFont;
	SkAutoTMalloc<GlyphRect> fFBoundingBoxes;
    uint16_t fFBoundingBoxesGlyphOffset;
    uint16_t fGlyphCount;
    bool fGeneratedFBoundingBoxes;
    const bool fDoSubPosition;

    friend class Offscreen;

    using INHERITED = SkScalerContext;
};

#endif
#endif //SkScalerContext_mac_ct_DEFINED
