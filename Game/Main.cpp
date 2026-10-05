#include "DemoApp.h"

#include <string>
#include <vector>

int main(int argc, char** argv)
{
    // The arguments after the program name (M60): --gpu, --quality, ...
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i)
    {
        arguments.emplace_back(argv[i]);
    }

    AtomGame::DemoApp application(std::move(arguments));

    return application.Run();
}
