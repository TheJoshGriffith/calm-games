#include "Platform.h"

#include <QClipboard>
#include <QGuiApplication>

#ifdef Q_OS_WASM
#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <string>
#include <utility>
#endif

namespace calm::platform {

void configureAudioSession()
{
}

void haptic(const QString&, qreal)
{
}

#ifdef Q_OS_WASM

// The page (tools/web/index.html) provides window.calm.share(text, copied)
// and window.calm.copy(text): the Web Share API where the browser has it,
// else the Clipboard API, else a hidden text area and execCommand('copy').
namespace {

std::function<void()>& pendingCopied()
{
    static std::function<void()> callback;
    return callback;
}

// Called by the page when the share sheet was refused and it copied instead.
void shareFellBackToClipboard()
{
    if (auto callback = std::exchange(pendingCopied(), {}))
        callback();
}

emscripten::val page()
{
    return emscripten::val::global("calm");
}

bool pageHas(const char* function)
{
    const emscripten::val calm = page();
    return !calm.isUndefined() && !calm.isNull() && calm[function].typeOf().as<std::string>() == "function";
}

} // namespace

EMSCRIPTEN_BINDINGS(calm_platform)
{
    emscripten::function("calmShareCopied", &shareFellBackToClipboard);
}

void share(const QString& text, const QRectF&, const std::function<void()>& copied)
{
    if (!pageHas("share")) {
        copyToClipboard(text);
        if (copied)
            copied();
        return;
    }
    pendingCopied() = copied;
    // True when a share sheet opened; false when the page copied instead.
    const bool sheet = page().call<bool>("share", text.toStdString(),
        emscripten::val::module_property("calmShareCopied"));
    if (!sheet)
        shareFellBackToClipboard();
}

void copyToClipboard(const QString& text)
{
    if (pageHas("copy"))
        page().call<void>("copy", text.toStdString());
    else
        QGuiApplication::clipboard()->setText(text);
}

#else

// Desktop: no share sheet, so the link goes on the clipboard.
void share(const QString& text, const QRectF&, const std::function<void()>& copied)
{
    copyToClipboard(text);
    if (copied)
        copied();
}

void copyToClipboard(const QString& text)
{
    QGuiApplication::clipboard()->setText(text);
}

#endif

} // namespace calm::platform
