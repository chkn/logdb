
solution "LogDB"
   configurations { "Debug", "Release" }

project "LogDB"
   language "C"
   kind "SharedLib"

   files {
       "include/**.h",
       "src/**.h",
       "src/**.c"
   }
   includedirs { "include" }

   configuration "Debug"
      defines { "DEBUG" }
      flags { "Symbols" }
      targetdir "bin/Debug"

   configuration "Release"
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
