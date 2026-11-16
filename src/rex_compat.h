// WWE13 on the 2K14 SDK (ReXGlue v0.10): the old v0.7 names our game hooks use.
// v0.10 renamed PPC_* to REX_* and moved the guest memory macros into the generated header.
#pragma once

#include "generated/default/wwe13_pch.h"

#ifndef PPC_FUNC
#define PPC_FUNC(x) REX_FUNC(x)
#endif
#define PPC_RAW_ADDR(x) REX_RAW_ADDR(x)
#define PPC_PHYS_HOST_OFFSET(x) REX_PHYS_HOST_OFFSET(x)
#define PPC_LOAD_U8(x) REX_LOAD_U8(x)
#define PPC_LOAD_U16(x) REX_LOAD_U16(x)
#define PPC_LOAD_U32(x) REX_LOAD_U32(x)
#define PPC_LOAD_U64(x) REX_LOAD_U64(x)
#define PPC_LOAD_STRING(x, len) REX_LOAD_STRING(x, len)
#define PPC_STORE_U8(x, y) REX_STORE_U8(x, y)
#define PPC_STORE_U16(x, y) REX_STORE_U16(x, y)
#define PPC_STORE_U32(x, y) REX_STORE_U32(x, y)
#define PPC_STORE_U64(x, y) REX_STORE_U64(x, y)
#define PPC_MM_LOAD_U8(x) REX_MM_LOAD_U8(x)
#define PPC_MM_LOAD_U16(x) REX_MM_LOAD_U16(x)
#define PPC_MM_LOAD_U32(x) REX_MM_LOAD_U32(x)
#define PPC_MM_LOAD_U64(x) REX_MM_LOAD_U64(x)
#define PPC_MM_STORE_U8(x, y) REX_MM_STORE_U8(x, y)
#define PPC_MM_STORE_U16(x, y) REX_MM_STORE_U16(x, y)
#define PPC_MM_STORE_U32(x, y) REX_MM_STORE_U32(x, y)
#define PPC_MM_STORE_U64(x, y) REX_MM_STORE_U64(x, y)
#define PPC_IS_MMIO_ADDR(x) REX_IS_MMIO_ADDR(x)
#define PPC_MEMORY_SIZE REX_MEMORY_SIZE
#define PPC_QUERY_TIMEBASE REX_QUERY_TIMEBASE
#ifdef REX_CALL_FUNC
#define PPC_CALL_FUNC REX_CALL_FUNC
#endif
