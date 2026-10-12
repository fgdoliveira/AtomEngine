#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <string_view>

// v0.0.16 (M103): test cases in the "benchmark" suite time something on
// this machine - opt-in, never part of a normal run: `AtomTests
// -ts=benchmark` runs them. Any other run leaves them out.
int main(int argc, char** argv)
{
    doctest::Context context;
    bool suiteChosen = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        suiteChosen = suiteChosen || arg.starts_with("-ts") || arg.starts_with("--test-suite");
    }
    if (!suiteChosen)
    {
        context.addFilter("test-suite-exclude", "benchmark");
    }
    context.applyCommandLine(argc, argv);
    return context.run();
}
