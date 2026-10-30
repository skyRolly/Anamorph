// Nonshipping OS control: only Apple's AU and event-listener APIs; no JUCE/Anamorph.
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <climits>
#include <thread>

static unsigned delivered = 0;
static void callback(void*, void*, const AudioUnitEvent*, UInt64, AudioUnitParameterValue)
{
    ++delivered;
}
static void require(OSStatus result)
{
    if (result != noErr) { std::fprintf(stderr, "OSStatus %d\n", static_cast<int>(result)); std::exit(2); }
}
struct Lifetime
{
    AudioUnit unit = nullptr;
    AUEventListenerRef listener = nullptr;
};
static void dispose(Lifetime& lifetime)
{
    if (lifetime.listener != nullptr)
    {
        require(AUListenerDispose(lifetime.listener));
        lifetime.listener = nullptr;
        require(AudioComponentInstanceDispose(lifetime.unit));
        lifetime.unit = nullptr;
    }
}
static void disposeFromTimer(CFRunLoopTimerRef, void* context)
{
    dispose(*static_cast<Lifetime*>(context));
}
int main(int argc, char** argv)
{
    const bool queued = argc == 2 && std::strcmp(argv[1], "--queued-disposal") == 0;
    if (argc != 1 && !queued) return 2;
    std::printf("Apple-only AU listener control: %s\n", queued ? "queued disposal" : "drained disposal");
    std::fflush(stdout);
    AudioComponentDescription description {};
    description.componentType = kAudioUnitType_Effect;
    description.componentSubType = kAudioUnitSubType_LowPassFilter;
    description.componentManufacturer = kAudioUnitManufacturer_Apple;
    const auto component = AudioComponentFindNext(nullptr, &description);
    if (component == nullptr) return 2;
    for (unsigned iteration = 0; iteration < 2000; ++iteration)
    {
        Lifetime lifetime;
        require(AudioComponentInstanceNew(component, &lifetime.unit));
        require(AUEventListenerCreate(callback, nullptr, CFRunLoopGetMain(),
                                     kCFRunLoopDefaultMode, 0, 0, &lifetime.listener));
        AudioUnitEvent event {};
        event.mEventType = kAudioUnitEvent_ParameterValueChange;
        event.mArgument.mParameter = {lifetime.unit, kLowPassParam_CutoffFrequency, kAudioUnitScope_Global, 0};
        require(AUEventListenerAddEventType(lifetime.listener, nullptr, &event));
        for (unsigned change = 0; change < 8; ++change)
        {
            require(AUParameterSet(nullptr, nullptr, &event.mArgument.mParameter,
                                   200.0f + static_cast<float>(change)*100.0f, 0));
        }
        if (queued && iteration != 0)
        {
            // Make the AU delivery timer and an independent disposal timer due in
            // the same run-loop turn. This is an ordering probe, not a reproduction
            // claim: disposal may run before the delivery block that timerFired posts.
            CFRunLoopTimerContext context {0, &lifetime, nullptr, nullptr, nullptr};
            auto timer = CFRunLoopTimerCreate(kCFAllocatorDefault,
                CFAbsoluteTimeGetCurrent() + 0.001, 0, 0, LONG_MAX, disposeFromTimer, &context);
            if (timer == nullptr) return 2;
            CFRunLoopAddTimer(CFRunLoopGetMain(), timer, kCFRunLoopDefaultMode);
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.003, false);
            CFRunLoopTimerInvalidate(timer);
            CFRelease(timer);
            if (lifetime.listener != nullptr) return 4; // disposal timer must execute
        }
        else
        {
            // Vary the opportunity for a timer to enqueue delivery before disposal.
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, iteration == 0 ? 0.05 : 0.0001*(iteration%7), false);
            if (iteration == 0 && delivered == 0) return 3; // event path must actually be live
            dispose(lifetime);
        }
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, false);
    }
    std::printf("Apple AU listener control completed; callbacks=%u\n", delivered);
}
