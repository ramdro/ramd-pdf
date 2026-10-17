#include "editor.h"
#include "pdf_tools.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCryptographicHash>
#include <QTreeWidget>
#include <QHeaderView>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QTabWidget>
#include <QProgressBar>
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
    QTreeWidget *library = new QTreeWidget(this);
    QLabel *librarySummary = new QLabel(this);
    QLineEdit *libraryFilter = new QLineEdit(this);
    QStringList indexedFiles;
    QToolBar *readerBar = nullptr;
    QAction *homeAction = nullptr;
    QString currentCategory = "Home";
    QWidget *contentPanel = nullptr;
    QLabel *welcomeTitle = nullptr;
    QListWidget *recentList = nullptr;
    QList<QPushButton*> navButtons;

    QListWidget *thumbnails = new QListWidget(this);
    QDockWidget *thumbDock = new QDockWidget("Pages", this);
    QLineEdit *searchInput = new QLineEdit(this);
    QPdfSearchModel *searchModel = new QPdfSearchModel(this);
    QComboBox *sortOrder = new QComboBox(this);
    QListWidget *savedPages = new QListWidget(this);
    QLabel *emptyState = new QLabel(this);
    QPushButton *resumeButton = nullptr;
    QLabel *activity = new QLabel(this);
    QProgressBar *operationProgress = new QProgressBar(this);
    QAction *bookmarkAction = nullptr;
    bool loadingDocument = false;
    bool scanBusy = false;
    bool toolBusy = false;
    QString documentKey() const {
        return "documents/"+QString::fromLatin1(QCryptographicHash::hash(file.toUtf8(),QCryptographicHash::Sha256).toHex());
    }
    void savePosition() {
        if(!loadingDocument && !file.isEmpty() && doc.status()==QPdfDocument::Status::Ready)
            QSettings().setValue(documentKey()+"/page",view->pageNavigator()->currentPage());
    }
    void refreshBookmarks() {
        savedPages->clear();
        if(file.isEmpty() || doc.status()!=QPdfDocument::Status::Ready)return;
        const auto bookmarks=QSettings().value(documentKey()+"/bookmarks").toMap();
        QList<int> numbers;for(auto it=bookmarks.cbegin();it!=bookmarks.cend();++it)numbers.append(it.key().toInt());
        std::sort(numbers.begin(),numbers.end());
        for(int number:numbers)if(number>=0 && number<doc.pageCount()) {
            auto *item=new QListWidgetItem(QString("%1  ·  %2").arg(number+1).arg(bookmarks.value(QString::number(number)).toString()),savedPages);
            item->setData(Qt::UserRole,number);
        }
        if(bookmarkAction)bookmarkAction->setText(bookmarks.contains(QString::number(view->pageNavigator()->currentPage()))?"Remove bookmark":"Bookmark page");
    }
    void toggleBookmark() {
        if(doc.status()!=QPdfDocument::Status::Ready || file.isEmpty())return;
        const auto key=QString::number(view->pageNavigator()->currentPage());
        auto bookmarks=QSettings().value(documentKey()+"/bookmarks").toMap();
        if(bookmarks.contains(key))bookmarks.remove(key);
        else {
            bool ok=false;
            const auto label=QInputDialog::getText(this,"Bookmark this page","Name",QLineEdit::Normal,QString("Page %1").arg(key.toInt()+1),&ok);
            if(!ok)return;
            bookmarks.insert(key,label.trimmed().isEmpty()?QString("Page %1").arg(key.toInt()+1):label.trimmed());
        }
        QSettings().setValue(documentKey()+"/bookmarks",bookmarks);refreshBookmarks();
    }
