add_executable(test-Spectrum test-Spectrum.cpp ../common/Spectrum.cpp)
target_link_libraries(test-Spectrum Qt${QT_VERSION}::Core
	Qt${QT_VERSION}::OpenGL glm::glm)
foreach(testNum RANGE 1 4)
    add_test(NAME "\"Spectrum test ${testNum}\"" COMMAND test-Spectrum ${testNum})
endforeach()

add_executable(test-Fourier-interpolation test-Fourier-interpolation.cpp)
target_link_libraries(test-Fourier-interpolation Eigen3::Eigen)
foreach(testId "identity transformation" "integral upsampling" "fractional upsampling")
    add_test(NAME "\"Fourier interpolation,  odd-length input, ${testId}\"" COMMAND test-Fourier-interpolation ${testId} odd)
    add_test(NAME "\"Fourier interpolation, even-length input, ${testId}\"" COMMAND test-Fourier-interpolation ${testId} even)
endforeach()

add_executable(test-Spline-interpolation test-Spline-interpolation.cpp)
target_link_libraries(test-Spline-interpolation Eigen3::Eigen)
add_test(NAME "\"Spline interpolation\"" COMMAND test-Spline-interpolation)

add_executable(test-exception-catch test-exception-catch.cpp)
target_link_libraries(test-exception-catch PUBLIC Qt${QT_VERSION}::Core Qt${QT_VERSION}::Widgets Qt${QT_VERSION}::OpenGL)
target_compile_definitions(test-exception-catch PRIVATE -DLIBRARY_FILE_PATH="$<TARGET_FILE:ShowMySky>")
add_test(NAME "\"Catching exceptions from libShowMySky\"" COMMAND test-exception-catch)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose)
