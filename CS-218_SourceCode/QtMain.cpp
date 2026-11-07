// Qt GUI frontend. All editing logic lives in the backend classes; this file only
// builds the window and forwards user actions to them.
#include <QtWidgets>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QTimer>
#include <QTextBlock>
#include <QRegularExpression>
#include <algorithm>
#include <functional>
#ifdef CS218_HAS_SVG
#include <QSvgRenderer>
#endif
#include "TextDocument.h"
#include "EditOperations.h"
#include "TextSearch.h"
#include "TextReplace.h"
#include "FileManager.h"

namespace gui {

// ---------------------------------------------------------------- theme colours
// Return the complete palette used by widgets and generated icons for a theme.
QMap<QString, QString> themeColors(bool dark) {
    if (dark) return {
        {"bg1", "#17122f"}, {"bg2", "#0e0a21"}, {"card", "#1f1941"}, {"cardBorder", "#352c66"},
        {"text", "#ebe7ff"}, {"muted", "#a39cd3"}, {"accent", "#8266ff"}, {"accentHover", "#957dff"},
        {"accentText", "#ffffff"}, {"soft", "#322965"}, {"softHover", "#3f3380"}, {"softText", "#d9d1ff"},
        {"tool", "#271f50"}, {"toolHover", "#32286a"}, {"toolBorder", "#3a306f"},
        {"input", "#17122f"}, {"inputBorder", "#3a306f"}, {"hl", "#6b5a22"}, {"hlCur", "#b07a1c"},
        {"notice", "#3d3420"}, {"noticeText", "#f3dca0"}, {"tabBar", "#191433"}, {"tab", "#1f1941"},
        {"sel", "#4b3fa0"}, {"disabledBg", "#241d46"}, {"disabledFg", "#8a83bd"},
        {"icon_nav", "#ffffff"}, {"icon_tool", "#a893ff"}, {"icon_primary", "#ffffff"},
        {"icon_soft", "#d9d1ff"}, {"icon_muted", "#a39cd3"}, {"icon_accent", "#a893ff"},
        {"sideTop", "#120a35"}, {"sideBottom", "#2b1672"}, {"switchOn", "#8266ff"}, {"switchOff", "#4b3fa0"}};
    return {
        {"bg1", "#f3efff"}, {"bg2", "#e3dbff"}, {"card", "#ffffff"}, {"cardBorder", "#ddd5fb"},
        {"text", "#2a2152"}, {"muted", "#7a72a6"}, {"accent", "#4f2fd0"}, {"accentHover", "#6240e4"},
        {"accentText", "#ffffff"}, {"soft", "#ddd4fb"}, {"softHover", "#cdc1f7"}, {"softText", "#3d2b9c"},
        {"tool", "#faf8ff"}, {"toolHover", "#eee9ff"}, {"toolBorder", "#ddd5fb"},
        {"input", "#ffffff"}, {"inputBorder", "#d9d1f7"}, {"hl", "#ffe3a0"}, {"hlCur", "#ffb84d"},
        {"notice", "#fff4d6"}, {"noticeText", "#614314"}, {"tabBar", "#f3f0ff"}, {"tab", "#ffffff"},
        {"sel", "#cfc7ff"}, {"disabledBg", "#efecfa"}, {"disabledFg", "#9a93c6"},
        {"icon_nav", "#ffffff"}, {"icon_tool", "#4f2fd0"}, {"icon_primary", "#ffffff"},
        {"icon_soft", "#3d2b9c"}, {"icon_muted", "#7a72a6"}, {"icon_accent", "#4f2fd0"},
        {"sideTop", "#1a0c4c"}, {"sideBottom", "#4a22b8"}, {"switchOn", "#4f2fd0"}, {"switchOff", "#cfc6f5"}};
}

// ---------------------------------------------------------------- icons (Qt SVG is optional)
// Render a named SVG icon when QtSvg is available; otherwise return an empty pixmap.
QPixmap svgPixmap(const QString &name, const QString &color, int size) {
#ifdef CS218_HAS_SVG
    static const QHash<QString, QString> shapes = {
        {"home", R"S(<path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><polyline points="9 22 9 12 15 12 15 22"/>)S"},
        {"file-plus", R"S(<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="12" y1="18" x2="12" y2="12"/><line x1="9" y1="15" x2="15" y2="15"/>)S"},
        {"file", R"S(<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/>)S"},
        {"file-minus", R"S(<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="9" y1="15" x2="15" y2="15"/>)S"},
        {"file-text", R"S(<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="8" y1="13" x2="16" y2="13"/><line x1="8" y1="17" x2="16" y2="17"/>)S"},
        {"folder", R"S(<path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/>)S"},
        {"save", R"S(<path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/>)S"},
        {"settings", R"S(<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"/>)S"},
        {"help", R"S(<circle cx="12" cy="12" r="10"/><path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3"/><line x1="12" y1="17" x2="12.01" y2="17"/>)S"},
        {"undo", R"S(<polyline points="1 4 1 10 7 10"/><path d="M3.51 15a9 9 0 1 0 2.13-9.36L1 10"/>)S"},
        {"redo", R"S(<polyline points="23 4 23 10 17 10"/><path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10"/>)S"},
        {"history", R"S(<polyline points="1 4 1 10 7 10"/><path d="M3.51 15a9 9 0 1 0 2.13-9.36L1 10"/><polyline points="12 7 12 12 15 14"/>)S"},
        {"corner-undo", R"S(<polyline points="9 14 4 9 9 4"/><path d="M20 20v-7a4 4 0 0 0-4-4H4"/>)S"},
        {"search", R"S(<circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/>)S"},
        {"trash", R"S(<polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/><line x1="10" y1="11" x2="10" y2="17"/><line x1="14" y1="11" x2="14" y2="17"/>)S"},
        {"plus", R"S(<line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>)S"},
        {"plus-square", R"S(<rect x="3" y="3" width="18" height="18" rx="3"/><line x1="12" y1="8" x2="12" y2="16"/><line x1="8" y1="12" x2="16" y2="12"/>)S"},
        {"x", R"S(<line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>)S"},
        {"minus", R"S(<line x1="5" y1="12" x2="19" y2="12"/>)S"},
        {"square", R"S(<rect x="5" y="5" width="14" height="14" rx="1"/>)S"},
        {"dashed", R"S(<rect x="3" y="3" width="18" height="18" rx="3" stroke-dasharray="3.2 3.2"/>)S"},
        {"chevron-down", R"S(<polyline points="6 9 12 15 18 9"/>)S"},
        {"chevron-up", R"S(<polyline points="18 15 12 9 6 15"/>)S"},
        {"chevron-right", R"S(<polyline points="9 18 15 12 9 6"/>)S"},
        {"sun", R"S(<circle cx="12" cy="12" r="5"/><line x1="12" y1="1" x2="12" y2="3"/><line x1="12" y1="21" x2="12" y2="23"/><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/><line x1="1" y1="12" x2="3" y2="12"/><line x1="21" y1="12" x2="23" y2="12"/><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/>)S"},
        {"moon", R"S(<path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/>)S"},
        {"layers", R"S(<polygon points="12 2 2 7 12 12 22 7 12 2"/><polyline points="2 17 12 22 22 17"/><polyline points="2 12 12 17 22 12"/>)S"},
        {"newline", R"S(<polyline points="9 10 4 15 9 20"/><path d="M20 4v7a4 4 0 0 1-4 4H4"/>)S"},
        {"backspace", R"S(<path d="M21 4H8l-7 8 7 8h13a2 2 0 0 0 2-2V6a2 2 0 0 0-2-2z"/><line x1="18" y1="9" x2="12" y2="15"/><line x1="12" y1="9" x2="18" y2="15"/>)S"},
        {"type", R"S(<polyline points="4 7 4 4 20 4 20 7"/><line x1="9" y1="20" x2="15" y2="20"/><line x1="12" y1="4" x2="12" y2="20"/>)S"},
        {"play", R"S(<polygon points="6 3 20 12 6 21 6 3" fill="@c@"/>)S"},
        {"repeat", R"S(<polyline points="17 1 21 5 17 9"/><path d="M3 11V9a4 4 0 0 1 4-4h14"/><polyline points="7 23 3 19 7 15"/><path d="M21 13v2a4 4 0 0 1-4 4H3"/>)S"},
        {"swap", R"S(<polyline points="16 3 21 8 16 13"/><line x1="21" y1="8" x2="3" y2="8"/><polyline points="8 21 3 16 8 11"/><line x1="3" y1="16" x2="21" y2="16"/>)S"},
        {"window-plus", R"S(<rect x="3" y="4" width="18" height="16" rx="2"/><line x1="3" y1="9" x2="21" y2="9"/><line x1="12" y1="12" x2="12" y2="18"/><line x1="9" y1="15" x2="15" y2="15"/>)S"},
        {"save-as", R"S(<path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><line x1="9" y1="5.5" x2="13" y2="5.5"/><line x1="11" y1="3.5" x2="11" y2="7.5"/>)S"},
        {"log-out", R"S(<path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><polyline points="16 17 21 12 16 7"/><line x1="21" y1="12" x2="9" y2="12"/>)S"},
    };
    const auto it = shapes.constFind(name);
    if (it == shapes.constEnd()) return {};
    QString body = it.value();
    body.replace("@c@", color);
    const QString svg = QString("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='%1' "
                                "stroke-width='2' stroke-linecap='round' stroke-linejoin='round'>%2</svg>").arg(color, body);
    QSvgRenderer renderer(svg.toUtf8());
    const qreal dpr = std::max<qreal>(2.0, qApp->devicePixelRatio());
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    renderer.render(&p, QRectF(0, 0, size, size));
    return pm;
#else
    Q_UNUSED(name); Q_UNUSED(color); Q_UNUSED(size);
    return {};
#endif
}

QPoint mousePos(const QMouseEvent *e) {
    // Keep mouse handling source-compatible with both Qt 5 and Qt 6 event APIs.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

// ---------------------------------------------------------------- small custom widgets
// Paints the sidebar background and its low-contrast decorative waves.
class Sidebar : public QWidget {
public:
    QColor top{"#1a0c4c"}, bottom{"#4a22b8"};
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0, top); g.setColorAt(1, bottom);
        p.fillRect(rect(), g);
        // soft decorative waves near the bottom
        for (int i = 0; i < 2; ++i) {
            QPainterPath path;
            const qreal base = height() * (0.62 + i * 0.12), w = width();
            path.moveTo(0, base);
            path.cubicTo(w * 0.35, base - 70 + i * 25, w * 0.65, base + 60 - i * 30, w, base - 30);
            path.lineTo(w, height()); path.lineTo(0, height()); path.closeSubpath();
            p.fillPath(path, QColor(170, 120, 255, 26 + i * 14));
        }
    }
};

