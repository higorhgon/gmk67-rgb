#pragma once

#include <QStringList>

// Commands that need no window, so scripts and agents can drive the
// keyboard: list, status, set, animate. Returns -1 when args ask for the
// window instead (no command, or "open").
namespace Cli {
int run(const QStringList &args);
bool wantsWindow(const QStringList &args);
}
