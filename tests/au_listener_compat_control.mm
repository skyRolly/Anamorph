// Real API/lifetime and genuine callback-exception controls for the CI host adapter.
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <functional>
#include <pthread.h>

struct Observation
{
    std::atomic<unsigned> delivered {0}, late {0}, wrongThread {0};
    std::atomic<float> value {0};
    std::atomic<bool> closed {false};
    bool throws = false;
};
static void callback(void* userData, void*, const AudioUnitEvent*, UInt64, AudioUnitParameterValue value)
{
    auto& observation = *static_cast<Observation*>(userData);
    if (pthread_main_np() == 0) ++observation.wrongThread;
    if (observation.closed.load()) ++observation.late;
    observation.value.store(value);
    ++observation.delivered;
    if (observation.throws) { std::function<void()> empty; empty(); }
}
static void require(OSStatus result)
{
    if (result != noErr) { std::fprintf(stderr, "OSStatus %d\n", static_cast<int>(result)); std::exit(2); }
}
int main(int argc, char** argv)
{
    bool requireAdapter = false, throws = false;
    for (int argument = 1; argument < argc; ++argument)
    {
        if (std::strcmp(argv[argument], "--require-adapter") == 0) requireAdapter = true;
        else if (std::strcmp(argv[argument], "--throw-in-callback") == 0) throws = true;
        else return 2;
    }
    @autoreleasepool
    {
        AudioComponentDescription description {};
        description.componentType = kAudioUnitType_Effect;
        description.componentSubType = kAudioUnitSubType_LowPassFilter;
        description.componentManufacturer = kAudioUnitManufacturer_Apple;
        const auto component = AudioComponentFindNext(nullptr, &description);
        if (component == nullptr) return 2;
        AudioUnit unit = nullptr;
        require(AudioComponentInstanceNew(component, &unit));
        Observation observation;
        observation.throws = throws;
        AUEventListenerRef listener = nullptr;
        require(AUEventListenerCreate(callback, &observation, CFRunLoopGetMain(),
                                     kCFRunLoopDefaultMode, 0, 0, &listener));
        using Count = unsigned (*)();
        const auto count = reinterpret_cast<Count>(dlsym(RTLD_DEFAULT, "anamorph_au_listener_routes"));
        if (requireAdapter && (count == nullptr || count() == 0)) return 6;
        AudioUnitEvent event {};
        event.mEventType = kAudioUnitEvent_ParameterValueChange;
        event.mArgument.mParameter = {unit, kLowPassParam_CutoffFrequency, kAudioUnitScope_Global, 0};
        require(AUEventListenerAddEventType(listener, nullptr, &event));
        for (unsigned change = 0; change < 8; ++change)
        {
            require(AudioUnitSetParameter(unit, kLowPassParam_CutoffFrequency,
                kAudioUnitScope_Global, 0, 200.0f + static_cast<float>(change)*100.0f, 0));
            require(AUEventListenerNotify(nullptr, nullptr, &event));
        }
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
        if (observation.delivered.load() == 0 || std::fabs(observation.value.load() - 900.0f) > 0.01f) return 3;
        require(AUListenerDispose(listener));
        observation.closed.store(true);
        require(AudioComponentInstanceDispose(unit));
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
        if (observation.late.load() != 0 || observation.wrongThread.load() != 0) return 4;
        std::printf("AU adapter control: callbacks=%u late=%u wrong-thread=%u routes=%u\n",
            observation.delivered.load(), observation.late.load(), observation.wrongThread.load(),
            count != nullptr ? count() : 0);
    }
}
