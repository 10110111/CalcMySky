add_executable(test-Spectrum test-Spectrum.cpp ../common/Spectrum.cpp)
target_link_libraries(test-Spectrum Qt5::Core Qt5::OpenGL)
foreach(testNum RANGE 1 4)
    add_test(NAME "\"Spectrum test ${testNum}\"" COMMAND test-Spectrum ${testNum})
endforeach()

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose)
