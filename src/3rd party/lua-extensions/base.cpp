#include "../../build_config_defines.h"

#include "luapanda.h"

#ifdef USE_LUAJIT_ONE
#pragma comment(lib, "LuaJIT-1.1.8.lib")
#else
#pragma comment(lib, "lua51.lib")
#endif //-USE_LUAJIT_ONE

#include "lua.hpp"

// #pragma comment(lib, "xrCore.lib")
// #pragma comment(lib, "user32.lib")

extern "C" {
#include "lfs.h"
#include "lmarshal.h"
#include "luasocket/luasocket.h"
#include "luasocket/socket.h"

}

// #include "Libs.h"
#include "script_additional_libs.h"

static const struct luaL_Reg R[] = {
    {NULL, NULL},
};

// extern "C" __declspec(dllexport)
int luaopen_lua_extensions(lua_State *L, bool IsDebug = false) {

  open_additional_libs(L);

  luaopen_lfs(L);
  // open_string(L);
  // open_math(L);
  // open_table(L);
  luaopen_marshal(L);
  // open_kb(L);
  // open_log(L);

  if (IsDebug) {
    luaopen_jit(L);
    luaopen_ffi(L);
    luaopen_debug(L);
  }

  luaL_register(L, "lua_extensions", R);
  return 0;
}

lua_CFunction luaopen_socket_core_init() { return luaopen_socket_core; }

void pdebug_init_init(lua_State *L) { pdebug_init(L); }