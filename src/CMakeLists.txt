
option(ENABLE_SANITIZERS "Enable use of AddressSanitizer and Undefined Behavior Sanitizer?" OFF)
if(ENABLE_SANITIZERS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer -fsanitize-recover=address,undefined")
endif()

find_library(SNDFILE_LIBS sndfile)
if(NOT SNDFILE_LIBS)
    message(FATAL_ERROR "libsndfile not found")
endif()

find_library(JACK_LIBS jack)
if(NOT JACK_LIBS)
    message(FATAL_ERROR "libjack not found")
endif()

find_package(Threads REQUIRED)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS program_options REQUIRED)

set(CMAKE_CXX_STANDARD 14)

add_executable(arrow1
    cli.cpp
    cli.hpp
    io.cpp
    io.hpp
    jack_client.cpp
    jack_client.hpp
    log.cpp
    log.hpp
    main.cpp
    reactor.cpp
    reactor.hpp
)

target_link_libraries(arrow1
    PRIVATE
        ${SNDFILE_LIBS}
        ${JACK_LIBS}
        Threads::Threads
        Boost::boost
        Boost::program_options
    )

install(TARGETS arrow1 DESTINATION bin)

if(CMAKE_SYSTEM_NAME MATCHES Linux)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Christopher Brown")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS libjack libsndfile1)
elseif(WIN32)
    # TODO configure CPack for Windows
endif()
include(CPack)
