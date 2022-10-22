set(ver "")

file(GLOB dot_git LIST_DIRECTORIES true ".git")
if(NOT dot_git)
else()
	execute_process(COMMAND whoami OUTPUT_VARIABLE whoami_result ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(whoami_result STREQUAL "root")
		# Prevent git paranoid "unsafe directory" error when
		# running as root (e.g. while doing `sudo make install`)
		execute_process(COMMAND stat -c %U . OUTPUT_VARIABLE owner ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
		execute_process(COMMAND su -c "git --git-dir=${dot_git} describe --always --dirty" ${owner} OUTPUT_VARIABLE ver ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
	else()
		execute_process(COMMAND git --git-dir=${dot_git} describe --always --dirty OUTPUT_VARIABLE ver ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
	endif()
endif()

if(NOT ver)
	set(ver ${staticVersion})
endif()

set(PROJECT_VERSION "${ver}")
configure_file(${inputFile} ${outputFile} @ONLY)
