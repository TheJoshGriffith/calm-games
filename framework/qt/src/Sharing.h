#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QRectF>

// The system share sheet, for sharing a link with friends: iOS's share sheet,
// the browser's Web Share API on the web where there is one, and the
// clipboard everywhere else. Whenever a share ends up on the clipboard
// instead, `copiedToClipboard` fires so the UI can say "Link copied".
class Sharing : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // `anchor` is the control that asked, in window coordinates (iPad points
    // its share popover at it); pass an empty rect if there is none.
    Q_INVOKABLE void share(const QString& text, const QRectF& anchor = {});
    Q_INVOKABLE void copy(const QString& text);

signals:
    void copiedToClipboard();
};
