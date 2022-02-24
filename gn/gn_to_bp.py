#!/usr/bin/env python
#
# Copyright 2016 Google Inc.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generate Android.bp for Skia from GN configuration.

from __future__ import print_function

import os
import pprint
import string
import subprocess
import tempfile

import skqp_gn_args
import gn_to_bp_utils

# First we start off with a template for Android.bp,
# with holes for source lists and include directories.
bp = string.Template('''// This file is autogenerated by gn_to_bp.py.
// To make changes to this file, follow the instructions on skia.org for
// downloading Skia and submitting changes. Modify gn_to_bp.py (or the build
// files it uses) and submit. The autoroller will then create the updated
// Android.bp. Or ask a Skia engineer for help.

package {
    default_applicable_licenses: ["external_skia_license"],
}

// Added automatically by a large-scale-change that took the approach of
// 'apply every license found to every target'. While this makes sure we respect
// every license restriction, it may not be entirely correct.
//
// e.g. GPL in an MIT project might only apply to the contrib/ directory.
//
// Please consider splitting the single license below into multiple licenses,
// taking care not to lose any license_kind information, and overriding the
// default license using the 'licenses: [...]' property on targets as needed.
//
// For unused files, consider creating a 'fileGroup' with "//visibility:private"
// to attach the license to, and including a comment whether the files may be
// used in the current project.
//
// large-scale-change included anything that looked like it might be a license
// text as a license_text. e.g. LICENSE, NOTICE, COPYING etc.
//
// Please consider removing redundant or irrelevant files from 'license_text:'.
//
// large-scale-change filtered out the below license kinds as false-positives:
//   SPDX-license-identifier-CC-BY-NC
//   SPDX-license-identifier-GPL-2.0
//   SPDX-license-identifier-LGPL-2.1
//   SPDX-license-identifier-OFL:by_exception_only
// See: http://go/android-license-faq
license {
    name: "external_skia_license",
    visibility: [":__subpackages__"],
    license_kinds: [
        "SPDX-license-identifier-Apache-2.0",
        "SPDX-license-identifier-BSD",
        "SPDX-license-identifier-CC0-1.0",
        "SPDX-license-identifier-FTL",
        "SPDX-license-identifier-MIT",
        "legacy_unencumbered",
    ],
    license_text: [
        "LICENSE",
        "NOTICE",
    ],
}

cc_defaults {
    name: "skia_arch_defaults",
    arch: {
        arm: {
            srcs: [
                $arm_srcs
            ],

            neon: {
                srcs: [
                    $arm_neon_srcs
                ],
            },
        },

        arm64: {
            srcs: [
                $arm64_srcs
            ],
        },

        x86: {
            srcs: [
                $x86_srcs
            ],
        },

        x86_64: {
            srcs: [
                $x86_srcs
            ],
        },
    },

    target: {
      android: {
        srcs: [
          "third_party/vulkanmemoryallocator/GrVulkanMemoryAllocator.cpp",
        ],
        local_include_dirs: [
          "third_party/vulkanmemoryallocator/",
        ],
      },
    },
}

cc_defaults {
    name: "skia_defaults",
    defaults: ["skia_arch_defaults"],
    cflags: [
        $cflags
    ],

    cppflags:[
        $cflags_cc
    ],

    export_include_dirs: [
        $export_includes
    ],

    local_include_dirs: [
        $local_includes
    ]
}

cc_library_static {
    // Smaller version of Skia, without e.g. codecs, intended for use by RenderEngine.
    name: "libskia_renderengine",
    defaults: ["skia_defaults",
               "skia_renderengine_deps"],
    srcs: [
        $renderengine_srcs
    ],
    local_include_dirs: [
        "renderengine",
    ],
    export_include_dirs: [
        "renderengine",
    ],
}

cc_library_static {
    name: "libskia",
    host_supported: true,
    cppflags:[
        // Exceptions are necessary for SkRawCodec.
        // FIXME: Should we split SkRawCodec into a separate target so the rest
        // of Skia need not be compiled with exceptions?
        "-fexceptions",
    ],

    srcs: [
        $srcs
    ],

    target: {
      android: {
        srcs: [
          $android_srcs
        ],
        local_include_dirs: [
          "android",
        ],
        export_include_dirs: [
          "android",
        ],
      },
      linux_glibc: {
        srcs: [
          $linux_srcs
        ],
        local_include_dirs: [
          "linux",
        ],
        export_include_dirs: [
          "linux",
        ],
      },
      darwin: {
        srcs: [
          $mac_srcs
        ],
        local_include_dirs: [
          "mac",
        ],
        export_include_dirs: [
          "mac",
        ],
      },
      windows: {
        enabled: true,
        cflags: [
          "-Wno-unknown-pragmas",
        ],
        srcs: [
          $win_srcs
        ],
        local_include_dirs: [
          "win",
        ],
        export_include_dirs: [
          "win",
        ],
      },
    },

    defaults: ["skia_deps",
               "skia_defaults",
    ],
}

cc_defaults {
    // Subset of the larger "skia_deps", which includes only the dependencies
    // needed for libskia_renderengine. Note that it includes libpng and libz
    // for the purposes of MSKP captures, but we could instead leave it up to
    // RenderEngine to provide its own SkSerializerProcs if another client
    // wants an even smaller version of libskia.
    name: "skia_renderengine_deps",
    shared_libs: [
        "libcutils",
        "liblog",
        "libpng",
        "libz",
    ],
    static_libs: [
        "libarect",
    ],
    group_static_libs: true,
    target: {
      android: {
        shared_libs: [
            "libEGL",
            "libGLESv2",
            "libvulkan",
            "libnativewindow",
        ],
        export_shared_lib_headers: [
            "libvulkan",
        ],
      },
    },
}

cc_defaults {
    name: "skia_deps",
    defaults: ["skia_renderengine_deps"],
    shared_libs: [
        "libdng_sdk",
        "libjpeg",
        "libpiex",
        "libexpat",
        "libft2",
    ],
    static_libs: [
        "libwebp-decode",
        "libwebp-encode",
        "libsfntly",
        "libwuffs_mirror_release_c",
    ],
    target: {
      android: {
        shared_libs: [
            "libheif",
        ],
      },
      darwin: {
        host_ldlibs: [
            "-framework AppKit",
        ],
      },
      windows: {
        host_ldlibs: [
            "-lgdi32",
            "-loleaut32",
            "-lole32",
            "-lopengl32",
            "-luuid",
            "-lwindowscodecs",
        ],
      },
    },
}

cc_defaults {
    name: "skia_tool_deps",
    defaults: [
        "skia_deps",
    ],
    shared_libs: [
        "libicu",
        "libharfbuzz_ng",
    ],
    static_libs: [
        "libskia",
    ],
    cflags: [
        "-DSK_SHAPER_HARFBUZZ_AVAILABLE",
        "-DSK_UNICODE_AVAILABLE",
        "-Wno-implicit-fallthrough",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
    ],
    target: {
      windows: {
        enabled: true,
      },
    },

    data: [
        "resources/**/*",
    ],
}

cc_defaults {
    name: "skia_gm_srcs",
    local_include_dirs: [
        $gm_includes
    ],

    srcs: [
        $gm_srcs
    ],
}

cc_defaults {
    name: "skia_test_minus_gm_srcs",
    local_include_dirs: [
        $test_minus_gm_includes
    ],

    srcs: [
        $test_minus_gm_srcs
    ],
}

cc_test {
    name: "skia_dm",

    defaults: [
        "skia_gm_srcs",
        "skia_test_minus_gm_srcs",
        "skia_tool_deps",
    ],

    local_include_dirs: [
        $dm_includes
    ],

    srcs: [
        $dm_srcs
    ],

    shared_libs: [
        "libbinder",
        "libutils",
    ],
}

cc_test {
    name: "skia_nanobench",

    defaults: [
        "skia_gm_srcs",
        "skia_tool_deps"
    ],

    local_include_dirs: [
        $nanobench_includes
    ],

    srcs: [
        $nanobench_srcs
    ],

    lto: {
        never: true,
    },
}

cc_library_shared {
    name: "libskqp_jni",
    sdk_version: "$skqp_sdk_version",
    stl: "libc++_static",
    compile_multilib: "both",

    defaults: [
        "skia_arch_defaults",
    ],

    cflags: [
        $skqp_cflags
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
    ],

    cppflags:[
        $skqp_cflags_cc
    ],

    local_include_dirs: [
        "skqp",
        $skqp_includes
    ],

    export_include_dirs: [
        "skqp",
    ],

    srcs: [
        $skqp_srcs
    ],

    header_libs: ["jni_headers"],

    shared_libs: [
          "libandroid",
          "libEGL",
          "libGLESv2",
          "liblog",
          "libvulkan",
          "libz",
    ],
    static_libs: [
          "libexpat",
          "libjpeg_static_ndk",
          "libpng_ndk",
          "libwebp-decode",
          "libwebp-encode",
          "libwuffs_mirror_release_c",
    ]
}

android_test {
    name: "CtsSkQPTestCases",
    defaults: ["cts_defaults"],

    libs: ["android.test.runner.stubs"],
    jni_libs: ["libskqp_jni"],
    compile_multilib: "both",

    static_libs: [
        "android-support-design",
        "ctstestrunner-axt",
    ],
    manifest: "platform_tools/android/apps/skqp/src/main/AndroidManifest.xml",
    test_config: "platform_tools/android/apps/skqp/src/main/AndroidTest.xml",

    asset_dirs: ["platform_tools/android/apps/skqp/src/main/assets", "resources"],
    resource_dirs: ["platform_tools/android/apps/skqp/src/main/res"],
    srcs: ["platform_tools/android/apps/skqp/src/main/java/**/*.java"],

    sdk_version: "test_current",
}
''')

