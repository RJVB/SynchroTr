solution "pmSynchroTr"
	configurations { "Debug", "Release" }
	configuration "Debug"
		defines { "DEBUG" }
		flags { "Symbols" }

	configuration "Release"
		flags { "Optimize" }

	project "pmSynchroTr"
		kind "StaticLib"
		language "C++"
		files { "../CritSectEx/msemul.cpp", "../CritSectEx/CritSectEx.cpp", "../CritSectEx/timing.c", "../CritSectEx/**.h",
			"../Thread/Thread.cpp", "../Thread/Thread.h" }
		includedirs { ".." }

	project "pmcseTest"
		kind "ConsoleApp"
		language "C++"
		files { "../cseTest.c", "../CritSectEx/*.h" }
		includedirs { ".." }
		links { "pmSynchroTr", "pthread", "dl", "rt" }
	
	project "pmthreadTest"
		kind "ConsoleApp"
		language "C++"
		files { "../threadTest.cpp", "../Thread/Thread.h", "../CritSectEx/*.h" }
		includedirs { ".." }
		links { "pmSynchroTr", "pthread", "dl", "rt" }
