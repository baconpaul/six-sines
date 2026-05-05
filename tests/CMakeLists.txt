# And finally the test suite
add_executable(six-sines-test
		test_main.cpp

		structure.cpp
		factory_patches.cpp
		output_stage_dsp.cpp
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


