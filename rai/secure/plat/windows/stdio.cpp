#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#include <cstdio>
#include <cstdarg>
#include <cwchar>

FILE _iob[] = {*stdin, *stdout, *stderr};

extern "C" FILE * __cdecl __iob_func(void)
{
	return _iob;
}

extern "C" int (__cdecl * __vsnwprintf)(wchar_t *, size_t, const wchar_t *, va_list) = _vsnwprintf;
#endif // _MSC_VER
