// Minimal test harness: no dependencies. TEST("name") { CHECK(cond); CHECK_NEAR(a, b, tol); }
#pragma once
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace spn_test
{
struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
struct Registrar { Registrar (const char* n, std::function<void()> f) { registry().push_back ({ n, std::move (f) }); } };
}

#define SPN_CAT2(a, b) a##b
#define SPN_CAT(a, b) SPN_CAT2(a, b)
#define TEST(name) \
    static void SPN_CAT(spn_test_fn_, __LINE__)(); \
    static spn_test::Registrar SPN_CAT(spn_test_reg_, __LINE__)(name, SPN_CAT(spn_test_fn_, __LINE__)); \
    static void SPN_CAT(spn_test_fn_, __LINE__)()
#define CHECK(cond) \
    do { if (!(cond)) { std::printf ("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++spn_test::failures(); } } while (0)
#define CHECK_NEAR(a, b, tol) \
    do { double va_ = (a), vb_ = (b); if (!(std::fabs (va_ - vb_) <= (tol))) { \
        std::printf ("    FAIL %s:%d  %s ~ %s  (%g vs %g, tol %g)\n", __FILE__, __LINE__, #a, #b, va_, vb_, (double) (tol)); \
        ++spn_test::failures(); } } while (0)
