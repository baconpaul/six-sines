# And finally the test suite
add_executable(six-sines-test
		test_main.cpp

		structure.cpp
)

target_link_libraries(six-sines-test
		fmt
		six-sines-impl
		catch2

        )

if (WIN32)
	if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		target_link_libraries(scxt-test DbgHelp.lib)
	endif()
endif()


