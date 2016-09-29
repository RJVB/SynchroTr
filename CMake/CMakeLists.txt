# kate: auto-insert-doxygen true; backspace-indents true; indent-width 4; keep-extra-spaces true; replace-tabs false; tab-indents true; tab-width 4;
cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(SynchroTr)
include(FindPkgConfig)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(FeatureSummary)

pkg_check_modules(SPARSEHASH libsparsehash)

include_directories(${CMAKE_CURRENT_LIST_DIR}/..)
link_directories(${CMAKE_CURRENT_LIST_DIR} ${CMAKE_BINARY_DIR} ${SPARSEHASH_LIBRARY_DIRS})

check_function_exists(backtrace HAVE_BACKTRACE)
set(HAVE_GNUSTEP NO)
if (${CMAKE_C_COMPILER_ID} MATCHES "Clang")
	message(STATUS "CompilerID=${CMAKE_C_COMPILER_ID}")
	include_directories(SYSTEM /usr/include/GNUstep)
	check_include_file(Foundation/Foundation.h HAVE_GNUSTEP "-v -x objective-c -isystem /usr/include/GNUstep")
	message(STATUS "HAVE_GNUSTEP=${HAVE_GNUSTEP}")
endif()

if(APPLE OR HAVE_GNUSTEP)
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

# libraries that should be added to all executables created after this point:
link_libraries(SynchroTr)
if(UNIX)
	if(NOT APPLE)
		add_compile_options(-pthread)
		link_libraries("-pthread -ldl -lrt")
	endif(NOT APPLE)
endif(UNIX)

add_executable(cseTest ${CMAKE_CURRENT_LIST_DIR}/../cseTest.c)
set_target_properties(cseTest PROPERTIES LINKER_LANGUAGE CXX)

if(APPLE OR HAVE_GNUSTEP)
	add_executable(cseTestNS ${CMAKE_CURRENT_LIST_DIR}/../cseTest.m)
	set_target_properties(cseTestNS PROPERTIES LINKER_LANGUAGE CXX)
	if(APPLE)
		target_link_libraries(cseTestNS "-framework Foundation")
	else()
		target_link_libraries(cseTestNS "-lgnustep-base -lobjc")
	endif()
endif(APPLE)

add_executable(threadTest ${CMAKE_CURRENT_LIST_DIR}/../threadTest.cpp)

FEATURE_SUMMARY(WHAT ALL)
