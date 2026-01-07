#pragma once

#include <string>
#include <cstdlib>

namespace gird {

// Get the appropriate config directory based on platform
inline std::string GetConfigDir()
{
#ifdef _WIN32
    // Windows: %APPDATA%\gird
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        return std::string(appData) + "\\gird";
    }
    return ".";

#elif __APPLE__
    // macOS: ~/Library/Application Support/myapp
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/Library/Application Support/gird";
    }
    return ".";

#else
    // Linux/Unix: ~/.config/myapp (XDG spec)
    const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfigHome) {
        return std::string(xdgConfigHome) + "/gird";
    }

    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.config/gird";
    }
    return ".";
#endif
}

} // namespace gird
