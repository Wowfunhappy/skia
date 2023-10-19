/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkTypes.h"
#if defined(SK_BUILD_FOR_MAC) || defined(SK_BUILD_FOR_IOS)

#ifdef SK_BUILD_FOR_MAC
#import <ApplicationServices/ApplicationServices.h>
#endif

#ifdef SK_BUILD_FOR_IOS
#include <CoreText/CoreText.h>
#include <CoreText/CTFontManager.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#endif

#include "include/core/SkData.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkStream.h"
#include "include/core/SkString.h"
#include "include/core/SkTypeface.h"
#include "include/ports/SkFontMgr_mac_ct.h"
#include "include/private/base/SkFixed.h"
#include "include/private/base/SkOnce.h"
#include "include/private/base/SkTPin.h"
#include "include/private/base/SkTemplates.h"
#include "include/private/base/SkTo.h"
#include "src/base/SkUTF.h"
#include "src/core/SkFontDescriptor.h"
#include "src/ports/SkTypeface_mac_ct.h"
#include "src/utils/mac/SkCTFont.h"

#include <string.h>
#include <memory>

#include <sys/utsname.h>
// See Source/WebKit/chromium/base/mac/mac_util.mm DarwinMajorVersionInternal for original source.
static int readVersion() {
    struct utsname info;
    if (uname(&info) != 0) {
        SkDebugf("uname failed\n");
        return 0;
    }
    if (strcmp(info.sysname, "Darwin") != 0) {
        SkDebugf("unexpected uname sysname %s\n", info.sysname);
        return 0;
    }
    char* dot = strchr(info.release, '.');
    if (!dot) {
        SkDebugf("expected dot in uname release %s\n", info.release);
        return 0;
    }
    int version = atoi(info.release);
    if (version == 0) {
        SkDebugf("could not parse uname release %s\n", info.release);
    }
    return version;
}
static int darwinVersion() {
    static int darwin_version = readVersion();
    return darwin_version;
}
static bool isLion() {
    return darwinVersion() == 11;
}
static bool isMountainLion() {
    return darwinVersion() == 12;
}
static bool isMavericks() {
    return darwinVersion() == 13;
}

using namespace skia_private;

#if (defined(SK_BUILD_FOR_IOS) && defined(__IPHONE_14_0) &&  \
      __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_14_0) ||  \
    (defined(SK_BUILD_FOR_MAC) && defined(__MAC_11_0) &&     \
      __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_11_0)

static uint32_t SkGetCoreTextVersion() {
    // If compiling for iOS 14.0+ or macOS 11.0+, the CoreText version number
    // must be derived from the OS version number.
    static const uint32_t kCoreTextVersionNEWER = 0x000D0000;
    return kCoreTextVersionNEWER;
}

#else

static uint32_t SkGetCoreTextVersion() {
    // Check for CoreText availability before calling CTGetCoreTextVersion().
    static const bool kCoreTextIsAvailable = (&CTGetCoreTextVersion != nullptr);
    if (kCoreTextIsAvailable) {
        return CTGetCoreTextVersion();
    }

    // Default to a value that's smaller than any known CoreText version.
    static const uint32_t kCoreTextVersionUNKNOWN = 0;
    return kCoreTextVersionUNKNOWN;
}

#endif

static SkUniqueCFRef<CFStringRef> make_CFString(const char s[]) {
    return SkUniqueCFRef<CFStringRef>(CFStringCreateWithCString(nullptr, s, kCFStringEncodingUTF8));
}

/** Creates a typeface from a descriptor, searching the cache. */
static sk_sp<SkTypeface> create_from_desc(CTFontDescriptorRef desc) {
    SkUniqueCFRef<CTFontRef> ctFont(CTFontCreateWithFontDescriptor(desc, 0, nullptr));
    if (!ctFont) {
        return nullptr;
    }

    return SkTypeface_Mac::Make(std::move(ctFont), OpszVariation(), nullptr);
}

