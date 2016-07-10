cmake_minimum_required(VERSION 2.8)
project (asio-test)

##
## General configuration
##
set(CMAKE_MODULE_PATH $ENV{CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ECLIPSE_GENERATE_SOURCE_PROJECT TRUE)
set(BUILD_SHARED_LIBS TRUE)

##
## MSVC specific configuration
## Taken from OSG CMake build
##
if(MSVC)
    set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "add a postfix, usually d on windows")

    # This option is to enable the /MP switch for Visual Studio 2005 and above compilers
    option(WIN32_USE_MP "Set to ON to build ReviaOSG with the /MP option (Visual Studio 2005 and above)." ON)
    mark_as_advanced(WIN32_USE_MP)
    if(WIN32_USE_MP)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    endif(WIN32_USE_MP)

    option(WIN32_DISABLE_OPTIMIZATION_RELEASEDEBINFO "Build RelWithDebInfo with no optimization? (simplifies debugging)" ON)
    mark_as_advanced(WIN32_DISABLE_OPTIMIZATION_RELEASEDEBINFO)
    if(WIN32_DISABLE_OPTIMIZATION_RELEASEDEBINFO)
        string(REPLACE "/O2" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
        string(REPLACE "/O2" "" CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO})
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Od")
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /Od")
    endif(WIN32_DISABLE_OPTIMIZATION_RELEASEDEBINFO)

    # turn off various warnings
    # foreach(warning 4244 4251 4267 4275 4290 4786 4305 4996)
    #     SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd${warning}")
    # endforeach(warning)

    # This option is to enable the /DYNAMICBASE switch
    # It is used to workaround a bug in Windows 7 when linking in release, which results in corrupt
    # binaries. See this page for details: http://www.wintellect.com/CS/blogs/jrobbins/archive/2009/01/24/the-case-of-the-corrupt-pe-binaries.aspx
    option(WIN32_USE_DYNAMICBASE "Set to ON to build OpenSceneGraph with the /DYNAMICBASE option to work around a bug when linking release executables on Windows 7." OFF)
    mark_as_advanced(WIN32_USE_DYNAMICBASE)
    if(WIN32_USE_DYNAMICBASE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DYNAMICBASE")
    endif(WIN32_USE_DYNAMICBASE)

    # More MSVC specific compilation flags
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)

    option(MSVC_DISABLE_CHECKED_ITERATORS "Set to ON to disable Visual C++ checked iterators. If you do this you must ensure that every other project in your solution and all dependencies are compiled with _SECURE_SCL=0." OFF)
    mark_as_advanced(MSVC_DISABLE_CHECKED_ITERATORS)
    if(MSVC_DISABLE_CHECKED_ITERATORS)
        add_definitions(-D_SECURE_SCL=0)
    endif(MSVC_DISABLE_CHECKED_ITERATORS)

    option(MSVC_USE_DEFAULT_STACK_SIZE "Set to ON to use the default Visual C++ stack size. CMake forces a high stack size by default, which can cause problems for applications with large number of threads." OFF)
    mark_as_advanced(MSVC_USE_DEFAULT_STACK_SIZE)
    if(MSVC_USE_DEFAULT_STACK_SIZE)
        string(REGEX REPLACE "/STACK:[0-9]+" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
        string(REGEX REPLACE "/STACK:[0-9]+" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
        string(REGEX REPLACE "/STACK:[0-9]+" "" CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS}")
    endif(MSVC_USE_DEFAULT_STACK_SIZE)

    # purify complains about this, relocatable code?
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /FIXED:NO")
endif()

### C++11 on!
set(CMAKE_CXX_FLAGS "-std=c++11" CACHE STRING "compile flags" FORCE)

##
## Deps
##

find_package(Boost COMPONENTS thread system program_options REQUIRED)
link_directories(${Boost_LIBRARY_DIRS}) # seems to be required for boost otherwise it doesn't find some of the libs?

include_directories(
    ${Boost_INCLUDE_DIR}
    .
    )

set(LIBS
    )

if(MSVC)
    set(LIBS ${LIBS}
        winmm
        )
else(MSVC)
    set(LIBS ${LIBS} 
        ${Boost_LIBRARIES} # uses pragmas on Windows
        pthread
        ) 
endif(MSVC)

file(GLOB SRC *.cpp)
file(GLOB INC *.h)

add_executable(asio ${SRC} ${INC})
target_link_libraries(asio ${LIBS})

