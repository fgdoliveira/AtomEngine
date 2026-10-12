#include "DriftApp.h"
#include "Platform/RunLog.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // the Windows entry point of a windowed program (M67)

#include <string>
#include <vector>

int main(int argc, char** argv)
{
    AtomFramework::RunLog log("Drift"); // %APPDATA%\AtomEngine\Drift\logs\Drift.log

    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i)
    {
        arguments.emplace_back(argv[i]);
    }

    int exitCode = 0;
    bool startFailed = false;
    {
        Drift::DriftApp application(std::move(arguments));
        exitCode = application.Run();
        startFailed = application.StartFailed();
    }
    if (startFailed && log.IsWithoutConsole())
    {
        std::string message = "DRIFT could not start.\n\n" + log.GetLastError();
        if (!log.GetPath().empty())
        {
            message += "\n\nDetails are in the log:\n" + log.GetPath();
        }
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DRIFT", message.c_str(), nullptr);
    }
    return exitCode;
}