# We'll run GN to get the main source lists and include directories for Skia.
def generate_args(target_os, enable_gpu, renderengine = False):
  d = {
    'is_official_build':                    'true',

    # gn_to_bp_utils' GetArchSources will take care of architecture-specific
    # files.
    'target_cpu':                           '"none"',

    # Use the custom FontMgr, as the framework will handle fonts.
    'skia_enable_fontmgr_custom_directory': 'false',
    'skia_enable_fontmgr_custom_embedded':  'false',
    'skia_enable_fontmgr_android':          'false',
    'skia_enable_fontmgr_win':              'false',
    'skia_enable_fontmgr_win_gdi':          'false',
    'skia_use_fonthost_mac':                'false',

    # enable features used in skia_nanobench
    'skia_tools_require_resources':         'true',

    'skia_use_fontconfig':                  'false',
    'skia_include_multiframe_procs':        'false',
  }
  d['target_os'] = target_os
  if target_os == '"android"':
    d['skia_enable_tools'] = 'true'
    d['skia_include_multiframe_procs'] = 'true'

  if enable_gpu:
    d['skia_use_vulkan']   = 'true'
  else:
    d['skia_use_vulkan']   = 'false'
    d['skia_enable_gpu']   = 'false'

  if target_os == '"win"':
    # The Android Windows build system does not provide FontSub.h
    d['skia_use_xps'] = 'false'

    # BUILDCONFIG.gn expects these to be set when building for Windows, but
    # we're just creating Android.bp, so we don't need them. Populate with
    # some placeholder values.
    d['win_vc'] = '"placeholder_version"'
    d['win_sdk_version'] = '"placeholder_version"'
    d['win_toolchain_version'] = '"placeholder_version"'

  if target_os == '"android"' and not renderengine:
    d['skia_use_libheif']  = 'true'
  else:
    d['skia_use_libheif']  = 'false'

  if renderengine:
    d['skia_use_libpng_decode'] = 'false'
    d['skia_use_libjpeg_turbo_decode'] = 'false'
    d['skia_use_libjpeg_turbo_encode'] = 'false'
    d['skia_use_libwebp_decode'] = 'false'
    d['skia_use_libwebp_encode'] = 'false'
    d['skia_use_libgifcodec'] = 'false'
    d['skia_enable_pdf'] = 'false'
    d['skia_use_freetype'] = 'false'
    d['skia_use_fixed_gamma_text'] = 'false'
    d['skia_use_expat'] = 'false'
    d['skia_enable_fontmgr_custom_empty'] = 'false'
  else:
    d['skia_enable_android_utils'] = 'true'
    d['skia_use_freetype'] = 'true'
    d['skia_use_fixed_gamma_text'] = 'true'
    d['skia_enable_fontmgr_custom_empty'] = 'true'
    d['skia_use_wuffs'] = 'true'

  return d