static SkUniqueCFRef<CTFontDescriptorRef> create_descriptor(const char familyName[],
                                                            const SkFontStyle& style) {
    SkUniqueCFRef<CFMutableDictionaryRef> cfAttributes(
            CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                      &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks));

    SkUniqueCFRef<CFMutableDictionaryRef> cfTraits(
            CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                      &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks));

    if (!cfAttributes || !cfTraits) {
        return nullptr;
    }

    // TODO(crbug.com/1018581) Some CoreText versions have errant behavior when
    // certain traits set.  Temporary workaround to omit specifying trait for those
    // versions.
    // Long term solution will involve serializing typefaces instead of relying upon
    // this to match between processes.
    //
    // Compare CoreText.h in an up to date SDK for where these values come from.
    static const uint32_t kSkiaLocalCTVersionNumber10_14 = 0x000B0000;
    static const uint32_t kSkiaLocalCTVersionNumber10_15 = 0x000C0000;

    // CTFontTraits (symbolic)
    // macOS 14 and iOS 12 seem to behave badly when kCTFontSymbolicTrait is set.
    // macOS 15 yields LastResort font instead of a good default font when
    // kCTFontSymbolicTrait is set.
    if (SkGetCoreTextVersion() < kSkiaLocalCTVersionNumber10_14) {
        CTFontSymbolicTraits ctFontTraits = 0;
        if (style.weight() >= SkFontStyle::kBold_Weight) {
            ctFontTraits |= kCTFontBoldTrait;
        }
        if (style.slant() != SkFontStyle::kUpright_Slant) {
            ctFontTraits |= kCTFontItalicTrait;
        }
        SkUniqueCFRef<CFNumberRef> cfFontTraits(
                CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &ctFontTraits));
        if (cfFontTraits) {
            CFDictionaryAddValue(cfTraits.get(), kCTFontSymbolicTrait, cfFontTraits.get());
        }
    }

    // CTFontTraits (weight)
    CGFloat ctWeight = SkCTFontCTWeightForCSSWeight(style.weight());
    SkUniqueCFRef<CFNumberRef> cfFontWeight(
            CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &ctWeight));
    if (cfFontWeight) {
        CFDictionaryAddValue(cfTraits.get(), kCTFontWeightTrait, cfFontWeight.get());
    }
    // CTFontTraits (width)
    CGFloat ctWidth = SkCTFontCTWidthForCSSWidth(style.width());
    SkUniqueCFRef<CFNumberRef> cfFontWidth(
            CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &ctWidth));
    if (cfFontWidth) {
        CFDictionaryAddValue(cfTraits.get(), kCTFontWidthTrait, cfFontWidth.get());
    }
    // CTFontTraits (slant)
    // macOS 15 behaves badly when kCTFontSlantTrait is set.
    if (SkGetCoreTextVersion() != kSkiaLocalCTVersionNumber10_15) {
        CGFloat ctSlant = style.slant() == SkFontStyle::kUpright_Slant ? 0 : 1;
        SkUniqueCFRef<CFNumberRef> cfFontSlant(
                CFNumberCreate(kCFAllocatorDefault, kCFNumberCGFloatType, &ctSlant));
        if (cfFontSlant) {
            CFDictionaryAddValue(cfTraits.get(), kCTFontSlantTrait, cfFontSlant.get());
        }
    }
    // CTFontTraits
    CFDictionaryAddValue(cfAttributes.get(), kCTFontTraitsAttribute, cfTraits.get());

    // CTFontFamilyName
    if (familyName) {
        SkUniqueCFRef<CFStringRef> cfFontName = make_CFString(familyName);
        if (cfFontName) {
            CFDictionaryAddValue(cfAttributes.get(), kCTFontFamilyNameAttribute, cfFontName.get());
        }
    }

    return SkUniqueCFRef<CTFontDescriptorRef>(
            CTFontDescriptorCreateWithAttributes(cfAttributes.get()));
}

// Same as the above function except style is included so we can
// compare whether the created font conforms to the style. If not, we need
// to recreate the font with symbolic traits. This is needed due to MacOS 10.11
// font creation problem https://bugs.chromium.org/p/skia/issues/detail?id=8447.
static sk_sp<SkTypeface> create_from_desc_and_style(CTFontDescriptorRef desc,
                                                    const SkFontStyle& style) {
    SkUniqueCFRef<CTFontRef> ctFont(CTFontCreateWithFontDescriptor(desc, 0, nullptr));
    if (!ctFont) {
        return nullptr;
    }

    const CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(ctFont.get());
    CTFontSymbolicTraits expected_traits = traits;
    if (style.slant() != SkFontStyle::kUpright_Slant) {
        expected_traits |= kCTFontItalicTrait;
    }
    if (style.weight() >= SkFontStyle::kBold_Weight) {
        expected_traits |= kCTFontBoldTrait;
    }

    if (expected_traits != traits) {
        SkUniqueCFRef<CTFontRef> ctNewFont(CTFontCreateCopyWithSymbolicTraits(
                    ctFont.get(), 0, nullptr, expected_traits, expected_traits));
        if (ctNewFont) {
            ctFont = std::move(ctNewFont);
        }
    }

    return SkTypeface_Mac::Make(std::move(ctFont), OpszVariation(), nullptr);
}

/** Creates a typeface from a name, searching the cache. */
static sk_sp<SkTypeface> create_from_name(const char familyName[], const SkFontStyle& style) {
    SkUniqueCFRef<CTFontDescriptorRef> desc = create_descriptor(familyName, style);
    if (!desc) {
        return nullptr;
    }
    return create_from_desc_and_style(desc.get(), style);
}

