cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(SynchroTr)
include(FindPkgConfig)
include(CheckFunctionExists)
include(FeatureSummary)

pkg_check_modules(SPARSEHASH libsparsehash)

message(STATUS "CURLISTDIR=${CMAKE_CURRENT_LIST_DIR} CURSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} SOURCE_DIR=${CMAKE_SOURCE_DIR}")
message(STATUS "BINDIR=${CMAKE_BINARY_DIR} CURBINDIR=${CMAKE_CURRENT_BINARY_DIR}")
include_directories(${CMAKE_CURRENT_LIST_DIR}/..)
link_directories(${CMAKE_CURRENT_LIST_DIR} ${CMAKE_BINARY_DIR} ${SPARSEHASH_LIBRARY_DIRS})

check_function_exists(backtrace HAVE_BACKTRACE)

if(APPLE)
    # it should be possible to add NSCriticalSection.mm via target_sources
    # but somehow I can't seem to get a functional library that way.
	add_library(SynchroTr STATIC
		${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/msemul.cpp
		${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/CritSectEx.cpp
		${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/timing.c
		${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/NSCriticalSection.mm
		${CMAKE_CURRENT_LIST_DIR}/../Thread/Thread.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/msemul.h
        ${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/CritSectEx.h
        ${CMAKE_CURRENT_LIST_DIR}/../Thread/Thread.h
        ${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/timing.h)
else()
	add_library(SynchroTr STATIC
		${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/msemul.cpp
		${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/CritSectEx.cpp
		${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/timing.c
		${CMAKE_CURRENT_LIST_DIR}/../Thread/Thread.cpp
        ${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/msemul.h
        ${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/CritSectEx.h
        ${CMAKE_CURRENT_LIST_DIR}/../Thread/Thread.h
        ${CMAKE_CURRENT_LIST_DIR}/../CritSectEx/timing.h)
endif(APPLE)
target_include_directories(SynchroTr PRIVATE ${SPARSEHASH_INCLUDEDIR})
target_link_libraries(SynchroTr ${SPARSEHASH_LIBRARIES})
if(HAVE_BACKTRACE)
    target_compile_definitions(SynchroTr PRIVATE "-DHAVE_BACKTRACE")
    if (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
        # this probably doesn't suffice with a static SyncroTr library...
        target_link_libraries(SynchroTr "-lexecinfo")
    endif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
endif(HAVE_BACKTRACE)

add_executable(cseTest ${CMAKE_CURRENT_LIST_DIR}/../cseTest.c)
target_link_libraries(cseTest SynchroTr)
set_target_properties(cseTest PROPERTIES LINKER_LANGUAGE CXX)

if(APPLE)
	add_executable(cseTestNS ${CMAKE_CURRENT_LIST_DIR}/../cseTest.m)
	target_link_libraries(cseTestNS SynchroTr "-framework Foundation")
	set_target_properties(cseTestNS PROPERTIES LINKER_LANGUAGE CXX)
endif(APPLE)

add_executable(threadTest ${CMAKE_CURRENT_LIST_DIR}/../threadTest.cpp)
target_link_libraries(threadTest SynchroTr)

if(UNIX)
	if(NOT APPLE)
		target_link_libraries(cseTest "-lpthread -ldl -lrt")
		target_link_libraries(threadTest "-lpthread -ldl -lrt")
	endif(NOT APPLE)
endif(UNIX)

FEATURE_SUMMARY(WHAT ALL)
