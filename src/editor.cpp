#include "editor.h"
#include <algorithm>
#include <cmath>

namespace Ramd {
bool exportPdf(const State &state, const QString &target, QString &error) {
    if(state.isEmpty()) { error="A PDF must contain at least one page."; return false; }
    QTemporaryDir work;
    if(!work.isValid()) { error="Cannot create temporary storage."; return false; }
    const QString merged=work.filePath("pages.pdf"), output=work.filePath("edited.pdf");
    QStringList args{"--empty", "--pages"};
    for(const auto &s:state) args << s.source << QString::number(s.page+1);
    args << "--" << merged;
    QProcess process;
    QString qpdf = QStandardPaths::findExecutable("qpdf");
#ifdef Q_OS_WIN
    const QString bundled = QCoreApplication::applicationDirPath() + "/qpdf.exe";
    if (QFileInfo::exists(bundled)) qpdf = bundled;
#endif
    if (qpdf.isEmpty()) { error="qpdf was not found. Reinstall RAMD PDF or install qpdf."; return false; }
    process.start(qpdf,args);
    if(!process.waitForStarted(5000)) { error="qpdf is missing. Install it using your distribution's package manager."; return false; }
    if(!process.waitForFinished(120000)) { process.kill(); process.waitForFinished(); error="Page assembly timed out. Original files are unchanged."; return false; }
    if(process.exitStatus()!=QProcess::NormalExit || (process.exitCode()!=0 && process.exitCode()!=3)) {
        error=QString::fromLocal8Bit(process.readAllStandardError()); return false;
    }
    auto doc=Poppler::Document::load(merged);
    if(!doc || doc->isLocked() || doc->numPages()!=state.size()) { error="Could not verify assembled PDF."; return false; }
    for(int i=0;i<state.size();++i) {
        auto page=doc->page(i);
        if(!page) {error="Cannot access output page.";return false;}
        for(const auto &m:state[i].marks) {
            std::unique_ptr<Poppler::Annotation> ann;
            if(m.kind==Mark::Text) {
                auto a=std::make_unique<Poppler::TextAnnotation>(Poppler::TextAnnotation::InPlace);
                a->setContents(m.text);
                a->setTextFont(QFont("Helvetica",qRound(m.size)));
                a->setTextColor(m.color);
                ann=std::move(a);
            } else if(m.kind==Mark::Highlight) {
                auto a=std::make_unique<Poppler::HighlightAnnotation>();
                Poppler::HighlightAnnotation::Quad q{};
                q.points[0]=m.rect.topLeft();q.points[1]=m.rect.topRight();
                q.points[2]=m.rect.bottomRight();q.points[3]=m.rect.bottomLeft();
                q.capStart=false;q.capEnd=false;q.feather=0.1;
                a->setHighlightQuads({q});ann=std::move(a);
            } else {
                auto a=std::make_unique<Poppler::InkAnnotation>();
                a->setInkPaths({m.points});ann=std::move(a);
            }
            ann->setBoundary(m.rect);
            ann->setAuthor("RAMD PDF");
            ann->setFlags(Poppler::Annotation::Flags{});
            Poppler::Annotation::Style style;
            style.setColor(m.kind==Mark::Highlight ? QColor("#ffdf43") : m.kind==Mark::Text ? QColor() : m.color);
            style.setWidth(m.kind==Mark::Ink ? m.size : 0);
            style.setOpacity(m.kind==Mark::Highlight ? 0.4 : 1.0);
            ann->setStyle(style);
            page->addAnnotation(ann.get());
        }
    }
    auto converter=doc->pdfConverter();
    converter->setOutputFileName(output);
    converter->setPDFOptions(Poppler::PDFConverter::WithChanges);
    if(!converter->convert()) { error="PDF conversion failed. Your existing file was not changed."; return false; }
    auto check=Poppler::Document::load(output);
    if(!check || check->numPages()!=state.size()) { error="Output validation failed."; return false; }
    QFile in(output);
    QSaveFile out(target);
    out.setDirectWriteFallback(false);
    if(!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly)) { error="Cannot write destination: "+out.errorString();return false; }
    while(!in.atEnd()) {
        const QByteArray bytes=in.read(1024*1024);
        if(bytes.isEmpty() && in.error()!=QFile::NoError) {error=in.errorString();return false;}
        if(out.write(bytes)!=bytes.size()) {error=out.errorString();return false;}
    }
    if(!out.commit()) {error=out.errorString();return false;}
    return true;
}

