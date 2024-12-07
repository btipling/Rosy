#include <Windows.h>

namespace rosy_config {
	void debug() {
		OutputDebugStringW(L"Rosy started\n");
	}
}