

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wswitch-default"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma clang diagnostic ignored "-Wbad-function-cast"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wfloat-conversion"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wint-conversion"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"
#pragma clang diagnostic ignored "-Wunused-function"
#define NK_INT8 s8
#define NK_UINT8 u8
#define NK_INT16 s16
#define NK_UINT16 u16
#define NK_INT32 s32
#define NK_UINT32 u32
#define NK_SIZE_TYPE u64
#define NK_POINTER_TYPE uintptr

#define _In_
#define _Check_Return

#define NK_IMPLEMENTATION
#define NK_GDI_IMPLEMENTATION
#define NKGDI_IMPLEMENT_WINDOW
#define CALLBACK

#define STBRP_ASSERT assert

static void swap_bytes(char *a, char *b, size_t size)
{
	while (size--)
	{
		char tmp = *a;
		*a++ = *b;
		*b++ = tmp;
	}
}

static void qsort_recursive(char *base,
	size_t n,
	size_t size,
	int (*compar)(const void *, const void *))
{
	if (n < 2)
	return;
	
	size_t pivot_index = n / 2;
	char *pivot = base + pivot_index * size;
	
	swap_bytes(pivot, base + (n - 1) * size, size);
	pivot = base + (n - 1) * size;
	
	size_t store = 0;
	for (size_t i = 0; i < n - 1; ++i)
	{
		char *elem = base + i * size;
		if ((*compar)(elem, pivot) < 0)
		{
			swap_bytes(elem, base + store * size, size);
			++store;
		}
	}
	swap_bytes(base + store * size, pivot, size);
	
	qsort_recursive(base,                store,              size, compar);
	qsort_recursive(base + (store + 1) * size,
		n - store - 1,       size, compar);
}

void qsort(void *base,
	size_t nitems,
	size_t size,
	int (*compar)(const void *, const void *))
{
	if (!base || !compar || size == 0 || nitems < 2)
	return;
	
	qsort_recursive((char *)base, nitems, size, compar);
}
#define STBRP_SORT qsort

#define STBTT_ifloor(x) ((int)floor64((float64)x))
#define STBTT_iceil(x)  ((int) ceil64(x))

#define STBTT_fmod(x, y) fmod_cycling(x, y)

#define STBTT_cos cos
#define STBTT_fabs abs

#define STBTT_malloc(x,u)  ((void)(u),HeapAlloc(GetProcessHeap(), 0, x))
#define STBTT_free(x,u)    ((void)(u),HeapFree(GetProcessHeap(), 0, x))

#define STBTT_assert assert

#define STBTT_strlen c_style_strlen

#define STBTT_memcpy       memcpy
#define STBTT_memset       memset

#define STBTT_assert assert

#pragma push_macro("assert")

#define SIZE_T u64

#define NK_INCLUDE_DEFAULT_ALLOCATOR

#define NULL 0
typedef u16 wchar_t;

#include "Nuklear/nuklear.h"
#include "Nuklear/nuklear_gdi.h"
#include "Nuklear/window.h"

#undef NULL

#pragma pop_macro("assert")

#pragma clang diagnostic pop

