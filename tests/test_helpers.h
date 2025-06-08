#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H
#include <Core/Core.h>

using namespace Upp;

class TestRunner {
public:
    using TestFunc = void(*)();
    Vector<std::pair<String, TestFunc>> tests;
    static TestRunner& Instance() { static TestRunner r; return r; }
    void Add(const String& name, TestFunc f) { tests.Add(std::make_pair(name, f)); }
    int Run() {
        StdLogSetup(LOG_COUT|LOG_FILE);
        int errors = 0;
        for(auto& t : tests) {
            LOG("Running: " + t.first);
            try {
                t.second();
                LOG("  OK");
            } catch(const Exc& e) {
                LOG("  FAILED: " + String(e));
                errors++;
            } catch(...) {
                LOG("  FAILED: unknown exception");
                errors++;
            }
        }
        return errors;
    }
};

#define TEST(name) \
    static void name(); \
    static struct name##_registrar { name##_registrar() { TestRunner::Instance().Add(#name, &name); } } name##_registrar_instance; \
    static void name()

#define TEST_APP_MAIN \
    CONSOLE_APP_MAIN \
    { \
        int errors = TestRunner::Instance().Run(); \
        if(errors) { \
            LOG(Format("%d tests failed", errors)); \
            SetExitCode(1); \
        } else { \
            LOG("All tests passed"); \
        } \
    }

#endif // TEST_HELPERS_H