class Logo : public QWidget {
public:
    Logo() { setFixedSize(38, 38); }
protected:
    // App icon: violet squircle with a glossy top, a white page with a folded corner,
    // three text lines and an amber caret (the "editor" cue). Everything scales with the widget size.
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const qreal w = r.width(), h = r.height(), rad = w * 0.29;

        QLinearGradient bg(r.topLeft(), r.bottomRight());
        bg.setColorAt(0.0, QColor("#b78bff")); bg.setColorAt(0.55, QColor("#7a4dff")); bg.setColorAt(1.0, QColor("#4a22d8"));
        p.setPen(Qt::NoPen); p.setBrush(bg);
        p.drawRoundedRect(r, rad, rad);

        QPainterPath clip; clip.addRoundedRect(r, rad, rad);
        p.save(); p.setClipPath(clip);
        QLinearGradient gloss(0, r.top(), 0, r.top() + h * 0.55);
        gloss.setColorAt(0.0, QColor(255, 255, 255, 80)); gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillRect(QRectF(r.left(), r.top(), w, h * 0.55), gloss);
        p.restore();

        const QRectF page(r.left() + w * 0.27, r.top() + h * 0.20, w * 0.46, h * 0.60);
        const qreal fold = w * 0.15;
        QPainterPath doc;
        doc.moveTo(page.topLeft());
        doc.lineTo(page.right() - fold, page.top());
        doc.lineTo(page.right(), page.top() + fold);
        doc.lineTo(page.bottomRight());
        doc.lineTo(page.bottomLeft());
        doc.closeSubpath();
        p.setBrush(QColor(255, 255, 255, 245)); p.drawPath(doc);

        QPainterPath corner;
        corner.moveTo(page.right() - fold, page.top());
        corner.lineTo(page.right() - fold, page.top() + fold);
        corner.lineTo(page.right(), page.top() + fold);
        corner.closeSubpath();
        p.setBrush(QColor("#cdb9ff")); p.drawPath(corner);

        const qreal lx = page.left() + w * 0.07, lh = h * 0.055;
        p.setBrush(QColor("#6a3df0"));
        p.drawRoundedRect(QRectF(lx, page.top() + h * 0.24, page.width() * 0.55, lh), lh / 2, lh / 2);
        p.drawRoundedRect(QRectF(lx, page.top() + h * 0.35, page.width() * 0.70, lh), lh / 2, lh / 2);
        p.drawRoundedRect(QRectF(lx, page.top() + h * 0.46, page.width() * 0.38, lh), lh / 2, lh / 2);
        p.setBrush(QColor("#ffb84d"));
        p.drawRoundedRect(QRectF(lx + page.width() * 0.38 + w * 0.03, page.top() + h * 0.435, w * 0.04, h * 0.085), 1, 1);
    }
};

// Supplies the frameless window's draggable title area and maximize gesture.
class HeaderBar : public QWidget {
protected:
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton && window()->windowHandle()) window()->windowHandle()->startSystemMove();
    }
    void mouseDoubleClickEvent(QMouseEvent *) override {
        window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
    }
};

// A bar that never forces the window wider (its size hint is ignored) and reports its real width,
// so the owner can switch to an icon-only layout instead of overflowing small screens.
class ResponsiveBar : public QFrame {
public:
    std::function<void(int)> onWidth;
    ResponsiveBar() { setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed); }
protected:
    void resizeEvent(QResizeEvent *e) override {
        QFrame::resizeEvent(e);
        if (onWidth) onWidth(width());
    }
};

class ToggleSwitch : public QAbstractButton {
public:
    QColor onColor{"#4f2fd0"}, offColor{"#cfc6f5"};
    QPixmap sunPixmap, moonPixmap;
    ToggleSwitch() { setCheckable(true); setCursor(Qt::PointingHandCursor); setFixedSize(56, 28); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(isChecked() ? onColor : offColor);
        p.drawRoundedRect(rect(), height() / 2.0, height() / 2.0);
        const int d = height() - 6, x = isChecked() ? width() - d - 3 : 3;
        p.setBrush(Qt::white);
        p.drawEllipse(QRect(x, 3, d, d));
        const QPixmap &pm = isChecked() ? moonPixmap : sunPixmap;
        if (!pm.isNull()) p.drawPixmap(QRect(x + 5, 8, d - 10, d - 10), pm);
    }
};

// Plain-text editor whose gutter tracks visible blocks as the viewport scrolls.
class CodeEditor : public QPlainTextEdit {
    // The child widget delegates painting so the gutter stays clipped to its own area.
    class Gutter : public QWidget {
        CodeEditor *editor;
    public:
        explicit Gutter(CodeEditor *e) : QWidget(e), editor(e) {}
        QSize sizeHint() const override { return {editor->gutterWidth(), 0}; }
    protected:
        void paintEvent(QPaintEvent *ev) override { editor->paintGutter(ev); }
    };
    Gutter *gutter;
public:
    QColor gutterText{"#8b83b9"}, gutterLine{"#ddd5fb"}, gutterBack{"#ffffff"};
    CodeEditor() : gutter(new Gutter(this)) {
        connect(this, &QPlainTextEdit::blockCountChanged, this, [this] { updateMargin(); });
        connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect &r, int dy) {
            if (dy) gutter->scroll(0, dy); else gutter->update(0, r.y(), gutter->width(), r.height());
            if (r.contains(viewport()->rect())) updateMargin();
        });
        updateMargin();
    }
    int gutterWidth() const {
        // Reserve enough digits for the current number of text blocks.
        int digits = 2, max = std::max(1, blockCount());
        while (max >= 100) { max /= 10; ++digits; }
        return 30 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    }
    void updateMargin() { setViewportMargins(gutterWidth(), 0, 0, 0); }
    void refreshGutter() { gutter->update(); }
    void paintGutter(QPaintEvent *ev) {
        // Paint only visible line numbers, keeping the gutter efficient while scrolling.
        QPainter p(gutter);
        p.fillRect(ev->rect(), gutterBack);
        p.setPen(gutterLine);
        p.drawLine(gutter->width() - 1, ev->rect().top(), gutter->width() - 1, ev->rect().bottom());
        QTextBlock block = firstVisibleBlock();
        int number = block.blockNumber();
        int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
        int bottom = top + qRound(blockBoundingRect(block).height());
        p.setPen(gutterText);
        while (block.isValid() && top <= ev->rect().bottom()) {
            if (block.isVisible() && bottom >= ev->rect().top())
                p.drawText(0, top, gutter->width() - 14, fontMetrics().height(), Qt::AlignRight, QString::number(number + 1));
            block = block.next();
            top = bottom;
            bottom = top + qRound(blockBoundingRect(block).height());
            ++number;
        }
    }
protected:
    void resizeEvent(QResizeEvent *e) override {
        QPlainTextEdit::resizeEvent(e);
        const QRect cr = contentsRect();
        gutter->setGeometry(QRect(cr.left(), cr.top(), gutterWidth(), cr.height()));
    }
};

}  // namespace gui
using namespace gui;

// ================================================================ main window
// Owns the GUI, synchronizes Qt text with the backend document, and routes all edits
// through the undoable backend services.
class EditorWindow : public QMainWindow {
    // ---- backend (unchanged) ----
    TextDocument doc;
    EditOperations edits{doc, 50};
    TextSearch search{doc};
    TextReplace replacer{doc, edits};
    std::string filename;
    bool modified = false, syncing = false, darkMode = false;
    FileManager files{doc, filename, modified};

    // ---- widgets ----
    CodeEditor *editor = nullptr;
    QLineEdit *findInput = nullptr, *replaceInput = nullptr, *insertInput = nullptr;
    QListWidget *matchList = nullptr;
    QSpinBox *insertLine = nullptr, *insertColumn = nullptr, *rangeLine = nullptr, *rangeFrom = nullptr, *rangeTo = nullptr,
             *deleteFrom = nullptr, *deleteTo = nullptr;
    QCheckBox *caseBox = nullptr, *wholeBox = nullptr;
    QLabel *searchMessage = nullptr, *historyLabel = nullptr, *readyTitle = nullptr, *readySub = nullptr, *tabLabel = nullptr;
    QPushButton *replaceOne = nullptr, *replaceAll = nullptr, *undoButton = nullptr, *undoLast = nullptr, *maxButton = nullptr;
    ToggleSwitch *themeSwitch = nullptr;
    Sidebar *sidebar = nullptr;

    // ---- File commands (shared by the sidebar, the File menu and the keyboard shortcuts) ----
    QAction *actNew = nullptr, *actNewWindow = nullptr, *actOpen = nullptr,
            *actSave = nullptr, *actSaveAs = nullptr, *actExit = nullptr;

    // ---- responsive layout state ----
    struct LabelledButton { QPushButton *b; QString text; QString tip; };
    QVector<LabelledButton> navButtons, toolButtons;
    QVBoxLayout *sideLayout = nullptr;
    QLabel *brandLabel = nullptr;
    QToolButton *encButton = nullptr;
    ResponsiveBar *toolbarBar = nullptr;
    QHBoxLayout *toolbarLayout = nullptr;
    QWidget *rightCol = nullptr;
    QBoxLayout *readyLayout = nullptr;
    QLabel *readyIcon = nullptr;
    int toolbarNeed = 0, sideW = 284;
    bool sidebarCompact = false, toolbarCompact = false, readyShort = false, screenHooked = false;

