#include "Engine.h"

using namespace rosy;

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    rosy::engine engine{};
    if (const result res = engine.init(); res != result::ok)
    {
        engine.deinit();
        return 1;
    }
    if (const result res = engine.run(); res != result::ok)
    {
        engine.deinit();
        return 1;
    }
    engine.deinit();
    return 0;
}