gn_args       = generate_args('"android"', True)
gn_args_linux = generate_args('"linux"',   False)
gn_args_mac   = generate_args('"mac"',     False)
gn_args_win   = generate_args('"win"',     False)
gn_args_renderengine  = generate_args('"android"', True, True)

js = gn_to_bp_utils.GenerateJSONFromGN(gn_args)

def strip_slashes(lst):
  return {str(p.lstrip('/')) for p in lst}

android_srcs    = strip_slashes(js['targets']['//:skia']['sources'])
cflags          = strip_slashes(js['targets']['//:skia']['cflags'])
cflags_cc       = strip_slashes(js['targets']['//:skia']['cflags_cc'])
local_includes  = strip_slashes(js['targets']['//:skia']['include_dirs'])
export_includes = strip_slashes(js['targets']['//:public']['include_dirs'])

gm_srcs         = strip_slashes(js['targets']['//:gm']['sources'])
gm_includes     = strip_slashes(js['targets']['//:gm']['include_dirs'])

test_srcs         = strip_slashes(js['targets']['//:tests']['sources'])
test_includes     = strip_slashes(js['targets']['//:tests']['include_dirs'])

dm_srcs         = strip_slashes(js['targets']['//:dm']['sources'])
dm_includes     = strip_slashes(js['targets']['//:dm']['include_dirs'])

