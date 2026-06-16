/*
 * Six Sines
 *
 * A synth with audio rate modulation.
 *
 * Copyright 2024-2025, Paul Walker and Various authors, as described in the github
 * transaction log.
 *
 * This source repo is released under the MIT license, but has
 * GPL3 dependencies, as such the combined work will be
 * released under GPL3.
 *
 * The source code and license are at https://github.com/baconpaul/six-sines
 */

#include <clap/clap.h>
#include "six-sines-clap-entry-impl.h"
#include "sst/cpputils/rtsan_support.h"
#include <stdio.h>

extern "C"
{

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

    // clang-format off
    const CLAP_EXPORT struct clap_plugin_entry clap_entry = {
        CLAP_VERSION,
        baconpaul::six_sines::clap_init,
        baconpaul::six_sines::clap_deinit,
        baconpaul::six_sines::get_factory
    };
    // clang-format on
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}

#if SST_CPPUTILS_HAS_RTSAN
#define SST_RTSAN_EXPORT __attribute__((visibility("default"), used))
extern "C" SST_RTSAN_EXPORT const char *__rtsan_default_options() { return "halt_on_error=false"; }
extern "C" SST_RTSAN_EXPORT void __sanitizer_report_error_summary(const char *error_summary)
{
    fprintf(stderr, "%s\n", error_summary);
}
#endif