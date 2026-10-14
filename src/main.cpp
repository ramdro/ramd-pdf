#include "editor.h"
#include <QModelIndex>
#include <QApplication>
#include <QMainWindow>
#include <QPdfDocument>
#include <QPdfView>
#include <QPdfPageNavigator>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QMenuBar>
#include <QToolBar>
#include <QSpinBox>
#include <QLabel>
#include <QComboBox>
#include <QStatusBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QAction>
#include <QDir>
#include <algorithm>
#include <QPdfSearchModel>
#include <QLineEdit>
#include <QDockWidget>
#include <QListWidget>
#include <QSplitter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPainter>
#include <QFile>
#include <QCloseEvent>
#include <QCheckBox>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QIcon>
#include <QStackedWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidgetItem>
#include <QDirIterator>
#include <QStandardPaths>
#include <QPdfWriter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QScrollBar>
#include <QScreen>
#include <QKeySequence>
#include <QFrame>
#include <QButtonGroup>
#include <QGraphicsDropShadowEffect>
#include <QPixmap>
#include <QStyle>
#include <QFileIconProvider>
#include <QListView>
#include <QPainterPath>

#include <QDate>
#include <QFont>
#include <QSet>
#include <QFileInfoList>
#include <QGroupBox>
#include <QScrollArea>
#include <QFileSystemWatcher>
#include <QInputDialog>
#include <QLocale>
#include <QPageSize>
#include <QPdfWriter>


class Reader : public QMainWindow {
    QPdfDocument doc{this};
    QPdfView *view = new QPdfView(this);
    QSpinBox *page = new QSpinBox(this);
    QLabel *count = new QLabel("/ 0", this);
    QComboBox *zoom = new QComboBox(this);
    QMenu *recent;
    QString file;
    QStringList history;
    int searchCursor = 0;
    bool dark = false;
    QStackedWidget *pages = new QStackedWidget(this);
    QWidget *home = new QWidget(this);
    QWidget *readerPage = new QWidget(this);
    QListWidget *library = new QListWidget(this);
    QLabel *librarySummary = new QLabel(this);
    QLineEdit *libraryFilter = new QLineEdit(this);
    QStringList indexedFiles;
    QToolBar *readerBar = nullptr;
    QAction *homeAction = nullptr;
    QString currentCategory = "All";
    QWidget *contentPanel = nullptr;
    QLabel *welcomeTitle = nullptr;
    QListWidget *recentList = nullptr;
    QList<QPushButton*> navButtons;

