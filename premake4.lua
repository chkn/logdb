
solution "LogDB"
   configurations { "Debug", "Release" }

project "LogDB"
   language "C"
   kind "SharedLib"

   files { "**.h", "**.c" }

   configuration "Debug"
      defines { "DEBUG" }
      flags { "Symbols" }
      targetdir "bin/Debug"

   configuration "Release"
      flags { "Optimize" }
      targetdir "bin/Release"
