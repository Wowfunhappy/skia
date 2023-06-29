/*
 * Copyright 2023 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SlotManager_DEFINED
#define SlotManager_DEFINED

#include "include/core/SkColor.h"
#include "include/core/SkString.h"
#include "include/private/base/SkTArray.h"
#include "src/core/SkTHash.h"

namespace skjson {
class ObjectValue;
}

namespace skresources {
class ImageAsset;
}

namespace sksg {
class Node;
}
namespace skottie {

struct TextPropertyValue;

namespace internal {
class AnimationBuilder;
class SceneGraphRevalidator;
class AnimatablePropertyContainer;
class TextAdapter;
} // namespace internal

using namespace skia_private;

class SK_API SlotManager final : public SkRefCnt {

public:
    using SlotID = SkString;

    SlotManager(sk_sp<skottie::internal::SceneGraphRevalidator>);
    ~SlotManager() override;

    void setColorSlot(SlotID, SkColor);
    void setImageSlot(SlotID, sk_sp<skresources::ImageAsset>);
    void setScalarSlot(SlotID, SkScalar);
    void setTextSlot(SlotID, TextPropertyValue&);

    SkColor getColorSlot(SlotID) const;
    sk_sp<const skresources::ImageAsset> getImageSlot(SlotID) const;
    SkScalar getScalarSlot(SlotID) const;
    TextPropertyValue getTextSlot(SlotID) const;

    struct SlotInfo {
        SlotID slotID;
        int type;
    };

    // Helper function to get all slot IDs and their value types
    const TArray<SlotInfo>& getSlotInfo() const { return fSlotInfos; }

private:

    // pass value to the SlotManager for manipulation and node for invalidation
    void trackColorValue(SlotID, SkColor*, sk_sp<sksg::Node>);
    sk_sp<skresources::ImageAsset> trackImageValue(SlotID, sk_sp<skresources::ImageAsset>);
    void trackScalarValue(SlotID, SkScalar*, sk_sp<sksg::Node>);
    void trackScalarValue(SlotID, SkScalar*, sk_sp<skottie::internal::AnimatablePropertyContainer>);
    void trackTextValue(SlotID, sk_sp<skottie::internal::TextAdapter>);

    TArray<SlotInfo> fSlotInfos;


    // ValuePair tracks a pointer to a value to change, and a means to invalidate the render tree.
    // For the latter, we can take either a node in the scene graph that directly the scene graph,
    // or an adapter which takes the value passed and interprets it before pushing to the scene
    // (clamping, normalizing, etc.)
    // Only one should be set, it is UB to create a ValuePair with both a node and an adapter.
    template <typename T>
    struct ValuePair
    {
        T value;
        sk_sp<sksg::Node> node;
        sk_sp<skottie::internal::AnimatablePropertyContainer> adapter;

        ValuePair(T _value, sk_sp<sksg::Node> _node,
                  sk_sp<skottie::internal::AnimatablePropertyContainer> _adapter) {
            value = std::move(_value);
            node = std::move(_node);
            adapter = _adapter;
            SkASSERT(!node != !adapter);
        }
    };

    class ImageAssetProxy;
    template <typename T>
    using SlotMap = THashMap<SlotID, TArray<T>>;

    SlotMap<ValuePair<SkColor*>>                       fColorMap;
    SlotMap<ValuePair<SkScalar*>>                      fScalarMap;
    SlotMap<sk_sp<ImageAssetProxy>>                    fImageMap;
    SlotMap<sk_sp<skottie::internal::TextAdapter>>     fTextMap;

    const sk_sp<skottie::internal::SceneGraphRevalidator> fRevalidator;

    friend class skottie::internal::AnimationBuilder;
};

} // namespace skottie

#endif // SlotManager_DEFINED