nanobench_target = js['targets']['//:nanobench']
nanobench_srcs     = strip_slashes(nanobench_target['sources'])
nanobench_includes = strip_slashes(nanobench_target['include_dirs'])


gn_to_bp_utils.GrabDependentValues(js, '//:gm', 'sources', gm_srcs, '//:skia')
gn_to_bp_utils.GrabDependentValues(js, '//:tests', 'sources', test_srcs, '//:skia')
gn_to_bp_utils.GrabDependentValues(js, '//:dm', 'sources',
                                   dm_srcs, ['//:skia', '//:gm', '//:tests'])
gn_to_bp_utils.GrabDependentValues(js, '//:nanobench', 'sources',
                                   nanobench_srcs, ['//:skia', '//:gm'])

# skcms is a little special, kind of a second-party library.
local_includes.add("include/third_party/skcms")
gm_includes   .add("include/third_party/skcms")

# Android's build will choke if we list headers.
def strip_headers(sources):
  return {s for s in sources if not s.endswith('.h')}

gn_to_bp_utils.GrabDependentValues(js, '//:skia', 'sources', android_srcs, None)
android_srcs    = strip_headers(android_srcs)

js_linux        = gn_to_bp_utils.GenerateJSONFromGN(gn_args_linux)
linux_srcs      = strip_slashes(js_linux['targets']['//:skia']['sources'])
gn_to_bp_utils.GrabDependentValues(js_linux, '//:skia', 'sources', linux_srcs,
                                   None)
linux_srcs      = strip_headers(linux_srcs)

js_mac          = gn_to_bp_utils.GenerateJSONFromGN(gn_args_mac)
mac_srcs        = strip_slashes(js_mac['targets']['//:skia']['sources'])
gn_to_bp_utils.GrabDependentValues(js_mac, '//:skia', 'sources', mac_srcs,
                                   None)
mac_srcs        = strip_headers(mac_srcs)

js_win          = gn_to_bp_utils.GenerateJSONFromGN(gn_args_win)
win_srcs        = strip_slashes(js_win['targets']['//:skia']['sources'])
gn_to_bp_utils.GrabDependentValues(js_win, '//:skia', 'sources', win_srcs,
                                   None)
win_srcs        = strip_headers(win_srcs)

srcs = android_srcs.intersection(linux_srcs).intersection(mac_srcs)
srcs = srcs.intersection(win_srcs)
android_srcs    = android_srcs.difference(srcs)
linux_srcs      =   linux_srcs.difference(srcs)
mac_srcs        =     mac_srcs.difference(srcs)
win_srcs        =     win_srcs.difference(srcs)

gm_srcs         = strip_headers(gm_srcs)
test_srcs       = strip_headers(test_srcs)
dm_srcs         = strip_headers(dm_srcs).difference(gm_srcs).difference(test_srcs)
nanobench_srcs  = strip_headers(nanobench_srcs).difference(gm_srcs)

test_minus_gm_includes = test_includes.difference(gm_includes)
test_minus_gm_srcs = test_srcs.difference(gm_srcs)

