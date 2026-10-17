#include "editor.h"
#include <QPdfWriter>
#include <iostream>
#include <cstdlib>
static void require(bool pass,const char *message){if(!pass){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QTemporaryDir dir;require(dir.isValid(),"temporary directory");
    QString source=dir.filePath("original.pdf"), second=dir.filePath("second.pdf");
    {
        QPdfWriter w(source);w.setResolution(72);QPainter p(&w);
        p.drawText(100,100,"ORIGINAL_ALPHA");w.newPage();p.drawText(100,100,"ORIGINAL_BETA");
    }
    {QPdfWriter w(second);w.setResolution(72);QPainter p(&w);p.drawText(100,100,"MERGED_GAMMA");}
    Ramd::Mark text;text.kind=Ramd::Mark::Text;text.rect=QRectF(.1,.3,.6,.1);text.text="Added offline";
    Ramd::Mark highlight;highlight.kind=Ramd::Mark::Highlight;highlight.rect=QRectF(.1,.15,.4,.04);
    Ramd::Mark ink;ink.kind=Ramd::Mark::Ink;ink.rect=QRectF(.1,.5,.3,.1);ink.points={{.1,.5},{.2,.6},{.4,.5}};ink.size=2;
    Ramd::State state{{source,1,{text,highlight,ink}},{second,0,{}},{source,0,{}}};
    QString error,target=dir.filePath("output.pdf");
    require(Ramd::exportPdf(state,target,error),qPrintable(error));
    auto result=Poppler::Document::load(target);require(result && result->numPages()==3,"merged page count");
    require(result->page(0)->text(QRectF()).contains("ORIGINAL_BETA"),"page reorder preserves source text");
    require(result->page(1)->text(QRectF()).contains("MERGED_GAMMA"),"merged file text preserved");
    require(result->page(2)->text(QRectF()).contains("ORIGINAL_ALPHA"),"last page text preserved");
    auto anns=result->page(0)->annotations();require(anns.size()==3,"three saved native annotations");
    bool found=false;for(const auto &a:anns)if(a->contents()=="Added offline")found=true;require(found,"annotation text roundtrip");
    require(!result->page(0)->renderToImage().isNull(),"saved page renders");
    QFile original(source);require(original.open(QIODevice::ReadOnly),"source readable");auto originalBytes=original.readAll();original.close();
    require(!Ramd::exportPdf({},source,error),"empty output rejected");
    require(original.open(QIODevice::ReadOnly) && original.readAll()==originalBytes,"failed save leaves original intact");original.close();
    require(!Ramd::exportPdf(state,dir.filePath("missing/no.pdf"),error),"invalid destination rejected");
    require(Ramd::exportPdf(state,source,error),"safe overwrite from immutable source snapshots");
    auto replaced=Poppler::Document::load(source);require(replaced && replaced->numPages()==3,"overwrite readable");
    require(Ramd::exportPdf({state[1]},dir.filePath("extract.pdf"),error),"extract page");
    {
        Ramd::Editor editor(target);require(editor.ready,"editor opens real PDF");
        editor.show();app.processEvents();
        Ramd::Canvas *canvas=nullptr;
        for(auto widget:editor.findChildren<QWidget*>())if(auto c=dynamic_cast<Ramd::Canvas*>(widget))canvas=c;
        require(canvas!=nullptr,"editor canvas exists");
        canvas->added(text);require(canvas->marks.size()==1,"UI annotation addition");
        auto stack=editor.findChild<QUndoStack*>();require(stack!=nullptr,"undo stack exists");
        stack->undo();require(canvas->marks.isEmpty(),"UI undo");
        stack->redo();require(canvas->marks.size()==1,"UI redo");
        if(argc>1)editor.grab().save(QString::fromLocal8Bit(argv[1]));
        stack->undo();editor.close();
    }
    std::cout<<"PASS: text/ink/highlight save, merge, reorder, extract, overwrite, failure protection\n";
}

