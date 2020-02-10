cmake_minimum_required(VERSION 3.1)

project(olinout 
    VERSION 1.0
    DESCRIPTION "Play and record multi-channel audio using jack."
    HOMEPAGE_URL "https://github.com/cbrown1/olinout"
    LANGUAGES CXX
)

find_library(SNDFILE_LIBS sndfile)
if(NOT SNDFILE_LIBS) 
    message(FATAL_ERROR "libsndfile not found")
endif()

find_library(JACK_LIBS jack)
if(NOT JACK_LIBS)
    message(FATAL_ERROR "libjack not found")
endif()

find_package(Threads REQUIRED)

# set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS program_options REQUIRED)

set(CMAKE_CXX_STANDARD 14)

add_executable(olinout
    cli.cpp 
    io.cpp
    jack_client.cpp
    log.cpp
    main.cpp
    reactor.cpp
)

target_link_libraries(olinout
    PRIVATE
        ${SNDFILE_LIBS}
        ${JACK_LIBS}
        Threads::Threads
        Boost::boost
        Boost::program_options
    )

