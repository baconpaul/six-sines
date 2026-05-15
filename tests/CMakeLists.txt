# And finally the test suite
add_executable(six-sines-test
		test_main.cpp

		structure.cpp
		factory_patches.cpp
		output_stage_dsp.cpp
		mpe_smoothing.cpp
)

target_link_libraries(six-sines-test
		fmt
		six-sines-impl
		catch2
		six-sines-patches

        )

if (WIN32)
	if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		target_link_libraries(six-sines-test DbgHelp.lib)
	endif()
endif()


# Performance benchmark target. Built only when explicitly requested
# (cmake --build … --target six-sines-perf). Not in any default aggregate,
# so day-to-day builds are untouched.
#
# Run a Release build for meaningful numbers:
#   cmake -B cmake-build-perf -DCMAKE_BUILD_TYPE=Release -GNinja
#   cmake --build cmake-build-perf --target six-sines-perf
#   ./cmake-build-perf/tests/six-sines-perf
#
# Filter scenarios with Catch2 tags, e.g. "[scn:8v_dense]" or "[bench][plugin]".
add_executable(six-sines-perf
		perf_main.cpp
		perf_scenarios.cpp
)

target_link_libraries(six-sines-perf
		fmt
		six-sines-impl
		catch2
		six-sines-patches
)

if (WIN32)
	if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		target_link_libraries(six-sines-perf DbgHelp.lib)
	endif()
endif()