Canvas::Canvas(QWidget *p):QWidget(p) {setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);}
QPointF Canvas::normalized(QPointF p) const {
    return {std::clamp(p.x()/width(),0.0,1.0),std::clamp(p.y()/height(),0.0,1.0)};
}
void Canvas::setPage(QImage img,QSizeF pts,const QVector<Mark> &items) {
    image=std::move(img);points=pts;marks=items;selected=-1;drawing=false;
    setFixedSize(image.size());update();
}
void Canvas::paintMark(QPainter &p,const Mark &m) {
    QRectF r(m.rect.x()*width(),m.rect.y()*height(),m.rect.width()*width(),m.rect.height()*height());
    const double scale=width()/std::max(1.0,points.width());
    if(m.kind==Mark::Highlight) p.fillRect(r,QColor(255,223,67,102));
    else if(m.kind==Mark::Text) {
        QFont font("Helvetica");font.setPixelSize(std::max(1,qRound(m.size*scale)));
        p.setFont(font);p.setPen(m.color);p.drawText(r,Qt::AlignLeft|Qt::AlignTop|Qt::TextWordWrap,m.text);
    } else {
        p.setPen(QPen(m.color,m.size*scale,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        QPainterPath path;
        for(int i=0;i<m.points.size();++i) {QPointF pt(m.points[i].x()*width(),m.points[i].y()*height());if(i==0)path.moveTo(pt);else path.lineTo(pt);}
        p.drawPath(path);
    }
}
void Canvas::paintEvent(QPaintEvent *) {
    QPainter p(this);p.fillRect(rect(),Qt::white);p.drawImage(rect(),image);p.setRenderHint(QPainter::Antialiasing);
    for(const auto &m:marks)paintMark(p,m);
    if(drawing) {
        paintMark(p,draft);
        if(draft.kind==Mark::Text) {
            p.setPen(QPen(QColor("#3478f6"),1,Qt::DashLine));p.setBrush(Qt::NoBrush);
            auto r=draft.rect;p.drawRect(QRectF(r.x()*width(),r.y()*height(),r.width()*width(),r.height()*height()));
        }
    }
    if(selected>=0 && selected<marks.size()) {
        auto r=marks[selected].rect;p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor("#3478f6"),2,Qt::DashLine));
        p.drawRect(QRectF(r.x()*width(),r.y()*height(),r.width()*width(),r.height()*height()));
    }
}
void Canvas::mousePressEvent(QMouseEvent *e) {
    if(e->button()!=Qt::LeftButton)return;
    setFocus();start=normalized(e->position());
    if(tool==0) {
        selected=-1;for(int i=marks.size()-1;i>=0;--i)if(marks[i].rect.adjusted(-.005,-.005,.005,.005).contains(start)){selected=i;break;}
        update();return;
    }
    draft=Mark{};draft.kind=tool==1?Mark::Text:tool==2?Mark::Highlight:Mark::Ink;
    draft.color=tool==4?QColor("#173985"):ink;draft.size=tool==1?fontSize:tool==4?1.5:2.0;
    draft.points={start};draft.rect=QRectF(start,start);drawing=true;
}
void Canvas::mouseMoveEvent(QMouseEvent *e) {
    if(!drawing)return;auto pt=normalized(e->position());
    if(draft.kind==Mark::Ink) {
        draft.points.append(pt);
        double x=pt.x(),y=pt.y(),r=x,b=y;
        for(auto v:draft.points){x=std::min(x,v.x());y=std::min(y,v.y());r=std::max(r,v.x());b=std::max(b,v.y());}
        draft.rect=QRectF(QPointF(x,y),QPointF(r,b));
    } else draft.rect=QRectF(start,pt).normalized();
    update();
}
void Canvas::mouseReleaseEvent(QMouseEvent *e) {
    if(!drawing || e->button()!=Qt::LeftButton)return;
    mouseMoveEvent(e);drawing=false;
    if(draft.kind==Mark::Text) {
        if(draft.rect.width()<.02 || draft.rect.height()<.01)
            draft.rect=QRectF(start,QSizeF(std::min(.45,1-start.x()),std::min(.12,1-start.y())));
        bool ok=false;draft.text=QInputDialog::getMultiLineText(this,"Add text","Text for the selected box:",{},&ok);
        if(!ok || draft.text.isEmpty()){update();return;}
    } else if(draft.kind==Mark::Ink) {
        if(draft.points.size()<2){update();return;}
        draft.rect=draft.rect.adjusted(-.003,-.003,.003,.003).intersected(QRectF(0,0,1,1));
    } else if(draft.rect.width()<.002 || draft.rect.height()<.002){update();return;}
    if(added)added(draft);update();
}
void Canvas::mouseDoubleClickEvent(QMouseEvent *) {if(tool==0 && selected>=0 && editRequested)editRequested(selected);}

