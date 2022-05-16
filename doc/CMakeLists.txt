configure_file(Doxyfile.cmake "${CMAKE_BINARY_DIR}/doc/Doxyfile-unversioned")
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/doc/Doxyfile"
                   DEPENDS "${CMAKE_BINARY_DIR}/doc/Doxyfile-unversioned"
                   COMMAND "${CMAKE_SOURCE_DIR}/version.sh" "${staticProjectVersion}" "${CMAKE_BINARY_DIR}/doc/Doxyfile-unversioned" "${CMAKE_BINARY_DIR}/doc/Doxyfile")
add_custom_target(doc
                  COMMAND doxygen
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc
                  DEPENDS ${CMAKE_BINARY_DIR}/doc/Doxyfile)