static const char* map_css_names(const char* name) {
    static const struct {
        const char* fFrom;  // name the caller specified
        const char* fTo;    // "canonical" name we map to
    } gPairs[] = {
        { "sans-serif", "Helvetica" },
        { "serif",      "Times"     },
        { "monospace",  "Courier"   }
    };

    for (size_t i = 0; i < std::size(gPairs); i++) {
        if (strcmp(name, gPairs[i].fFrom) == 0) {
            return gPairs[i].fTo;
        }
    }
    return name;    // no change
}

namespace {

static sk_sp<SkData> skdata_from_skstreamasset(std::unique_ptr<SkStreamAsset> stream) {
    size_t size = stream->getLength();
    if (const void* base = stream->getMemoryBase()) {
        return SkData::MakeWithProc(base, size,
                                    [](const void*, void* ctx) -> void {
                                        delete (SkStreamAsset*)ctx;
                                    }, stream.release());
    }
    return SkData::MakeFromStream(stream.get(), size);
}

static SkUniqueCFRef<CFDataRef> cfdata_from_skdata(sk_sp<SkData> data) {
    void const * const addr = data->data();
    size_t const size = data->size();

    CFAllocatorContext ctx = {
        0, // CFIndex version
        data.release(), // void* info
        nullptr, // const void *(*retain)(const void *info);
        nullptr, // void (*release)(const void *info);
        nullptr, // CFStringRef (*copyDescription)(const void *info);
        nullptr, // void * (*allocate)(CFIndex size, CFOptionFlags hint, void *info);
        nullptr, // void*(*reallocate)(void* ptr,CFIndex newsize,CFOptionFlags hint,void* info);
        [](void*,void* info) -> void { // void (*deallocate)(void *ptr, void *info);
            SkASSERT(info);
            ((SkData*)info)->unref();
        },
        nullptr, // CFIndex (*preferredSize)(CFIndex size, CFOptionFlags hint, void *info);
    };
    SkUniqueCFRef<CFAllocatorRef> alloc(CFAllocatorCreate(kCFAllocatorDefault, &ctx));
    return SkUniqueCFRef<CFDataRef>(CFDataCreateWithBytesNoCopy(
            kCFAllocatorDefault, (const UInt8 *)addr, size, alloc.get()));
}

static SkUniqueCFRef<CTFontRef> ctfont_from_skdata(sk_sp<SkData> data, int ttcIndex) {
    // TODO: Use CTFontManagerCreateFontDescriptorsFromData when available.
    if (ttcIndex != 0) {
        return nullptr;
    }

    SkUniqueCFRef<CFDataRef> cfData(cfdata_from_skdata(std::move(data)));

    SkUniqueCFRef<CTFontDescriptorRef> desc(
            CTFontManagerCreateFontDescriptorFromData(cfData.get()));
    if (!desc) {
        return nullptr;
    }
    return SkUniqueCFRef<CTFontRef>(CTFontCreateWithFontDescriptor(desc.get(), 0, nullptr));
}

static bool find_desc_str(CTFontDescriptorRef desc, CFStringRef name, SkString* value) {
    SkUniqueCFRef<CFStringRef> ref((CFStringRef)CTFontDescriptorCopyAttribute(desc, name));
    if (!ref) {
        return false;
    }
    SkStringFromCFString(ref.get(), value);
    return true;
}

static inline int sqr(int value) {
    SkASSERT(SkAbs32(value) < 0x7FFF);  // check for overflow
    return value * value;
}

// We normalize each axis (weight, width, italic) to be base-900
static int compute_metric(const SkFontStyle& a, const SkFontStyle& b) {
    return sqr(a.weight() - b.weight()) +
           sqr((a.width() - b.width()) * 100) +
           sqr((a.slant() != b.slant()) * 900);
}

static SkUniqueCFRef<CFSetRef> name_required() {
    CFStringRef set_values[] = {kCTFontFamilyNameAttribute};
    return SkUniqueCFRef<CFSetRef>(CFSetCreate(kCFAllocatorDefault,
        reinterpret_cast<const void**>(set_values), std::size(set_values),
        &kCFTypeSetCallBacks));
}

class SkFontStyleSet_Mac : public SkFontStyleSet {
public:
    SkFontStyleSet_Mac(CTFontDescriptorRef desc)
        : fArray(CTFontDescriptorCreateMatchingFontDescriptors(desc, name_required().get()))
        , fCount(0)
    {
        if (!fArray) {
            fArray.reset(CFArrayCreate(nullptr, nullptr, 0, nullptr));
        }
        fCount = SkToInt(CFArrayGetCount(fArray.get()));
    }

    int count() override {
        return fCount;
    }

