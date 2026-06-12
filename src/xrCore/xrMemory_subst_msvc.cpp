#include "stdafx.h"
#pragma hdrstop

#ifndef __BORLANDC__

#ifndef DEBUG_MEMORY_MANAGER
# define debug_mode 0
#endif // DEBUG_MEMORY_MANAGER

#ifdef DEBUG_MEMORY_MANAGER
XRCORE_API void* g_globalCheckAddr = nullptr;
#endif // DEBUG_MEMORY_MANAGER

#ifdef DEBUG_MEMORY_MANAGER
extern void save_stack_trace();
#endif // DEBUG_MEMORY_MANAGER

#define PURE_MEMORY_ALIGNMENT 1 << 4

void* xrMemory::mem_alloc(size_t size
# ifdef DEBUG_MEMORY_NAME
                          , const char* _name
# endif // DEBUG_MEMORY_NAME
)
{
	stat_calls++;

#ifdef DEBUG_MEMORY_MANAGER
    if (mem_initialized) debug_cs.Enter();
    save_stack_trace();
#endif // DEBUG_MEMORY_MANAGER

	void* _ptr = _aligned_malloc(size, PURE_MEMORY_ALIGNMENT);
	if (_ptr)
		memset(_ptr, 0, size);

#ifdef DEBUG_MEMORY_MANAGER
    if (debug_mode) dbg_register(_ptr, size, _name);
    if (mem_initialized) debug_cs.Leave();
#endif // DEBUG_MEMORY_MANAGER
#ifdef USE_MEMORY_MONITOR
    memory_monitor::monitor_alloc(_ptr, size, _name);
#endif // USE_MEMORY_MONITOR

	return _ptr;
}

void xrMemory::mem_free(void* P)
{
	stat_calls++;
#ifdef USE_MEMORY_MONITOR
    memory_monitor::monitor_free(P);
#endif // USE_MEMORY_MONITOR

#ifdef DEBUG_MEMORY_MANAGER
    if (g_globalCheckAddr == P)
        __asm int 3;
    if (mem_initialized) debug_cs.Enter();
#endif // DEBUG_MEMORY_MANAGER
	if (debug_mode) dbg_unregister(P);

	_aligned_free(P);

#ifdef DEBUG_MEMORY_MANAGER
    if (mem_initialized) debug_cs.Leave();
#endif // DEBUG_MEMORY_MANAGER
}

extern BOOL g_bDbgFillMemory;

void* xrMemory::mem_realloc(void* P, size_t size
#ifdef DEBUG_MEMORY_NAME
                            , const char* _name
#endif // DEBUG_MEMORY_NAME
)
{
	stat_calls++;

	if (0 == P)
	{
		return mem_alloc(size
# ifdef DEBUG_MEMORY_NAME
			, _name
# endif // DEBUG_MEMORY_NAME
		);
	}

#ifdef DEBUG_MEMORY_MANAGER
    if (g_globalCheckAddr == P)
        __asm int 3;
    if (mem_initialized) debug_cs.Enter();
    if (debug_mode)
    {
        g_bDbgFillMemory = false;
        dbg_unregister(P);
        g_bDbgFillMemory = true;
    }
#endif // DEBUG_MEMORY_MANAGER

	size_t old_size = _aligned_msize(P, PURE_MEMORY_ALIGNMENT, 0);
	void* _ptr = _aligned_realloc(P, size, PURE_MEMORY_ALIGNMENT);

	if (_ptr && size > old_size)
		memset((u8*)_ptr + old_size, 0, size - old_size);

#ifdef DEBUG_MEMORY_MANAGER
    if (debug_mode) dbg_register(_ptr, size, _name);
    if (mem_initialized) debug_cs.Leave();
    if (g_globalCheckAddr == _ptr)
        __asm int 3;
#endif // DEBUG_MEMORY_MANAGER
#ifdef USE_MEMORY_MONITOR
    memory_monitor::monitor_free(P);
    memory_monitor::monitor_alloc(_ptr, size, _name);
#endif // USE_MEMORY_MONITOR

	return _ptr;
}

#endif // __BORLANDC__
