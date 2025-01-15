
if (APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
    set(CMAKE_OBJC_VISIBILITY_PRESET hidden)
    set(CMAKE_OBJCXX_VISIBILITY_PRESET hidden)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Never want shared if not specified")
if (${BUILD_SHARED_LIBS})
    message(WARNING "You have set BUILD_SHARED_LIBS to ON. This is an untested and unlikedly to work config")
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Compiler specific choices
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
            $<$<BOOL:${USE_SANITIZER}>:-fsanitize=address>
            $<$<BOOL:${USE_SANITIZER}>:-fsanitize=undefined>

            $<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:OBJC>,$<COMPILE_LANGUAGE:OBJCXX>>:-fno-char8_t>
    )

    add_link_options(
            $<$<BOOL:${USE_SANITIZER}>:-fsanitize=address>
            $<$<BOOL:${USE_SANITIZER}>:-fsanitize=undefined>
    )
    if (NOT APPLE)
        add_compile_options(-march=nehalem)
    endif()
endif()


if (WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS=1)
endif()

if (MSVC)
    add_compile_options(
            # Set source and executable charsets to UTF-8
            $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
            # Do *not* use the new, breaking char8_t UTF-8 bits in C++20.
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:char8_t->
            # make msvc define __cplulsplus properly
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
    )
endif()