    void getStyle(int index, SkFontStyle* style, SkString* name) override {
        SkASSERT((unsigned)index < (unsigned)fCount);
        CTFontDescriptorRef desc = (CTFontDescriptorRef)CFArrayGetValueAtIndex(fArray.get(), index);
        if (style) {
            *style = SkCTFontDescriptorGetSkFontStyle(desc, false);
        }
        if (name) {
            if (!find_desc_str(desc, kCTFontStyleNameAttribute, name)) {
                name->reset();
            }
        }
    }

    sk_sp<SkTypeface> createTypeface(int index) override {
        SkASSERT((unsigned)index < (unsigned)CFArrayGetCount(fArray.get()));
        CTFontDescriptorRef desc = (CTFontDescriptorRef)CFArrayGetValueAtIndex(fArray.get(), index);

        return create_from_desc(desc);
    }

    sk_sp<SkTypeface> matchStyle(const SkFontStyle& pattern) override {
        if (0 == fCount) {
            return nullptr;
        }
        return create_from_desc(findMatchingDesc(pattern));
    }

private:
    SkUniqueCFRef<CFArrayRef> fArray;
    int fCount;

    CTFontDescriptorRef findMatchingDesc(const SkFontStyle& pattern) const {
        int bestMetric = SK_MaxS32;
        CTFontDescriptorRef bestDesc = nullptr;

        for (int i = 0; i < fCount; ++i) {
            CTFontDescriptorRef desc = (CTFontDescriptorRef)CFArrayGetValueAtIndex(fArray.get(), i);
            int metric = compute_metric(pattern, SkCTFontDescriptorGetSkFontStyle(desc, false));
            if (0 == metric) {
                return desc;
            }
            if (metric < bestMetric) {
                bestMetric = metric;
                bestDesc = desc;
            }
        }
        SkASSERT(bestDesc);
        return bestDesc;
    }
};

SkUniqueCFRef<CFArrayRef> SkCopyAvailableFontFamilyNames(CTFontCollectionRef collection) {
    // Create a CFArray of all available font descriptors.
    SkUniqueCFRef<CFArrayRef> descriptors(
        CTFontCollectionCreateMatchingFontDescriptors(collection));

    // Copy the font family names of the font descriptors into a CFSet.
    auto addDescriptorFamilyNameToSet = [](const void* value, void* context) -> void {
        CTFontDescriptorRef descriptor = static_cast<CTFontDescriptorRef>(value);
        CFMutableSetRef familyNameSet = static_cast<CFMutableSetRef>(context);
        SkUniqueCFRef<CFTypeRef> familyName(
            CTFontDescriptorCopyAttribute(descriptor, kCTFontFamilyNameAttribute));
        if (familyName) {
            CFSetAddValue(familyNameSet, familyName.get());
        }
    };
    SkUniqueCFRef<CFMutableSetRef> familyNameSet(
        CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks));
    CFArrayApplyFunction(descriptors.get(), CFRangeMake(0, CFArrayGetCount(descriptors.get())),
                         addDescriptorFamilyNameToSet, familyNameSet.get());

    // Get the set of family names into an array; this does not retain.
    CFIndex count = CFSetGetCount(familyNameSet.get());
    std::unique_ptr<const void*[]> familyNames(new const void*[count]);
    CFSetGetValues(familyNameSet.get(), familyNames.get());

    // Sort the array of family names (to match CTFontManagerCopyAvailableFontFamilyNames).
    std::sort(familyNames.get(), familyNames.get() + count, [](const void* a, const void* b){
        return CFStringCompare((CFStringRef)a, (CFStringRef)b, 0) == kCFCompareLessThan;
    });

    // Copy family names into a CFArray; this does retain.
    return SkUniqueCFRef<CFArrayRef>(
        CFArrayCreate(kCFAllocatorDefault, familyNames.get(), count, &kCFTypeArrayCallBacks));
}

/** Use CTFontManagerCopyAvailableFontFamilyNames if available, simulate if not. */
SkUniqueCFRef<CFArrayRef> SkCTFontManagerCopyAvailableFontFamilyNames() {
#ifdef SK_BUILD_FOR_IOS
    using CTFontManagerCopyAvailableFontFamilyNamesProc = CFArrayRef (*)(void);
    CTFontManagerCopyAvailableFontFamilyNamesProc ctFontManagerCopyAvailableFontFamilyNames;
    *(void**)(&ctFontManagerCopyAvailableFontFamilyNames) =
        dlsym(RTLD_DEFAULT, "CTFontManagerCopyAvailableFontFamilyNames");
    if (ctFontManagerCopyAvailableFontFamilyNames) {
        return SkUniqueCFRef<CFArrayRef>(ctFontManagerCopyAvailableFontFamilyNames());
    }
    SkUniqueCFRef<CTFontCollectionRef> collection(
        CTFontCollectionCreateFromAvailableFonts(nullptr));
    return SkUniqueCFRef<CFArrayRef>(SkCopyAvailableFontFamilyNames(collection.get()));
#else
    return SkUniqueCFRef<CFArrayRef>(CTFontManagerCopyAvailableFontFamilyNames());
#endif
}

} // namespace