    struct Hit { int line, col, len; std::size_t byteCol, byteLen; };
    QVector<Hit> hits;
    int currentHit = 0;

    QMap<QString, QString> tk;                       // current theme colours + asset paths
    QVector<std::function<void()>> iconUpdaters;     // re-applied whenever the theme changes

    // ---- small builders ----
    QLabel *label(const QString &text, const char *name = nullptr) {
        auto *l = new QLabel(text);
        if (name) l->setObjectName(name);
        return l;
    }
    QSpinBox *number(int value = 1, bool compact = true) {
        auto *s = new QSpinBox; s->setRange(1, 1000000); s->setValue(value); s->setMinimumWidth(60);
        s->setFocusPolicy(Qt::StrongFocus);   // mouse wheel scrolls the page instead of changing the value
        if (compact) s->setMaximumWidth(74);
        return s;
    }
    void bindIcon(QAbstractButton *b, const QString &name, const QString &role, int size = 18) {
        b->setIconSize(QSize(size, size));
        auto apply = [=] { b->setIcon(QIcon(svgPixmap(name, tk["icon_" + role], size))); };
        apply(); iconUpdaters << apply;
    }
    QLabel *iconLabel(const QString &name, const QString &role, int size) {
        auto *l = new QLabel; l->setFixedSize(size, size);
        auto apply = [=] { l->setPixmap(svgPixmap(name, tk["icon_" + role], size)); };
        apply(); iconUpdaters << apply;
        return l;
    }
    QPushButton *button(const QString &text, const QString &role, const QString &icon = QString()) {
        auto *b = new QPushButton(text); b->setObjectName(role); b->setCursor(Qt::PointingHandCursor);
        if (!icon.isEmpty()) bindIcon(b, icon, role == "tool" ? "tool" : role);
        return b;
    }
    QToolButton *iconButton(const QString &icon, const QString &tip, const QString &role = "muted", int size = 18) {
        auto *b = new QToolButton; b->setObjectName("icon"); b->setToolTip(tip); b->setCursor(Qt::PointingHandCursor);
        bindIcon(b, icon, role, size);
        return b;
    }
    // A titled, collapsible card.
    QFrame *card(const QString &title, const QString &icon, QWidget *body, const QString &openChevron) {
        auto *f = new QFrame; f->setObjectName("card");
        auto *v = new QVBoxLayout(f); v->setContentsMargins(12, 10, 12, 12); v->setSpacing(8);
        auto *head = new QHBoxLayout; head->setSpacing(8);
        auto *chev = new QToolButton; chev->setObjectName("icon"); chev->setCursor(Qt::PointingHandCursor);
        chev->setIconSize(QSize(16, 16)); chev->setToolTip("Collapse / expand");
        auto apply = [=] { chev->setIcon(QIcon(svgPixmap(body->isHidden() ? "chevron-down" : openChevron, tk["icon_muted"], 16))); };
        apply(); iconUpdaters << apply;
        connect(chev, &QToolButton::clicked, f, [=] { body->setVisible(body->isHidden()); apply(); });
        head->addWidget(iconLabel(icon, "accent", 20)); head->addWidget(label(title, "cardTitle")); head->addStretch(); head->addWidget(chev);
        v->addLayout(head); v->addWidget(body);
        return f;
    }

