// Nonshipping pluginval host adapter. The official validator bytes stay locked.
// Avoid AudioToolbox's legacy CFRunLoop delivery block on native Intel CI.
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>

static std::atomic<unsigned> routes {0};
#if defined(ANAMORPH_AU_LISTENER_TRACE)
static std::atomic<unsigned> frameworkEnqueues {0};
static bool observeOnly = false;
#endif
extern "C" unsigned anamorph_au_listener_routes() { return routes.load(); }

__attribute__((constructor)) static void loaded()
{
    std::fputs("ANAMORPH_AU_LISTENER_ADAPTER=loaded\n", stderr);
#if defined(ANAMORPH_AU_LISTENER_TRACE)
    const auto* mode = std::getenv("ANAMORPH_AU_LISTENER_MODE");
    observeOnly = mode != nullptr && std::strcmp(mode, "observe") == 0;
#endif
}

static OSStatus createListener(AUEventListenerProc procedure, void* userData,
    CFRunLoopRef loop, CFStringRef mode, Float32 interval, Float32 granularity,
    AUEventListenerRef* listener)
{
    Dl_info caller {};
    dladdr(__builtin_return_address(0), &caller);
    const char* image = caller.dli_fname != nullptr ? std::strrchr(caller.dli_fname, '/') : nullptr;
    image = image != nullptr ? image + 1 : caller.dli_fname;
    const bool ownedHost = image != nullptr
        && (std::strcmp(image, "pluginval") == 0 || std::strcmp(image, "apple-au-compat-control") == 0);
    const bool mainLoop = (loop != nullptr ? loop : CFRunLoopGetCurrent()) == CFRunLoopGetMain();
    bool adapt = true;
#if defined(ANAMORPH_AU_LISTENER_TRACE)
    adapt = !observeOnly;
#endif
    if (adapt && ownedHost && procedure != nullptr && mainLoop
        && (mode == nullptr || CFEqual(mode, kCFRunLoopDefaultMode)))
    {
        const auto result = AUEventListenerCreateWithDispatchQueue(listener, interval, granularity,
            dispatch_get_main_queue(), ^(void* object, const AudioUnitEvent* event,
                                         UInt64 time, AudioUnitParameterValue value) {
                procedure(userData, object, event, time, value);
            });
        if (result == noErr)
        {
            ++routes;
            std::fputs("ANAMORPH_AU_LISTENER_ADAPTER=dispatch-main\n", stderr);
        }
        return result; // API errors remain fatal; never silently use the failed path.
    }
    return AUEventListenerCreate(procedure, userData, loop, mode, interval, granularity, listener);
}

__attribute__((used, section("__DATA,__interpose")))
static const struct { const void* replacement; const void* original; } interposition = {
    reinterpret_cast<const void*>(&createListener), reinterpret_cast<const void*>(&AUEventListenerCreate)
};

#if defined(ANAMORPH_AU_LISTENER_TRACE)
static void observeEnqueue(CFRunLoopRef loop, CFTypeRef modes, void (^block)(void))
{
    Dl_info caller {};
    dladdr(__builtin_return_address(0), &caller);
    if (caller.dli_fname != nullptr && std::strstr(caller.dli_fname, "AudioToolboxCore") != nullptr
        && frameworkEnqueues.fetch_add(1) == 0)
        std::fputs("ANAMORPH_AU_LISTENER_FRAMEWORK_CF_SEEN=1\n", stderr);
    CFRunLoopPerformBlock(loop, modes, block);
}
__attribute__((used, section("__DATA,__interpose")))
static const struct { const void* replacement; const void* original; } traceInterposition = {
    reinterpret_cast<const void*>(&observeEnqueue), reinterpret_cast<const void*>(&CFRunLoopPerformBlock)
};
__attribute__((destructor)) static void report()
{
    std::fprintf(stderr, "ANAMORPH_AU_LISTENER_FRAMEWORK_CF=%u\n", frameworkEnqueues.load());
}
#endif