static void unref_proc(void* info, const void* addr, size_t size) {
    SkASSERT(info);
    ((SkRefCnt*)info)->unref();
}
static void delete_stream_proc(void* info, const void* addr, size_t size) {
    SkASSERT(info);
    SkStream* stream = (SkStream*)info;
    SkASSERT(stream->getMemoryBase() == addr);
    SkASSERT(stream->getLength() == size);
    delete stream;
}
// These are used by CGDataProviderSequentialCallbacks
static size_t get_bytes_proc(void* info, void* buffer, size_t bytes) {
    SkASSERT(info);
    return ((SkStream*)info)->read(buffer, bytes);
}
static off_t skip_forward_proc(void* info, off_t bytes) {
    return ((SkStream*)info)->skip((size_t) bytes);
}
static void rewind_proc(void* info) {
    SkASSERT(info);
    ((SkStream*)info)->rewind();
}
// Used when info is an SkStream.
static void release_info_proc(void* info) {
    SkASSERT(info);
    delete (SkStream*)info;
}
CGDataProviderRef SkCreateDataProviderFromStream(std::unique_ptr<SkStreamRewindable> stream) {
    // TODO: Replace with SkStream::getData() when that is added. Then we only
    // have one version of CGDataProviderCreateWithData (i.e. same release proc)
    const void* addr = stream->getMemoryBase();
    if (addr) {
        // special-case when the stream is just a block of ram
        size_t size = stream->getLength();
        return CGDataProviderCreateWithData(stream.release(), addr, size, delete_stream_proc);
    }
    CGDataProviderSequentialCallbacks rec;
    sk_bzero(&rec, sizeof(rec));
    rec.version = 0;
    rec.getBytes = get_bytes_proc;
    rec.skipForward = skip_forward_proc;
    rec.rewind = rewind_proc;
    rec.releaseInfo = release_info_proc;
    return CGDataProviderCreateSequential(stream.release(), &rec);
}
static bool find_dict_CGFloat(CFDictionaryRef dict, CFStringRef name, CGFloat* value) {
    CFNumberRef num;
    return CFDictionaryGetValueIfPresent(dict, name, (const void**)&num)
        && CFNumberIsFloatType(num)
        && CFNumberGetValue(num, kCFNumberCGFloatType, value);
}
template <typename S, typename D, typename C> struct LinearInterpolater {
    struct Mapping {
        S src_val;
        D dst_val;
    };
    constexpr LinearInterpolater(Mapping const mapping[], int mappingCount)
        : fMapping(mapping), fMappingCount(mappingCount) {}

    static D map(S value, S src_min, S src_max, D dst_min, D dst_max) {
        SkASSERT(src_min < src_max);
        SkASSERT(dst_min <= dst_max);
        return C()(dst_min + (((value - src_min) * (dst_max - dst_min)) / (src_max - src_min)));
    }

    D map(S val) const {
        // -Inf to [0]
        if (val < fMapping[0].src_val) {
            return fMapping[0].dst_val;
        }

        // Linear from [i] to [i+1]
        for (int i = 0; i < fMappingCount - 1; ++i) {
            if (val < fMapping[i+1].src_val) {
                return map(val, fMapping[i].src_val, fMapping[i+1].src_val,
                                fMapping[i].dst_val, fMapping[i+1].dst_val);
            }
        }

        // From [n] to +Inf
        // if (fcweight < Inf)
        return fMapping[fMappingCount - 1].dst_val;
    }

    Mapping const * fMapping;
    int fMappingCount;
};
struct RoundCGFloatToInt {
    int operator()(CGFloat s) { return s + 0.5; }
};
/** Convert the [-1, 1] CTFontDescriptor width to [0, 10] CSS weight. */
static int ct_width_to_fontstyle(CGFloat cgWidth) {
    using Interpolator = LinearInterpolater<CGFloat, int, RoundCGFloatToInt>;

    // Values determined by creating font data with every width, creating a CTFont,
    // and asking the CTFont for its width. See TypefaceStyle test for basics.
    static constexpr Interpolator::Mapping widthMappings[] = {
        { -0.5,  0 },
        {  0.5, 10 },
    };
    static constexpr Interpolator interpolator(widthMappings, std::size(widthMappings));
    return interpolator.map(cgWidth);
}
/** Convert the [-1, 1] CTFontDescriptor weight to [0, 1000] CSS weight.
 *
 *  The -1 to 1 weights reported by CTFontDescriptors have different mappings depending on if the
 *  CTFont is native or created from a CGDataProvider.
 */
