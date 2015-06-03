cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(SynchroTr)
include(FindPkgConfig)
include(FeatureSummary)

pkg_check_modules(SPARSEHASH libsparsehash)

include_directories(${CMAKE_SOURCE_DIR}/..)
link_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${SPARSEHASH_LIBRARY_DIRS})

if(APPLE)
    # it should be possible to add NSCriticalSection.mm via target_sources
    # but somehow I can't seem to get a functional library that way.
	add_library(SynchroTr STATIC
		../CritSectEx/msemul.cpp
		../CritSectEx/CritSectEx.cpp
		../CritSectEx/timing.c
		../CritSectEx/NSCriticalSection.mm
		../Thread/Thread.cpp
        ../CritSectEx/msemul.h
        ../CritSectEx/CritSectEx.h
        ../Thread/Thread.h
        ../CritSectEx/timing.h)
else()
	add_library(SynchroTr STATIC
		../CritSectEx/msemul.cpp
		../CritSectEx/CritSectEx.cpp
		../CritSectEx/timing.c
		../Thread/Thread.cpp
        ../CritSectEx/msemul.h
        ../CritSectEx/CritSectEx.h
        ../Thread/Thread.h
        ../CritSectEx/timing.h)
endif(APPLE)
target_include_directories(SynchroTr PRIVATE ${SPARSEHASH_INCLUDEDIR})
target_link_libraries(SynchroTr ${SPARSEHASH_LIBRARIES})

add_executable(cseTest ../cseTest.c)
target_link_libraries(cseTest SynchroTr)
set_target_properties(cseTest PROPERTIES LINKER_LANGUAGE CXX)

if(APPLE)
	add_executable(cseTestNS ../cseTest.m)
	target_link_libraries(cseTestNS SynchroTr "-framework Foundation")
	set_target_properties(cseTestNS PROPERTIES LINKER_LANGUAGE CXX)
endif(APPLE)

add_executable(threadTest ../threadTest.cpp)
target_link_libraries(threadTest SynchroTr)

if(UNIX)
	if(NOT APPLE)
		target_link_libraries(cseTest "-lpthread -ldl -lrt")
		target_link_libraries(threadTest "-lpthread -ldl -lrt")
	endif(NOT APPLE)
endif(UNIX)

FEATURE_SUMMARY(WHAT ALL)
