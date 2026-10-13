#include "Platform.h"

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

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

} // namespace calm::platform
