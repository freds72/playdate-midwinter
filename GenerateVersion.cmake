# Run this with CMake in "script mode" (-P flag) at build time

# in case Git is not available, we default to "unknown"
set(GIT_COUNT "00")

string(TIMESTAMP BUILD_DATE "%Y.%m.%d")

# find Git and if available set GIT_HASH variable
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND git rev-list --count HEAD -- .
        OUTPUT_VARIABLE GIT_COUNT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

message(STATUS "Git rev. count is ${GIT_COUNT}")

# generate file based on xxx.in
configure_file(
    ${IN_FILE}
    ${OUT_FILE}
    @ONLY
)