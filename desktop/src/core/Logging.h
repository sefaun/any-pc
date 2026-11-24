#pragma once

#include <QString>

namespace anypc::Logging {

/**
 * Routes Qt log output (including the embedded WebRTC page's console) to a
 * rotating file in the application data directory, in addition to stderr.
 * Call once, after the application name is final.
 */
void install();

/** Directory containing the log files. */
QString directory();

/** Full path of the current log file. */
QString currentFile();

} // namespace anypc::Logging
