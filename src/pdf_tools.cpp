#include "pdf_tools.h"
#include "editor.h"
#include <QRegularExpression>
namespace Ramd {
bool parsePageRange(const QString &range, int count, QVector<int> &pages, QString &error) {
    pages.clear(); error.clear();
    const auto parts = range.split(',', Qt::KeepEmptyParts);
    const QRegularExpression pattern("^\\s*(\\d+)\\s*(?:-\\s*(\\d+)\\s*)?$");
    for (const auto &part : parts) {
        const auto match = pattern.match(part);
        bool firstOk=false, lastOk=false;
        int first=match.captured(1).toInt(&firstOk);
        int last=match.captured(2).isEmpty()?first:match.captured(2).toInt(&lastOk);
        if(match.captured(2).isEmpty()) lastOk=firstOk;
        if(!match.hasMatch() || !firstOk || !lastOk || first<1 || last<first || last>count || pages.size()+last-first+1>5000) {
            pages.clear(); error="Enter pages such as 1-3,5. Use page numbers within the document (up to 5,000 output pages)."; return false;
        }
        for(int page=first;page<=last;++page) pages.append(page-1);
    }
    return !pages.isEmpty();
}
ToolResult assembleDocuments(const QStringList &inputs, const QString &range, const QString &output) {
    if(inputs.isEmpty())return {false,"Choose at least one PDF."};
    if(!range.isEmpty() && inputs.size()!=1)return {false,"Page extraction needs one source PDF."};
    const QFileInfo destination(output);
    const auto destinationPath=destination.exists()?destination.canonicalFilePath():destination.absoluteFilePath();
    QTemporaryDir session;
    if(!session.isValid())return {false,"Cannot create temporary storage."};
    State sheets;
    int serial=0;
    for(const auto &path:inputs) {
        const QFileInfo input(path);
        if(!input.isFile() || !input.isReadable())return {false,"Cannot read: "+input.fileName()};
        if(input.canonicalFilePath()==destinationPath)return {false,"Save to a different file. Source PDFs are kept unchanged."};
        const QString snapshot=session.filePath(QString::number(serial++)+".pdf");
        if(!QFile::copy(input.absoluteFilePath(),snapshot))return {false,"Cannot create a working copy of "+input.fileName()};
        auto document=Poppler::Document::load(snapshot);
        if(!document || document->isLocked() || document->numPages()==0)return {false,"Cannot process "+input.fileName()+". Use an unlocked, readable PDF."};
        QVector<int> selected; QString error;
        if(range.isEmpty()) {
            if(sheets.size()+document->numPages()>5000)return {false,"This operation is limited to 5,000 output pages."};
            for(int i=0;i<document->numPages();++i)selected.append(i);
        } else if(!parsePageRange(range,document->numPages(),selected,error))return {false,error};
        for(int page:selected)sheets.append({snapshot,page,{}});
    }
    QString error;
    if(!exportPdf(sheets,output,error))return {false,error};
    return {true,{}};
}
}
