#pragma comment(lib, "Advapi32.lib")
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include "Windows.h"  // IWYU pragma: keep
#include "libhat.hpp" // IWYU pragma: keep
#include "nlohmann/json.hpp"
#include "polyhook2/Detour/x64Detour.hpp"
#include <fstream>  // IWYU pragma: keep
#include <iostream> // IWYU pragma: keep
#include <string>
std::string get_main_exe_filename() {
  char exe_filename[1024]{0};
  GetModuleFileName(NULL, exe_filename, 1024);
  std::string res = exe_filename;
  return res.substr(res.find_last_of("\\") + 1);
}
std::string get_cwd() {
  char exe_filename[1024]{0};
  GetModuleFileName(NULL, exe_filename, 1024);
  std::string res = exe_filename;
  return res.substr(0, res.find_last_of("\\"));
}
std::string get_path() {

  if (get_main_exe_filename() == "Minecraft.Windows.exe") {
    char username[100]{0};
    DWORD len = 100;
    GetUserName(username, &len);
    std::string name = username;
    return R"(C:\Users\)" + name +
           R"(\AppData\Local\Packages\Microsoft.MinecraftUWP_8wekyb3d8bbwe\LocalState\games\com.mojang)";
  } else {
    return get_cwd() + R"(\plugins\dimhook)";
  }
}
nlohmann::json config = nlohmann::json::parse(R"({
  "Overworld": {
    "max": 512,
    "min": -64
  },
  "Nether": {
    "max": 512,
    "min": 0
  },
  "TheEnd": {
    "max": 512,
    "min": 0
  }
})");
void load_config() {
  std::filesystem::create_directories(get_path());
  if (std::filesystem::exists(get_path() + R"(\config.json)")) {
    config.merge_patch(nlohmann::json::parse(
        std::fstream(get_path() + R"(\config.json)", std::ios::in), nullptr,
        true, true));
  } else {
    std::fstream(get_path() + R"(\config.json)", std::ios::out) << config;
  }
}
struct height_range_t {
  short min, max;
};
uint64_t tramp;
PLH::x64Detour *detour;
uint64_t (*dim_ctor)(void *_this, void *a, int i, height_range_t r, void *b,
                     std::string n);
uint64_t dim_ctor_hook(void *_this, void *a, int i, height_range_t r, void *b,
                       std::string n) {
  detour->unHook();
  try {
    r.max = config[n]["max"].get<int>();
    r.min = config[n]["min"].get<int>();
  } catch (...) {
    std::cerr << "Error at dimension name:" << n << "\n";
  }
  auto res = dim_ctor(_this, a, i, r, b, n);
  detour->hook();
  return res;
}
void init() {
  load_config();
  auto minecraft_module = hat::process::get_module(get_main_exe_filename());
  if (get_main_exe_filename() == "Minecraft.Windows.exe") {
    auto address = hat::find_pattern(
        minecraft_module->get_module_data().begin(),
        minecraft_module->get_module_data().end(),
        hat::compile_signature<
            "48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 55 41 54 41 55 41 56 41 "
            "57 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 "
            "45 ? 41 8B D9 41 8B F8">());
    dim_ctor = reinterpret_cast<decltype(dim_ctor)>(address.get());
  } else {
    auto address = hat::find_pattern(
        minecraft_module->get_module_data().begin(),
        minecraft_module->get_module_data().end(),
        hat::compile_signature<
            "48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 55 41 54 41 55 41 56 41 "
            "57 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 "
            "45 ? 41 8B D9 41 8B F8">());
    dim_ctor = reinterpret_cast<decltype(dim_ctor)>(address.get());
  }
  detour =
      new PLH::x64Detour(reinterpret_cast<uint64_t>(dim_ctor),
                         reinterpret_cast<uint64_t>(dim_ctor_hook), &tramp);
  detour->hook();
}

void init_packet_hook();
void init_console() {
  if (get_main_exe_filename() == "Minecraft.Windows.exe") {
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    freopen("CONIN$", "r", stdin);
  }
  system("chcp 65001");
  std::cout << "dimhook loaded\n";
}
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  switch (fdwReason) {
  case DLL_PROCESS_ATTACH:
    init_console();
    init();
    init_packet_hook();
    break;
  case DLL_THREAD_ATTACH:
    break;

  case DLL_THREAD_DETACH:
    break;

  case DLL_PROCESS_DETACH:
    if (detour->isHooked()) {
      detour->unHook();
    }
    delete detour;
    if (lpvReserved != nullptr) {
      break;
    }

    break;
  }
  return TRUE;
}
class pkt_t {
public:
  virtual ~pkt_t() = 0;                       // 0
  virtual int get_id() = 0;                   // 1
  virtual std::string get_name() = 0;         // 2
  virtual void unk1() = 0;                    // 3
  virtual void unk2() = 0;                    // 4
  virtual void unk3() = 0;                    // 5
  virtual bool disallow_batching() const = 0; // 6
  virtual bool is_valid() const = 0;          // 7
  virtual void unk4() = 0;                    // 8
  char filler[0x28];
};
class dim_def_pkt_t : public pkt_t {
public:
  virtual ~dim_def_pkt_t() = 0;

  struct def_t {
    int min;
    int max;
    int type;
  };

