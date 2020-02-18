if(WIN32)
    find_library(SNDFILE_LIBS libsndfile-1 HINTS "${CMAKE_SOURCE_DIR}/deps")
    find_path(SNDFILE_INCLUDE_DIR sndfile.h HINTS "${CMAKE_SOURCE_DIR}/deps")
else()
    find_library(SNDFILE_LIBS sndfile)
    find_path(SNDFILE_INCLUDE_DIR sndfile.h)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sndfile REQUIRED_VARS SNDFILE_LIBS SNDFILE_INCLUDE_DIR)

if(Sndfile_FOUND AND NOT TARGET Sndfile::libsndfile)
    add_library(Sndfile::libsndfile INTERFACE IMPORTED)
    set_target_properties(Sndfile::libsndfile PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SNDFILE_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${SNDFILE_LIBS}"
    )
endif()