cflags = gn_to_bp_utils.CleanupCFlags(cflags)
cflags_cc = gn_to_bp_utils.CleanupCCFlags(cflags_cc)

# Execute GN for specialized RenderEngine target
js_renderengine   = gn_to_bp_utils.GenerateJSONFromGN(gn_args_renderengine)
renderengine_srcs = strip_slashes(
    js_renderengine['targets']['//:skia']['sources'])
gn_to_bp_utils.GrabDependentValues(js_renderengine, '//:skia', 'sources',
                                   renderengine_srcs, None)
renderengine_srcs = strip_headers(renderengine_srcs)

# Execute GN for specialized SkQP target
skqp_sdk_version = 26
js_skqp = gn_to_bp_utils.GenerateJSONFromGN(skqp_gn_args.GetGNArgs(api_level=skqp_sdk_version,
                                                                   debug=False,
                                                                   is_android_bp=True))
skqp_srcs      = strip_slashes(js_skqp['targets']['//:libskqp_app']['sources'])
skqp_includes  = strip_slashes(js_skqp['targets']['//:libskqp_app']['include_dirs'])
skqp_cflags    = strip_slashes(js_skqp['targets']['//:libskqp_app']['cflags'])
skqp_cflags_cc = strip_slashes(js_skqp['targets']['//:libskqp_app']['cflags_cc'])
skqp_defines   = strip_slashes(js_skqp['targets']['//:libskqp_app']['defines'])

skqp_includes.update(strip_slashes(js_skqp['targets']['//:public']['include_dirs']))

gn_to_bp_utils.GrabDependentValues(js_skqp, '//:libskqp_app', 'sources',
                                   skqp_srcs, None)
gn_to_bp_utils.GrabDependentValues(js_skqp, '//:libskqp_app', 'include_dirs',
                                   skqp_includes, ['//:gif'])
gn_to_bp_utils.GrabDependentValues(js_skqp, '//:libskqp_app', 'cflags',
                                   skqp_cflags, None)
gn_to_bp_utils.GrabDependentValues(js_skqp, '//:libskqp_app', 'cflags_cc',
                                   skqp_cflags_cc, None)
gn_to_bp_utils.GrabDependentValues(js_skqp, '//:libskqp_app', 'defines',
                                   skqp_defines, None)

skqp_defines.add("SK_ENABLE_DUMP_GPU")
skqp_defines.add("SK_BUILD_FOR_SKQP")
skqp_defines.add("SK_ALLOW_STATIC_GLOBAL_INITIALIZERS=1")

skqp_srcs = strip_headers(skqp_srcs)
skqp_cflags = gn_to_bp_utils.CleanupCFlags(skqp_cflags)
skqp_cflags_cc = gn_to_bp_utils.CleanupCCFlags(skqp_cflags_cc)

here = os.path.dirname(__file__)
defs = gn_to_bp_utils.GetArchSources(os.path.join(here, 'opts.gni'))

def get_defines(json):
  return {str(d) for d in json['targets']['//:skia']['defines']}
android_defines      = get_defines(js)
linux_defines        = get_defines(js_linux)
mac_defines          = get_defines(js_mac)
win_defines          = get_defines(js_win)
renderengine_defines = get_defines(js_renderengine)
renderengine_defines.add('SK_IN_RENDERENGINE')

def mkdir_if_not_exists(path):
  if not os.path.exists(path):
    os.makedirs(path)
mkdir_if_not_exists('android/include/config/')
mkdir_if_not_exists('linux/include/config/')
mkdir_if_not_exists('mac/include/config/')
mkdir_if_not_exists('win/include/config/')
mkdir_if_not_exists('renderengine/include/config/')
mkdir_if_not_exists('skqp/include/config/')

platforms = { 'IOS', 'MAC', 'WIN', 'ANDROID', 'UNIX' }

def disallow_platforms(config, desired):
  with open(config, 'a') as f:
    p = sorted(platforms.difference({ desired }))
    s = '#if '
    for i in range(len(p)):
      s = s + 'defined(SK_BUILD_FOR_%s)' % p[i]
      if i < len(p) - 1:
        s += ' || '
        if i % 2 == 1:
          s += '\\\n    '
    print(s, file=f)
    print('    #error "Only SK_BUILD_FOR_%s should be defined!"' % desired, file=f)
    print('#endif', file=f)