class StateCommand:public QUndoCommand {
    State before,after;
    std::function<void(const State&)> apply;
public:
    StateCommand(State b,State a,QString title,std::function<void(const State&)> f):QUndoCommand(title),before(std::move(b)),after(std::move(a)),apply(std::move(f)){}
    void undo() override {apply(before);}
    void redo() override {apply(after);}
};

Editor::Editor(const QString &file,QWidget *parent):QDialog(parent),undo(this),target(file) {
    setWindowTitle("RAMD PDF — Edit document");resize(1200,850);setMinimumSize(720,500);
    auto root=new QVBoxLayout(this);
    auto toolbar=new QToolBar;root->addWidget(toolbar);
    auto act=[this,toolbar](QString text,QKeySequence key,auto callback) {
        auto a=toolbar->addAction(text);a->setShortcut(key);a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        addAction(a);connect(a,&QAction::triggered,this,callback);return a;
    };
    act("Save",QKeySequence::Save,[this]{save(false);});
    act("Save As…",QKeySequence::SaveAs,[this]{save(true);});
    toolbar->addSeparator();
    auto u=undo.createUndoAction(this,"Undo");u->setShortcut(QKeySequence::Undo);addAction(u);toolbar->addAction(u);
    auto r=undo.createRedoAction(this,"Redo");r->setShortcuts({QKeySequence::Redo,QKeySequence("Ctrl+Shift+Z")});addAction(r);toolbar->addAction(r);
    act("Merge PDF…",QKeySequence("Ctrl+M"),[this]{append();});
    act("Export page…",QKeySequence(),[this]{
        int row=list->currentRow();if(row<0)return;
        QString p=QFileDialog::getSaveFileName(this,"Export current page",{},"PDF (*.pdf)");if(p.isEmpty())return;
        if(!p.endsWith(".pdf",Qt::CaseInsensitive))p+=".pdf";
        QString error;if(!exportPdf({state[row]},p,error))QMessageBox::warning(this,"Export failed",error);
        else status->setText("Exported current page to "+p);
    });
    auto tools=new QToolBar;root->addWidget(tools);
    auto group=new QActionGroup(this);
    QStringList names{"Select","Text","Highlight","Draw","Signature"};
    for(int i=0;i<names.size();++i){auto a=tools->addAction(names[i]);a->setCheckable(true);group->addAction(a);if(i==0)a->setChecked(true);connect(a,&QAction::triggered,this,[this,i]{canvas->tool=i;canvas->setCursor(i?Qt::CrossCursor:Qt::ArrowCursor);status->setText(i==0?"Select a new annotation. Double-click text to edit; Delete removes it.":i==4?"Draw your handwritten signature on the page. This is not a digital certificate signature.":"Drag on the page to place your annotation.");});}
    tools->addSeparator();auto color=tools->addAction("Ink color…");connect(color,&QAction::triggered,this,[this]{auto c=QColorDialog::getColor(canvas->ink,this);if(c.isValid())canvas->ink=c;});
    tools->addWidget(new QLabel(" Text size "));auto size=new QSpinBox;size->setRange(6,96);size->setValue(14);tools->addWidget(size);connect(size,qOverload<int>(&QSpinBox::valueChanged),this,[this](int n){canvas->fontSize=n;});
    auto remove=tools->addAction("Delete annotation");remove->setShortcut(QKeySequence::Delete);addAction(remove);connect(remove,&QAction::triggered,this,[this]{deleteMark();});
    auto splitter=new QSplitter;root->addWidget(splitter,1);
    auto side=new QWidget;auto sl=new QVBoxLayout(side);sl->setContentsMargins(0,0,0,0);sl->addWidget(new QLabel("PAGES"));list=new QListWidget;sl->addWidget(list);
    auto controls=new QHBoxLayout;sl->addLayout(controls);
    for(auto pair:{qMakePair(QString("↑"),-1),qMakePair(QString("↓"),1),qMakePair(QString("Remove"),0)}) {
        auto b=new QPushButton(pair.first);controls->addWidget(b);connect(b,&QPushButton::clicked,this,[this,pair]{if(pair.second)movePage(pair.second);else deletePage();});
    }
    splitter->addWidget(side);scroll=new QScrollArea;scroll->setAlignment(Qt::AlignHCenter|Qt::AlignTop);scroll->setStyleSheet("QScrollArea{background:#565c67;}");
    canvas=new Canvas;scroll->setWidget(canvas);splitter->addWidget(scroll);splitter->setStretchFactor(1,1);splitter->setSizes({200,950});
    auto bottom=new QHBoxLayout;root->addLayout(bottom);status=new QLabel("Offline • Drag to add text or highlights. Draw with the mouse.");status->setWordWrap(true);bottom->addWidget(status,1);
    auto zoomBox=new QComboBox;zoomBox->addItems({"50%","75%","100%","125%","150%","200%"});zoomBox->setCurrentText("100%");bottom->addWidget(zoomBox);connect(zoomBox,&QComboBox::currentTextChanged,this,[this](QString s){s.remove('%');zoom=s.toDouble()/100;render();});
    auto done=new QPushButton("Done");bottom->addWidget(done);connect(done,&QPushButton::clicked,this,&Editor::reject);
    connect(list,&QListWidget::currentRowChanged,this,[this]{render();});
    canvas->added=[this](Mark m){int row=list->currentRow();if(row<0)return;auto next=state;next[row].marks.append(m);change(next,"Add annotation");};
    canvas->editRequested=[this](int i){editMark(i);};
    connect(&undo,&QUndoStack::cleanChanged,this,[this](bool clean){setWindowTitle(QString("RAMD PDF — Edit document%1").arg(clean?"":" *"));});
    QString error;
    if(!session.isValid() || !importFile(file,state,error)) {QMessageBox::warning(this,"Cannot edit",error.isEmpty()?"Temporary storage is unavailable.":error);return;}
    ready=true;refresh(0);undo.setClean();
}
bool Editor::importFile(const QString &path,State &into,QString &error) {
    const QString copy=session.filePath(QString::number(++serial)+".pdf");
    if(!QFile::copy(path,copy)){error="Cannot make a working copy of the PDF.";return false;}
    auto doc=Poppler::Document::load(copy);
    if(!doc || doc->isLocked()){error="This PDF is damaged or password protected. Open an unlocked copy.";QFile::remove(copy);return false;}
    if(!doc->okToChange() || !doc->okToAddNotes()){error="This PDF does not permit editing or annotations.";QFile::remove(copy);return false;}
    if(doc->numPages()<1){error="The PDF contains no pages.";return false;}
    for(int i=0;i<doc->numPages();++i)into.append({copy,i,{}});
    return true;
}
void Editor::change(const State &next,const QString &name) {
    undo.push(new StateCommand(state,next,name,[this](const State &s){int row=list->currentRow();state=s;refresh(row);}));
}
void Editor::refresh(int selected) {
    QSignalBlocker block(list);list->clear();for(int i=0;i<state.size();++i)list->addItem(QString("%1   Page %2%3").arg(i+1).arg(state[i].page+1).arg(state[i].marks.isEmpty()?"":"  •"));
    list->setCurrentRow(std::clamp(selected,0,int(state.size())-1));render();
}
void Editor::render() {
    int row=list->currentRow();if(row<0 || row>=state.size())return;
    auto doc=Poppler::Document::load(state[row].source);if(!doc)return;
    doc->setRenderHint(Poppler::Document::Antialiasing);doc->setRenderHint(Poppler::Document::TextAntialiasing);
    auto p=doc->page(state[row].page);if(!p)return;
    QSizeF pts=p->pageSizeF();
    // Bound raster allocation for unusually large architectural PDFs.
    double dpi=std::min(96*zoom,3000.0*72/std::max(pts.width(),pts.height()));
    auto image=p->renderToImage(dpi,dpi);if(image.isNull()){status->setText("Page rendering failed.");return;}
    canvas->setPage(image,pts,state[row].marks);
}
bool Editor::save(bool as) {
    QString dest=target;
    if(as){dest=QFileDialog::getSaveFileName(this,"Save edited PDF",target,"PDF (*.pdf)");if(dest.isEmpty())return false;}
    if(!dest.endsWith(".pdf",Qt::CaseInsensitive))dest+=".pdf";
    QApplication::setOverrideCursor(Qt::WaitCursor);QString error;bool ok=exportPdf(state,dest,error);QApplication::restoreOverrideCursor();
    if(!ok){QMessageBox::warning(this,"Save failed",error);return false;}
    target=dest;savedPath=dest;undo.setClean();status->setText("Saved "+QFileInfo(dest).fileName());return true;
}
bool Editor::canClose() {
    if(undo.isClean())return true;
    auto answer=QMessageBox::question(this,"Unsaved edits","Save your PDF edits before closing?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);
    return answer==QMessageBox::Discard || (answer==QMessageBox::Save && save(false));
}
void Editor::closeEvent(QCloseEvent *e){if(canClose())e->accept();else e->ignore();}
void Editor::reject(){if(canClose())QDialog::reject();}
void Editor::append() {
    auto paths=QFileDialog::getOpenFileNames(this,"Append PDF files",{},"PDF (*.pdf)");if(paths.isEmpty())return;
    State next=state;QString error;for(const auto &p:paths)if(!importFile(p,next,error)){QMessageBox::warning(this,"Merge failed",error);return;}
    change(next,"Merge PDFs");
}
void Editor::movePage(int delta) {
    int row=list->currentRow(),to=row+delta;if(row<0 || to<0 || to>=state.size())return;
    auto next=state;next.swapItemsAt(row,to);change(next,"Move page");list->setCurrentRow(to);
}
void Editor::deletePage() {
    int row=list->currentRow();if(row<0)return;
    if(state.size()==1){QMessageBox::information(this,"Keep one page","A PDF must have at least one page.");return;}
    auto next=state;next.removeAt(row);change(next,"Remove page");
}
void Editor::deleteMark() {
    int row=list->currentRow(),mark=canvas->selected;if(row<0 || mark<0 || mark>=state[row].marks.size())return;
    auto next=state;next[row].marks.removeAt(mark);change(next,"Remove annotation");
}
void Editor::editMark(int index) {
    int row=list->currentRow();if(row<0 || index<0 || index>=state[row].marks.size())return;
    if(state[row].marks[index].kind!=Mark::Text)return;
    bool ok;auto value=QInputDialog::getMultiLineText(this,"Edit text","Text:",state[row].marks[index].text,&ok);if(!ok)return;
    auto next=state;next[row].marks[index].text=value;change(next,"Edit text");
}
}