static int ct_weight_to_fontstyle(CGFloat cgWeight, bool fromDataProvider) {
    using Interpolator = LinearInterpolater<CGFloat, int, RoundCGFloatToInt>;

    // Note that Mac supports the old OS2 version A so 0 through 10 are as if multiplied by 100.
    // However, on this end we can't tell, so this is ignored.

    static Interpolator::Mapping nativeWeightMappings[11];
    static Interpolator::Mapping dataProviderWeightMappings[11];
    static SkOnce once;
    once([&] {
        const CGFloat(&nsFontWeights)[11] = SkCTFontGetNSFontWeightMapping();
        const CGFloat(&userFontWeights)[11] = SkCTFontGetDataFontWeightMapping();
        for (int i = 0; i < 11; ++i) {
            nativeWeightMappings[i].src_val = nsFontWeights[i];
            nativeWeightMappings[i].dst_val = i * 100;

            dataProviderWeightMappings[i].src_val = userFontWeights[i];
            dataProviderWeightMappings[i].dst_val = i * 100;
        }
    });
    static constexpr Interpolator nativeInterpolator(
            nativeWeightMappings, std::size(nativeWeightMappings));
    static constexpr Interpolator dataProviderInterpolator(
            dataProviderWeightMappings, std::size(dataProviderWeightMappings));

    return fromDataProvider ? dataProviderInterpolator.map(cgWeight)
                            : nativeInterpolator.map(cgWeight);
}
static SkFontStyle fontstyle_from_descriptor(CTFontDescriptorRef desc, bool fromDataProvider) {
    SkUniqueCFRef<CFTypeRef> traits(CTFontDescriptorCopyAttribute(desc, kCTFontTraitsAttribute));
    if (!traits || CFDictionaryGetTypeID() != CFGetTypeID(traits.get())) {
        return SkFontStyle();
    }
    SkUniqueCFRef<CFDictionaryRef> fontTraitsDict(static_cast<CFDictionaryRef>(traits.release()));
    CGFloat weight, width, slant;
    if (!find_dict_CGFloat(fontTraitsDict.get(), kCTFontWeightTrait, &weight)) {
        weight = 0;
    }
    if (!find_dict_CGFloat(fontTraitsDict.get(), kCTFontWidthTrait, &width)) {
        width = 0;
    }
    if (!find_dict_CGFloat(fontTraitsDict.get(), kCTFontSlantTrait, &slant)) {
        slant = 0;
    }
    return SkFontStyle(ct_weight_to_fontstyle(weight, fromDataProvider),
                       ct_width_to_fontstyle(width),
                       slant ? SkFontStyle::kItalic_Slant
                             : SkFontStyle::kUpright_Slant);
}
/** Creates a typeface, searching the cache if isLocalStream is false. */
static sk_sp<SkTypeface> create_from_CTFontRef(SkUniqueCFRef<CTFontRef> font,
                                               SkUniqueCFRef<CFTypeRef> resource,
                                               OpszVariation opszVariation,
                                               std::unique_ptr<SkStreamAsset> providedData) {
    SkASSERT(font);
    const bool isFromStream(providedData);
    SkUniqueCFRef<CTFontDescriptorRef> desc(CTFontCopyFontDescriptor(font.get()));
    SkFontStyle style = fontstyle_from_descriptor(desc.get(), isFromStream);
    CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(font.get());
    bool isFixedPitch = SkToBool(traits & kCTFontMonoSpaceTrait);
    sk_sp<SkTypeface> face(SkTypeface_Mac::Make(std::move(font),
                                              opszVariation,
                                              std::move(providedData)));

    return face;
}
///////////////////////////////////////////////////////////////////////////////
// Returns nullptr on failure
static sk_sp<SkTypeface> create_from_dataProvider(SkUniqueCFRef<CGDataProviderRef> provider,
                                                  std::unique_ptr<SkStreamAsset> providedData,
                                                  int ttcIndex) {
    if (ttcIndex != 0) {
        return nullptr;
    }
    SkUniqueCFRef<CGFontRef> cg(CGFontCreateWithDataProvider(provider.get()));
    if (!cg) {
        return nullptr;
    }
    SkUniqueCFRef<CTFontRef> ct(CTFontCreateWithGraphicsFont(cg.get(), 0, nullptr, nullptr));
    if (!ct) {
        return nullptr;
    }
    return create_from_CTFontRef(std::move(ct), nullptr, OpszVariation(), std::move(providedData));
}


class SkFontMgr_Mac : public SkFontMgr {
    SkUniqueCFRef<CFArrayRef> fNames;
    int fCount;

    CFStringRef getFamilyNameAt(int index) const {
        SkASSERT((unsigned)index < (unsigned)fCount);
        return (CFStringRef)CFArrayGetValueAtIndex(fNames.get(), index);
    }

    static sk_sp<SkFontStyleSet> CreateSet(CFStringRef cfFamilyName) {
        SkUniqueCFRef<CFMutableDictionaryRef> cfAttr(
                 CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                           &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks));

