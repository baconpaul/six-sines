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
#if WIN32
#include <windows.h>
#endif

#include <iostream>
#define LG std::cout << __FILE__ << ":" << __LINE__ << " "

#include "../../libs/clap-libs/clap-wrapper/src/detail/shared/sha1.h"

extern "C"
{

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif

    bool custom_clap_init(const char *p) {
#if WIN32
#define WIN_DEBUG_CONSOLE_ACTIVE 1
#if WIN_DEBUG_CONSOLE_ACTIVE
        static FILE *confp{nullptr};
        AllocConsole();
        freopen_s(&confp, "CONOUT$", "w", stdout);
        HANDLE hConOut =
            CreateFile(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
        std::cout.clear();
        std::wcout.clear();
        printf("Hello Console\n");
        std::cout << "Heya" << std::endl;
#endif
#endif

        LG << "Custom Clap Init " << p << std::endl;

        std::string id{"org.thisthat.plugin.name"};
        auto g = Crypto::create_sha1_guid_from_name(id.c_str(), id.size());
        char c[2048];
        memcpy(c, &g, sizeof(Crypto::uuid_object));
        LG << "UUID : " << std::hex;
        for (int i=0; i<sizeof(Crypto::uuid_object); ++i)
        {
            std::cout << (int)c[i];
        }
        std::cout << std::dec << std::endl;

        return true;
    }

    // clang-format off
    const CLAP_EXPORT struct clap_plugin_entry clap_entry = {
        CLAP_VERSION,
        // baconpaul::six_sines::clap_init,
custom_clap_init,
        baconpaul::six_sines::clap_deinit,
        baconpaul::six_sines::get_factory
    };
    // clang-format on
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}