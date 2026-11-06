#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace anamorph::gui
{
// ADR-0072: the event stack retains the binding, never the component. Parameter
// delivery waits until JUCE's mouse/text notification frames have returned.
struct SliderEventBoundary
{
    virtual ~SliderEventBoundary() = default;
    virtual void enterEvent() = 0;
    virtual void leaveEvent() = 0;
};

struct SliderEventSource
{
    virtual ~SliderEventSource() = default;
    virtual std::shared_ptr<SliderEventBoundary> sliderEventBoundary() const = 0;
};

class ScopedSliderEvent
{
public:
    explicit ScopedSliderEvent (juce::Component* slider)
    {
        if (auto* source = dynamic_cast<SliderEventSource*> (slider))
            record = source->sliderEventBoundary();
        if (record) record->enterEvent();
    }
    ~ScopedSliderEvent() { if (record) record->leaveEvent(); }
    ScopedSliderEvent (const ScopedSliderEvent&) = delete;
    ScopedSliderEvent& operator= (const ScopedSliderEvent&) = delete;
private:
    std::shared_ptr<SliderEventBoundary> record;
};
}
