require 'configure'

solution "LogDB"
	configurations { "Debug", "DebugVerbose", "Release" }

	filter "configurations:Debug*"
		defines { "DEBUG" }
		flags { "Symbols" }
		targetdir "bin/Debug"

	filter "configurations:DebugVerbose"
		defines { "VERBOSE" }

	filter "configurations:Release"
		flags { "Optimize" }
		targetdir "bin/Release"

project "LogDB"
	language "C"
	kind "SharedLib"

	files {
		"include/**.h",
		"src/**.h",
		"src/**.c"
	}
	includedirs { "include" }

	filter "system:macosx"
		linkoptions { '-Wl,-install_name', '-Wl,@loader_path/%{cfg.linktarget.name}' }

project "Tests"
	language "C"
	kind "ConsoleApp"

	files {
		"tests/**.h",
		"tests/**.def",
		"tests/**.c"
	}
	links { "LogDB" }
	includedirs { "include" }

project "StressTest"
	language "C"
	kind "ConsoleApp"

	files {
		"stress/stress.c"
	}
	links { "LogDB" }
	includedirs { "include" }

project "StressTestDump"
	language "C"
	kind "ConsoleApp"

	files {
		"stress/dump.c"
	}
	links { "LogDB" }
	includedirs { "include" }
