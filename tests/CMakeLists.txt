add_executable(test-Spectrum test-Spectrum.cpp ../common/Spectrum.cpp)
target_link_libraries(test-Spectrum Qt5::Core Qt5::OpenGL)
foreach(testNum RANGE 1 4)
    add_test(NAME "\"Spectrum test ${testNum}\"" COMMAND test-Spectrum ${testNum})
endforeach()

add_executable(test-Fourier-interpolation test-Fourier-interpolation.cpp)
foreach(testId "identity transformation" "integral upsampling" "fractional upsampling")
    add_test(NAME "\"Fourier interpolation,  odd-length input, ${testId}\"" COMMAND test-Fourier-interpolation ${testId} odd)
    add_test(NAME "\"Fourier interpolation, even-length input, ${testId}\"" COMMAND test-Fourier-interpolation ${testId} even)
endforeach()

add_executable(test-Spline-interpolation test-Spline-interpolation.cpp)
add_test(NAME "\"Spline interpolation\"" COMMAND test-Spline-interpolation)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose)
