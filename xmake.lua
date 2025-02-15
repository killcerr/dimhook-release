add_rules("mode.debug", "mode.release")
add_requires("polyhook2", "libhat 5cf79adf86152233371b43adab5a0f9db6daa4e7", "nlohmann_json")
add_repositories("iceblcokmc https://github.com/IceBlcokMC/xmake-repo.git")
add_requires("endstone 0.5.7.1")
add_requires(
    "entt 3.14.0",
    "microsoft-gsl 4.0.0",
    "nlohmann_json 3.11.3",
    "boost 1.85.0",
    "glm 1.0.1",
    "concurrentqueue 1.0.4",
    "magic_enum 0.9.7",
    "expected-lite 0.8.0"
)
add_requires("fmt >=10.0.0 <11.0.0")
option("hijack", {default = false}) -- enable dll hijack to inject BDS
target("dimhook")
    set_options("type")
    set_languages("c++20")
    set_kind("shared")
    add_packages("polyhook2", "libhat", "nlohmann_json")
    add_files("*.cpp")
    set_symbols("debug")
    add_packages(
    "fmt",
    "expected-lite",
    "entt",
    "microsoft-gsl",
    "nlohmann_json",
    "boost",
    "glm",
    "concurrentqueue",
    "endstone",
    "magic_enum"
    )
    add_defines("ENTT_SPARSE_PAGE=2048")
    if has_config("hijack") then
        add_defines("HIJACK")
    end