public:
    Reader() {
        setWindowTitle("RAMD PDF — by RAMD Studios");
        resize(1240,820);
        setAcceptDrops(true);
        setMinimumSize(820,580);
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
        auto *navigationTabs=new QTabWidget(this);
        navigationTabs->setObjectName("navigationTabs");
        navigationTabs->addTab(thumbnails,"Pages");
        navigationTabs->addTab(savedPages,"Bookmarks");
        savedPages->setObjectName("savedPages");
        savedPages->setToolTip("Save pages with Ctrl+D. Double-click a bookmark to return to it.");
        connect(savedPages,&QListWidget::itemActivated,this,[this](QListWidgetItem *item){view->pageNavigator()->jump(item->data(Qt::UserRole).toInt(),QPointF(),0);});
        thumbDock->setWidget(navigationTabs);
        thumbDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        addDockWidget(Qt::LeftDockWidgetArea, thumbDock);
        thumbDock->setMinimumWidth(190);
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
        homeAction=bar->addAction(style()->standardIcon(QStyle::SP_DirHomeIcon),"Library");
        connect(homeAction,&QAction::triggered,this,[this]{showHome();});
        bar->addSeparator();
        bar->addAction(open);bar->addAction(editAction);bar->addSeparator();
        auto *prev=bar->addAction(style()->standardIcon(QStyle::SP_ArrowBack),"Previous");
        page->setObjectName("pageNumber");page->setRange(1,1);page->setFixedWidth(70);bar->addWidget(page);bar->addWidget(count);
        auto *next=bar->addAction(style()->standardIcon(QStyle::SP_ArrowForward),"Next");bar->addSeparator();
        auto *minus=bar->addAction("−");
        zoom->addItems({"50%","75%","100%","125%","150%","200%"});zoom->setCurrentText("100%");zoom->setFixedWidth(90);bar->addWidget(zoom);
        auto *plus=bar->addAction("+");
        auto *fitWidth=bar->addAction("Fit Width");
        auto *fitPage=bar->addAction("Fit Page");
        bar->addSeparator();
        searchInput->setPlaceholderText("Search document…");
        searchInput->setClearButtonEnabled(true);
        searchInput->setFixedWidth(165);
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
        auto *tools=menuBar()->addMenu("&Tools");
        tools->addAction("Merge PDFs…",this,[this]{mergeDocuments();});
        tools->addAction("Extract pages…",this,[this]{extractPages();});
        tools->addSeparator();
        tools->addAction("Document information",this,[this]{
            if(file.isEmpty() || doc.status()!=QPdfDocument::Status::Ready)return;
            const QFileInfo info(file);
            QMessageBox::information(this,"Document information",QString("%1\n\n%2 pages · %3\nModified: %4\n\n%5").arg(info.fileName()).arg(doc.pageCount()).arg(QLocale().formattedDataSize(info.size()),QLocale().toString(info.lastModified(),QLocale::LongFormat),info.absoluteFilePath()));
        });
        bookmarkAction=tools->addAction("Bookmark page",this,[this]{toggleBookmark();});
        bookmarkAction->setShortcut(QKeySequence("Ctrl+D"));
        auto *reading=menuBar()->addMenu("&Reading");
        reading->addAction(thumbDock->toggleViewAction());
        auto *single=reading->addAction("Single page");single->setCheckable(true);
        connect(single,&QAction::toggled,this,[this](bool on){view->setPageMode(on?QPdfView::PageMode::SinglePage:QPdfView::PageMode::MultiPage);});
        auto *full=reading->addAction("Full screen");full->setShortcut(QKeySequence("F11"));full->setCheckable(true);
        connect(full,&QAction::toggled,this,[this](bool on){if(on)showFullScreen();else showNormal();});
        auto *appearance=menuBar()->addMenu("&Appearance");
        auto *darkAction=appearance->addAction("Dark mode");
        darkAction->setCheckable(true);
        darkAction->setChecked(QSettings().value("dark",false).toBool());
        auto applyTheme=[this](bool on){
            dark=on;
            QFile sheet(on?":/theme/dark.qss":":/theme/light.qss");
            if(sheet.open(QIODevice::ReadOnly))setStyleSheet(QString::fromUtf8(sheet.readAll()));
            view->setStyleSheet(on?"background:#171e28;":"background:#dce3eb;");
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
            thumbnails->setCurrentRow(nav->currentPage());
            savePosition();refreshBookmarks();});
        zoom->setEditable(true);
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
        menuBar()->addMenu("&Help")->addAction("About",this,[this]{QMessageBox::about(this,"RAMD PDF","<h2>RAMD PDF</h2>Offline PDF workspace v0.9 Full<br>RAMD Studios");});
        history=QSettings().value("recent").toStringList();refreshRecent();
        restoreGeometry(QSettings().value("windowGeometry").toByteArray());
        operationProgress->setRange(0,0);operationProgress->setMaximumWidth(140);operationProgress->hide();
        statusBar()->addPermanentWidget(activity);statusBar()->addPermanentWidget(operationProgress);
        statusBar()->showMessage("Offline · Files stay on your computer");
        showHome();
        QTimer::singleShot(100, this, [this]{scanLibrary();});
    }
    void load(const QString &path){
        QFileInfo info(path);
        if(!info.isFile()||!info.isReadable()){QMessageBox::warning(this,"Open failed","File not found or unreadable.");return;}
        // Probe first so a bad file does not discard the current document.
        QPdfDocument probe;QString password;
        auto result=probe.load(info.absoluteFilePath());
        if(result==QPdfDocument::Error::IncorrectPassword) {
            bool ok=false;password=QInputDialog::getText(this,"Protected PDF","Password",QLineEdit::Password,{},&ok);
            if(!ok)return;
            probe.close();probe.setPassword(password);result=probe.load(info.absoluteFilePath());
        }
        if(result!=QPdfDocument::Error::None){QMessageBox::warning(this,"Open failed","This PDF could not be opened. Check its password or try another file.");return;}
        probe.close();savePosition();loadingDocument=true;
        doc.close();file=info.absoluteFilePath();searchInput->clear();doc.setPassword(password);
        if(doc.load(file)!=QPdfDocument::Error::None){file.clear();loadingDocument=false;showHome();QMessageBox::warning(this,"Open failed","The document could not be loaded.");return;}
        setWindowTitle(info.fileName()+" — RAMD PDF");
        pages->setCurrentWidget(readerPage);readerBar->show();thumbDock->show();
        history.removeAll(file);history.prepend(file);while(history.size()>30)history.removeLast();
        QSettings().setValue("recent",history);
        QSettings().setValue(documentKey()+"/lastOpened",QDateTime::currentDateTime());
        refreshRecent();
        if(!indexedFiles.contains(file))indexedFiles.prepend(file);
        updateLibrary();refreshBookmarks();
        const QString opened=file;
        const int saved=std::clamp(QSettings().value(documentKey()+"/page",0).toInt(),0,std::max(0,doc.pageCount()-1));
        QTimer::singleShot(0,this,[this,saved,opened]{if(file!=opened)return;view->pageNavigator()->jump(saved,QPointF(),0);loadingDocument=false;refreshBookmarks();});
        statusBar()->showMessage(saved?QString("Resumed %1 at page %2").arg(info.fileName()).arg(saved+1):"Opened "+info.fileName(),5000);
    }
    void showHome() {
        savePosition();
        if(library)updateLibrary();
        pages->setCurrentWidget(home);
        if(readerBar)readerBar->setVisible(false);
        thumbDock->setVisible(false);
        setWindowTitle("RAMD PDF — Library");
    }
    void selectCategory(const QString &category) {
        currentCategory=category;
        for(auto *button:navButtons)button->setChecked(button->property("section").toString()==category);
        updateLibrary();
    }
    void choosePdf() {
        const auto path=QFileDialog::getOpenFileName(this,"Open PDF",QDir::homePath(),"PDF documents (*.pdf)");
        if(!path.isEmpty())load(path);
    }
    void buildHome() {
        home->setObjectName("homePage");
        auto *shell=new QHBoxLayout(home);shell->setContentsMargins(0,0,0,0);shell->setSpacing(0);
        auto *sidebar=new QFrame(home);sidebar->setObjectName("sidebar");sidebar->setFixedWidth(192);
        auto *side=new QVBoxLayout(sidebar);side->setContentsMargins(16,24,16,18);side->setSpacing(6);
        auto *brand=new QHBoxLayout;
        auto *logo=new QLabel(sidebar);logo->setPixmap(QPixmap(":/brand/ramd-pdf.png").scaled(34,34,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        brand->addWidget(logo);auto *name=new QLabel("RAMD PDF",sidebar);name->setObjectName("brandName");brand->addWidget(name);brand->addStretch();side->addLayout(brand);
        auto *edition=new QLabel("WORKSPACE  /  0.9 FULL",sidebar);edition->setObjectName("sectionLabel");side->addSpacing(10);side->addWidget(edition);side->addSpacing(28);
        auto nav=[this,side,sidebar](const QString &key,const QString &label,QStyle::StandardPixmap icon) {
            auto *button=new QPushButton(style()->standardIcon(icon),label,sidebar);button->setObjectName("navButton");
            button->setProperty("section",key);button->setCheckable(true);button->setChecked(key=="Home");button->setMinimumHeight(40);
            side->addWidget(button);navButtons.append(button);connect(button,&QPushButton::clicked,this,[this,key]{selectCategory(key);});
        };
        nav("Home","Overview",QStyle::SP_ComputerIcon);nav("Library","All documents",QStyle::SP_DirIcon);
        nav("Recent","Recently opened",QStyle::SP_FileDialogContentsView);nav("Favorites","Favorites",QStyle::SP_DialogYesButton);
        side->addSpacing(24);
        auto *foldersTitle=new QLabel("LIBRARY FOLDERS",sidebar);foldersTitle->setObjectName("sectionLabel");side->addWidget(foldersTitle);
        auto *manage=new QPushButton("Manage folders…",sidebar);manage->setObjectName("locationButton");side->addWidget(manage);
        connect(manage,&QPushButton::clicked,this,[this]{manageFolders();});
        auto *refresh=new QPushButton(style()->standardIcon(QStyle::SP_BrowserReload),"Refresh library",sidebar);refresh->setObjectName("locationButton");side->addWidget(refresh);
        connect(refresh,&QPushButton::clicked,this,[this]{scanLibrary();});side->addStretch();
        auto *tip=new QLabel("LOCAL BY DEFAULT\nNo account. No upload.\n\nDrop a PDF to open it.",sidebar);tip->setObjectName("privacyText");tip->setWordWrap(true);side->addWidget(tip);
        shell->addWidget(sidebar);
        auto *body=new QWidget(home);body->setObjectName("dashboardBody");auto *outer=new QVBoxLayout(body);outer->setContentsMargins(28,26,28,20);outer->setSpacing(18);
        auto *heading=new QHBoxLayout;auto *titles=new QVBoxLayout;
        welcomeTitle=new QLabel("Your workspace",body);welcomeTitle->setObjectName("heroTitle");titles->addWidget(welcomeTitle);
        auto *subtitle=new QLabel("Read, collect, and work with your PDFs.",body);subtitle->setObjectName("mutedText");titles->addWidget(subtitle);heading->addLayout(titles);heading->addStretch();
        auto *open=new QPushButton(style()->standardIcon(QStyle::SP_DialogOpenButton),"Open PDF",body);open->setObjectName("primaryButton");connect(open,&QPushButton::clicked,this,[this]{choosePdf();});heading->addWidget(open);outer->addLayout(heading);
        resumeButton=new QPushButton(body);resumeButton->setObjectName("resumeButton");resumeButton->setMinimumHeight(65);resumeButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));resumeButton->hide();
        connect(resumeButton,&QPushButton::clicked,this,[this]{if(!history.isEmpty())load(history.first());});outer->addWidget(resumeButton);
        auto *toolsLayout=new QGridLayout;toolsLayout->setSpacing(10);
        auto tool=[this,body,toolsLayout](int column,const QString &label,const QString &tip,QStyle::StandardPixmap glyph,std::function<void()> action) {
            auto *button=new QPushButton(style()->standardIcon(glyph),label,body);button->setObjectName("toolCard");button->setMinimumHeight(48);button->setToolTip(tip);button->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
            toolsLayout->addWidget(button,column/2,column%2);connect(button,&QPushButton::clicked,this,[action]{action();});
        };
        tool(0,"New from template","Create a blank PDF, meeting notes, report, invoice or resume",QStyle::SP_FileIcon,[this]{newDocument();});
        tool(1,"Merge PDFs","Combine documents in the order you choose",QStyle::SP_FileDialogNewFolder,[this]{mergeDocuments();});
        tool(2,"Extract pages","Save selected pages as a new PDF",QStyle::SP_FileDialogDetailedView,[this]{extractPages();});
        tool(3,"Open a PDF to annotate","Add text, highlights, drawings and signatures",QStyle::SP_FileDialogContentsView,[this]{choosePdf();});
        outer->addLayout(toolsLayout);
        auto *filters=new QHBoxLayout;libraryFilter->setObjectName("librarySearch");libraryFilter->setPlaceholderText("Search by document name…");libraryFilter->setClearButtonEnabled(true);filters->addWidget(libraryFilter,1);
        sortOrder->addItems({"Recently modified","Name A–Z","Largest first","Recently opened"});sortOrder->setObjectName("librarySort");sortOrder->setAccessibleName("Sort documents");sortOrder->setMaximumWidth(170);filters->addWidget(sortOrder);
        connect(libraryFilter,&QLineEdit::textChanged,this,[this]{updateLibrary();});
        connect(sortOrder,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){updateLibrary();});outer->addLayout(filters);
        library->setObjectName("documentList");library->setColumnCount(4);library->setHeaderLabels({"Document","Last opened","Modified","Size"});library->setRootIsDecorated(false);library->setUniformRowHeights(true);library->setAlternatingRowColors(true);library->setSelectionMode(QAbstractItemView::SingleSelection);
        library->setSortingEnabled(false);library->setIconSize(QSize(23,23));library->setMinimumHeight(160);
        library->header()->setSectionResizeMode(0,QHeaderView::Stretch);for(int i=1;i<4;++i)library->header()->setSectionResizeMode(i,QHeaderView::ResizeToContents);
        connect(library,&QTreeWidget::itemActivated,this,[this](QTreeWidgetItem *item,int){load(item->data(0,Qt::UserRole).toString());});
        library->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(library,&QWidget::customContextMenuRequested,this,[this](const QPoint &point){
            auto *item=library->itemAt(point);if(!item)return;const auto path=item->data(0,Qt::UserRole).toString();
            auto favorites=QSettings().value("favorites").toStringList();QMenu menu(this);
            auto *open=menu.addAction("Open");auto *favorite=menu.addAction(favorites.contains(path)?"Remove from favorites":"Add to favorites");auto *reveal=menu.addAction("Open containing folder");
            const auto chosen=menu.exec(library->viewport()->mapToGlobal(point));
            if(chosen==open)load(path);
            else if(chosen==favorite){if(favorites.contains(path))favorites.removeAll(path);else favorites.append(path);QSettings().setValue("favorites",favorites);updateLibrary();}
            else if(chosen==reveal)QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        });
        outer->addWidget(library,1);emptyState->setObjectName("emptyState");emptyState->setAlignment(Qt::AlignCenter);emptyState->setWordWrap(true);emptyState->hide();outer->addWidget(emptyState);
        librarySummary->setObjectName("mutedText");outer->addWidget(librarySummary);shell->addWidget(body,1);
    }
    void manageFolders() {
        QDialog dialog(this);dialog.setWindowTitle("Library folders");dialog.resize(560,380);auto *layout=new QVBoxLayout(&dialog);
        auto *note=new QLabel("Documents and Downloads are included automatically. Add or remove extra folders below.",&dialog);note->setWordWrap(true);layout->addWidget(note);
        auto *list=new QListWidget(&dialog);list->addItems(QSettings().value("libraryFolders").toStringList());layout->addWidget(list);
        auto *row=new QHBoxLayout;auto *add=new QPushButton("Add folder…",&dialog);auto *remove=new QPushButton("Remove selected",&dialog);row->addWidget(add);row->addWidget(remove);layout->addLayout(row);
        connect(add,&QPushButton::clicked,&dialog,[&dialog,list]{auto folder=QFileDialog::getExistingDirectory(&dialog,"Add a folder");if(!folder.isEmpty() && list->findItems(folder,Qt::MatchExactly).isEmpty())list->addItem(folder);});
        connect(remove,&QPushButton::clicked,&dialog,[list]{delete list->takeItem(list->currentRow());});
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()!=QDialog::Accepted)return;
        QStringList folders;for(int i=0;i<list->count();++i)folders.append(list->item(i)->text());QSettings().setValue("libraryFolders",folders);scanLibrary();
    }
    void scanLibrary() {
        if(scanBusy)return;scanBusy=true;activity->setText("Scanning library…");
        QStringList folders;
        for(auto location:{QStandardPaths::DocumentsLocation,QStandardPaths::DownloadLocation}) {
            const auto folder=QStandardPaths::writableLocation(location);if(!folder.isEmpty()&&!folders.contains(folder))folders.append(folder);
        }
        for(const auto &folder:QSettings().value("libraryFolders").toStringList())if(!folders.contains(folder))folders.append(folder);
        const QStringList recentPaths=history;
        auto *watcher=new QFutureWatcher<QStringList>(this);
        connect(watcher,&QFutureWatcher<QStringList>::finished,this,[this,watcher]{
            indexedFiles=watcher->result();for(const auto &path:history)if(QFileInfo(path).isFile()&&!indexedFiles.contains(path))indexedFiles.append(path);
            scanBusy=false;if(!toolBusy)activity->clear();updateLibrary();watcher->deleteLater();
            statusBar()->showMessage(QString("Found %1 PDFs. Scan limit: 12,000 files per refresh.").arg(indexedFiles.size()),5000);
        });
        watcher->setFuture(QtConcurrent::run([folders,recentPaths]{
            QSet<QString> unique;for(const auto &path:recentPaths)if(QFileInfo(path).isFile())unique.insert(QFileInfo(path).absoluteFilePath());
            int inspected=0;
            for(const auto &folder:folders) {
                QDirIterator it(folder,QDir::Files|QDir::NoSymLinks|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
                while(inspected<12000 && it.hasNext()) {const auto path=it.next();++inspected;if(path.endsWith(".pdf",Qt::CaseInsensitive))unique.insert(path);}
                if(inspected>=12000)break;
            }
            return unique.values();
        }));
    }
    void updateLibrary() {
        if(!welcomeTitle)return;
        welcomeTitle->setText(currentCategory=="Home"?"Your workspace":currentCategory=="Library"?"All documents":currentCategory=="Recent"?"Recently opened":"Favorites");
        QString previous;
        if(library->currentItem())previous=library->currentItem()->data(0,Qt::UserRole).toString();
        library->clear();const auto query=libraryFilter->text().trimmed();const auto favorites=QSettings().value("favorites").toStringList();
        QStringList files=currentCategory=="Recent"?history:indexedFiles;
        if(currentCategory=="Favorites")files=favorites;
        const int sort=sortOrder->currentIndex();
        if(sort==3 || currentCategory=="Recent") {
            std::stable_sort(files.begin(),files.end(),[this](const QString &a,const QString &b){const int ia=history.indexOf(a),ib=history.indexOf(b);return (ia<0?100000:ia)<(ib<0?100000:ib);});
        } else std::sort(files.begin(),files.end(),[sort](const QString &a,const QString &b){
            const QFileInfo first(a),second(b);
            if(sort==1)return QString::localeAwareCompare(first.fileName(),second.fileName())<0;
            if(sort==2)return first.size()>second.size();
            return first.lastModified()>second.lastModified();
        });
        for(const auto &path:files) {
            QFileInfo info(path);if(!info.isFile()||!info.fileName().contains(query,Qt::CaseInsensitive))continue;
            const auto key="documents/"+QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(),QCryptographicHash::Sha256).toHex());
            const auto opened=QSettings().value(key+"/lastOpened").toDateTime();
            auto *item=new QTreeWidgetItem(library,{info.fileName(),opened.isValid()?opened.toString("dd MMM HH:mm"):"—",info.lastModified().toString("dd MMM yyyy"),QLocale().formattedDataSize(info.size())});
            item->setData(0,Qt::UserRole,path);item->setToolTip(0,path);item->setIcon(0,style()->standardIcon(favorites.contains(path)?QStyle::SP_DialogYesButton:QStyle::SP_FileIcon));
            if(path==previous)library->setCurrentItem(item);
        }
        const int visible=library->topLevelItemCount();
        emptyState->setVisible(visible==0);
        emptyState->setText(!query.isEmpty()?"No matching documents. Try a different name.":currentCategory=="Favorites"?"Your favorites will appear here. Right-click a document to add it.":"No documents yet. Open a PDF, drop one here, or add a library folder.");
        librarySummary->setText(QString("%1 documents  ·  Double-click or press Enter to open  ·  Right-click for options").arg(visible));
        if(resumeButton) {
            const bool available=!history.isEmpty()&&QFileInfo(history.first()).isFile();resumeButton->setVisible(available&&currentCategory=="Home");
            if(available){const auto path=history.first();const auto key="documents/"+QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(),QCryptographicHash::Sha256).toHex());resumeButton->setText("Continue reading\n"+QFileInfo(path).fileName()+QString("  ·  Page %1").arg(QSettings().value(key+"/page",0).toInt()+1));resumeButton->setToolTip(path);}
        }
    }
    void startPdfTool(const QStringList &inputs,const QString &range,const QString &output) {
        if(toolBusy){QMessageBox::information(this,"PDF tools","Please wait for the current operation.");return;}
        toolBusy=true;operationProgress->show();activity->setText("Creating PDF…");
        auto *watcher=new QFutureWatcher<Ramd::ToolResult>(this);
        connect(watcher,&QFutureWatcher<Ramd::ToolResult>::finished,this,[this,watcher,output]{
            const auto result=watcher->result();watcher->deleteLater();toolBusy=false;operationProgress->hide();activity->setText(scanBusy?"Scanning library…":"");
            if(!result.ok){QMessageBox::warning(this,"Could not create PDF",result.error);return;}
            statusBar()->showMessage("Saved "+QFileInfo(output).fileName(),5000);load(output);
        });
        watcher->setFuture(QtConcurrent::run([inputs,range,output]{return Ramd::assembleDocuments(inputs,range,output);}));
    }
    void mergeDocuments() {
        if(toolBusy){QMessageBox::information(this,"PDF tools","Please wait for the current operation.");return;}
        QDialog dialog(this);dialog.setWindowTitle("Merge PDFs");dialog.resize(620,450);auto *layout=new QVBoxLayout(&dialog);
        auto *hint=new QLabel("Add PDFs, then arrange them in the order you want. Source files remain unchanged.",&dialog);hint->setWordWrap(true);layout->addWidget(hint);
        auto *list=new QListWidget(&dialog);list->setDragDropMode(QAbstractItemView::InternalMove);layout->addWidget(list);
        auto *row=new QHBoxLayout;auto *add=new QPushButton("Add PDFs…",&dialog);auto *remove=new QPushButton("Remove",&dialog);auto *up=new QPushButton("Move up",&dialog);auto *down=new QPushButton("Move down",&dialog);for(auto *button:{add,remove,up,down})row->addWidget(button);layout->addLayout(row);
        connect(add,&QPushButton::clicked,&dialog,[&dialog,list]{for(const auto &path:QFileDialog::getOpenFileNames(&dialog,"Choose PDFs",QDir::homePath(),"PDF (*.pdf)")){auto *item=new QListWidgetItem(QFileInfo(path).fileName(),list);item->setData(Qt::UserRole,path);item->setToolTip(path);}});
        connect(remove,&QPushButton::clicked,&dialog,[list]{delete list->takeItem(list->currentRow());});
        auto move=[list](int delta){const int row=list->currentRow(),target=row+delta;if(row<0||target<0||target>=list->count())return;auto *item=list->takeItem(row);list->insertItem(target,item);list->setCurrentRow(target);};
        connect(up,&QPushButton::clicked,&dialog,[move]{move(-1);});connect(down,&QPushButton::clicked,&dialog,[move]{move(1);});
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);buttons->button(QDialogButtonBox::Save)->setText("Merge and save…");layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,[&dialog,list]{if(list->count()<2){QMessageBox::information(&dialog,"Add documents","Choose at least two PDFs.");return;}dialog.accept();});connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()!=QDialog::Accepted)return;
        QStringList inputs;for(int i=0;i<list->count();++i)inputs.append(list->item(i)->data(Qt::UserRole).toString());
        auto output=QFileDialog::getSaveFileName(this,"Save merged PDF",QDir::homePath()+"/Merged.pdf","PDF (*.pdf)");if(output.isEmpty())return;if(!output.endsWith(".pdf",Qt::CaseInsensitive)){output+=".pdf";if(QFileInfo::exists(output) && QMessageBox::question(this,"Replace PDF?","Replace the existing file "+output+"?",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;}
        startPdfTool(inputs,{},output);
    }
    void extractPages() {
        if(toolBusy){QMessageBox::information(this,"PDF tools","Please wait for the current operation.");return;}
        const auto input=QFileDialog::getOpenFileName(this,"Choose a PDF to extract pages from",file.isEmpty()?QDir::homePath():file,"PDF (*.pdf)");if(input.isEmpty())return;
        QPdfDocument probe;if(probe.load(input)!=QPdfDocument::Error::None){QMessageBox::warning(this,"Cannot extract","Choose an unlocked, readable PDF.");return;}
        const int total=probe.pageCount();probe.close();bool ok=false;
        const auto range=QInputDialog::getText(this,"Extract pages",QString("This document has %1 pages. Enter a range, for example 1-3,5.").arg(total),QLineEdit::Normal,"1",&ok);if(!ok)return;
        QVector<int> selected;QString error;if(!Ramd::parsePageRange(range,total,selected,error)){QMessageBox::warning(this,"Check the page range",error);return;}
        auto output=QFileDialog::getSaveFileName(this,"Save extracted pages",QFileInfo(input).absolutePath()+"/Extracted.pdf","PDF (*.pdf)");if(output.isEmpty())return;if(!output.endsWith(".pdf",Qt::CaseInsensitive)){output+=".pdf";if(QFileInfo::exists(output) && QMessageBox::question(this,"Replace PDF?","Replace the existing file "+output+"?",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;}
        startPdfTool({input},range,output);
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
        auto writer=std::make_unique<QPdfWriter>(target);
        writer->setPageSize(QPageSize(QPageSize::A4));
        writer->setResolution(144);
        writer->setTitle(chosen);
        writer->setCreator("RAMD PDF by RAMD Studios");
        QPainter painter(writer.get());
        if(!painter.isActive()) { QMessageBox::warning(this,"Save failed","Cannot create PDF here.");return; }
        const QRect pageRect=writer->pageLayout().paintRectPixels(writer->resolution());
        const int left=pageRect.left()+90;
        const int width=pageRect.width()-180;
        if(chosen=="Blank A4") { painter.end(); writer.reset(); load(target); return; }
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
        writer.reset();
        if(!QFileInfo(target).exists() || QFileInfo(target).size()==0) {
            QMessageBox::warning(this,"Save failed","The PDF was not created.");return;
        }
        load(target);
        QMessageBox::information(this,"PDF created",
            "Your starter PDF was created. Use Edit PDF (Ctrl+E) to add text, highlights, drawing or a handwritten signature.");
    }
protected:
    void closeEvent(QCloseEvent *event) override {
        if(toolBusy){QMessageBox::information(this,"Finishing PDF","Please wait for the current PDF operation to finish.");event->ignore();return;}
        savePosition();QSettings().setValue("windowGeometry",saveGeometry());QMainWindow::closeEvent(event);
    }
    void dragEnterEvent(QDragEnterEvent *event) override {
        const auto urls=event->mimeData()->urls();
        if(urls.size()==1 && urls.first().isLocalFile() && urls.first().toLocalFile().endsWith(".pdf",Qt::CaseInsensitive))event->acceptProposedAction();
    }
    void dropEvent(QDropEvent *event) override {
        const auto urls=event->mimeData()->urls();
        if(urls.size()==1 && urls.first().isLocalFile() && urls.first().toLocalFile().endsWith(".pdf",Qt::CaseInsensitive)){load(urls.first().toLocalFile());event->acceptProposedAction();}
    }
private:
    void refreshRecent(){recent->clear();for(const auto &p:history){auto *a=recent->addAction(QFileInfo(p).fileName());a->setToolTip(p);connect(a,&QAction::triggered,this,[this,p]{load(p);});}if(history.isEmpty())recent->addAction("No recent files")->setEnabled(false);}
};
#ifndef RAMD_NO_MAIN
int main(int argc,char **argv){
    QApplication app(argc,argv);
    app.setWindowIcon(QIcon(":/brand/ramd-pdf.png"));
    QCoreApplication::setOrganizationName("RAMD Studios");
    QCoreApplication::setApplicationName("RAMD PDF");
    Reader reader;reader.show();
    if(argc>1)reader.load(QString::fromLocal8Bit(argv[1]));
    return app.exec();
}


#endif
