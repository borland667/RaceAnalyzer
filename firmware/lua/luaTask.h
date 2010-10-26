#ifndef LUATASK_H_
#define LUATASK_H_
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


void lockLua(void);
void unlockLua(void);

void startLuaTask(void);
void luaTask(void *params);

void *myAlloc (void *ud, void *ptr, size_t osize,size_t nsize);

lua_State * getLua(void);

#endif /*LUATASK_H_*/
