
local api = premake.api
local action = premake.action

-- Add iOS to the allowed system values and `os` command line option
api.addAllowed("system", { "ios" })
table.insert(premake.option.list["os"].allowed, { "ios", "Apple iOS" })

-- The `xcode4` action hardcodes OS to `macosx`. Unset that.
local act = action.get("xcode4")
if act then
	act.os = nil
end

filter "system:ios"
	kind "StaticLib"
	xcodebuildsettings
	{
		["ARCHS"] = "$(ARCHS_STANDARD)";
		["SDKROOT"] = "iphoneos";
		["SUPPORTED_PLATFORMS"] = "iphonesimulator iphoneos";
	}
