#ifndef COMPILER_HACKS
#define COMPILER_HACKS

#ifdef __MWERKS__
	#define _INLINE_
	#define _UNUSED_
#else
	#define _INLINE_   	inline __attribute__ ((always_inline))
	#define _UNUSED_	__attribute__((unused))
#endif

#endif