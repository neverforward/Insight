add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

-- add_requires("levilamina x.x.x") for a specific version
-- add_requires("levilamina develop") to use develop version
-- please note that you should add bdslibrary yourself if using dev version
add_requires("levilamina 26.20.*", {configs = {target_type = get_config("target_type")}})

add_requires("levibuildscript")

-- ImGui HUD overlay (client only): Dear ImGui + minhook for the DXGI
-- Present hook that renders the info panel with a CJK-capable font. The
-- vanilla UI text pipeline cannot render Chinese names reliably on this GDK
-- build, so the client overlay draws through ImGui instead.
if (get_config("target_type") or "server") == "client" then
    add_requires("imgui v1.91.9", {configs = {shared = false, win32 = true, dx11 = true, no_demo_windows = true}})
    add_requires("minhook", {configs = {shared = false}})
end

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("Insight")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    if is_plat("windows") then
        add_defines("NOMINMAX", "UNICODE")
        set_exceptions("none") -- To avoid conflicts with /EHa.
        add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
        add_cxflags(
            "/EHs",
            "-Wno-microsoft-cast",
            "-Wno-invalid-offsetof",
            "-Wno-c++2b-extensions",
            "-Wno-microsoft-include",
            "-Wno-overloaded-virtual",
            "-Wno-ignored-qualifiers",
            "-Wno-missing-field-initializers",
            "-Wno-potentially-evaluated-expression",
            "-Wno-pragma-system-header-outside-header",
            {tools = {"clang_cl"}}
        )
        set_toolchains("clang-cl")
    end
    add_packages("levilamina")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_includedirs("src")

    -- Platform selection. Common code lives in src/, platform code in
    -- src-server/ (BDS) or src-client/ (GDK client / LeviLamina client).
    local target_type = get_config("target_type") or "server"
    if target_type == "client" then
        add_defines("INSIGHT_TARGET_CLIENT")
        add_includedirs("src-client")
        add_packages("imgui", "minhook")
        add_syslinks("user32", "d3d11", "d3d12", "dxgi")
        add_headerfiles("src/**.h", "src-client/**.h")
        add_files("src/**.cpp", "src-client/**.cpp")
    else
        add_defines("INSIGHT_TARGET_SERVER")
        add_includedirs("src-server")
        add_headerfiles("src/**.h", "src-server/**.h")
        add_files("src/**.cpp", "src-server/**.cpp")
    end

    -- Messages: lang/<locale>.json is loaded at runtime through
    -- ll::i18n::getInstance().load(getSelf().getLangDir()). The modpacker rule
    -- only copies the dll + manifest, so the folder is copied next to it here.
    after_build(function (target)
        local langdir = path.join(os.projectdir(), "lang")
        if os.isdir(langdir) then
            local outdir = path.join(os.projectdir(), "bin", target:name(), "lang")
            os.mkdir(outdir)
            os.cp(path.join(langdir, "*.json"), outdir)
            cprint("${bright green}[Mod Packer]: ${reset}language files -> " .. outdir)
        end
    end)
