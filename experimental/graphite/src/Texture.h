/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_Texture_DEFINED
#define skgpu_graphite_Texture_DEFINED

#include "experimental/graphite/include/TextureInfo.h"
#include "experimental/graphite/src/Resource.h"
#include "experimental/graphite/src/ResourceTypes.h"
#include "include/core/SkSize.h"

namespace skgpu::graphite {

class Texture : public Resource {
public:
    ~Texture() override;

    int numSamples() const { return fInfo.numSamples(); }
    Mipmapped mipmapped() const { return Mipmapped(fInfo.numMipLevels() > 1); }

    SkISize dimensions() const { return fDimensions; }
    const TextureInfo& textureInfo() const { return fInfo; }

protected:
    Texture(const Gpu*, SkISize dimensions, const TextureInfo& info, Ownership);

private:
    SkISize fDimensions;
    TextureInfo fInfo;
};

} // namepsace skgpu::graphite

#endif // skgpu_graphite_Texture_DEFINED
