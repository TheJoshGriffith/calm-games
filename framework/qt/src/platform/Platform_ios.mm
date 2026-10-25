#include "Platform.h"

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

namespace calm::platform {

void configureAudioSession()
{
    // Ambient: sounds mix with the player's own audio, never interrupt it,
    // and stay silent when the ring/silent switch is set to silent.
    AVAudioSession* session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategoryAmbient error:nil];
    [session setActive:YES error:nil];
}

void haptic(const QString& event, qreal scale)
{
    // The "soft" impact style has a rounded, cushioned feel that suits a calm
    // game better than the sharper light/rigid styles.
    static UIImpactFeedbackGenerator* soft = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleSoft];
    static UIImpactFeedbackGenerator* medium = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];

    auto pulse = [](UIImpactFeedbackGenerator* generator, qreal intensity) {
        [generator impactOccurredWithIntensity:qBound(0.05, intensity, 1.0)];
    };

    if (event == QLatin1String("tap"))
        pulse(soft, 0.7 * scale);
    else if (event == QLatin1String("soft"))
        pulse(soft, 0.4 * scale);
    else if (event == QLatin1String("erase"))
        pulse(soft, 0.5 * scale);
    else if (event == QLatin1String("error"))
        pulse(soft, 0.9 * scale);
    else if (event == QLatin1String("complete"))
        pulse(medium, 0.6 * scale);
    else if (event == QLatin1String("win")) {
        // A slow, settling double beat rather than a buzz.
        pulse(medium, 0.8 * scale);
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 180 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
            pulse(soft, 0.5 * scale);
        });
    }
}

namespace {

// The view controller on top: the root of the foreground window, or whatever
// it is presenting (a share sheet already open, say).
UIViewController* topViewController()
{
    UIWindow* window = nil;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:[UIWindowScene class]])
            continue;
        for (UIWindow* candidate in ((UIWindowScene*)scene).windows) {
            if (candidate.isKeyWindow) {
                window = candidate;
                break;
            }
            if (!window)
                window = candidate;
        }
        if (window && window.isKeyWindow)
            break;
    }
    UIViewController* top = window.rootViewController;
    while (top.presentedViewController)
        top = top.presentedViewController;
    return top;
}

} // namespace

void share(const QString& text, const QRectF& anchor, const std::function<void()>& copied)
{
    UIViewController* top = topViewController();
    if (!top) {
        copyToClipboard(text);
        if (copied)
            copied();
        return;
    }
    UIActivityViewController* sheet = [[UIActivityViewController alloc]
        initWithActivityItems:@[ text.toNSString() ]
        applicationActivities:nil];
    // iPad shows the sheet as a popover, which needs something to point at:
    // the button that asked, or the middle of the screen.
    if (UIPopoverPresentationController* popover = sheet.popoverPresentationController) {
        UIView* view = top.view;
        popover.sourceView = view;
        popover.sourceRect = anchor.isValid()
            ? anchor.toCGRect()
            : CGRectMake(CGRectGetMidX(view.bounds), CGRectGetMidY(view.bounds), 0, 0);
        if (!anchor.isValid())
            popover.permittedArrowDirections = 0;
    }
    [top presentViewController:sheet animated:YES completion:nil];
}

void copyToClipboard(const QString& text)
{
    UIPasteboard.generalPasteboard.string = text.toNSString();
}

namespace {

UIColor* g_barColor = nil;
UIUserInterfaceStyle g_barStyle = UIUserInterfaceStyleUnspecified;

void applySystemBars()
{
    if (!g_barColor)
        return;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:[UIWindowScene class]])
            continue;
        for (UIWindow* window in ((UIWindowScene*)scene).windows) {
            window.backgroundColor = g_barColor;
            window.overrideUserInterfaceStyle = g_barStyle;
            UIViewController* root = window.rootViewController;
            if (!root)
                continue;
            root.view.backgroundColor = g_barColor;
            static NSMutableSet* adapted = [NSMutableSet set];
            if (![adapted containsObject:[root class]]) {
                [adapted addObject:[root class]];
                IMP followStyle = imp_implementationWithBlock(^UIStatusBarStyle(id) { return UIStatusBarStyleDefault; });
                class_replaceMethod([root class], @selector(preferredStatusBarStyle), followStyle, "q@:");
            }
            [root setNeedsStatusBarAppearanceUpdate];
        }
    }
}

} // namespace

void setSystemBars(const QColor& background, const QString& style)
{
    // Qt sizes its window to the safe area, so the status bar and home
    // indicator sit over the root view's background (black by default), and
    // Qt's view controller always asks for white status bar text. Colour the
    // background, and have the status bar follow the window's interface style
    // (pinned to the theme, or left to the system) instead.
    g_barColor = [UIColor colorWithRed:background.redF() green:background.greenF() blue:background.blueF() alpha:1];
    g_barStyle = style == QLatin1String("dark") ? UIUserInterfaceStyleDark
        : style == QLatin1String("light")       ? UIUserInterfaceStyleLight
                                                : UIUserInterfaceStyleUnspecified;
    applySystemBars();

    // The first call can come before the scene and its window are up, so
    // apply again whenever a window appears or the scene becomes active.
    static bool observing = false;
    if (!observing) {
        observing = true;
        NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
        for (NSNotificationName name in @[ UIWindowDidBecomeVisibleNotification, UIWindowDidBecomeKeyNotification,
                                           UISceneDidActivateNotification ]) {
            [center addObserverForName:name object:nil queue:NSOperationQueue.mainQueue
                            usingBlock:^(NSNotification*) { applySystemBars(); }];
        }
    }
}

} // namespace calm::platform
