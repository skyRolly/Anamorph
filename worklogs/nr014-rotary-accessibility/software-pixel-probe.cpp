#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
#include <memory>
#include <vector>
int main (int argc, char** argv)
{
    if (argc != 2) return 2;
    juce::ScopedJuceInitialiser_GUI init;
    auto proc = std::make_unique<AnamorphAudioProcessor>();
    proc->prepareToPlay (48000.0, 128);
    int epoch = 0;
    for (bool expanded : { false, true, false })
    {
        proc->getAPVTS().getParameter (pid::advancedMode)->setValueNotifyingHost (expanded ? 1.0f : 0.0f);
        const auto dispose = [&] (juce::AudioProcessorEditor* editor)
        { if (editor) { proc->editorBeingDeleted (editor); delete editor; } };
        std::unique_ptr<juce::AudioProcessorEditor, decltype(dispose)> editor (proc->createEditor(), dispose);
        std::vector<juce::Slider*> controls;
        const auto visit = [&] (auto&& self, juce::Component& component) -> void
        {
            if (auto* slider = dynamic_cast<juce::Slider*> (&component))
                if (slider->getTitle().isNotEmpty()) controls.push_back (slider);
            for (auto* child : component.getChildren()) self (self, *child);
        };
        visit (visit, *editor);
        if (controls.size() != 12) return 3;
        std::vector<juce::String> titles;
        for (auto* control : controls) titles.push_back (control->getTitle());
        auto corrected = editor->createComponentSnapshot (editor->getLocalBounds());
        for (auto* control : controls) control->setTitle ({});
        auto unnamed = editor->createComponentSnapshot (editor->getLocalBounds());
        for (std::size_t i = 0; i < controls.size(); ++i) controls[i]->setTitle (titles[i]);
        auto restored = editor->createComponentSnapshot (editor->getLocalBounds());
        if (!corrected.isValid() || !unnamed.isValid() || !restored.isValid()) return 4;
        long changed = 0, restoredChanged = 0, nonTransparent = 0;
        for (int y = 0; y < corrected.getHeight(); ++y)
            for (int x = 0; x < corrected.getWidth(); ++x)
            {
                const auto colour = corrected.getPixelAt (x, y);
                changed += colour != unnamed.getPixelAt (x, y);
                restoredChanged += colour != restored.getPixelAt (x, y);
                nonTransparent += colour.getAlpha() != 0;
            }
        std::printf ("epoch=%d advanced=%d pixels=%dx%d titled_controls=%zu changed=%ld restored_changed=%ld nontransparent=%ld\n",
                     epoch, expanded ? 1 : 0, corrected.getWidth(), corrected.getHeight(), controls.size(), changed, restoredChanged, nonTransparent);
        auto stream = juce::File (juce::String (argv[1])).getChildFile ("view-" + juce::String (epoch++) + ".png").createOutputStream();
        if (!stream || !juce::PNGImageFormat().writeImageToStream (corrected, *stream)) return 5;
        if (changed || restoredChanged || !nonTransparent) return 1;
    }
}
