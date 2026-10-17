#pragma once
#include <QtWidgets>
#include <poppler-qt6.h>
#include <poppler-annotation.h>
#include <memory>
#include <functional>

namespace Ramd {
struct Mark {
    enum Kind { Text, Highlight, Ink } kind = Text;
    QRectF rect;
    QVector<QPointF> points;
    QString text;
    QColor color = QColor("#172033");
    double size = 14;
};
struct Sheet {
    QString source;
    int page = 0;
    QVector<Mark> marks;
};
using State = QVector<Sheet>;
// Input PDFs are private session copies. QSaveFile commits the validated output atomically.
bool exportPdf(const State &state, const QString &target, QString &error);
class Canvas : public QWidget {
public:
    QImage image;
    QSizeF points;
    QVector<Mark> marks;
    int tool = 0; // select, text, highlight, ink, signature
    QColor ink = QColor("#172033");
    double fontSize = 14;
    int selected = -1;
    std::function<void(Mark)> added;
    std::function<void(int)> editRequested;
    explicit Canvas(QWidget *parent = nullptr);
    void setPage(QImage image, QSizeF pointSize, const QVector<Mark> &items);
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
private:
    QPointF start;
    Mark draft;
    bool drawing = false;
    QPointF normalized(QPointF pos) const;
    void paintMark(QPainter &, const Mark &);
};
class Editor : public QDialog {
public:
    explicit Editor(const QString &file, QWidget *parent = nullptr);
    QString savedPath;
    bool ready = false;
protected:
    void closeEvent(QCloseEvent *) override;
    void reject() override;
private:
    QTemporaryDir session;
    State state;
    QUndoStack undo;
    QListWidget *list;
    Canvas *canvas;
    QScrollArea *scroll;
    QLabel *status;
    QString target;
    int serial = 0;
    double zoom = 1;
    bool busy = false;
    bool importFile(const QString &, State &, QString &);
    void change(const State &, const QString &);
    void refresh(int selected = -1);
    void render();
    bool save(bool as);
    bool canClose();
    void append();
    void movePage(int delta);
    void deletePage();
    void deleteMark();
    void editMark(int index);
};
}