    QListWidget *thumbnails = new QListWidget(this);
    QDockWidget *thumbDock = new QDockWidget("Pages", this);
    QLineEdit *searchInput = new QLineEdit(this);
    QPdfSearchModel *searchModel = new QPdfSearchModel(this);
public:
    Reader() {
        setWindowTitle("RAMD PDF — by RAMD Studios");
        resize(1320,850);
        setMinimumSize(700,480);
        setStyleSheet("QMainWindow{background:#f4f5f7} QToolBar{background:white;padding:10px;spacing:8px;border-bottom:1px solid #e2e5eb} QToolButton{padding:6px 10px;border-radius:7px} QToolButton:hover{background:#eaf1ff} QSpinBox,QComboBox{padding:5px;border:1px solid #dce0e7;border-radius:7px;background:white}");
        view->setDocument(&doc);
        view->setPageMode(QPdfView::PageMode::MultiPage);
        view->setZoomMode(QPdfView::ZoomMode::FitInView);
        // Home / Reader navigation. The PDF viewer is embedded in its own page.
        auto *readerLayout = new QVBoxLayout(readerPage);
        readerLayout->setContentsMargins(0,0,0,0);
        readerLayout->addWidget(view);
        pages->addWidget(home);
        pages->addWidget(readerPage);
        setCentralWidget(pages);
        buildHome();
        pages->setCurrentWidget(home);
        thumbDock->setWidget(thumbnails);
        thumbDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        addDockWidget(Qt::LeftDockWidgetArea, thumbDock);
        thumbDock->setMinimumWidth(150);
        thumbDock->resize(185, height());
        connect(thumbnails, &QListWidget::currentRowChanged, this, [this](int row) {
            if (row >= 0 && row < doc.pageCount())
                view->pageNavigator()->jump(row, QPointF(), 0);
        });
        searchModel->setDocument(&doc);
        auto *fm=menuBar()->addMenu("&File");
        auto *open=fm->addAction("&Open PDF…");
        open->setShortcut(QKeySequence::Open);
        connect(open,&QAction::triggered,this,[this]{auto p=QFileDialog::getOpenFileName(this,"Open PDF",QDir::homePath(),"PDF (*.pdf)");if(!p.isEmpty())load(p);});
        recent=fm->addMenu("Open &Recent");
        auto *editAction=fm->addAction("Edit PDF…");
        editAction->setShortcut(QKeySequence("Ctrl+E"));
        connect(editAction,&QAction::triggered,this,[this]{
            if(file.isEmpty()) { QMessageBox::information(this,"Open a PDF","Open a PDF before editing.");return; }
            Ramd::Editor editor(file,this);
            if(editor.ready) editor.exec();
            if(!editor.savedPath.isEmpty()) load(editor.savedPath);
        });

        fm->addSeparator();
        auto *printAction=fm->addAction("&Print…");
        printAction->setShortcut(QKeySequence::Print);
        connect(printAction, &QAction::triggered, this, [this] {
            if (doc.status()!=QPdfDocument::Status::Ready) return;
            QPrinter printer(QPrinter::HighResolution);
            QPrintDialog dlg(&printer, this);
            if (dlg.exec()!=QDialog::Accepted) return;
            QPainter painter(&printer);
            if (!painter.isActive()) { QMessageBox::warning(this,"Print","Printer unavailable."); return; }
            const int start=printer.fromPage()>0 ? printer.fromPage()-1 : 0;
            const int end=printer.toPage()>0 ? std::min(printer.toPage(),doc.pageCount()) : doc.pageCount();
            for (int i=start; i<end; ++i) {
                if (i>start && !printer.newPage()) break;
                const QSizeF pts=doc.pagePointSize(i);
                if (pts.isEmpty()) continue;
                QSize target=pts.toSize();
                target.scale(printer.pageRect(QPrinter::DevicePixel).size().toSize(),Qt::KeepAspectRatio);
                QImage image=doc.render(i,target);
                if (!image.isNull()) painter.drawImage(QRect(QPoint(0,0),target),image);
            }
            painter.end();
        });
        fm->addSeparator();
        fm->addAction("E&xit",this,&QWidget::close,QKeySequence::Quit);
        auto *bar=addToolBar("Reader");readerBar=bar;bar->setMovable(false);
        homeAction=bar->addAction("⌂ Library");
        connect(homeAction,&QAction::triggered,this,[this]{showHome();});
        bar->addSeparator();
        bar->addAction(open);bar->addAction(editAction);bar->addSeparator();
        auto *prev=bar->addAction("◀ Previous");
        page->setRange(1,1);page->setFixedWidth(70);bar->addWidget(page);bar->addWidget(count);
        auto *next=bar->addAction("Next ▶");bar->addSeparator();
        auto *minus=bar->addAction("−");
        zoom->addItems({"50%","75%","100%","125%","150%","200%"});zoom->setCurrentText("100%");zoom->setFixedWidth(90);bar->addWidget(zoom);
        auto *plus=bar->addAction("+");
        auto *fitWidth=bar->addAction("Fit Width");
        auto *fitPage=bar->addAction("Fit Page");
        bar->addSeparator();
        searchInput->setPlaceholderText("Search document…");
        searchInput->setClearButtonEnabled(true);
        searchInput->setFixedWidth(195);
        bar->addWidget(searchInput);
        auto *searchNext=bar->addAction("Find Next");
        searchNext->setShortcut(QKeySequence::FindNext);
        auto *findAction=new QAction(this);findAction->setShortcut(QKeySequence::Find);addAction(findAction);
        connect(findAction,&QAction::triggered,this,[this]{searchInput->setFocus();searchInput->selectAll();});
        connect(searchInput,&QLineEdit::textChanged,this,[this](const QString &text){searchCursor=0;searchModel->setSearchString(text);});
        connect(searchNext,&QAction::triggered,this,[this]{
            const int n=searchModel->rowCount(QModelIndex());
            if(n<=0){statusBar()->showMessage("No search results",3000);return;}
            if(searchCursor>=n)searchCursor=0;
            const QModelIndex index=searchModel->index(searchCursor++,0);
            const auto pageIndex=searchModel->data(index,static_cast<int>(QPdfSearchModel::Role::Page)).toInt();
            if(pageIndex>=0 && pageIndex<doc.pageCount())
                view->pageNavigator()->jump(pageIndex,QPointF(),0);
            statusBar()->showMessage(QString("Search result %1 of %2").arg(searchCursor).arg(n),2500);
        });
        auto *appearance=menuBar()->addMenu("&Appearance");
        auto *darkAction=appearance->addAction("Dark mode");
        darkAction->setCheckable(true);
        darkAction->setChecked(QSettings().value("dark",false).toBool());
        auto applyTheme=[this](bool on){
            dark=on;
            QFile sheet(on?":/theme/dark.qss":":/theme/light.qss");
            if(sheet.open(QIODevice::ReadOnly))setStyleSheet(QString::fromUtf8(sheet.readAll()));
            QSettings().setValue("dark",on);
        };
        connect(darkAction,&QAction::toggled,this,applyTheme);
        applyTheme(darkAction->isChecked());
        auto nav=view->pageNavigator();
        auto go=[this,nav](int n){if(doc.status()==QPdfDocument::Status::Ready && n>=0 && n<doc.pageCount())nav->jump(n,QPointF(),0);};
        connect(prev,&QAction::triggered,this,[nav,go]{go(nav->currentPage()-1);});
        connect(next,&QAction::triggered,this,[nav,go]{go(nav->currentPage()+1);});
        connect(page,qOverload<int>(&QSpinBox::valueChanged),this,[go](int n){go(n-1);});
        connect(nav,&QPdfPageNavigator::currentPageChanged,this,[this,nav](int){QSignalBlocker b(page);page->setValue(nav->currentPage()+1);
            QSignalBlocker tb(thumbnails);
            thumbnails->setCurrentRow(nav->currentPage());});
        auto setZoom=[this](qreal factor){view->setZoomMode(QPdfView::ZoomMode::Custom);view->setZoomFactor(std::clamp(factor,0.2,5.0));zoom->setCurrentText(QString::number(qRound(view->zoomFactor()*100))+"%");};
        connect(plus,&QAction::triggered,this,[this,setZoom]{setZoom(view->zoomFactor()*1.2);});
        connect(minus,&QAction::triggered,this,[this,setZoom]{setZoom(view->zoomFactor()/1.2);});
        connect(zoom,&QComboBox::textActivated,this,[setZoom](const QString &s){QString t=s;t.remove('%');bool ok=false;double n=t.toDouble(&ok);if(ok&&n>=20&&n<=500)setZoom(n/100);});
        connect(fitWidth,&QAction::triggered,this,[this]{view->setZoomMode(QPdfView::ZoomMode::FitToWidth);});
        connect(fitPage,&QAction::triggered,this,[this]{view->setZoomMode(QPdfView::ZoomMode::FitInView);});
        connect(&doc,&QPdfDocument::statusChanged,this,[this]{if(doc.status()==QPdfDocument::Status::Ready){page->setRange(1,std::max(1,doc.pageCount()));count->setText("/ "+QString::number(doc.pageCount()));
            thumbnails->clear();
            for(int i=0;i<std::min(doc.pageCount(),150);++i) {
                auto *item=new QListWidgetItem(QString("Page %1").arg(i+1),thumbnails);
                // Render small previews to limit memory usage.
                const QSizeF size=doc.pagePointSize(i);
                if (!size.isEmpty()) {
                    QSize px=size.toSize();px.scale(QSize(96,126),Qt::KeepAspectRatio);
                    if(px.width()>0 && px.height()>0) {
                        const QImage preview=doc.render(i,px);
                        if(!preview.isNull())item->setIcon(QPixmap::fromImage(preview));
                    }
                }
            }
            thumbnails->setIconSize(QSize(96,126));
            if(doc.pageCount()>150)statusBar()->showMessage("Showing first 150 page previews to preserve memory",5000);}else{page->setRange(1,1);count->setText("/ 0");thumbnails->clear();}});
        menuBar()->addMenu("&Help")->addAction("About",this,[this]{QMessageBox::about(this,"RAMD PDF","<h2>RAMD PDF</h2>Offline PDF reader and annotation editor v0.6<br>RAMD Studios");});
        history=QSettings().value("recent").toStringList();refreshRecent();
        statusBar()->showMessage("Offline · Files stay on your computer");
        showHome();
        QTimer::singleShot(100, this, [this]{scanLibrary();});
    }
    void load(const QString &path){
        QFileInfo info(path);
        if(!info.isFile()||!info.isReadable()){QMessageBox::warning(this,"Open failed","File not found or unreadable.");return;}
        doc.close();searchInput->clear();
        if(doc.load(info.absoluteFilePath())!=QPdfDocument::Error::None){QMessageBox::warning(this,"Open failed","Unable to open PDF (damaged, encrypted or unsupported).");return;}
        file=info.absoluteFilePath();setWindowTitle(info.fileName()+" — RAMD PDF");
        pages->setCurrentWidget(readerPage);
        readerBar->setVisible(true);
        thumbDock->setVisible(true);
        history.removeAll(file);history.prepend(file);while(history.size()>8)history.removeLast();QSettings().setValue("recent",history);refreshRecent();
        statusBar()->showMessage("Opened "+info.fileName(),4000);
        if (!indexedFiles.contains(file)) { indexedFiles.prepend(file); updateLibrary(); }
    }
    void showHome() {
        pages->setCurrentWidget(home);
        if(readerBar)readerBar->setVisible(false);
        thumbDock->setVisible(false);
        setWindowTitle("RAMD PDF — Library");
    }
    QPushButton *makeNav(const QString &label, const QString &symbol, QVBoxLayout *layout) {
        auto *b = new QPushButton(symbol + "   " + label, home);
        b->setObjectName("navButton");
        b->setCheckable(true);
        b->setMinimumHeight(42);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("section",label);
        layout->addWidget(b);
        navButtons.append(b);
        connect(b,&QPushButton::clicked,this,[this,label]{
            for(auto *other:navButtons)other->setChecked(other->property("section").toString()==label);
            currentCategory=label;
            if(welcomeTitle)welcomeTitle->setText(label=="Home"?"Welcome to RAMD PDF":label);
            if(label=="Home") libraryFilter->clear();
            updateLibrary();
        });
        return b;
    }
    void buildHome() {
        home->setObjectName("homePage");
        auto *shell=new QHBoxLayout(home);shell->setContentsMargins(0,0,0,0);shell->setSpacing(0);
        auto *sidebar=new QFrame(home);sidebar->setObjectName("sidebar");sidebar->setFixedWidth(238);
        auto *side=new QVBoxLayout(sidebar);side->setContentsMargins(20,28,20,20);side->setSpacing(8);
        auto *brand=new QHBoxLayout;
        auto *logo=new QLabel(sidebar);logo->setPixmap(QPixmap(":/brand/ramd-pdf.png").scaled(46,46,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        brand->addWidget(logo);
        auto *brandName=new QLabel("RAMD PDF",sidebar);brandName->setObjectName("brandName");brand->addWidget(brandName);brand->addStretch();
        side->addLayout(brand);side->addSpacing(36);
        makeNav("Home","⌂",side)->setChecked(true);
        makeNav("Library","▤",side);
        makeNav("Recent","◷",side);
        makeNav("Favorites","☆",side);
        side->addSpacing(23);
        auto *locations=new QLabel("LOCATIONS",sidebar);locations->setObjectName("sectionLabel");side->addWidget(locations);
        auto *docs=new QPushButton("▧   Documents",sidebar);docs->setObjectName("locationButton");
        connect(docs,&QPushButton::clicked,this,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));});side->addWidget(docs);
        auto *downloads=new QPushButton("↓   Downloads",sidebar);downloads->setObjectName("locationButton");
        connect(downloads,&QPushButton::clicked,this,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)));});side->addWidget(downloads);
        auto *addFolder=new QPushButton("＋  Add Folder",sidebar);addFolder->setObjectName("locationButton");
        connect(addFolder,&QPushButton::clicked,this,[this]{
            QString folder=QFileDialog::getExistingDirectory(this,"Add a library folder",QDir::homePath());
            if(folder.isEmpty())return;
            QSettings settings;QStringList dirs=settings.value("libraryFolders").toStringList();
            if(!dirs.contains(folder)){dirs.append(folder);settings.setValue("libraryFolders",dirs);}scanLibrary();
        });side->addWidget(addFolder);
        side->addStretch();
        auto *privacy=new QLabel("●  100% offline\nYour files stay on this PC",sidebar);privacy->setObjectName("privacyText");side->addWidget(privacy);
        shell->addWidget(sidebar);

        auto *scroll=new QScrollArea(home);scroll->setObjectName("homeScroll");scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
        auto *body=new QWidget(scroll);body->setObjectName("dashboardBody");
        auto *outer=new QVBoxLayout(body);outer->setContentsMargins(38,32,38,32);outer->setSpacing(22);
        auto *top=new QHBoxLayout;
        libraryFilter->setPlaceholderText("⌕  Search your documents...");libraryFilter->setClearButtonEnabled(true);
        libraryFilter->setMinimumHeight(39);libraryFilter->setObjectName("librarySearch");
        connect(libraryFilter,&QLineEdit::textChanged,this,[this]{updateLibrary();});top->addWidget(libraryFilter,1);
        auto *refresh=new QPushButton("⟳",body);refresh->setObjectName("secondaryButton");refresh->setToolTip("Refresh library");
        connect(refresh,&QPushButton::clicked,this,[this]{scanLibrary();});top->addWidget(refresh);
        auto *openButton=new QPushButton("Open PDF",body);openButton->setObjectName("primaryButton");
        connect(openButton,&QPushButton::clicked,this,[this]{
            const QString path=QFileDialog::getOpenFileName(this,"Open PDF",QDir::homePath(),"PDF documents (*.pdf)");
            if(!path.isEmpty())load(path);
        });top->addWidget(openButton);outer->addLayout(top);
        auto *hero=new QFrame(body);hero->setObjectName("hero");hero->setMinimumHeight(190);
        auto *heroLayout=new QVBoxLayout(hero);heroLayout->setContentsMargins(30,28,30,28);
        welcomeTitle=new QLabel("Welcome to RAMD PDF",hero);welcomeTitle->setObjectName("heroTitle");heroLayout->addWidget(welcomeTitle);
        auto *heroSubtitle=new QLabel("Your documents, all in one place.",hero);heroSubtitle->setObjectName("heroSubtitle");heroLayout->addWidget(heroSubtitle);
        heroLayout->addStretch();outer->addWidget(hero);
        auto *quickTitle=new QLabel("QUICK ACTIONS",body);quickTitle->setObjectName("sectionLabel");outer->addWidget(quickTitle);
        auto *cards=new QGridLayout;cards->setSpacing(12);
        auto addCard=[this,cards,body](int col,const QString &glyph,const QString &name,const QString &detail,const QString &color,std::function<void()> action){
            auto *card=new QPushButton(body);card->setObjectName("quickCard");card->setCursor(Qt::PointingHandCursor);
            card->setMinimumHeight(156);card->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
            auto *v=new QVBoxLayout(card);v->setContentsMargins(18,18,18,18);v->setSpacing(10);
            auto *symbol=new QLabel(glyph,card);symbol->setFixedSize(40,40);symbol->setAlignment(Qt::AlignCenter);
            symbol->setStyleSheet(QString("background:%1;color:white;border-radius:11px;font-size:22px;font-weight:700;").arg(color));v->addWidget(symbol,0,Qt::AlignLeft);
            auto *title=new QLabel(name,card);title->setObjectName("cardHeading");v->addWidget(title);
            auto *hint=new QLabel(detail,card);hint->setObjectName("mutedText");hint->setWordWrap(true);v->addWidget(hint);
            connect(card,&QPushButton::clicked,this,[action]{action();});cards->addWidget(card,0,col);
        };
        addCard(0,"▣","Open PDF","Browse your files","#3478f6",[this]{const auto path=QFileDialog::getOpenFileName(this,"Open PDF",QDir::homePath(),"PDF (*.pdf)");if(!path.isEmpty())load(path);});
        addCard(1,"＋","New PDF","Start from a template","#f3a42d",[this]{newDocument();});
        addCard(2,"▤","Library","Explore your documents","#23b88a",[this]{currentCategory="Library";for(auto *b:navButtons)b->setChecked(b->property("section").toString()=="Library");welcomeTitle->setText("Library");updateLibrary();});
        addCard(3,"◷","Recent","Continue where you left off","#8a63ee",[this]{currentCategory="Recent";for(auto *b:navButtons)b->setChecked(b->property("section").toString()=="Recent");welcomeTitle->setText("Recent");updateLibrary();});
        outer->addLayout(cards);
        auto *listHeader=new QHBoxLayout;
        auto *section=new QLabel("Your Documents",body);section->setObjectName("listHeading");listHeader->addWidget(section);listHeader->addStretch();
        auto *newButton=new QPushButton("＋ New PDF",body);newButton->setObjectName("secondaryButton");
        connect(newButton,&QPushButton::clicked,this,[this]{newDocument();});listHeader->addWidget(newButton);
        outer->addLayout(listHeader);
        library->setObjectName("documentList");library->setAlternatingRowColors(false);library->setSpacing(7);
        library->setIconSize(QSize(34,42));library->setMinimumHeight(320);
        library->setSelectionMode(QAbstractItemView::SingleSelection);
        connect(library,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *item){load(item->data(Qt::UserRole).toString());});
        library->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(library,&QWidget::customContextMenuRequested,this,[this](const QPoint &pos){
            auto *item=library->itemAt(pos);if(!item)return;
            const QString path=item->data(Qt::UserRole).toString();
            QStringList favorites=QSettings().value("favorites").toStringList();
            QMenu menu(this);QAction *toggle=menu.addAction(favorites.contains(path)?"Remove from favorites":"Add to favorites");
            QAction *reveal=menu.addAction("Show in file manager");
            QAction *chosen=menu.exec(library->mapToGlobal(pos));
            if(chosen==toggle){if(favorites.contains(path))favorites.removeAll(path);else favorites.append(path);QSettings().setValue("favorites",favorites);updateLibrary();}
            if(chosen==reveal)QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        });
        outer->addWidget(library,1);
        librarySummary=new QLabel(body);librarySummary->setObjectName("mutedText");outer->addWidget(librarySummary);
        auto *footer=new QLabel("RAMD PDF Free  •  RAMD Studios  •  Private by design",body);footer->setObjectName("mutedText");outer->addWidget(footer);
        scroll->setWidget(body);shell->addWidget(scroll,1);
    }
    void scanLibrary() {
        // Scan common user folders, bounded to avoid unexpectedly traversing huge disks.
        QStringList folders;
        for (auto loc: {QStandardPaths::DocumentsLocation,QStandardPaths::DownloadLocation}) {
            const QString path=QStandardPaths::writableLocation(loc);
            if(!path.isEmpty() && !folders.contains(path)) folders.append(path);
        }
        for(const QString &extra:QSettings().value("libraryFolders").toStringList())
            if(!folders.contains(extra))folders.append(extra);
        QSet<QString> unique;
        for(const QString &recentPath:history)
            if(QFileInfo(recentPath).isFile())unique.insert(QFileInfo(recentPath).absoluteFilePath());
        int inspected=0;
        const int maxFiles=12000;
        for(const QString &folder:folders) {
            if(!QDir(folder).exists())continue;
            QDirIterator it(folder,QDir::Files|QDir::NoSymLinks|QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while(it.hasNext() && inspected<maxFiles) {
                const QString path=it.next();++inspected;
                if(path.endsWith(".pdf",Qt::CaseInsensitive))unique.insert(path);
            }
            if(inspected>=maxFiles)break;
        }
        indexedFiles=unique.values();
        std::sort(indexedFiles.begin(),indexedFiles.end(),[](const QString &a,const QString &b){
            return QFileInfo(a).lastModified()>QFileInfo(b).lastModified();
        });
        updateLibrary();
        statusBar()->showMessage(QString("Library indexed: %1 PDFs").arg(indexedFiles.size()),4000);
    }
    void updateLibrary() {
        library->clear();
        const QString query=libraryFilter->text().trimmed();
        int visible=0;
        for(const QString &path:indexedFiles) {
            QFileInfo fi(path);
            if(!fi.exists() || !fi.fileName().contains(query,Qt::CaseInsensitive))continue;
            if(currentCategory=="Recent" && !history.contains(path))continue;
            if(currentCategory=="Favorites" && !QSettings().value("favorites").toStringList().contains(path))continue;
            auto *item=new QListWidgetItem(QString("%1     •     %2     •     %3")
                .arg(fi.fileName(),QLocale().formattedDataSize(fi.size()),fi.lastModified().toString("dd MMM yyyy")),library);
            item->setData(Qt::UserRole,path);
            item->setToolTip(path);
            ++visible;
        }
        librarySummary->setText(QString("%1 documents · Double-click to open · Right-click to favorite").arg(visible));
    }
    void newDocument() {
        QDialog dialog(this);
        dialog.setWindowTitle("Create a document — RAMD PDF");
        dialog.resize(780,520);
        dialog.setObjectName("templateDialog");
        auto *dialogLayout = new QVBoxLayout(&dialog);
        dialogLayout->setContentsMargins(28,26,28,24);
        dialogLayout->setSpacing(18);
        auto *titleLabel = new QLabel("Start something new", &dialog);
        titleLabel->setObjectName("templateTitle");
        dialogLayout->addWidget(titleLabel);
        auto *subtitle = new QLabel("Choose a starting layout. Everything is created locally on your computer.", &dialog);
        subtitle->setObjectName("mutedText");
        dialogLayout->addWidget(subtitle);
        auto *templateGrid = new QGridLayout;
        templateGrid->setSpacing(14);
        QString chosen;
        const QStringList options={"Blank A4", "Meeting Notes", "Project Report", "Invoice", "Resume"};
        const QStringList descriptions={"An empty page", "Agenda and action items", "A structured report", "Billing details", "Your professional profile"};
        const QStringList symbols={"＋", "☷", "▤", "▣", "◉"};
        for(int i=0;i<options.size();++i) {
            auto *button = new QPushButton(&dialog);
            button->setObjectName("templateCard");
            button->setMinimumSize(192,156);
            button->setCursor(Qt::PointingHandCursor);
            auto *layout = new QVBoxLayout(button);
            layout->setContentsMargins(18,18,18,18);
            auto *symbol = new QLabel(symbols.at(i),button);
            symbol->setObjectName("templateSymbol");
            layout->addWidget(symbol);
            layout->addStretch();
            auto *name = new QLabel(options.at(i),button);
            name->setObjectName("cardHeading");
            layout->addWidget(name);
            auto *desc = new QLabel(descriptions.at(i),button);
            desc->setObjectName("mutedText");
            layout->addWidget(desc);
            templateGrid->addWidget(button,i/3,i%3);
            connect(button,&QPushButton::clicked,&dialog,[&dialog,&chosen,options,i]{
                chosen=options.at(i);dialog.accept();
            });
        }
        dialogLayout->addLayout(templateGrid);
        dialogLayout->addStretch();
        auto *cancel = new QPushButton("Cancel", &dialog);
        cancel->setObjectName("secondaryButton");
        connect(cancel,&QPushButton::clicked,&dialog,&QDialog::reject);
        dialogLayout->addWidget(cancel,0,Qt::AlignRight);
        if(dialog.exec()!=QDialog::Accepted || chosen.isEmpty())return;
        const QString path=QFileDialog::getSaveFileName(this,"Save new PDF",QDir::homePath()+"/New Document.pdf","PDF (*.pdf)");
        if(path.isEmpty())return;
        QString target=path.endsWith(".pdf",Qt::CaseInsensitive)?path:path+".pdf";
        QPdfWriter writer(target);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(144);
        writer.setTitle(chosen);
        writer.setCreator("RAMD PDF by RAMD Studios");
        QPainter painter(&writer);
        if(!painter.isActive()) { QMessageBox::warning(this,"Save failed","Cannot create PDF here.");return; }
        const QRect pageRect=writer.pageLayout().paintRectPixels(writer.resolution());
        const int left=pageRect.left()+90;
        const int width=pageRect.width()-180;
        if(chosen=="Blank A4") { painter.end(); load(target); return; }
        painter.setPen(QColor("#1e293b"));
        QFont heading("Sans Serif",22,QFont::Bold);painter.setFont(heading);
        const QString title=chosen=="Blank A4"?"":chosen;
        painter.drawText(QRect(left,180,width,100),Qt::AlignLeft|Qt::AlignVCenter,title);
        painter.setPen(QColor("#2563eb"));
        painter.fillRect(QRect(left,300,110,6),QColor("#2563eb"));
        painter.setPen(QColor("#475569"));
        painter.setFont(QFont("Sans Serif",11));
        QStringList lines;
        if(chosen=="Meeting Notes") lines={"Date: _______________________", "Attendees:", "Agenda:", "Discussion:", "Action items:"};
        if(chosen=="Project Report") lines={"Project name:", "Prepared by:", "Date:", "Executive summary:", "Objectives:", "Results:", "Next steps:"};
        if(chosen=="Invoice") lines={"INVOICE #:", "Date:", "From:", "Bill to:", "Description                        Quantity                        Amount", "Subtotal:", "Total:"};
        if(chosen=="Resume") lines={"Full Name", "Email  •  Phone  •  Location", "PROFILE", "EXPERIENCE", "EDUCATION", "SKILLS"};
        int y=365;
        for(const QString &line:lines) {painter.drawText(left,y,line);y+=90;}
        painter.end();
        if(!QFileInfo(target).exists() || QFileInfo(target).size()==0) {
            QMessageBox::warning(this,"Save failed","The PDF was not created.");return;
        }
        load(target);
        QMessageBox::information(this,"PDF created",
            "Your starter PDF was created. Use Edit PDF (Ctrl+E) to add text, highlights, drawing or a handwritten signature.");
    }
    void refreshRecent(){recent->clear();for(const auto &p:history){auto *a=recent->addAction(QFileInfo(p).fileName());a->setToolTip(p);connect(a,&QAction::triggered,this,[this,p]{load(p);});}if(history.isEmpty())recent->addAction("No recent files")->setEnabled(false);}
};
int main(int argc,char **argv){
    QApplication app(argc,argv);
    app.setWindowIcon(QIcon(":/brand/ramd-pdf.png"));
    QCoreApplication::setOrganizationName("RAMD Studios");
    QCoreApplication::setApplicationName("RAMD PDF");
    Reader reader;reader.show();
    if(argc>1)reader.load(QString::fromLocal8Bit(argv[1]));
    return app.exec();
}
