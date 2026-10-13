#include "Sharing.h"

#include "platform/Platform.h"

#include <QPointer>

void Sharing::share(const QString& text, const QRectF& anchor)
{
    if (text.isEmpty())
        return;
    QPointer<Sharing> self(this);
    calm::platform::share(text, anchor, [self] {
        if (self)
            emit self->copiedToClipboard();
    });
}

void Sharing::copy(const QString& text)
{
    if (!text.isEmpty())
        calm::platform::copyToClipboard(text);
}
