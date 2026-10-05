#include "DemoApp.h"
#include "Platform/RunLog.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // the Windows entry point of a windowed program (M67)

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    // First: where this run's output goes (the terminal, a pipe, the log).
    AtomGame::RunLog log;
    const char* basePath = SDL_GetBasePath();
    std::cout << "Executable folder: " << (basePath ? basePath : "unknown") << '\n'
              << "Log: " << (log.GetPath().empty() ? std::string("none (scripted run)") : log.GetPath()) << '\n';

    // The arguments after the program name (M60): --gpu, --quality, ...
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i)
    {
        arguments.emplace_back(argv[i]);
    }

    int exitCode = 0;
    bool startFailed = false;
    {
        AtomGame::DemoApp application(std::move(arguments));
        exitCode = application.Run();
        startFailed = application.StartFailed();
    }

    // Double-clicked and nothing to show for it: say why, and where to look.
    if (startFailed && log.IsWithoutConsole())
    {
        std::string message = "AtomGame could not start.\n\n";
        const std::string reason = log.GetLastError();
        message += reason.empty() ? std::string("No reason was reported.") : reason;
        if (!log.GetPath().empty())
        {
            message += "\n\nDetails are in the log:\n" + log.GetPath();
        }
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "AtomGame", message.c_str(), nullptr);
    }
    return exitCode;
}