def append_to_file(config, s):
  with open(config, 'a') as f:
    print(s, file=f)

def write_android_config(config_path, defines, isNDKConfig = False):
  gn_to_bp_utils.WriteUserConfig(config_path, defines)
  append_to_file(config_path, '''
#ifndef SK_BUILD_FOR_ANDROID
    #error "SK_BUILD_FOR_ANDROID must be defined!"
#endif''')
  disallow_platforms(config_path, 'ANDROID')

  if isNDKConfig:
    append_to_file(config_path, '''
#undef SK_BUILD_FOR_ANDROID_FRAMEWORK''')

write_android_config('android/include/config/SkUserConfig.h', android_defines)
write_android_config('renderengine/include/config/SkUserConfig.h', renderengine_defines)
write_android_config('skqp/include/config/SkUserConfig.h', skqp_defines, True)

def write_config(config_path, defines, platform):
  gn_to_bp_utils.WriteUserConfig(config_path, defines)
  append_to_file(config_path, '''
// Correct SK_BUILD_FOR flags that may have been set by
// SkTypes.h/Android.bp
#ifndef SK_BUILD_FOR_%s
    #define SK_BUILD_FOR_%s
#endif
#ifdef SK_BUILD_FOR_ANDROID
    #undef SK_BUILD_FOR_ANDROID
#endif''' % (platform, platform))
  disallow_platforms(config_path, platform)

write_config('linux/include/config/SkUserConfig.h', linux_defines, 'UNIX')
write_config('mac/include/config/SkUserConfig.h',   mac_defines, 'MAC')
write_config('win/include/config/SkUserConfig.h',   win_defines, 'WIN')

# Turn a list of strings into the style bpfmt outputs.
def bpfmt(indent, lst, sort=True):
  if sort:
    lst = sorted(lst)
  return ('\n' + ' '*indent).join('"%s",' % v for v in lst)

# OK!  We have everything to fill in Android.bp...
with open('Android.bp', 'w') as Android_bp:
  print(bp.substitute({
    'export_includes': bpfmt(8, export_includes),
    'local_includes':  bpfmt(8, local_includes),
    'srcs':            bpfmt(8, srcs),
    'cflags':          bpfmt(8, cflags, False),
    'cflags_cc':       bpfmt(8, cflags_cc),

    'arm_srcs':      bpfmt(16, strip_headers(defs['armv7'])),
    'arm_neon_srcs': bpfmt(20, strip_headers(defs['neon'])),
    'arm64_srcs':    bpfmt(16, strip_headers(defs['arm64'] +
                                             defs['crc32'])),
    'x86_srcs':      bpfmt(16, strip_headers(defs['sse2'] +
                                             defs['ssse3'] +
                                             defs['sse41'] +
                                             defs['sse42'] +
                                             defs['avx'  ] +
                                             defs['hsw'  ] +
                                             defs['skx'  ])),

    'gm_includes'       : bpfmt(8, gm_includes),
    'gm_srcs'           : bpfmt(8, gm_srcs),

    'test_minus_gm_includes' : bpfmt(8, test_minus_gm_includes),
    'test_minus_gm_srcs'     : bpfmt(8, test_minus_gm_srcs),

    'dm_includes'       : bpfmt(8, dm_includes),
    'dm_srcs'           : bpfmt(8, dm_srcs),

    'nanobench_includes'    : bpfmt(8, nanobench_includes),
    'nanobench_srcs'        : bpfmt(8, nanobench_srcs),

    'skqp_sdk_version': skqp_sdk_version,
    'skqp_includes':    bpfmt(8, skqp_includes),
    'skqp_srcs':        bpfmt(8, skqp_srcs),
    'skqp_cflags':      bpfmt(8, skqp_cflags, False),
    'skqp_cflags_cc':   bpfmt(8, skqp_cflags_cc),

    'android_srcs':  bpfmt(10, android_srcs),
    'linux_srcs':    bpfmt(10, linux_srcs),
    'mac_srcs':      bpfmt(10, mac_srcs),
    'win_srcs':      bpfmt(10, win_srcs),

    'renderengine_srcs': bpfmt(8, renderengine_srcs),
  }), file=Android_bp)
