#include "ui/Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace anypc::Theme {

namespace {
constexpr auto kStyleSheet = R"(
QWidget#card {
    background: palette(base);
    border: 1px solid palette(mid);
    border-radius: 10px;
}
QLabel#cardTitle {
    font-size: 11pt;
    font-weight: 600;
    color: palette(text);
}
QLabel#cardHint {
    color: #6b7280;
}
QLabel#deviceId {
    font-size: 26pt;
    font-weight: 700;
    letter-spacing: 1px;
    color: #e5484d;
}
QLineEdit#remoteId {
    font-size: 16pt;
    padding: 6px 10px;
}
QPushButton#primary {
    background: #e5484d;
    color: white;
    border: none;
    border-radius: 6px;
    padding: 8px 18px;
    font-weight: 600;
}
QPushButton#primary:hover { background: #dc3e42; }
QPushButton#primary:disabled { background: #f0a5a8; }
QFrame#controlBanner {
    background: #fff4d6;
    border: 1px solid #f5d27a;
    border-radius: 8px;
}
QFrame#controlBanner QLabel { color: #5c4300; }
QLabel#statusDot { font-size: 14pt; }
)";
} // namespace

void apply(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setStyleSheet(QString::fromLatin1(kStyleSheet));
}

} // namespace anypc::Theme