        CFDictionaryAddValue(cfAttr.get(), kCTFontFamilyNameAttribute, cfFamilyName);

        SkUniqueCFRef<CTFontDescriptorRef> desc(
                CTFontDescriptorCreateWithAttributes(cfAttr.get()));
        return sk_sp<SkFontStyleSet>(new SkFontStyleSet_Mac(desc.get()));
    }

public:
    SkUniqueCFRef<CTFontCollectionRef> fFontCollection;
    SkFontMgr_Mac(CTFontCollectionRef fontCollection)
        : fNames(fontCollection ? SkCopyAvailableFontFamilyNames(fontCollection)
                                : SkCTFontManagerCopyAvailableFontFamilyNames())
        , fCount(fNames ? SkToInt(CFArrayGetCount(fNames.get())) : 0)
        , fFontCollection(fontCollection ? (CTFontCollectionRef)CFRetain(fontCollection)
                                         : CTFontCollectionCreateFromAvailableFonts(nullptr))
    {}

protected:
    int onCountFamilies() const override {
        return fCount;
    }

    void onGetFamilyName(int index, SkString* familyName) const override {
        if ((unsigned)index < (unsigned)fCount) {
            SkStringFromCFString(this->getFamilyNameAt(index), familyName);
        } else {
            familyName->reset();
        }
    }

    sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override {
        if ((unsigned)index >= (unsigned)fCount) {
            return nullptr;
        }
        return CreateSet(this->getFamilyNameAt(index));
    }

    sk_sp<SkFontStyleSet> onMatchFamily(const char familyName[]) const override {
        if (!familyName) {
            return nullptr;
        }
        SkUniqueCFRef<CFStringRef> cfName = make_CFString(familyName);
        return CreateSet(cfName.get());
    }

    sk_sp<SkTypeface> onMatchFamilyStyle(const char familyName[],
                                   const SkFontStyle& style) const override {
        SkUniqueCFRef<CTFontDescriptorRef> reqDesc = create_descriptor(familyName, style);
        if (!familyName) {
            return create_from_desc(reqDesc.get());
        }
        SkUniqueCFRef<CTFontDescriptorRef> resolvedDesc(
            CTFontDescriptorCreateMatchingFontDescriptor(reqDesc.get(), name_required().get()));
        if (!resolvedDesc) {
            return nullptr;
        }
        return create_from_desc(resolvedDesc.get());
    }

    sk_sp<SkTypeface> onMatchFamilyStyleCharacter(const char familyName[],
                                                  const SkFontStyle& style,
                                                  const char* bcp47[], int bcp47Count,
                                                  SkUnichar character) const override {
        SkUniqueCFRef<CTFontDescriptorRef> desc = create_descriptor(familyName, style);
        SkUniqueCFRef<CTFontRef> familyFont(CTFontCreateWithFontDescriptor(desc.get(), 0, nullptr));

        // kCFStringEncodingUTF32 is BE unless there is a BOM.
        // Since there is no machine endian option, explicitly state machine endian.
#ifdef SK_CPU_LENDIAN
        constexpr CFStringEncoding encoding = kCFStringEncodingUTF32LE;
#else
        constexpr CFStringEncoding encoding = kCFStringEncodingUTF32BE;
#endif
        SkUniqueCFRef<CFStringRef> string(CFStringCreateWithBytes(
                kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(&character), sizeof(character),
                encoding, false));
        // If 0xD800 <= codepoint <= 0xDFFF || 0x10FFFF < codepoint 'string' may be nullptr.
        // No font should be covering such codepoints (even the magic fallback font).
        if (!string) {
            return nullptr;
        }
        CFRange range = CFRangeMake(0, CFStringGetLength(string.get()));  // in UniChar units.
        SkUniqueCFRef<CTFontRef> fallbackFont(
                CTFontCreateForString(familyFont.get(), string.get(), range));
        return SkTypeface_Mac::Make(std::move(fallbackFont), OpszVariation(), nullptr);
    }

    sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData> data, int ttcIndex) const override {
        if (ttcIndex != 0) {
            return nullptr;
        }

        SkUniqueCFRef<CTFontRef> ct;
        if(isMavericks()) {
          SkUniqueCFRef<CFDataRef> cfData(cfdata_from_skdata(std::move(data)));
          SkUniqueCFRef<CGDataProviderRef> pr(CGDataProviderCreateWithCFData(cfData.get()));
          SkUniqueCFRef<CGFontRef> cg(CGFontCreateWithDataProvider(pr.get()));
          if (nullptr == cg) {
              return nullptr;
          }
          SkUniqueCFRef<CTFontRef> ctbuf(CTFontCreateWithGraphicsFont(cg.get(), 0, nullptr, nullptr));
          ct = std::move(ctbuf);
        } else {
          SkUniqueCFRef<CTFontRef> ctbuf(ctfont_from_skdata(data, ttcIndex));
          ct = std::move(ctbuf);
        }

        if (!ct) {
          return nullptr;
        }
        return SkTypeface_Mac::Make(std::move(ct), OpszVariation(),
                                    SkMemoryStream::Make(std::move(data)));
    }

    sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream,
                                            int ttcIndex) const override {
        if (ttcIndex != 0) {
            return nullptr;
        }
        
        if(isMavericks()) {
          SkUniqueCFRef<CGDataProviderRef> pr(SkCreateDataProviderFromStream(stream->duplicate()));
          if (!pr) {
              return nullptr;
          }
          return create_from_dataProvider(std::move(pr), std::move(stream), ttcIndex);
        } else {
          sk_sp<SkData> data = skdata_from_skstreamasset(stream->duplicate());
          if (!data) {
              return nullptr;
          }
          SkUniqueCFRef<CTFontRef> ct = ctfont_from_skdata(std::move(data), ttcIndex);
          if (!ct) {
              return nullptr;
          }
          return SkTypeface_Mac::Make(std::move(ct), OpszVariation(), std::move(stream));
        }
    }

    sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset> stream,
                                           const SkFontArguments& args) const override
    {
        // TODO: Use CTFontManagerCreateFontDescriptorsFromData when available.
        int ttcIndex = args.getCollectionIndex();
        if (ttcIndex != 0) {
            return nullptr;
        }

        SkUniqueCFRef<CTFontRef> ct;
        if(isMavericks()) {
          SkUniqueCFRef<CGDataProviderRef> pr(SkCreateDataProviderFromStream(stream->duplicate()));
          if (!pr) {
              return nullptr;
          }
          SkUniqueCFRef<CGFontRef> cg(CGFontCreateWithDataProvider(pr.get()));
          if (nullptr == cg) {
              return nullptr;
          }
          SkUniqueCFRef<CTFontRef> ctbuf(CTFontCreateWithGraphicsFont(cg.get(), 0, nullptr, nullptr));
          ct = std::move(ctbuf);
				} else {
          sk_sp<SkData> data = skdata_from_skstreamasset(stream->duplicate());
          if (!data) {
              return nullptr;
          }
          SkUniqueCFRef<CTFontRef> ctbuf(ctfont_from_skdata(std::move(data), ttcIndex));
          ct = std::move(ctbuf);
				}

        if (!ct) {
					return nullptr;
        }

        SkUniqueCFRef<CFArrayRef> axes(CTFontCopyVariationAxes(ct.get()));
        CTFontVariation ctVariation = ctvariation_from_SkFontArguments(ct.get(), axes.get(), args);

        SkUniqueCFRef<CTFontRef> ctVariant;
        if (ctVariation.variation) {
            SkUniqueCFRef<CFMutableDictionaryRef> attributes(
                    CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                              &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks));
            CFDictionaryAddValue(attributes.get(),
                                 kCTFontVariationAttribute, ctVariation.variation.get());
            SkUniqueCFRef<CTFontDescriptorRef> varDesc(
                    CTFontDescriptorCreateWithAttributes(attributes.get()));
            ctVariant.reset(CTFontCreateCopyWithAttributes(ct.get(), 0, nullptr, varDesc.get()));
        } else {
            ctVariant.reset(ct.release());
        }
        if (!ctVariant) {
            return nullptr;
        }

        return SkTypeface_Mac::Make(std::move(ctVariant), ctVariation.opsz, std::move(stream));
    }

    sk_sp<SkTypeface> onMakeFromFile(const char path[], int ttcIndex) const override {
        if (ttcIndex != 0) {
            return nullptr;
        }

        sk_sp<SkData> data = SkData::MakeFromFileName(path);
        if (!data) {
            return nullptr;
        }

        return this->onMakeFromData(std::move(data), ttcIndex);
    }

    sk_sp<SkTypeface> onLegacyMakeTypeface(const char familyName[], SkFontStyle style) const override {
        if (familyName) {
            familyName = map_css_names(familyName);
        }

        sk_sp<SkTypeface> face = create_from_name(familyName, style);
        if (face) {
            return face;
        }

        static SkTypeface* gDefaultFace;
        static SkOnce lookupDefault;
        static const char FONT_DEFAULT_NAME[] = "Lucida Sans";
        lookupDefault([]{
            gDefaultFace = create_from_name(FONT_DEFAULT_NAME, SkFontStyle()).release();
        });
        return sk_ref_sp(gDefaultFace);
    }
};

sk_sp<SkFontMgr> SkFontMgr_New_CoreText(CTFontCollectionRef fontCollection) {
    return sk_make_sp<SkFontMgr_Mac>(fontCollection);
}

#endif//defined(SK_BUILD_FOR_MAC) || defined(SK_BUILD_FOR_IOS)
