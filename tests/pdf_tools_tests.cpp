#include "../src/pdf_tools.h"
#include "../src/editor.h"
#include <QPdfWriter>
#include <cstdlib>
#include <iostream>
static void require(bool ok,const char *message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
static QByteArray bytes(const QString &path){QFile f(path);f.open(QIODevice::ReadOnly);return f.readAll();}
int main(int argc,char **argv){
 QApplication app(argc,argv);QTemporaryDir dir;
 auto a=dir.filePath("a.pdf"),b=dir.filePath("b.pdf"),out=dir.filePath("out.pdf");
 {QPdfWriter w(a);QPainter p(&w);p.drawText(100,100,"ALPHA");w.newPage();p.drawText(100,100,"BETA");}
 {QPdfWriter w(b);QPainter p(&w);p.drawText(100,100,"GAMMA");}
 const auto original=bytes(a);
 QVector<int> pages;QString error;
 require(Ramd::parsePageRange("2, 1-2",2,pages,error)&&pages==QVector<int>({1,0,1}),"range order and repeats");
 for(const auto &range:QStringList{"0","3","2-1","1,","a","1-999999999999"})require(!Ramd::parsePageRange(range,2,pages,error),"invalid range rejected");
 auto merged=Ramd::assembleDocuments({a,b},"",out);require(merged.ok,qPrintable(merged.error));
 {auto pdf=Poppler::Document::load(out);require(pdf&&pdf->numPages()==3,"merge count");require(pdf->page(2)->text(QRectF()).contains("GAMMA"),"merge order");}
 auto extracted=Ramd::assembleDocuments({a},"2,1",out);require(extracted.ok,qPrintable(extracted.error));
 {auto pdf=Poppler::Document::load(out);require(pdf&&pdf->numPages()==2,"extract count");require(pdf->page(0)->text(QRectF()).contains("BETA"),"extract order");}
 const auto saved=bytes(out);
 require(!Ramd::assembleDocuments({a},"3",out).ok && bytes(out)==saved,"invalid extraction preserves destination");
 require(!Ramd::assembleDocuments({a,dir.filePath("missing.pdf")},"",out).ok&&bytes(out)==saved,"missing input preserves destination");
 require(!Ramd::assembleDocuments({a},"1",a).ok&&bytes(a)==original,"source overwrite refused");
 require(bytes(a)==original,"source unchanged");
 std::cout<<"PASS: ranges, merge, extraction, source and destination protection\n";
}
