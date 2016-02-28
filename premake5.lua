require 'configure'
require 'ios'

workspace "LogDB"
	configurations { "Debug", "DebugVerbose", "Release" }

	filter "configurations:Debug*"
		defines { "DEBUG" }
		flags { "Symbols" }
		targetdir "bin/Debug"

	filter "configurations:DebugVerbose"
		defines { "VERBOSE" }

	filter "configurations:Release"
		optimize "Speed"
		targetdir "bin/Release"

project "LogDB"
	language "C"

	files {
		"include/**.h",
		"src/**.h",
		"src/**.c"
	}
	includedirs { "include" }

	filter "system:not ios"
		kind "SharedLib"

	filter "system:macosx"
		linkoptions { '-Wl,-install_name', '-Wl,@loader_path/%{cfg.linktarget.name}' }

	filter "configurations:Release"
		-- build universal binary for release
		xcodebuildsettings
		{
			["ARCHS"] = "$(ARCHS_STANDARD_32_64_BIT)";
			["ONLY_ACTIVE_ARCH"] = "NO";
		}

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

newaction {
	trigger     = "clean",
	description = "Remove all binaries and generated files",

	execute = function()
		os.rmdir("bin")
		os.rmdir("obj")
		os.rmdir("DerivedData")
		os.rmdir(path.join("bindings", "C#", "bin"))
		os.rmdir(path.join("bindings", "C#", "obj"))
		for i, dir in ipairs(os.matchdirs("*.xc*")) do
			os.rmdir(dir)
		end
		os.remove("Makefile")
		for i, file in ipairs(os.matchfiles("*.make")) do
			os.remove(file)
		end
		for i, file in ipairs(os.matchfiles(path.join("bindings", "C#", "*.nupkg"))) do
			os.remove(file)
		end
	end
}
