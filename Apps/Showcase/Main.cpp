#include "ShowcaseApp.h"
#include "Platform/RunLog.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // the Windows entry point of a windowed program (M67)

#include <string>

int main(int /*argc*/, char** /*argv*/)
{
    AtomFramework::RunLog log("Showcase"); // %APPDATA%\AtomEngine\Showcase\logs\Showcase.log

    int exitCode = 0;
    bool startFailed = false;
    {
        Showcase::ShowcaseApp application;
        exitCode = application.Run();
        startFailed = application.StartFailed();
    }
    if (startFailed && log.IsWithoutConsole())
    {
        std::string message = "The AtomEngine Showcase could not start.\n\n" + log.GetLastError();
        if (!log.GetPath().empty())
        {
            message += "\n\nDetails are in the log:\n" + log.GetPath();
        }
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "AtomEngine Showcase", message.c_str(), nullptr);
    }
    return exitCode;
}
