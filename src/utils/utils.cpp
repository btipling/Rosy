// ReSharper disable CppClangTidyMiscUseInternalLinkage
#include <strsafe.h>
#include <Windows.h>

namespace rosy_utils {
    void debug_print_w(const wchar_t* format, ...)
    {
        WCHAR buffer[4096];
        va_list args;
        va_start(args, format);

        int _ = StringCbVPrintfW(buffer, sizeof(buffer), format, args);
        OutputDebugStringW(buffer);

        va_end(args);
    }


    void debug_print_a(const char* format, ...)
    {
        CHAR buffer[4096];
        va_list args;
        va_start(args, format);

        int _ = StringCbVPrintfA(buffer, sizeof(buffer), format, args);
        OutputDebugStringA(buffer);

        va_end(args);
    }
}