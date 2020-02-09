cmake_minimum_required(VERSION 3.1)

project(olinout 
    VERSION 1.0
    DESCRIPTION "Play and record multi-channel audio using jack."
    HOMEPAGE_URL "https://github.com/cbrown1/olinout"
    LANGUAGES C
)

find_library(SNDFILE_LIBS sndfile)
if(NOT SNDFILE_LIBS) 
    message(FATAL_ERROR "libsndfile not found")
endif()

find_library(JACK_LIBS jack)
if(NOT JACK_LIBS)
    message(FATAL_ERROR "libjack not found")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# required for out-of-order variable decls
set(CMAKE_C_STANDARD 99)

add_executable(olinout
    olinout.c
    cli.c
)

target_link_libraries(olinout
    PRIVATE
        ${SNDFILE_LIBS}
        ${JACK_LIBS}
        Threads::Threads
    )

