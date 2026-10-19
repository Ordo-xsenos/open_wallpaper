#pragma once
#include <QString>
#include <QStringList>

// Runtime diagnostics, independent of Debug/Release compilation.
namespace DebugLog {
bool initialize(const QStringList& arguments);
bool enabled();
QString directory();
void write(const QString& message);
} // namespace DebugLog
