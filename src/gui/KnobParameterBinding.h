#pragma once

#include "../PluginProcessor.h"
#include "../ParameterDispatch.h"
#include "SliderEventBoundary.h"
#include <deque>
#include <memory>

namespace anamorph::gui
{
// Message-thread owner for a single Knob parameter binding (ADR-0072 B2).
// The processor/parameter must survive every active event. Host feedback retains
// JUCE's ParameterAttachment async-update path; it creates no new thread path.
class KnobParameterBinding final : public SliderEventBoundary,
                                   private juce::Slider::Listener,
                                   public std::enable_shared_from_this<KnobParameterBinding>
{
public:
    static std::shared_ptr<KnobParameterBinding> create (AnamorphAudioProcessor& proc,
                                                       juce::RangedAudioParameter& param,
                                                       juce::Slider& slider)
    {
        auto result = std::shared_ptr<KnobParameterBinding> (new KnobParameterBinding (proc, param, slider));
        result->configure (slider);
        return result;
    }

    void enterEvent() override { ++eventDepth; }
    void leaveEvent() override
    {
        jassert (eventDepth > 0);
        if (--eventDepth == 0) flush();
    }

    void detach()
    {
        if (! attached) return;
        attached = false;
        if (auto* s = slider.getComponent()) s->removeListener (this);
        slider = nullptr;
        pending.clear();
        // Begin/value dispatch may currently be on the stack. It must return and
        // state its installed endpoint before close can notify a surviving host.
        if (eventDepth == 0 && processing == 0) close();
    }

    void begin() { enqueue ({ Kind::begin, 0.0f, {} }); }
    void end()   { enqueue ({ Kind::end,   0.0f, {} }); }
    void afterValue (std::function<void()> continuation) override
    { enqueue ({ Kind::afterValue, 0.0f, std::move (continuation) }); }

private:
    enum class State { idle, beginPending, open, endPending, closed };
    enum class Kind  { begin, value, end, afterValue };
    struct Operation { Kind kind; float normalised; std::function<void()> continuation; };

    KnobParameterBinding (AnamorphAudioProcessor& p, juce::RangedAudioParameter& rp, juce::Slider& s)
        : processor (p), parameter (rp), slider (&s),
          feedback (rp, [this] (float value) { parameterChanged (value); }, nullptr) {}

    void configure (juce::Slider& s)
    {
        // Same range/conversion functions as official SliderParameterAttachment.
        s.valueFromTextFunction = [p = &parameter] (const juce::String& text)
            { return (double) p->convertFrom0to1 (p->getValueForText (text)); };
        s.textFromValueFunction = [p = &parameter] (double value)
            { return p->getText (p->convertTo0to1 ((float) value), 0); };
        s.setDoubleClickReturnValue (true, parameter.convertFrom0to1 (parameter.getDefaultValue()));
        auto range = parameter.getNormalisableRange();
        auto from = [range] (double lo, double hi, double v) mutable
            { range.start = (float) lo; range.end = (float) hi; return (double) range.convertFrom0to1 ((float) v); };
        auto to = [range] (double lo, double hi, double v) mutable
            { range.start = (float) lo; range.end = (float) hi; return (double) range.convertTo0to1 ((float) v); };
        auto snap = [range] (double lo, double hi, double v) mutable
            { range.start = (float) lo; range.end = (float) hi; return (double) range.snapToLegalValue ((float) v); };
        juce::NormalisableRange<double> mapped { range.start, range.end, std::move (from), std::move (to), std::move (snap) };
        mapped.interval = range.interval;
        mapped.skew = range.skew;
        mapped.symmetricSkew = range.symmetricSkew;
        s.setNormalisableRange (mapped);
        feedback.sendInitialUpdate();
        s.valueChanged();
        s.addListener (this);
    }

    void parameterChanged (float value)
    {
        const auto keepAlive = shared_from_this();
        const juce::ScopedValueSetter<bool> suppress (ignoreCallbacks, true);
        if (auto* s = slider.getComponent()) s->setValue (value, juce::sendNotificationSync);
    }

    void sliderValueChanged (juce::Slider* s) override
    {
        const auto keepAlive = shared_from_this();
        if (! ignoreCallbacks)
            enqueue ({ Kind::value, parameter.convertTo0to1 ((float) s->getValue()), {} });
    }
    void sliderDragStarted (juce::Slider*) override { begin(); }
    void sliderDragEnded (juce::Slider*) override   { end(); }

    void enqueue (Operation op)
    {
        const auto keepAlive = shared_from_this();
        if (! attached) return;
        // A direct recursive Slider write outside a JUCE event frame keeps its
        // existing synchronous request nesting (State111/112). Only an actual
        // framework boundary postpones delivery.
        if (eventDepth == 0 && flushing && op.kind == Kind::value)
        {
            apply (op);
            return;
        }
        pending.push_back (op);
        if (op.kind == Kind::begin && state != State::open) state = State::beginPending;
        if (eventDepth == 0) flush();
    }

    void flush()
    {
        const auto keepAlive = shared_from_this();
        if (flushing) return;
        const juce::ScopedValueSetter<bool> flushingNow (flushing, true);
        while (attached && ! pending.empty())
        {
            const auto op = pending.front();
            pending.pop_front();
            apply (op);
        }
        if (! attached) { pending.clear(); close(); }
    }

    void apply (Operation op)
    {
        ++processing;
        if      (op.kind == Kind::begin) open();
        else if (op.kind == Kind::value) write (op.normalised);
        else if (op.kind == Kind::afterValue) op.continuation();
        else if (gestureDepth > 0 && --gestureDepth == 0) close();
        --processing;
    }

    void open()
    {
        if (! attached) return;
        ++gestureDepth;
        if (state == State::open) return; // nested framework scope borrows the same host gesture
        state = State::open; // installed before external Begin may detach/re-enter
        produced = false;
        anamorph::param::beginChangeGesture (&parameter);
    }

    void write (float requested)
    {
        if (! attached || ! std::isfinite (requested)) return;
        const float before = parameter.getValue();
        const auto previous = processor.noteAttachmentRequest (&parameter, requested);
        const auto serial = endpointSerial;
        if (! juce::approximatelyEqual (before, requested))
            anamorph::param::setValueNotifyingHost (&parameter, requested);
        processor.restoreAttachmentRequest (previous);
        if (! juce::exactlyEqual (parameter.getValue(), before))
        {
            produced = true;
            if (serial == endpointSerial)
            {
                processor.noteOwnedParamEndpoint (&parameter, requested);
                ++endpointSerial;
            }
        }
    }

    void close()
    {
        if (state != State::open) return;
        state = State::endPending;
        if (! produced) processor.noteOwnedParamRefused (&parameter);
        gestureDepth = 0;
        state = State::closed; // consume close before external End may detach
        anamorph::param::endChangeGesture (&parameter);
    }

    AnamorphAudioProcessor& processor;
    juce::RangedAudioParameter& parameter;
    juce::Component::SafePointer<juce::Slider> slider;
    juce::ParameterAttachment feedback;
    std::deque<Operation> pending;
    State state = State::idle;
    int eventDepth = 0, processing = 0, gestureDepth = 0;
    unsigned endpointSerial = 0;
    bool attached = true, ignoreCallbacks = false, flushing = false, produced = false;
};
}