    // ---- dialogs / file handling ----
    void showError(const QString &message) { QMessageBox::warning(this, "CS-218 Editor", message); }
    template <class F> void safely(F f) {
        try { f(); }
        catch (const std::exception &e) { showError(QString::fromUtf8(e.what())); }
        catch (...) { showError("An unexpected error occurred."); }
    }
    bool askSave() {
        if (!modified) return true;
        auto choice = QMessageBox::warning(this, "Unsaved changes", "Save changes before continuing?",
                                           QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (choice == QMessageBox::Cancel) return false;
        if (choice == QMessageBox::Save) return save(false);
        return true;
    }
    // Save (as == false) writes to the current file, or asks for a name when the document is still
    // unnamed. Save As (as == true) always asks. Returns true only when the file was really written.
    bool save(bool as) {
        QString path = QString::fromStdString(filename);
        if (as || path.isEmpty()) {
            // The file dialog itself asks before replacing an existing file, so we do not ask again.
            QFileDialog dialog(this, as ? "Save As" : "Save", path.isEmpty() ? QString("Untitled.txt") : path,
                               "Text files (*.txt);;All files (*)");
            dialog.setAcceptMode(QFileDialog::AcceptSave);
            dialog.setFileMode(QFileDialog::AnyFile);
            dialog.setDefaultSuffix("txt");
            if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) return false;   // cancelled: not an error
            path = dialog.selectedFiles().first();
            if (path.isEmpty()) return false;
        }
        const std::string target = path.toUtf8().toStdString();
        bool ok = false, threw = true;      // 'threw' stays true if the backend raises: safely() reports that error itself
        safely([&] {
            ok = (as || target != filename) ? files.saveAs(target, false) : files.saveFile();
            threw = false;
        });
        if (!ok && !threw) showError("Save was not completed. Check that the file name is valid.");
        updateStatus();
        return ok;
    }
    // New Window: starts another, independent copy of the editor (its own document and undo history).
    void newWindow() {
        QStringList args;
        if (!isMaximized() && !isFullScreen()) args << QString("--pos=%1,%2").arg(x() + 32).arg(y() + 32);   // cascade
        if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), args)) showError("Could not open a new window.");
    }
    // Each File command is ONE QAction. The sidebar buttons, the File menu and the keyboard shortcut all
    // trigger that same object, so clicking and typing the shortcut can never behave differently.
    QAction *makeAction(const QString &text, const QKeySequence &keys, std::function<void()> fn) {
        auto *a = new QAction(text, this);
        a->setShortcut(keys);
        a->setShortcutContext(Qt::WindowShortcut);
        connect(a, &QAction::triggered, this, [fn] { fn(); });
        addAction(a);       // makes the shortcut work wherever the focus is inside this window
        return a;
    }
    void createActions() {
        actNew       = makeAction("New",        QKeySequence("Ctrl+N"),       [this] { newDocument(); });
        actNewWindow = makeAction("New Window", QKeySequence("Ctrl+Shift+N"), [this] { newWindow(); });
        actOpen      = makeAction("Open…",      QKeySequence("Ctrl+O"),       [this] { openDocument(); });
        actSave      = makeAction("Save",       QKeySequence("Ctrl+S"),       [this] { save(false); });
        actSaveAs    = makeAction("Save As…",   QKeySequence("Ctrl+Shift+S"), [this] { save(true); });
        actExit      = makeAction("Exit",       QKeySequence("Ctrl+Q"),       [this] { close(); });
    }
    void newDocument() {
        if (!askSave()) return;
        safely([&] { files.createNewDocument(false); edits.clearUndoHistory(); loadEditor(); });
    }
    void openDocument() {
        if (!askSave()) return;
        const QString p = QFileDialog::getOpenFileName(this, "Open document", {}, "Text files (*.txt);;All files (*)");
        if (p.isEmpty()) return;
        safely([&] {
            if (files.openFile(p.toStdString(), false)) { edits.clearUndoHistory(); loadEditor(); }
            else showError("Invalid filename.");
        });
    }

    // ---- editor <-> document sync ----
    void loadEditor() {
        // Suppress textChanged while refreshing from the backend and retain the cursor.
        syncing = true;
        auto cursor = editor->textCursor();
        int pos = cursor.position();
        editor->setPlainText(QString::fromStdString(doc.getAllText()));
        cursor = editor->textCursor();
        cursor.setPosition(std::min(pos, editor->document()->characterCount() - 1));
        editor->setTextCursor(cursor);
        syncing = false;
        refreshSearch();
        updateStatus();
    }
    void updateStatus() {
        // Derive labels and action availability from the current editor/document state.
        const QString content = editor->toPlainText();
        const int words = content.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
        const auto c = editor->textCursor();
        const QString name = filename.empty() ? QString("Untitled") : QFileInfo(QString::fromStdString(filename)).fileName();
        const QString tabText = name + (modified ? " *" : "");
        tabLabel->setText(QFontMetrics(tabLabel->font()).elidedText(tabText, Qt::ElideMiddle, 200));
        tabLabel->setToolTip(tabText);
        setWindowTitle(name + (modified ? " *" : "") + " – CS-218 Editor");
        if (content.isEmpty()) {
            readyTitle->setText("Ready to Edit!");
            readySub->setText("Use the tools above to start working on your document.");
        } else {
            readyTitle->setText(modified ? "Editing" : "All changes saved");
            readySub->setText(QString("Ln %1, Col %2\n%3 lines  ·  %4 words  ·  %5 characters")
                                  .arg(c.blockNumber() + 1).arg(c.positionInBlock() + 1)
                                  .arg(editor->document()->blockCount()).arg(words).arg(content.size()));
        }
        undoButton->setEnabled(!edits.undoIsEmpty());
        undoLast->setEnabled(!edits.undoIsEmpty());
        historyLabel->setText(QString("%1 of 50 operations").arg(edits.undoCount()));
    }
    void afterEdit(bool ok) {
        if (!ok) { showError("Operation could not be completed. Check the line/column or undo capacity."); return; }
        modified = true;
        loadEditor();
    }
    // Undo goes through the backend's bounded undo stack.
    void doUndo() {
        if (edits.undoIsEmpty()) return;
        safely([&] {
            if (edits.undoLastOperation()) { modified = true; loadEditor(); }
            else showError("Operation could not be completed.");
        });
    }
    // Converts a UTF-16 column of the editor into the byte offset the document uses.
    bool byteOffset(int line, int column, std::size_t &out) {
        // Qt columns count UTF-16 code units; the backend indexes UTF-8 bytes.
        if (line < 1 || static_cast<std::size_t>(line) > doc.lineCount() || column < 0) return false;
        const QString text = QString::fromStdString(doc.getLine(line));
        if (column > text.size()) return false;
        out = static_cast<std::size_t>(text.left(column).toUtf8().size());
        return true;
    }
    std::size_t cursorByteColumn() {
        // Convert the current UTF-16 cursor column to the backend's UTF-8 byte offset.
        const auto c = editor->textCursor();
        return static_cast<std::size_t>(QString::fromStdString(doc.getLine(c.blockNumber() + 1)).left(c.positionInBlock()).toUtf8().size());
    }

    // ---- search / replace ----
    void refreshSearch() {
        // Recompute backend matches, their Qt display positions, and the result list.
        hits.clear();
        const QString term = findInput->text();
        SearchStatus status = SearchStatus::NotFound;
        if (!term.isEmpty()) {
            std::vector<Match> found;
            const std::string pattern = term.toUtf8().toStdString();
            status = search.searchText(pattern, found, caseBox->isChecked(), wholeBox->isChecked());
            for (const Match &m : found) {
                const std::string raw = doc.getLine(m.line);
                const int col = QString::fromUtf8(raw.data(), static_cast<qsizetype>(m.column)).size();
                hits.push_back({static_cast<int>(m.line), col, static_cast<int>(term.size()), m.column, pattern.size()});
            }
        }
        if (currentHit >= hits.size()) currentHit = 0;

        // Summary message: whether the text occurs, how often, and on which lines.
        QString msg;
        if (status == SearchStatus::EmptyDocument) msg = "The document is empty. Nothing to search.";
        else if (hits.isEmpty()) msg = QString("\"%1\" was not found in the document.").arg(term);
        else {
            QStringList lines;
            int last = -1;
            for (const Hit &h : hits) {
                if (h.line == last) continue;
                last = h.line;
                if (lines.size() == 8) { lines << "…"; break; }
                lines << QString::number(h.line);
            }
            msg = QString("Found %1 match%2 for \"%3\" on line%4 %5")
                      .arg(hits.size()).arg(hits.size() == 1 ? "" : "es").arg(term)
                      .arg(lines.size() == 1 ? "" : "s").arg(lines.join(", "));
        }
        searchMessage->setVisible(!term.isEmpty());
        searchMessage->setText(msg);

        // One row per occurrence: "Line L, Column C" (1-based, same as the Ln/Col status). Click to jump.
        {
            QSignalBlocker block(matchList);
            matchList->clear();
            for (const Hit &h : hits) {
                auto *item = new QListWidgetItem(QString("Line %1, Column %2").arg(h.line).arg(h.col + 1));
                item->setToolTip(QString::fromStdString(doc.getLine(h.line)).trimmed());
                matchList->addItem(item);
            }
            matchList->setVisible(!hits.isEmpty());
            if (!hits.isEmpty()) matchList->setCurrentRow(currentHit);
        }
        replaceOne->setEnabled(!hits.isEmpty());
        replaceAll->setEnabled(!hits.isEmpty());
        paintHighlights();
        updateStatus();
    }
    void paintHighlights() {
        // Mark every hit, using a distinct color for the currently selected result.
        QList<QTextEdit::ExtraSelection> selections;
        for (int i = 0; i < hits.size(); ++i) {
            const Hit h = hits[i];
            QTextEdit::ExtraSelection s;
            auto b = editor->document()->findBlockByNumber(h.line - 1);
            s.cursor = QTextCursor(b);
            s.cursor.setPosition(b.position() + h.col);
            s.cursor.setPosition(b.position() + h.col + h.len, QTextCursor::KeepAnchor);
            s.format.setBackground(QColor(tk[i == currentHit ? "hlCur" : "hl"]));
            selections << s;
        }
        editor->setExtraSelections(selections);
    }
    void selectHit(int index) {
        // Move the editor cursor to a result and keep the list/highlighting in sync.
        if (index < 0 || index >= hits.size()) return;
        currentHit = index;
        const Hit h = hits[index];
        auto b = editor->document()->findBlockByNumber(h.line - 1);
        QTextCursor c(b);
        c.setPosition(b.position() + h.col);
        c.setPosition(b.position() + h.col + h.len, QTextCursor::KeepAnchor);
        editor->setTextCursor(c);
        if (matchList) { QSignalBlocker block(matchList); matchList->setCurrentRow(index); }
        paintHighlights();
    }
    void showSearchNotice(const QString &text) { searchMessage->setText(text); searchMessage->setVisible(true); }
    void findNext() {
        // Choose the next result after the cursor, wrapping to the first match.
        if (findInput->text().isEmpty()) { showSearchNotice("Enter the text to search for."); return; }
        if (editor->toPlainText().isEmpty()) { showSearchNotice("The document is empty. Nothing to search."); return; }
        if (hits.isEmpty()) return;
        const auto c = editor->textCursor();
        const int start = std::min(c.anchor(), c.position());
        const auto block = editor->document()->findBlock(start);
        const int line = block.blockNumber() + 1, col = start - block.position();
        int next = 0;
        for (int i = 0; i < hits.size(); ++i) {
            const bool after = c.hasSelection() ? (hits[i].line > line || (hits[i].line == line && hits[i].col > col))
                                                : (hits[i].line > line || (hits[i].line == line && hits[i].col >= col));
            if (after) { next = i; break; }
        }
        selectHit(next);
    }
    bool replaceHit(int index) {
        // Replace the exact matched bytes, including original case in insensitive searches.
        if (index < 0 || index >= hits.size()) return false;
        const Hit h = hits[index];
        const std::string actual = doc.getLine(h.line).substr(h.byteCol, h.byteLen);   // the text really found (case may differ)
        const std::string replacement = replaceInput->text().toUtf8().toStdString();
        if (actual == replacement) return false;                                         // nothing to change
        const ReplaceResult r = replacer.replaceOccurrenceAt(Match{static_cast<std::size_t>(h.line), h.byteCol}, actual, replacement);
        if (r.status != ReplaceStatus::Success) throw std::runtime_error(TextReplace::describe(r.status));
        return true;
    }

    // ---- theme ----
    // One folder per running window: a second window (File > New Window) must not overwrite these files.
    static QString assetDirPath() {
        return QDir::temp().filePath(QString("cs218_editor_ui_%1").arg(QCoreApplication::applicationPid()));
    }
    void makeAssets(bool dark) {
        // Generate the small raster controls used by stylesheets and spin boxes.
        QDir dir(assetDirPath());
        dir.mkpath(".");
        const QString tag = dark ? "dark" : "light";
        const QColor col(dark ? "#b9a8ff" : "#5b3bd8");
        auto chevron = [&](const QString &file, bool up) {
            QImage im(40, 40, QImage::Format_ARGB32); im.fill(Qt::transparent);
            QPainter p(&im); p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(col, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPolygonF pts;
            if (up) pts << QPointF(8, 26) << QPointF(20, 14) << QPointF(32, 26);
            else pts << QPointF(8, 14) << QPointF(20, 26) << QPointF(32, 14);
            p.drawPolyline(pts);
            im.save(dir.filePath(file));
        };
        chevron("up_" + tag + ".png", true);
        chevron("down_" + tag + ".png", false);
        QImage im(40, 40, QImage::Format_ARGB32); im.fill(Qt::transparent);
        QPainter p(&im); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(Qt::white, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF() << QPointF(10, 21) << QPointF(17, 28) << QPointF(30, 12));
        p.end();
        im.save(dir.filePath("check.png"));
        tk["upArrow"] = QDir::fromNativeSeparators(dir.filePath("up_" + tag + ".png"));
        tk["downArrow"] = QDir::fromNativeSeparators(dir.filePath("down_" + tag + ".png"));
        tk["checkImg"] = QDir::fromNativeSeparators(dir.filePath("check.png"));
    }
    void applyTheme(bool dark) {
        // Update colors, generated assets, widget styles, and the saved preference together.
        darkMode = dark;
        tk = themeColors(dark);
        makeAssets(dark);
        QString css = R"S(
            QMainWindow{background:@bg2@}
            QWidget#root{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 @bg1@,stop:1 @bg2@)}
            QWidget{color:@text@;font-size:12px}
            QLabel{background:transparent}
            QFrame#card,QFrame#toolbar{background:@card@;border:1px solid @cardBorder@;border-radius:14px}
            QLabel#cardTitle{font-size:14px;font-weight:700}
            QLabel#fieldLabel{font-size:11px;color:@muted@;font-weight:600}
            QLabel#tagline{font-size:13px;font-weight:600}
            QLabel#readyTitle{font-size:17px;font-weight:800}
            QLabel#readySub{color:@muted@;font-size:11px}
            QLabel#brand{color:white;font-size:12px;font-weight:700}
            QLabel#searchMessage{background:@notice@;color:@noticeText@;border-radius:8px;padding:6px 9px}
            QListWidget#matchList{background:@input@;border:1px solid @inputBorder@;border-radius:9px;padding:3px;outline:0}
            QListWidget#matchList::item{padding:4px 8px;border-radius:6px}
            QListWidget#matchList::item:hover:!selected{background:@soft@}
            QListWidget#matchList::item:selected{background:@accent@;color:@accentText@}
            QFrame#sep{background:@cardBorder@;border:0;max-height:1px;min-height:1px}
            QFrame#vsep{background:@cardBorder@;border:0;max-width:1px;min-width:1px}
            QFrame#tabBar{background:@tabBar@;border:0;border-bottom:1px solid @cardBorder@;border-radius:0;border-top-left-radius:13px;border-top-right-radius:13px}
            QFrame#tabItem{background:@tab@;border:1px solid @cardBorder@;border-bottom:0;border-top-left-radius:10px;border-top-right-radius:10px}
            QPlainTextEdit{background:transparent;border:0;selection-background-color:@sel@;selection-color:@text@}
            QPushButton{font-weight:600}
            QMessageBox QPushButton,QDialog QPushButton,QDialogButtonBox QPushButton{background:@soft@;color:@softText@;border:1px solid @toolBorder@;border-radius:9px;padding:7px 18px;min-width:72px;font-weight:700}
            QMessageBox QPushButton:hover,QDialog QPushButton:hover,QDialogButtonBox QPushButton:hover{background:@softHover@}
            QMessageBox QPushButton:default,QDialogButtonBox QPushButton:default{background:@accent@;color:@accentText@;border:0}
            QMessageBox QPushButton:default:hover,QDialogButtonBox QPushButton:default:hover{background:@accentHover@}
            QMessageBox QPushButton:disabled,QDialog QPushButton:disabled{background:@disabledBg@;color:@disabledFg@}
            QMessageBox QLabel,QDialog QLabel,QDialog QCheckBox{color:@text@}
            QPushButton#tool{background:@tool@;border:1px solid @toolBorder@;border-radius:10px;padding:7px 8px;color:@text@}
            QPushButton#tool:hover{background:@toolHover@}
            QPushButton#tool:disabled{color:@disabledFg@;background:@disabledBg@}
            QPushButton#tool::menu-indicator{image:url(@downArrow@);width:10px;height:10px;subcontrol-origin:padding;subcontrol-position:right center;right:8px}
            QPushButton#primary{background:@accent@;border:0;border-radius:10px;padding:9px 12px;color:@accentText@;font-weight:700}
            QPushButton#primary:hover{background:@accentHover@}
            QPushButton#primary:disabled{background:@disabledBg@;color:@disabledFg@}
            QPushButton#soft{background:@soft@;border:0;border-radius:10px;padding:9px 12px;color:@softText@;font-weight:700}
            QPushButton#soft:hover{background:@softHover@}
            QPushButton#soft:disabled{background:@disabledBg@;color:@disabledFg@}
            QPushButton#nav{background:transparent;color:white;border:0;border-radius:12px;padding:10px 10px;text-align:left;font-size:13px;font-weight:600}
            QPushButton#nav:hover{background:rgba(255,255,255,0.10)}
            QPushButton#nav[active="true"]{background:rgba(255,255,255,0.20)}
            QPushButton#nav[compact="true"]{text-align:center;padding:10px 0}
            QScrollArea#contentScroll{background:transparent;border:0}
            QScrollArea#contentScroll > QWidget > QWidget{background:transparent}
            QToolButton#icon{background:transparent;border:0;border-radius:9px;padding:6px}
            QToolButton#icon:hover{background:@soft@}
            QToolButton#win,QToolButton#winClose{background:transparent;border:0;border-radius:0;padding:0;min-width:46px;min-height:36px}
            QToolButton#win:hover{background:@soft@}
            QToolButton#winClose:hover{background:#e5484d}
            QToolButton#fileMenu{background:transparent;color:@text@;border:0;border-radius:9px;padding:6px 12px;font-size:13px;font-weight:700}
            QToolButton#fileMenu:hover,QToolButton#fileMenu:pressed{background:@soft@}
            QToolButton#fileMenu::menu-indicator{image:none}
            QMenu::separator{height:1px;background:@cardBorder@;margin:5px 8px}
            QToolButton#enc{background:transparent;color:white;border:0;padding:6px 8px;font-weight:600;font-size:12px}
            QToolButton#enc::menu-indicator{image:none}
            QLineEdit{background:@input@;border:1px solid @inputBorder@;border-radius:9px;padding:7px 9px;selection-background-color:@sel@;selection-color:@text@}
            QLineEdit:focus,QSpinBox:focus{border:1px solid @accent@}
            QSpinBox{background:@input@;border:1px solid @inputBorder@;border-radius:9px;padding:5px 20px 5px 7px;min-height:20px;selection-background-color:@sel@;selection-color:@text@}
            QSpinBox::up-button{subcontrol-origin:border;subcontrol-position:top right;width:18px;border:0;margin:3px 3px 0 0}
            QSpinBox::down-button{subcontrol-origin:border;subcontrol-position:bottom right;width:18px;border:0;margin:0 3px 3px 0}
            QSpinBox::up-arrow{image:url(@upArrow@);width:9px;height:9px}
            QSpinBox::down-arrow{image:url(@downArrow@);width:9px;height:9px}
            QCheckBox{spacing:8px}
            QCheckBox::indicator{width:17px;height:17px;border-radius:5px;border:1.5px solid @inputBorder@;background:@input@}
            QCheckBox::indicator:checked{background:@accent@;border-color:@accent@;image:url(@checkImg@)}
            QMenu{background:@card@;border:1px solid @cardBorder@;border-radius:10px;padding:6px}
            QMenu::item{padding:7px 20px;border-radius:7px}
            QMenu::item:selected{background:@soft@}
            QMenu::item:disabled{color:@disabledFg@}
            QToolTip{background:@card@;color:@text@;border:1px solid @cardBorder@;padding:4px}
            QMessageBox,QDialog{background:@card@}
            QScrollBar:vertical{background:transparent;width:10px;margin:2px}
            QScrollBar::handle:vertical{background:@sel@;border-radius:4px;min-height:30px}
            QScrollBar::handle:vertical:hover,QScrollBar::handle:horizontal:hover{background:@accent@}
            QScrollBar:horizontal{background:transparent;height:10px;margin:2px}
            QScrollBar::handle:horizontal{background:@sel@;border-radius:4px;min-width:30px}
            QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0}
            QScrollBar::add-page,QScrollBar::sub-page{background:transparent}
        )S";
        for (auto it = tk.constBegin(); it != tk.constEnd(); ++it) css.replace("@" + it.key() + "@", it.value());
        setStyleSheet(css);
        {
            QPalette ap;
            const QColor text(tk["text"]), card(tk["card"]), muted(tk["muted"]), off(tk["disabledFg"]);
            ap.setColor(QPalette::Window, card);              ap.setColor(QPalette::WindowText, text);
            ap.setColor(QPalette::Base, QColor(tk["input"])); ap.setColor(QPalette::AlternateBase, QColor(tk["tool"]));
            ap.setColor(QPalette::Text, text);
            ap.setColor(QPalette::Button, QColor(tk["soft"])); ap.setColor(QPalette::ButtonText, QColor(tk["softText"]));
            ap.setColor(QPalette::Highlight, QColor(tk["accent"])); ap.setColor(QPalette::HighlightedText, QColor(tk["accentText"]));
            ap.setColor(QPalette::ToolTipBase, card);         ap.setColor(QPalette::ToolTipText, text);
            ap.setColor(QPalette::Link, QColor(tk["accent"]));
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
            ap.setColor(QPalette::PlaceholderText, muted);
#endif
            for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) ap.setColor(QPalette::Disabled, role, off);
            qApp->setPalette(ap);
        }

        sidebar->top = QColor(tk["sideTop"]); sidebar->bottom = QColor(tk["sideBottom"]); sidebar->update();
        themeSwitch->onColor = QColor(tk["switchOn"]); themeSwitch->offColor = QColor(tk["switchOff"]);
        themeSwitch->sunPixmap = svgPixmap("sun", "#f5a000", 16); themeSwitch->moonPixmap = svgPixmap("moon", "#4f2fd0", 16);
        themeSwitch->update();
        editor->gutterBack = QColor(tk["card"]); editor->gutterLine = QColor(tk["cardBorder"]); editor->gutterText = QColor(tk["muted"]);
        QPalette pal = editor->palette();
        pal.setColor(QPalette::PlaceholderText, QColor(tk["muted"]));
        editor->setPalette(pal);
        editor->update(); editor->refreshGutter();
        for (auto &f : iconUpdaters) f();
        paintHighlights();
        QSettings().setValue("darkMode", dark);
    }
    void applyEditorFont() {
        QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        f.setPointSize(12);                                   // fixed editor font size
        editor->setFont(f);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);      // long lines scroll sideways
        editor->setTabStopDistance(4 * QFontMetricsF(f).horizontalAdvance(' '));
        editor->updateMargin();
    }
    void showSettings() {
        QDialog d(this);
        d.setWindowTitle("Settings");
        auto *form = new QFormLayout(&d);
        auto *dark = new QCheckBox("Dark theme"); dark->setChecked(darkMode);
        form->addRow(dark);
        auto *ok = new QPushButton("Done"); ok->setObjectName("primary"); form->addRow(ok);
        connect(ok, &QPushButton::clicked, &d, &QDialog::accept);
        connect(dark, &QCheckBox::toggled, themeSwitch, &QAbstractButton::setChecked);
        d.exec();
    }
    void showHelp() {
        QMessageBox::information(this, "Help",
            "<b>Keyboard shortcuts</b><br>"
            "Ctrl+N New &nbsp; Ctrl+Shift+N New window &nbsp; Ctrl+O Open<br>"
            "Ctrl+S Save &nbsp; Ctrl+Shift+S Save as &nbsp; Ctrl+Q Exit<br>"
            "Ctrl+Z Undo &nbsp; Ctrl+F Find &nbsp; Ctrl+H Replace &nbsp; F3 Find next<br>"
            "Every file command is also in the <b>File</b> menu (top left) and in the sidebar.<br><br>"
            "<b>Tips</b><br>"
            "• Type directly in the editor, or use the cards below it to insert/delete at exact positions.<br>"
            "• Undo keeps the newest 50 operations. Replace All is a single undo step.<br>"
            "• Files are read and saved as UTF-8; line endings (LF/CRLF) are preserved.");
    }

    // ---- window frame (custom title bar) ----
    Qt::Edges edgesAt(const QPoint &p) const {
        Qt::Edges e;
        if (isMaximized() || isFullScreen()) return e;
        const int m = 6;
        if (p.x() < m) e |= Qt::LeftEdge;
        if (p.x() >= width() - m) e |= Qt::RightEdge;
        if (p.y() < m) e |= Qt::TopEdge;
        if (p.y() >= height() - m) e |= Qt::BottomEdge;
        return e;
    }

    // ---- UI construction ----
    QWidget *buildSidebar() {
        sidebar = new Sidebar; sidebar->setFixedWidth(160);
        sideLayout = new QVBoxLayout(sidebar); sideLayout->setContentsMargins(10, 0, 10, 12); sideLayout->setSpacing(4);
        auto *v = sideLayout;
        auto *brand = new QWidget; brand->setFixedHeight(56);
        auto *b = new QHBoxLayout(brand); b->setContentsMargins(0, 0, 0, 0); b->setSpacing(8);
        brandLabel = label("<span style='font-size:14px;font-weight:800'>CS-218</span><br>"
                           "<span style='font-size:10px;font-weight:600;color:#cdbfff'>TEXT EDITOR</span>", "brand");
        b->addStretch(1); b->addWidget(new Logo); b->addWidget(brandLabel); b->addStretch(1);
        v->addWidget(brand); v->addSpacing(14);
        // 'act' (optional) supplies the shortcut that is shown in the button's tooltip.
        auto nav = [&](const QString &text, const QString &icon, std::function<void()> fn, const QAction *act = nullptr, bool active = false) {
            auto *btn = new QPushButton(text); btn->setObjectName("nav"); btn->setCursor(Qt::PointingHandCursor);
            btn->setProperty("active", active);
            bindIcon(btn, icon, "nav", 22);
            connect(btn, &QPushButton::clicked, this, [fn] { fn(); });
            QString tip = text;
            if (act && !act->shortcut().isEmpty()) tip += QString("  (%1)").arg(act->shortcut().toString(QKeySequence::NativeText));
            btn->setToolTip(tip);
            v->addWidget(btn);
            navButtons << LabelledButton{btn, text, tip};
        };
        nav("Home", "home", [this] { editor->setFocus(); }, nullptr, true);
        nav("New", "file-plus", [this] { actNew->trigger(); }, actNew);
        nav("New Window", "window-plus", [this] { actNewWindow->trigger(); }, actNewWindow);
        nav("Open", "folder", [this] { actOpen->trigger(); }, actOpen);
        nav("Save", "save", [this] { actSave->trigger(); }, actSave);
        nav("Save As", "save-as", [this] { actSaveAs->trigger(); }, actSaveAs);
        nav("Settings", "settings", [this] { showSettings(); });
        nav("Help", "help", [this] { showHelp(); });
        v->addStretch();
        nav("Exit", "log-out", [this] { actExit->trigger(); }, actExit);
        encButton = new QToolButton; encButton->setObjectName("enc"); encButton->setText("●  UTF-8  ▾"); encButton->setCursor(Qt::PointingHandCursor);
        encButton->setPopupMode(QToolButton::InstantPopup);
        auto *menu = new QMenu(encButton);
        auto *utf8 = menu->addAction("UTF-8 (the only supported encoding)"); utf8->setCheckable(true); utf8->setChecked(true); utf8->setEnabled(false);
        encButton->setMenu(menu);
        v->addWidget(encButton);
        return sidebar;
    }
    // Narrow windows: the sidebar shrinks to an icon-only rail so the editor keeps its room.
    void setSidebarCompact(bool compact) {
        // Preserve navigation while reclaiming width on narrow windows.
        if (!sidebar || compact == sidebarCompact) return;
        sidebarCompact = compact;
        sidebar->setFixedWidth(compact ? 64 : 160);
        sideLayout->setContentsMargins(compact ? 8 : 10, 0, compact ? 8 : 10, 12);
        brandLabel->setVisible(!compact);
        for (const LabelledButton &n : navButtons) {
            n.b->setText(compact ? QString() : n.text);
            n.b->setToolTip(!n.tip.isEmpty() ? n.tip : (compact ? n.text : QString()));
            n.b->setProperty("compact", compact);
            n.b->style()->unpolish(n.b); n.b->style()->polish(n.b);
        }
        encButton->setText(compact ? QString("●") : QString("●  UTF-8  ▾"));
        encButton->setToolTip(compact ? QString("UTF-8") : QString());
    }
    QWidget *buildHeader() {
        auto *bar = new HeaderBar; bar->setFixedHeight(56);
        auto *h = new QHBoxLayout(bar); h->setContentsMargins(18, 0, 0, 0); h->setSpacing(10);
        auto *fileButton = new QToolButton; fileButton->setObjectName("fileMenu"); fileButton->setText("File");
        fileButton->setCursor(Qt::PointingHandCursor); fileButton->setPopupMode(QToolButton::InstantPopup);
        fileButton->setToolTip("File menu");
        auto *fileMenu = new QMenu(fileButton);
        fileMenu->addAction(actNew); fileMenu->addAction(actNewWindow); fileMenu->addAction(actOpen);
        fileMenu->addAction(actSave); fileMenu->addAction(actSaveAs);
        fileMenu->addSeparator();
        fileMenu->addAction(actExit);
        fileButton->setMenu(fileMenu);
        h->addWidget(fileButton);
        auto *div = new QFrame; div->setObjectName("vsep"); div->setFixedHeight(18);
        h->addWidget(div); h->addWidget(label("Text Processing Made Easy", "tagline")); h->addStretch();
        auto win = [&](const QString &icon, const char *name, std::function<void()> fn) {
            auto *btn = new QToolButton; btn->setObjectName(name); btn->setProperty("x", 1); btn->setCursor(Qt::ArrowCursor);
            bindIcon(btn, icon, "muted", 16);
            connect(btn, &QToolButton::clicked, this, [fn] { fn(); });
            h->addWidget(btn);
            return btn;
        };
        win("minus", "win", [this] { showMinimized(); });
        win("square", "win", [this] { isMaximized() ? showNormal() : showMaximized(); });
        win("x", "winClose", [this] { close(); });
        return bar;
    }
    QWidget *buildToolbar() {
        auto *bar = new ResponsiveBar; bar->setObjectName("toolbar"); toolbarBar = bar;
        auto *h = new QHBoxLayout(bar); toolbarLayout = h; h->setContentsMargins(10, 8, 10, 8); h->setSpacing(5);
        undoButton = button("Undo", "tool", "undo");
        toolButtons << LabelledButton{undoButton, "Undo"};
        h->addWidget(undoButton);
        auto *sep = new QFrame; sep->setObjectName("vsep"); sep->setFixedHeight(24); h->addWidget(sep);
        auto *insertBtn = button("Insert", "tool", "type");
        toolButtons << LabelledButton{insertBtn, "Insert"};
        auto *menu = new QMenu(insertBtn);
        menu->addAction("Text at cursor…", this, [this] { insertInput->setFocus(); });
        menu->addAction("New line above", this, [this] { safely([&] { afterEdit(edits.insertLine(editor->textCursor().blockNumber() + 1, "")); }); });
        menu->addAction("New line below", this, [this] { safely([&] { afterEdit(edits.insertLine(editor->textCursor().blockNumber() + 2, "")); }); });
        insertBtn->setMenu(menu); insertBtn->setStyleSheet("padding-right:26px"); h->addWidget(insertBtn);
        auto tool = [&](const QString &text, const QString &icon, std::function<void()> fn) {
            auto *btn = button(text, "tool", icon);
            connect(btn, &QPushButton::clicked, this, [fn] { fn(); });
            h->addWidget(btn);
            toolButtons << LabelledButton{btn, text};
        };
        tool("New line", "newline", [this] { safely([&] { afterEdit(edits.insertLine(editor->textCursor().blockNumber() + 2, "")); }); });
        tool("Delete character", "backspace", [this] { safely([&] { afterEdit(edits.deleteCharacter(editor->textCursor().blockNumber() + 1, cursorByteColumn())); }); });
        tool("Delete word", "file-minus", [this] { safely([&] { afterEdit(edits.deleteWord(editor->textCursor().blockNumber() + 1, cursorByteColumn())); }); });
        tool("Delete range", "dashed", [this] { rangeFrom->setFocus(); });
        tool("Delete lines", "file-text", [this] { deleteFrom->setFocus(); });
        h->addStretch();
        auto *find = iconButton("search", "Find (Ctrl+F)", "tool", 20);
        connect(find, &QToolButton::clicked, this, [this] { findInput->setFocus(); findInput->selectAll(); });
        h->addWidget(find);
        auto *sep2 = new QFrame; sep2->setObjectName("vsep"); sep2->setFixedHeight(24); h->addWidget(sep2);
        themeSwitch = new ToggleSwitch; themeSwitch->setToolTip("Light / dark theme"); h->addWidget(themeSwitch);
        bar->onWidth = [this](int w) { fitToolbar(w); };
        return bar;
    }
    // Toolbar buttons keep their labels only while the whole row fits; otherwise they become icon-only.
    void setToolbarCompact(bool compact) {
        if (compact == toolbarCompact) return;
        toolbarCompact = compact;
        for (const LabelledButton &t : toolButtons) {
            t.b->setText(compact ? QString() : t.text);
            t.b->setToolTip(compact ? t.text : QString());
        }
    }
    void fitToolbar(int available) {
        if (!toolbarLayout || available < 50) return;
        if (toolbarNeed <= 0) {                 // measure the labelled layout once
            setToolbarCompact(false);
            toolbarNeed = toolbarLayout->totalSizeHint().width();
        }
        setToolbarCompact(available < toolbarNeed);
    }
    QWidget *buildEditorCard() {
        auto *f = new QFrame; f->setObjectName("card");
        auto *v = new QVBoxLayout(f); v->setContentsMargins(1, 1, 1, 1); v->setSpacing(0);
        auto *tabs = new QFrame; tabs->setObjectName("tabBar"); tabs->setFixedHeight(42);
        auto *t = new QHBoxLayout(tabs); t->setContentsMargins(10, 8, 12, 0); t->setSpacing(6);
        auto *item = new QFrame; item->setObjectName("tabItem");
        auto *ti = new QHBoxLayout(item); ti->setContentsMargins(12, 0, 4, 0); ti->setSpacing(8);
        tabLabel = label("Untitled"); tabLabel->setStyleSheet("font-weight:700");
        auto *close = iconButton("x", "Close document", "muted", 14);
        connect(close, &QToolButton::clicked, this, [this] { newDocument(); });
        ti->addWidget(iconLabel("dashed", "accent", 16)); ti->addWidget(tabLabel); ti->addWidget(close);
        auto *plus = iconButton("plus", "New document (Ctrl+N)", "muted", 18);
        connect(plus, &QToolButton::clicked, this, [this] { actNew->trigger(); });
        auto *saveAs = iconButton("save-as", "Save as… (Ctrl+Shift+S)", "muted", 18);
        connect(saveAs, &QToolButton::clicked, this, [this] { actSaveAs->trigger(); });
        t->addWidget(item); t->addWidget(plus); t->addStretch(); t->addWidget(saveAs);
        editor = new CodeEditor; editor->setObjectName("editor"); editor->setUndoRedoEnabled(false);
        editor->setPlaceholderText("Start typing your document here…");
        editor->installEventFilter(this);
        v->addWidget(tabs); v->addWidget(editor, 1);
        return f;
    }
    QWidget *buildBottomCards() {
        auto *row = new QWidget; auto *h = new QHBoxLayout(row); h->setContentsMargins(0, 0, 0, 0); h->setSpacing(12);

        // Insert / Delete
        auto *ib = new QWidget; auto *ig = new QGridLayout(ib); ig->setContentsMargins(0, 0, 0, 0); ig->setHorizontalSpacing(6); ig->setVerticalSpacing(4);
        insertLine = number(); insertColumn = number(); insertInput = new QLineEdit; insertInput->setPlaceholderText("Text to insert");
        ig->addWidget(label("Line", "fieldLabel"), 0, 0); ig->addWidget(label("Column", "fieldLabel"), 0, 1); ig->addWidget(label("Text", "fieldLabel"), 0, 2);
        ig->addWidget(insertLine, 1, 0); ig->addWidget(insertColumn, 1, 1); ig->addWidget(insertInput, 1, 2); ig->setColumnStretch(2, 1);
        auto *insertBtn = button("Insert text", "primary", "plus");
        ig->addWidget(insertBtn, 2, 0, 1, 3);
        connect(insertBtn, &QPushButton::clicked, this, [this] { safely([&] {
            std::size_t offset = 0;
            if (!byteOffset(insertLine->value(), insertColumn->value() - 1, offset)) { showError("Invalid line or column."); return; }
            afterEdit(edits.insertText(insertLine->value(), offset, insertInput->text().toUtf8().toStdString()));
        }); });
        h->addWidget(card("Insert / Delete", "plus-square", ib, "chevron-right"), 7);

        // Delete lines
        auto *db = new QWidget; auto *dg = new QGridLayout(db); dg->setContentsMargins(0, 0, 0, 0); dg->setHorizontalSpacing(6); dg->setVerticalSpacing(4);
        deleteFrom = number(1, false); deleteTo = number(1, false);
        dg->addWidget(label("From line", "fieldLabel"), 0, 0); dg->addWidget(label("To line", "fieldLabel"), 0, 1);
        dg->addWidget(deleteFrom, 1, 0); dg->addWidget(deleteTo, 1, 1);
        auto *deleteBtn = button("Delete line(s)", "soft", "trash");
        deleteBtn->setToolTip("Enter the same number twice to delete one line.");
        dg->addWidget(deleteBtn, 2, 0, 1, 2);
        connect(deleteBtn, &QPushButton::clicked, this, [this] {
            if (deleteTo->value() < deleteFrom->value()) { showError("End line must not precede start line."); return; }
            safely([&] { afterEdit(edits.deleteLineRange(deleteFrom->value(), deleteTo->value() - deleteFrom->value() + 1)); });
        });
        h->addWidget(card("Delete Lines", "trash", db, "chevron-right"), 5);

        // Delete range (within one line)
        auto *rb = new QWidget; auto *rg = new QGridLayout(rb); rg->setContentsMargins(0, 0, 0, 0); rg->setHorizontalSpacing(6); rg->setVerticalSpacing(4);
        rangeLine = number(); rangeFrom = number(); rangeTo = number(2);
        rangeTo->setToolTip("End column (not deleted)");
        rg->addWidget(label("Line", "fieldLabel"), 0, 0); rg->addWidget(label("From col", "fieldLabel"), 0, 1); rg->addWidget(label("To col", "fieldLabel"), 0, 2);
        rg->addWidget(rangeLine, 1, 0); rg->addWidget(rangeFrom, 1, 1); rg->addWidget(rangeTo, 1, 2);
        auto *rangeBtn = button("Delete range", "soft", "dashed");
        rg->addWidget(rangeBtn, 2, 0, 1, 3);
        connect(rangeBtn, &QPushButton::clicked, this, [this] { safely([&] {
            const int start = rangeFrom->value(), end = rangeTo->value();
            if (rangeLine->value() < 1 || static_cast<std::size_t>(rangeLine->value()) > doc.lineCount()) { showError("Invalid line."); return; }
            const QString line = QString::fromStdString(doc.getLine(rangeLine->value()));
            if (end < start || end > line.size() + 1) { showError("Invalid range."); return; }
            afterEdit(edits.deleteRange(rangeLine->value(), line.left(start - 1).toUtf8().size(), line.mid(start - 1, end - start).toUtf8().size()));
        }); });
        h->addWidget(card("Delete Range", "dashed", rb, "chevron-right"), 6);
        return row;
    }
    QWidget *buildRightColumn() {
        auto *col = new QWidget; col->setFixedWidth(sideW); rightCol = col;
        auto *v = new QVBoxLayout(col); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(12);

        auto *fb = new QWidget; auto *fv = new QVBoxLayout(fb); fv->setContentsMargins(0, 0, 0, 0); fv->setSpacing(7);
        findInput = new QLineEdit; findInput->setPlaceholderText("Search text…"); findInput->setClearButtonEnabled(true);
        fv->addWidget(findInput);
        auto *checks = new QHBoxLayout; caseBox = new QCheckBox("Match case"); wholeBox = new QCheckBox("Whole word");
        checks->addWidget(caseBox); checks->addWidget(wholeBox); checks->addStretch(); fv->addLayout(checks);
        searchMessage = label("", "searchMessage"); searchMessage->setWordWrap(true); searchMessage->hide(); fv->addWidget(searchMessage);
        matchList = new QListWidget; matchList->setObjectName("matchList");
        matchList->setMaximumHeight(96); matchList->setMinimumHeight(52);
        matchList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); matchList->setUniformItemSizes(true);
        matchList->setToolTip("Every occurrence - click one to jump to it (Replace acts on the selected one)");
        matchList->hide();
        connect(matchList, &QListWidget::currentRowChanged, this, [this](int row) { if (row >= 0) selectHit(row); });
        connect(matchList, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) { selectHit(matchList->row(it)); });
        fv->addWidget(matchList);
        auto *findNextBtn = button("Find Next", "primary", "play"); fv->addWidget(findNextBtn);
        connect(findNextBtn, &QPushButton::clicked, this, [this] { findNext(); });
        connect(findInput, &QLineEdit::returnPressed, this, [this] { findNext(); });
        auto *divider = new QHBoxLayout; auto *l1 = new QFrame; l1->setObjectName("sep"); auto *l2 = new QFrame; l2->setObjectName("sep");
        divider->addWidget(l1, 1); divider->addWidget(label("Replace", "fieldLabel")); divider->addWidget(l2, 1); fv->addLayout(divider);
        replaceInput = new QLineEdit; replaceInput->setPlaceholderText("Replace with…"); fv->addWidget(replaceInput);
        auto *rb = new QHBoxLayout; replaceOne = button("Replace", "soft"); replaceAll = button("Replace All", "primary");
        rb->addWidget(replaceOne); rb->addWidget(replaceAll); fv->addLayout(rb);
        v->addWidget(card("Find & Replace", "search", fb, "chevron-up"));

        auto *ub = new QWidget; auto *uv = new QVBoxLayout(ub); uv->setContentsMargins(0, 0, 0, 0); uv->setSpacing(8);
        historyLabel = label("0 of 50 operations"); uv->addWidget(historyLabel);
        undoLast = button("Undo last operation", "soft", "corner-undo"); uv->addWidget(undoLast);
        v->addWidget(card("Undo Stack", "history", ub, "chevron-up"));
        v->addStretch();
        v->addWidget(buildReadyCard());
        return col;
    }
    QWidget *buildReadyCard() {
        auto *f = new QFrame; f->setObjectName("card");
        readyLayout = new QBoxLayout(QBoxLayout::TopToBottom, f);
        readyLayout->setContentsMargins(14, 10, 14, 10); readyLayout->setSpacing(10);
        readyIcon = iconLabel("layers", "accent", 44);
        readyTitle = label("Ready to Edit!", "readyTitle"); readyTitle->setAlignment(Qt::AlignCenter);
        readySub = label("", "readySub"); readySub->setAlignment(Qt::AlignCenter); readySub->setWordWrap(true);
        auto *text = new QWidget; auto *tv = new QVBoxLayout(text); tv->setContentsMargins(0, 0, 0, 0); tv->setSpacing(2);
        tv->addWidget(readyTitle); tv->addWidget(readySub);
        readyLayout->addWidget(readyIcon, 0, Qt::AlignHCenter); readyLayout->addWidget(text, 1);
        return f;
    }
    // Short windows (typical 768-px laptops): the status card lays out icon-left / text-right to save height.
    void setReadyShort(bool isShort) {
        if (!readyLayout || isShort == readyShort) return;
        readyShort = isShort;
        readyLayout->setDirection(isShort ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
        readyLayout->setAlignment(readyIcon, isShort ? Qt::AlignVCenter : Qt::AlignHCenter);
        const Qt::Alignment a = isShort ? (Qt::AlignLeft | Qt::AlignVCenter) : Qt::AlignCenter;
        readyTitle->setAlignment(a); readySub->setAlignment(a);
    }

    // ---- fitting the window to the screen ----
    void applyResponsive() {
        // Adapt sidebar, side-panel width, and status text to the current window size.
        if (!rightCol) return;
        setSidebarCompact(width() < 1200);
        const int w = std::clamp(static_cast<int>(width() * 0.22), 236, 300);
        if (w != sideW) { sideW = w; rightCol->setFixedWidth(w); }
        setReadyShort(height() < 780);
    }
    void fitToScreen() {
        // Keep the frameless window inside the active screen's usable area.
        QScreen *s = windowHandle() ? windowHandle()->screen() : QGuiApplication::primaryScreen();
        if (!s || isMaximized() || isFullScreen()) return;
        const QRect a = s->availableGeometry();
        setMinimumSize(std::min(1020, a.width()), std::min(580, a.height()));
        QRect g = geometry();
        g.setSize(g.size().boundedTo(a.size()));
        if (g.right() > a.right()) g.moveRight(a.right());
        if (g.bottom() > a.bottom()) g.moveBottom(a.bottom());
        if (g.left() < a.left()) g.moveLeft(a.left());
        if (g.top() < a.top()) g.moveTop(a.top());
        if (g != geometry()) setGeometry(g);
    }

public:
    ~EditorWindow() override { QDir(assetDirPath()).removeRecursively(); }

    EditorWindow() {
        // Build the UI first, then connect it to the shared document and editing services.
        QSettings s;
        s.remove("fontSize"); s.remove("wrap");     // clean up values saved by older versions
        const bool dark = s.value("darkMode", false).toBool();
        tk = themeColors(dark);

        createActions();    // must exist before the sidebar and the File menu are built

        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setMouseTracking(true);
        setContentsMargins(3, 3, 3, 3);
        {   // size the window from the usable screen area (taskbar excluded), never from fixed pixels
            QScreen *scr = QGuiApplication::screenAt(QCursor::pos());
            if (!scr) scr = QGuiApplication::primaryScreen();
            const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1280, 720);
            setMinimumSize(std::min(1020, avail.width()), std::min(580, avail.height()));
            QSize want(std::max(1280, static_cast<int>(avail.width() * 0.7)), std::max(760, static_cast<int>(avail.height() * 0.75)));
            want = want.boundedTo(avail.size() * 0.96).expandedTo(minimumSize());
            setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, want, avail));
        }

        auto *root = new QWidget; root->setObjectName("root"); root->setCursor(Qt::ArrowCursor);
        setCentralWidget(root);
        auto *outer = new QHBoxLayout(root); outer->setContentsMargins(0, 0, 0, 0); outer->setSpacing(0);
        outer->addWidget(buildSidebar());

        auto *main = new QWidget; auto *mv = new QVBoxLayout(main); mv->setContentsMargins(0, 0, 14, 14); mv->setSpacing(10);
        mv->addWidget(buildHeader());
        auto *contentWrap = new QWidget; contentWrap->setObjectName("contentWrap");
        auto *cw = new QVBoxLayout(contentWrap); cw->setContentsMargins(14, 0, 0, 0); cw->setSpacing(12);
        cw->addWidget(buildToolbar());
        auto *grid = new QGridLayout; grid->setHorizontalSpacing(12); grid->setVerticalSpacing(12);
        grid->addWidget(buildEditorCard(), 0, 0);
        grid->addWidget(buildRightColumn(), 0, 1, 2, 1);
        grid->addWidget(buildBottomCards(), 1, 0);
        grid->setColumnStretch(0, 1); grid->setRowStretch(0, 1); grid->setRowStretch(1, 0);
        cw->addLayout(grid, 1);
        // Safety net: if a tiny screen still cannot hold everything, the page scrolls instead of clipping.
        auto *scroll = new QScrollArea; scroll->setObjectName("contentScroll");
        scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidget(contentWrap);
        scroll->viewport()->setAutoFillBackground(false); contentWrap->setAutoFillBackground(false);
        mv->addWidget(scroll, 1);
        outer->addWidget(main, 1);

        // soft drop shadow under the main cards
        applyTheme(dark);
        themeSwitch->setChecked(dark);
        applyEditorFont();

        // ---- wiring ----
        connect(themeSwitch, &QAbstractButton::toggled, this, [this](bool on) { applyTheme(on); });
        connect(undoButton, &QPushButton::clicked, this, [this] { doUndo(); });
        connect(undoLast, &QPushButton::clicked, this, [this] { doUndo(); });
        connect(editor, &QPlainTextEdit::textChanged, this, [this] {
            if (syncing) return;
            safely([&] {
                if (edits.replaceDocumentText(editor->toPlainText().toUtf8().toStdString(), true)) { modified = true; refreshSearch(); }
                else if (editor->toPlainText().toUtf8().toStdString() != doc.getAllText()) { showError("This edit could not be recorded and was reverted."); loadEditor(); }
            });
        });
        connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this] {
            const auto c = editor->textCursor();
            insertLine->setValue(c.blockNumber() + 1);
            insertColumn->setValue(c.positionInBlock() + 1);
            updateStatus();
        });
        connect(findInput, &QLineEdit::textChanged, this, [this] { currentHit = 0; refreshSearch(); });
        connect(caseBox, &QCheckBox::toggled, this, [this] { refreshSearch(); });
        connect(wholeBox, &QCheckBox::toggled, this, [this] { refreshSearch(); });
        connect(replaceOne, &QPushButton::clicked, this, [this] { safely([&] {
            if (hits.isEmpty()) return;
            if (replaceHit(std::clamp(currentHit, 0, int(hits.size()) - 1))) afterEdit(true);
        }); });
        connect(replaceAll, &QPushButton::clicked, this, [this] { safely([&] {
            if (hits.isEmpty()) return;
            const std::string before = doc.getAllText();
            const ReplaceResult r = replacer.replaceAllOccurrences(findInput->text().toUtf8().toStdString(), replaceInput->text().toUtf8().toStdString(),
                                                                   caseBox->isChecked(), wholeBox->isChecked());
            if (r.status != ReplaceStatus::Success) throw std::runtime_error(TextReplace::describe(r.status));
            if (doc.getAllText() != before) afterEdit(true);
        }); });

        auto shortcut = [this](const QString &seq, std::function<void()> fn) {
            auto *sc = new QShortcut(QKeySequence(seq), this);
            connect(sc, &QShortcut::activated, this, [fn] { fn(); });
        };
        // New / New Window / Open / Save / Save As / Exit shortcuts belong to the File actions (createActions);
        // registering them a second time here would make the key sequences ambiguous and stop them working.
        shortcut("Ctrl+F", [this] { findInput->setFocus(); findInput->selectAll(); });
        shortcut("Ctrl+H", [this] { replaceInput->setFocus(); });
        shortcut("F3", [this] { findNext(); });
        shortcut("Ctrl+Z", [this] { doUndo(); });

        files.createNewDocument(false);
        loadEditor();
        editor->setFocus();
    }

