// Nonshipping ordering control: queue an AU block, then dispose before delivery.
// Uses Apple's LowPassFilter and public APIs only; no JUCE/Anamorph is loaded.
#if !defined(ANAMORPH_PENDING_CONTROL_LIBRARY)
extern "C" int anamorph_pending_control_main(int, char**);
int main(int argc, char** argv) { return anamorph_pending_control_main(argc, argv); }
#else
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
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
    std::atomic<bool> bothQueued {false};
    std::atomic<unsigned> enqueued {0}, disposed {0}, blockReturned {0};
};
static std::atomic<Lifetime*> armed {nullptr};

static void enqueue(CFRunLoopRef loop, CFTypeRef modes, void (^block)(void))
{
    Dl_info caller {};
    dladdr(__builtin_return_address(0), &caller);
    auto* context = armed.load(std::memory_order_acquire);
    const bool defaultMode = CFEqual(modes, kCFRunLoopDefaultMode)
        || (CFGetTypeID(modes) == CFArrayGetTypeID()
            && CFArrayContainsValue(static_cast<CFArrayRef>(modes),
                CFRangeMake(0, CFArrayGetCount(static_cast<CFArrayRef>(modes))), kCFRunLoopDefaultMode));
    if (context != nullptr)
    {
        std::printf("AU pending CF enqueue: caller=%s default-mode=%d\n",
            caller.dli_fname != nullptr ? caller.dli_fname : "unknown", static_cast<int>(defaultMode));
        std::fflush(stdout);
    }
    if (context != nullptr && loop == CFRunLoopGetMain() && defaultMode
        && caller.dli_fname != nullptr && std::strstr(caller.dli_fname, "AudioToolbox") != nullptr
        && armed.compare_exchange_strong(context, nullptr, std::memory_order_acq_rel))
    {
        ++context->enqueued;
        std::printf("AU pending control: intercepted framework block from %s\n", caller.dli_fname);
        std::fflush(stdout);
        // Calls from this executable bind to the original CF function. Both
        // blocks are queued before disposal, even if enqueue runs off main.
        CFRunLoopPerformBlock(loop, modes, ^{
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
            while (!context->bothQueued.load(std::memory_order_acquire))
            {
                if (std::chrono::steady_clock::now() >= deadline) std::exit(5);
                std::this_thread::yield();
            }
            std::puts("AU pending control: disposing listener with framework block pending");
            std::fflush(stdout);
            require(AUListenerDispose(context->listener));
            context->listener = nullptr;
            ++context->disposed;
        });
        CFRunLoopPerformBlock(loop, modes, ^{
            std::puts("AU pending control: entering original framework block after disposal");
            std::fflush(stdout);
            block();
            ++context->blockReturned;
        });
        context->bothQueued.store(true, std::memory_order_release);
        return;
    }
    CFRunLoopPerformBlock(loop, modes, block);
}

// Standard dyld interposition, scoped to this independent test executable.
__attribute__((used, section("__DATA,__interpose")))
static const struct { const void* replacement; const void* original; } interposition = {
    reinterpret_cast<const void*>(&enqueue), reinterpret_cast<const void*>(&CFRunLoopPerformBlock)
};

extern "C" int anamorph_pending_control_main(int argc, char** argv)
{
    const bool disarmed = argc == 2 && std::strcmp(argv[1], "--no-enqueue-hook") == 0;
    if (argc != 1 && !disarmed) return 2;
    std::puts("Apple-only pending-block disposal control");
    std::fflush(stdout);
    AudioComponentDescription description {};
    description.componentType = kAudioUnitType_Effect;
    description.componentSubType = kAudioUnitSubType_LowPassFilter;
    description.componentManufacturer = kAudioUnitManufacturer_Apple;
    const auto component = AudioComponentFindNext(nullptr, &description);
    if (component == nullptr) return 2;
    Lifetime lifetime;
    require(AudioComponentInstanceNew(component, &lifetime.unit));
    require(AUEventListenerCreate(callback, nullptr, CFRunLoopGetMain(), kCFRunLoopDefaultMode,
                                 0, 0, &lifetime.listener));
    AudioUnitEvent event {};
    event.mEventType = kAudioUnitEvent_ParameterValueChange;
    event.mArgument.mParameter = {lifetime.unit, kLowPassParam_CutoffFrequency, kAudioUnitScope_Global, 0};
    require(AUEventListenerAddEventType(lifetime.listener, nullptr, &event));
    const auto notify = [&] {
        for (unsigned change = 0; change < 8; ++change)
        {
            require(AudioUnitSetParameter(lifetime.unit, kLowPassParam_CutoffFrequency,
                kAudioUnitScope_Global, 0, 200.0f + static_cast<float>(change)*100.0f, 0));
            // Match the production AU wrapper's event API, not the legacy
            // AUParameterSet/AUParameterListenerNotify convenience path.
            require(AUEventListenerNotify(nullptr, nullptr, &event));
        }
    };
    notify();
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
    if (delivered == 0) return 3;
    std::printf("AU pending control: initial callbacks=%u\n", delivered);
    std::fflush(stdout);
    if (!disarmed) armed.store(&lifetime, std::memory_order_release);
    notify();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (lifetime.blockReturned.load() == 0 && std::chrono::steady_clock::now() < deadline)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
    armed.store(nullptr, std::memory_order_release);
    if (lifetime.enqueued.load() != 1 || lifetime.disposed.load() != 1 || lifetime.blockReturned.load() != 1)
    {
        std::fprintf(stderr, "AU pending control not exercised: enqueued=%u disposed=%u returned=%u\n",
            lifetime.enqueued.load(), lifetime.disposed.load(), lifetime.blockReturned.load());
        return 5;
    }
    require(AudioComponentInstanceDispose(lifetime.unit));
    std::printf("Apple AU listener control completed; callbacks=%u\n", delivered);
    return 0;
}
#endif
