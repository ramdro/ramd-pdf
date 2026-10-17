#define RAMD_NO_MAIN
#include "../src/main.cpp"
#include <cstdlib>
#include <iostream>
static void require(bool ok,const char *message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
static void settle(){for(int i=0;i<30;++i){QApplication::processEvents();QThread::msleep(10);}}
int main(int argc,char **argv){
 QApplication app(argc,argv);QTemporaryDir dir;
 QCoreApplication::setOrganizationName("RAMD-Test");QCoreApplication::setApplicationName("Reader");
 QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
 QStandardPaths::setTestModeEnabled(true);
 const auto pdf=dir.filePath("Studio guide.pdf");
 {QPdfWriter w(pdf);QPainter p(&w);p.drawText(100,100,"RAMD Studio guide");w.newPage();p.drawText(100,100,"Page two");w.newPage();p.drawText(100,100,"Page three");}
 const auto key="documents/"+QString::fromLatin1(QCryptographicHash::hash(pdf.toUtf8(),QCryptographicHash::Sha256).toHex());
 QSettings().setValue(key+"/bookmarks",QVariantMap{{"1","Review this page"}});
 QString previews=argc>1?QString::fromLocal8Bit(argv[1]):dir.filePath("previews");QDir().mkpath(previews);
 {
 Reader reader;reader.show();reader.load(pdf);settle();
 auto spin=reader.findChild<QSpinBox*>("pageNumber");require(spin&&spin->maximum()==3,"page controls");
 require(reader.findChild<QListWidget*>("savedPages")->count()==1,"saved bookmark visible");
 spin->setValue(2);settle();reader.grab().save(previews+"/reader-light.png");reader.showHome();settle();reader.grab().save(previews+"/workspace-light.png");reader.close();
 }
 require(QSettings().value(key+"/page").toInt()==1,"position persisted");
 QSettings().setValue("dark",true);
 {
 Reader reader;reader.show();reader.load(pdf);settle();
 require(reader.findChild<QSpinBox*>("pageNumber")->value()==2,"position restored");
 reader.grab().save(previews+"/reader-dark.png");reader.showHome();settle();reader.grab().save(previews+"/workspace-dark.png");reader.close();
 }
 QThreadPool::globalInstance()->waitForDone();
 std::cout<<"PASS: document controls, bookmarks, reading position and both themes\n";
}