protected:
    // QPlainTextEdit claims Ctrl+Z for its own (disabled) undo whenever it has focus,
    // so the application-wide shortcuts would never fire. Swallow those keys here so the
    // shortcuts reach this window instead.
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == editor && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->matches(QKeySequence::Undo)) {
                if (event->type() == QEvent::KeyPress) doUndo();
                return true;
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }
    void resizeEvent(QResizeEvent *e) override {
        QMainWindow::resizeEvent(e);
        applyResponsive();
    }
    void showEvent(QShowEvent *e) override {
        QMainWindow::showEvent(e);
        if (!screenHooked && windowHandle()) {      // dragged to another monitor / DPI change: re-fit
            screenHooked = true;
            connect(windowHandle(), &QWindow::screenChanged, this, [this](QScreen *) { fitToScreen(); });
        }
        fitToScreen();
        applyResponsive();
        QTimer::singleShot(0, this, [this] { if (toolbarBar) fitToolbar(toolbarBar->width()); });
    }
    void closeEvent(QCloseEvent *event) override { if (askSave()) event->accept(); else event->ignore(); }
    void changeEvent(QEvent *e) override {
        QMainWindow::changeEvent(e);
        if (e->type() == QEvent::WindowStateChange) setContentsMargins(isMaximized() ? QMargins() : QMargins(3, 3, 3, 3));
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        const Qt::Edges ed = edgesAt(mousePos(e));
        if (ed == (Qt::LeftEdge | Qt::TopEdge) || ed == (Qt::RightEdge | Qt::BottomEdge)) setCursor(Qt::SizeFDiagCursor);
        else if (ed == (Qt::RightEdge | Qt::TopEdge) || ed == (Qt::LeftEdge | Qt::BottomEdge)) setCursor(Qt::SizeBDiagCursor);
        else if (ed & (Qt::LeftEdge | Qt::RightEdge)) setCursor(Qt::SizeHorCursor);
        else if (ed & (Qt::TopEdge | Qt::BottomEdge)) setCursor(Qt::SizeVerCursor);
        else unsetCursor();
        QMainWindow::mouseMoveEvent(e);
    }
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton) {
            const Qt::Edges ed = edgesAt(mousePos(e));
            if (ed && windowHandle()) { windowHandle()->startSystemResize(ed); return; }
        }
        QMainWindow::mousePressEvent(e);
    }
};

int main(int argc, char **argv) {
    // Make logical sizes behave the same on 100% / 125% / 150% laptop displays.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("CS-218");
    QCoreApplication::setApplicationName("TextEditor");
    app.setStyle("Fusion");
    EditorWindow w;
    for (const QString &arg : app.arguments()) {        // "--pos=X,Y" is passed by File > New Window so windows cascade
        if (!arg.startsWith("--pos=")) continue;
        const QStringList xy = arg.mid(6).split(',');
        bool okX = false, okY = false;
        const int px = xy.value(0).toInt(&okX), py = xy.value(1).toInt(&okY);
        if (okX && okY) w.move(px, py);
    }
    w.show();
    return app.exec();
}
