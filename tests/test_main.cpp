#include "test.h"
#include <cstring>

int main (int argc, char** argv)
{
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int ran = 0, bad = 0;
    for (auto& c : spn_test::registry())
    {
        if (filter && ! std::strstr (c.name, filter)) continue;
        const int before = spn_test::failures();
        c.fn();
        const bool ok = spn_test::failures() == before;
        std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", c.name);
        ++ran; if (! ok) ++bad;
    }
    std::printf ("\n%d run, %d failed\n", ran, bad);
    return bad == 0 ? 0 : 1;
}
