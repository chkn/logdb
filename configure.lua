
newaction {
    trigger = "configure",
    description = "Run system checks",
    execute = function()
        local cwd = os.getcwd()
        local files = os.matchfiles("system_checks/*.c")
        for i, path in ipairs(files) do
            if os.executef("cc -o %s/a.out %s", cwd, path) ~= true or os.executef("%s/a.out", cwd) ~= true then
                error(path .. ": FAILED")
            end
        end
        os.remove(cwd .. "/a.out")
        print("Success!")
    end
}
