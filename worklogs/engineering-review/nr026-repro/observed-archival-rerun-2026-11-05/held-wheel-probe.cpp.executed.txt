// New diagnostic, derived from the production event fixture in State test 88.
// This is not the unavailable historical held-wheel probe.
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cstdio>
#include <cstring>
#include <memory>

struct WidthListener final : juce::AudioProcessorParameter::Listener
{
    AnamorphAudioProcessor& processor;
    std::unique_ptr<juce::AudioProcessorEditor>& editor;
    bool destroy = false, armed = false;
    int values = 0, begins = 0, ends = 0, closes = 0;

    WidthListener (AnamorphAudioProcessor& p, std::unique_ptr<juce::AudioProcessorEditor>& e,
                   bool shouldDestroy) : processor (p), editor (e), destroy (shouldDestroy) {}

    void parameterValueChanged (int, float) override
    {
        ++values;
        if (armed && destroy && editor)
        {
            ++closes;
            processor.editorBeingDeleted (editor.get());
            editor.reset();
        }
    }
    void parameterGestureChanged (int, bool starting) override
    {
        if (starting) ++begins;
        else ++ends;
    }
};

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    if (argc != 2 || (std::strcmp (argv[1], "live") != 0
                      && std::strcmp (argv[1], "close") != 0)) return 2;
    const bool close = std::strcmp (argv[1], "close") == 0;
    juce::ScopedJuceInitialiser_GUI init;
    auto processor = std::make_unique<AnamorphAudioProcessor>();
    processor->prepareToPlay (48000.0, 128);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
    auto* width = processor->getAPVTS().getParameter (pid::width);
    juce::Slider* target = nullptr;
    const auto find = [&] (auto&& self, juce::Component& component) -> void
    {
        if (auto* slider = dynamic_cast<juce::Slider*> (&component);
            slider && slider->getTitle() == "Width") target = slider;
        for (auto* child : component.getChildren()) self (self, *child);
    };
    if (! editor || ! width) return 3;
    find (find, *editor);
    if (! target) return 3;

    WidthListener listener (*processor, editor, close);
    width->addListener (&listener);
    const auto before = width->getValue();
    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto position = target->getLocalBounds().getCentre().toFloat();
    const auto time = juce::Time::getCurrentTime();
    const auto event = [&] (bool held)
    {
        return juce::MouseEvent (source, position,
                                 held ? juce::ModifierKeys::leftButtonModifier : juce::ModifierKeys(),
                                 1.0f, 0, 0, 0, 0, target, target, time, position, time, 1, false);
    };
    target->mouseDown (event (true));
    std::printf ("premise: held=%d begins=%d ends=%d values=%d width=%g close=%d\n",
                 target->getThumbBeingDragged() >= 0, listener.begins, listener.ends,
                 listener.values, before, close);
    if (target->getThumbBeingDragged() < 0 || listener.begins != 1 || listener.values != 0) return 4;

    listener.armed = true;
    juce::MouseWheelDetails wheel;
    wheel.deltaX = 0.0f; wheel.deltaY = 0.2f;
    wheel.isReversed = false; wheel.isSmooth = false; wheel.isInertial = false;
    target->mouseWheelMove (event (true), wheel);

    if (editor) target->mouseUp (event (false));
    width->removeListener (&listener);
    std::printf ("result: editor=%d values=%d closes=%d begins=%d ends=%d width=%g old=%g\n",
                 editor != nullptr, listener.values, listener.closes, listener.begins,
                 listener.ends, width->getValue(), before);
    const bool good = ! close && editor && listener.values > 0 && listener.closes == 0
                   && listener.begins == 1 && listener.ends == 1
                   && ! juce::exactlyEqual (width->getValue(), before);
    if (editor) { processor->editorBeingDeleted (editor.get()); editor.reset(); }
    return good ? 0 : 5;
}
