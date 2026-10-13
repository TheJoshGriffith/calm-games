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

void haptic(const QString& event)
{
    static UIImpactFeedbackGenerator* light = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
    static UIImpactFeedbackGenerator* medium = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
    static UINotificationFeedbackGenerator* notify = [[UINotificationFeedbackGenerator alloc] init];

    if (event == QLatin1String("tap") || event == QLatin1String("erase"))
        [light impactOccurredWithIntensity:0.7];
    else if (event == QLatin1String("soft"))
        [light impactOccurredWithIntensity:0.4];
    else if (event == QLatin1String("error"))
        [light impactOccurredWithIntensity:1.0];
    else if (event == QLatin1String("complete"))
        [medium impactOccurredWithIntensity:0.8];
    else if (event == QLatin1String("win"))
        [notify notificationOccurred:UINotificationFeedbackTypeSuccess];
}

} // namespace calm::platform
