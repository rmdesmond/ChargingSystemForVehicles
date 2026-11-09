#pragma once

// The version string compiled into the daemon, from 'git describe' at build
// time (cmake/GenerateVersion.cmake writes the definition into the build
// directory). Printed by --version and in the first log line at start-up.
const char* chargerVersion();