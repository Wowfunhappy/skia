/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "experimental/graphite/src/Device.h"

#include "include/core/SkImageInfo.h"

namespace skgpu {

Device::Device(const SkImageInfo& ii) : SkBaseDevice(ii, SkSurfaceProps()) {}

} // namespace skgpu
