#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
namespace Ramd {
struct ToolResult { bool ok = false; QString error; };
// Page numbers are 1-based in the user-facing range; returned indices are 0-based.
bool parsePageRange(const QString &range, int pageCount, QVector<int> &pages, QString &error);
// Input files are copied into a private session before assembly. Output is atomic.
ToolResult assembleDocuments(const QStringList &inputs, const QString &range, const QString &output);
}
