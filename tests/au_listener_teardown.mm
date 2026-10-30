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
static void disposeAtEntry(CFRunLoopObserverRef, CFRunLoopActivity, void* context)
{
    dispose(*static_cast<Lifetime*>(context));
}
int main(int argc, char** argv)
{
    bool queued = false;
    bool entry = false;
    unsigned cycles = 2000;
    for (int argument = 1; argument < argc; ++argument)
    {
        if (std::strcmp(argv[argument], "--queued-disposal") == 0) queued = true;
        else if (std::strcmp(argv[argument], "--entry-disposal") == 0) entry = true;
        else if (std::strcmp(argv[argument], "--cycles") == 0 && argument + 1 < argc)
        {
            char* end = nullptr;
            const auto requested = std::strtoul(argv[++argument], &end, 10);
            if (*end != '\0' || requested < 2 || requested > 2000) return 2;
            cycles = static_cast<unsigned>(requested);
        }
        else return 2;
    }
    if (entry && queued) return 2;
    std::printf("Apple-only AU listener control: %s\n", entry ? "entry disposal" : queued ? "queued disposal" : "drained disposal");
    std::fflush(stdout);
    AudioComponentDescription description {};
    description.componentType = kAudioUnitType_Effect;
    description.componentSubType = kAudioUnitSubType_LowPassFilter;
    description.componentManufacturer = kAudioUnitManufacturer_Apple;
    const auto component = AudioComponentFindNext(nullptr, &description);
    if (component == nullptr) return 2;
    for (unsigned iteration = 0; iteration < cycles; ++iteration)
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
        if (entry && iteration != 0)
        {
            // An entry observer disposes before the run loop drains queued blocks.
            // This complements the timer probe, which can run after that drain.
            CFRunLoopObserverContext context {0, &lifetime, nullptr, nullptr, nullptr};
            auto observer = CFRunLoopObserverCreate(kCFAllocatorDefault,
                kCFRunLoopEntry, false, LONG_MAX, disposeAtEntry, &context);
            if (observer == nullptr) return 2;
            CFRunLoopAddObserver(CFRunLoopGetMain(), observer, kCFRunLoopDefaultMode);
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.003, false);
            CFRunLoopObserverInvalidate(observer);
            CFRelease(observer);
            if (lifetime.listener != nullptr) return 4;
        }
        else if (queued && iteration != 0)
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
        if ((iteration + 1) % 16 == 0)
        {
            std::printf("Apple AU control progress: cycles=%u callbacks=%u\n", iteration + 1, delivered);
            std::fflush(stdout);
        }
    }
    std::printf("Apple AU listener control completed; callbacks=%u\n", delivered);
}
