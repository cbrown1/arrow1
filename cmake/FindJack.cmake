if(WIN32)
    find_library(JACK_LIBS libjack64 "${CMAKE_SOURCE_DIR}/deps")
    find_path(JACK_INCLUDE_DIR jack/jack.h HINTS "${CMAKE_SOURCE_DIR}/deps")
else()
    find_library(JACK_LIBS jack)
    find_path(JACK_INCLUDE_DIR jack/jack.h)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jack REQUIRED_VARS JACK_LIBS JACK_INCLUDE_DIR)

if(Jack_FOUND AND NOT TARGET Jack::libjack)
    add_library(Jack::libjack INTERFACE IMPORTED)
    set_target_properties(Jack::libjack PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${JACK_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${JACK_LIBS}"
    )
endif()
