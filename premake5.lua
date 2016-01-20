
solution "LogDB"
	configurations { "Debug", "DebugVerbose", "Release" }

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

	filter "configurations:Debug*"
		defines { "DEBUG" }
		flags { "Symbols" }
		targetdir "bin/Debug"

	filter "configurations:DebugVerbose"
		defines { "VERBOSE" }

	filter "configurations:Release"
		flags { "Optimize" }
		targetdir "bin/Release"


project "Tests"
	language "C"
	kind "ConsoleApp"

	removeconfigurations { "Release" }

	files {
		"tests/**.h",
		"tests/**.def",
		"tests/**.c"
	}
	links { "LogDB" }

	includedirs { "include" }
	targetdir "bin/Debug"
