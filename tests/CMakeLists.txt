add_executable(test-Spectrum test-Spectrum.cpp ../CalcMySky/Spectrum.cpp)
target_link_libraries(test-Spectrum Qt5::Core Qt5::OpenGL)
add_test(NAME test-Spectrum COMMAND test-Spectrum)