  std::map<std::string, def_t> defs;
};
std::mutex mutex;
PLH::x64Detour *net_send_detour;
void (*net_send)(void *, void *id, pkt_t *pkt, unsigned char sub);
std::shared_ptr<pkt_t> (*make_pkt)(int);
void net_send_hook(void *_this, void *id, pkt_t *pkt, unsigned char sub) {
  mutex.lock();
  net_send_detour->unHook();
  std::cout << pkt->get_name() << std::endl;
  if (pkt->get_name() == "StartGamePacket") {
    auto dim_pkt = make_pkt(180);
    auto dim_raw = (dim_def_pkt_t *)dim_pkt.get();
    dim_raw->defs["minecraft:overworld"].max =
        std::min(config["Overworld"]["max"].get<int>(), 512);
    dim_raw->defs["minecraft:overworld"].min =
        std::max(config["Overworld"]["min"].get<int>(), -512);
    dim_raw->defs["minecraft:overworld"].type = 5;
    dim_raw->defs["minecraft:nether"].max = config["Nether"]["max"].get<int>();
    dim_raw->defs["minecraft:nether"].min = config["Nether"]["min"].get<int>();
    dim_raw->defs["minecraft:the_end"].max = config["TheEnd"]["max"].get<int>();
    dim_raw->defs["minecraft:the_end"].min = config["TheEnd"]["min"].get<int>();
    net_send(_this, id, dim_pkt.get(), sub);
    net_send(_this, id, pkt, sub);
  } else {
    net_send(_this, id, pkt, sub);
  }
  net_send_detour->hook();
  mutex.unlock();
}
uint64_t net_send_tramp;
void init_packet_hook() {
  auto minecraft_module = hat::process::get_module(get_main_exe_filename());
  if (get_main_exe_filename() == "Minecraft.Windows.exe") {
    auto address = hat::find_pattern(
        minecraft_module->get_module_data().begin(),
        minecraft_module->get_module_data().end(),
        hat::compile_signature<
            "40 53 55 56 57 41 54 41 56 41 57 48 81 EC ? ? ? ? 48 8B 05 ? ? ? "
            "? 48 33 C4 48 89 84 24 ? ? ? ? 45 0F B6 E1 4D 8B F8">());
    net_send = reinterpret_cast<decltype(net_send)>(address.get());
  } else {
    auto address = hat::find_pattern(
        minecraft_module->get_module_data().begin(),
        minecraft_module->get_module_data().end(),
        hat::compile_signature<
            "40 53 55 56 57 41 54 41 56 41 57 48 81 EC ? ? ? ? 48 8B 05 ? ? ? "
            "? 48 33 C4 48 89 84 24 ? ? ? ? 45 0F B6 E1 4D 8B F8">());
    net_send = reinterpret_cast<decltype(net_send)>(address.get());
  }
  if (get_main_exe_filename() == "Minecraft.Windows.exe") {
    auto address = hat::find_pattern(
        minecraft_module->get_module_data().begin(),
        minecraft_module->get_module_data().end(),
        hat::compile_signature<
            "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B 05 ? ? ? ? 48 33 "
            "C4 48 89 44 24 ? 48 8B D9 48 89 4C 24 ? 33 F6">());
    make_pkt = reinterpret_cast<decltype(make_pkt)>(address.get());
  } else {
    auto address = hat::find_pattern(
        minecraft_module->get_module_data().begin(),
        minecraft_module->get_module_data().end(),
        hat::compile_signature<
            "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B 05 ? ? ? ? 48 33 "
            "C4 48 89 44 24 ? 48 8B D9 48 89 4C 24 ? 33 F6">());
    make_pkt = reinterpret_cast<decltype(make_pkt)>(address.get());
  }
  net_send_detour = new PLH::x64Detour(
      reinterpret_cast<uint64_t>(net_send),
      reinterpret_cast<uint64_t>(net_send_hook), &net_send_tramp);
  net_send_detour->hook();
}
#if __has_include("endstone/plugin/plugin.h")
#include "endstone/plugin/plugin.h"
class Entry : public endstone::Plugin {
public:
  void onLoad() override {
    static bool loaded = false;
    if (loaded) {
      getLogger().critical("dimhook cannot be reloaded");
      throw std::runtime_error("dimhook cannot be reloaded");
    }
    getLogger().info("dimhook by killcerr loaded");
    loaded = true;
  }

  void onEnable() override {}

  void onDisable() override {}

  endstone::PluginDescription const &getDescription() const override {
    return description_;
  }

private:
  endstone::detail::PluginDescriptionBuilder builder{
      .description = "dimhook",
      .load = endstone::PluginLoadOrder::Startup,
      .authors = {"killcerr"},
  };
  endstone::PluginDescription description_ = builder.build("dimhook", "0.0.1");
};
extern "C" [[maybe_unused]] __declspec(dllexport) endstone::Plugin *
init_endstone_plugin() {
  static Entry *ins;
  ins = new Entry{};
  return ins;
}
#endif

#ifdef HIJACK
HMODULE load_winhttp() {
  static auto dll = LoadLibraryA(R"(C:\Windows\System32\winhttp.dll)");
  return dll;
}
HANDLE load_function(LPCSTR symbol) {
  return reinterpret_cast<HANDLE>(GetProcAddress(load_winhttp(), symbol));
}
#define FUNC(F) extern "C" __declspec(dllexport) auto F = load_function(#F);
FUNC(WinHttpCloseHandle)
FUNC(WinHttpSetOption)
FUNC(WinHttpGetIEProxyConfigForCurrentUser)
FUNC(WinHttpSetStatusCallback)
FUNC(WinHttpConnect)
FUNC(WinHttpReadData)
FUNC(WinHttpQueryDataAvailable)
FUNC(WinHttpOpen)
FUNC(WinHttpSetTimeouts)
FUNC(WinHttpOpenRequest)
FUNC(WinHttpAddRequestHeaders)
FUNC(WinHttpSendRequest)
FUNC(WinHttpReceiveResponse)
FUNC(WinHttpQueryHeaders)
FUNC(WinHttpGetProxyForUrl)
FUNC(WinHttpWriteData)
FUNC(WinHttpQueryOption)
#endif
