// Nonshipping OS control: only Apple's AU and event-listener APIs; no JUCE/Anamorph.
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cstdio>
#include <cstdlib>

static unsigned delivered = 0;
static void callback(void*, void*, const AudioUnitEvent*, UInt64, AudioUnitParameterValue)
{
    ++delivered;
}
static void require(OSStatus result)
{
    if (result != noErr) { std::fprintf(stderr, "OSStatus %d\n", static_cast<int>(result)); std::exit(2); }
}
int main()
{
    AudioComponentDescription description {};
    description.componentType = kAudioUnitType_Effect;
    description.componentSubType = kAudioUnitSubType_LowPassFilter;
    description.componentManufacturer = kAudioUnitManufacturer_Apple;
    const auto component = AudioComponentFindNext(nullptr, &description);
    if (component == nullptr) return 2;
    for (unsigned iteration = 0; iteration < 2000; ++iteration)
    {
        AudioUnit unit = nullptr;
        require(AudioComponentInstanceNew(component, &unit));
        AUEventListenerRef listener = nullptr;
        require(AUEventListenerCreate(callback, nullptr, CFRunLoopGetMain(),
                                     kCFRunLoopDefaultMode, 0, 0, &listener));
        AudioUnitEvent event {};
        event.mEventType = kAudioUnitEvent_ParameterValueChange;
        event.mArgument.mParameter = {unit, kLowPassParam_CutoffFrequency, kAudioUnitScope_Global, 0};
        require(AUEventListenerAddEventType(listener, nullptr, &event));
        for (unsigned change = 0; change < 8; ++change)
        {
            require(AUParameterSet(nullptr, nullptr, &event.mArgument.mParameter,
                                   200.0f + static_cast<float>(change)*100.0f, 0));
        }
        // Vary the opportunity for a timer to enqueue delivery before disposal.
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, iteration == 0 ? 0.05 : 0.0001*(iteration%7), false);
        if (iteration == 0 && delivered == 0) return 3; // event path must actually be live
        require(AUListenerDispose(listener));
        require(AudioComponentInstanceDispose(unit));
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, false);
    }
    std::printf("Apple AU listener control completed; callbacks=%u\n", delivered);
}
