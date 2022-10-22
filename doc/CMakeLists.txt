set(FUTURE_PROJECT_VERSION "@PROJECT_VERSION@")
configure_file(Doxyfile.cmake "${CMAKE_BINARY_DIR}/doc/Doxyfile-unversioned")
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/doc/Doxyfile"
	DEPENDS "${CMAKE_BINARY_DIR}/doc/Doxyfile-unversioned"
	WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
	COMMAND "${CMAKE_COMMAND}"
		-D "staticVersion=${staticProjectVersion}"
		-D "inputFile=${CMAKE_BINARY_DIR}/doc/Doxyfile-unversioned"
		-D "outputFile=${CMAKE_BINARY_DIR}/doc/Doxyfile"
		-P "${PROJECT_SOURCE_DIR}/cmake/gen_version.cmake")
add_custom_target(doc
                  COMMAND doxygen
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/doc
                  DEPENDS ${CMAKE_BINARY_DIR}/doc/Doxyfile)
