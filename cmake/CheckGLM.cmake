set(GLM_WORKS 0)
include(CheckIncludeFileCXX)
check_include_file_cxx("glm/glm.hpp" HAVE_GLM)
if(HAVE_GLM)
	message(STATUS "Checking that GLM has the required features")
	# constexpr works since GLM 0.9.9.5.
	try_compile(HAVE_GLM_CONSTEXPR "${CMAKE_BINARY_DIR}/detect/"
				"${CMAKE_CURRENT_LIST_DIR}/check-glm-constexpr-ctor.cpp")
	if(HAVE_GLM_CONSTEXPR)
		set(GLM_WORKS 1)
    elseif(NOT CPM_LOCAL_PACKAGES_ONLY)
        message(WARNING     "GLM doesn't appear to support constexpr constructors. Please check that your GLM is at least 0.9.9.5.")
    else()
		message(FATAL_ERROR "GLM doesn't appear to support constexpr constructors. Please check that your GLM is at least 0.9.9.5.")
	endif()
endif()
unset(HAVE_GLM CACHE)
unset(HAVE_GLM_CONSTEXPR CACHE)

if(GLM_WORKS)
	message(STATUS "Checking that GLM has the required features - done")
	add_library(glm::glm IMPORTED INTERFACE)
else()
	CPMAddPackage(NAME glm
		URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz
		URL_HASH SHA256=9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c
		EXCLUDE_FROM_ALL yes)
endif()
