configure_file(Doxyfile.cmake ${CMAKE_BINARY_DIR}/doc/Doxyfile)
add_custom_target(doc
                  COMMAND doxygen
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc
                  DEPENDS ${CMAKE_BINARY_DIR}/doc/Doxyfile)
