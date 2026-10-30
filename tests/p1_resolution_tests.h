// Included after state_hardening_tests.h in the existing real-processor target.

static void testFiniteParameterBoundaryAndRestoreWindow()
{
    std::printf ("State test 147: all parameter boundaries, readback, audio and usable-raw restore windows\n");
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float inf = std::numeric_limits<float>::infinity();
    for (bool advanced : { false, true })
    {
        auto p = std::make_unique<AnamorphAudioProcessor>();
        auto reference = std::make_unique<AnamorphAudioProcessor>();
        setRaw (*p, pid::advancedMode, advanced ? 1.0f : 0.0f);
        setRaw (*reference, pid::advancedMode, advanced ? 1.0f : 0.0f);
        p->prepareToPlay (48000.0, 64);
        reference->prepareToPlay (48000.0, 64);
        juce::AudioBuffer<float> a (2, 64), b (2, 64);
        juce::MidiBuffer midi;
        juce::Random random (147);
        for (auto* parameter : p->getParameters())
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (parameter);
            check (rp != nullptr, "every registered parameter has a finite-default ranged binding");
            if (rp == nullptr) continue;
            auto* twin = reference->getAPVTS().getParameter (rp->paramID);
            const float original = rp->getValue();
            for (int ingress = 0; ingress < 3; ++ingress) // host, text, direct raw-boundary fault
                for (int bad = 0; bad < 3; ++bad)
                {
                    rp->setValueNotifyingHost (0.37f);
                    twin->setValueNotifyingHost (0.37f);
                    const float value = bad == 0 ? nan : bad == 1 ? inf : -inf;
                    auto* atomic = p->getAPVTS().getRawParameterValue (rp->paramID);
                    if (ingress == 2) atomic->store (value);
                    else rp->setValueNotifyingHost (ingress == 0 ? value
                        : rp->getValueForText (bad == 0 ? "nan" : bad == 1 ? "inf" : "-inf"));
                    const float raw = atomic->load();
                    const float finiteNorm = std::isfinite (raw) ? rp->convertTo0to1 (raw) : rp->getDefaultValue();
                    twin->setValueNotifyingHost (finiteNorm);
                    const float hostReadback = rp->getValue();
                    const auto label = rp->paramID + " ingress=" + juce::String (ingress) + " nonfinite=" + juce::String (bad);
                    check (bulkswap::same (p->readEngineSnapshot(), reference->readEngineSnapshot()),
                           (label + ": entire snapshot matches the finite control").toRawUTF8());
                    if (ingress != 2)
                    {
                        // ARM's finite range/snap round-trip can format the
                        // zero-gain control as "-0.0 dB". NaN formatting uses
                        // the declared +0 dB directly; do not change valid
                        // finite formatting to make these strings coincide.
                        const auto expectedText = rp->paramID == pid::outputGain && std::isnan (hostReadback)
                            ? juce::String ("0.0 dB") : twin->getText (twin->getValue(), 128);
                        checkStr (rp->getText (hostReadback, 128), expectedText,
                                  (label + ": host display uses the declared NaN default or the unchanged finite control").toRawUTF8());
                    }
                    bool identical = true, finite = true;
                    for (int lifecycle = 0; lifecycle < 3; ++lifecycle)
                    {
                        if (lifecycle == 1) { p->reset(); reference->reset(); }
                        if (lifecycle == 2)
                        {
                            p->prepareToPlay (44100.0, 64);
                            reference->prepareToPlay (44100.0, 64);
                        }
                        for (int block = 0; block < 8; ++block)
                        {
                            for (int channel = 0; channel < 2; ++channel)
                                for (int i = 0; i < 64; ++i)
                                {
                                    const float sample = (random.nextFloat() - 0.5f) * 0.5f;
                                    a.setSample (channel, i, sample); b.setSample (channel, i, sample);
                                }
                            p->processBlock (a, midi); reference->processBlock (b, midi);
                            for (int channel = 0; channel < 2; ++channel)
                                for (int i = 0; i < 64; ++i)
                                {
                                    const float x = a.getSample (channel, i), y = b.getSample (channel, i);
                                    finite &= std::isfinite (x);
                                    identical &= std::memcmp (&x, &y, sizeof x) == 0;
                                }
                        }
                    }
                    check (finite && identical, (label + ": live/reset/re-prepare audio matches finite control bit-for-bit").toRawUTF8());
                    const float after = rp->getValue();
                    check (std::memcmp (&hostReadback, &after, sizeof after) == 0,
                           (label + ": DSP, formatting, reset and re-prepare preserve exact host readback").toRawUTF8());
                    rp->setValueNotifyingHost (original); twin->setValueNotifyingHost (original);
                }
        }
    }

    // JUCE used to install an unusable value before the authoritative raw value.
    // Observe the actual first notification, not only the eventual finite result.
    for (bool background : { false, true })
    {
        auto p = std::make_unique<AnamorphAudioProcessor>();
        auto* width = p->getAPVTS().getParameter (pid::width);
        for (const char* bad : { "nan", "inf", "-inf", "nan" })
        {
            width->setValueNotifyingHost (0.2f);
            struct Observer final : juce::AudioProcessorParameter::Listener
            {
                AnamorphAudioProcessor* p = nullptr;
                int calls = 0;
                bool coherent = true;
                float lastValue = 0.0f, lastSnapshot = 0.0f;
                void parameterValueChanged (int, float value) override
                {
                    ++calls;
                    lastValue = value; lastSnapshot = p->readEngineSnapshot().width;
                    // JUCE may notify this listener before its APVTS adapter
                    // updates the raw atomic. Old or resolved sound is valid;
                    // a transient NaN/default is not the authoritative raw value.
                    coherent &= std::isfinite (value) && std::abs (value - 0.71f) < 1e-6f
                             && (std::abs (lastSnapshot - 0.4f) < 1e-6f || std::abs (lastSnapshot - 1.42f) < 1e-6f);
                }
                void parameterGestureChanged (int, bool) override {}
            } observer;
            observer.p = p.get(); width->addListener (&observer);
            const auto blob = frameHostChunk (juce::String ("<AnamorphRoot><ANAMORPH><PARAM id=\"width\" value=\"")
                + bad + "\" raw=\"0.71\"/></ANAMORPH></AnamorphRoot>");
            if (background)
            {
                std::thread host ([&] { p->setStateInformation (blob.getData(), (int) blob.getSize()); });
                host.join(); p->pollUndoCoalesceFromTimer();
            }
            else p->setStateInformation (blob.getData(), (int) blob.getSize());
            width->removeListener (&observer);
            if (! observer.coherent)
                std::printf ("  unusable=%s background=%d notifications=%d last=%g snapshot=%g\n",
                             bad, background ? 1 : 0, observer.calls, (double) observer.lastValue, (double) observer.lastSnapshot);
            check (observer.calls > 0 && observer.coherent, "usable raw wins before JUCE's first notification, on either state thread");
            checkNear (width->getValue(), 0.71, 1e-6, "usable raw remains the exact final restore source");
            juce::MemoryBlock saved; p->getStateInformation (saved);
            const auto xml = BlobCodec::unwrap (saved);
            check (xml != nullptr, "the repaired restore can be saved");
            if (xml != nullptr)
            {
                const auto node = juce::ValueTree::fromXml (*xml).getChildWithName ("ANAMORPH").getChildWithProperty ("id", pid::width);
                float number = 0.0f;
                check (anamorph::usableSerializedNumber (node["value"], number), "the private repair keeps an unusable value out of the next saved session");
            }
        }
    }

    // Invalid serialized numbers retain the existing default resolution through
    // sessions, presets and both A/B slots; repeat on the same processor.
    auto p = std::make_unique<AnamorphAudioProcessor>();
    for (const char* bad : { "nan", "inf", "-inf", "nan" })
    {
        for (const char* id : { pid::width, pid::velvetDensity, pid::outputGain })
        {
            auto* parameter = p->getAPVTS().getParameter (id);
            const auto text = juce::String ("<ANAMORPH><PARAM id=\"") + id + "\" value=\"" + bad + "\"/></ANAMORPH>";
            const auto blob = frameHostChunk ("<AnamorphRoot>" + text + "</AnamorphRoot>");
            parameter->setValueNotifyingHost (0.87f);
            p->setStateInformation (blob.getData(), (int) blob.getSize());
            checkNear (parameter->getValue(), parameter->getDefaultValue(), 1e-6, "unusable serialized session value retains the declared-default rule");
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                .getNonexistentChildFile ("AnamorphP1Finite", ".anamorph");
            check (file.replaceWithText (text), "the invalid-number preset fixture is written");
            check (opCompleted (p->getPresets().loadFile (file)), "the existing preset loader accepts the well-formed invalid-number fixture");
            checkNear (parameter->getValue(), parameter->getDefaultValue(), 1e-6, "preset invalid values keep their existing default resolution");
            file.deleteFile();
            parameter->setValueNotifyingHost (bad[0] == '-' ? -inf : bad[0] == 'i' ? inf : nan);
            p->abCopyToOther();
            p->abSwitchTo (1); p->abSwitchTo (0);
            check (std::isfinite (parameter->getValue()), "A/B return resolves invalid captured values to finite session values");
            parameter->setValueNotifyingHost (0.23f);
            check (std::isfinite (p->readEngineSnapshot().outputGainDb), "a later finite automation value recovers without history-dependent state");
        }
    }
}

static void testRestoreCommitBeforeLatencyCallbacks()
{
    std::printf ("State test 148: complete Settings/A-B/Undo/preset state before nested latency callbacks\n");
    probeRestoreTailReentry(); // Retain every assertion in the original failing diagnostic.
    const auto makeRoot = [] (const char* name, float width, int oversample, int active)
    {
        juce::ValueTree root ("AnamorphRoot"), sound ("ANAMORPH"), param ("PARAM"), settings ("ANAMORPH_INTERNAL"), ab ("AB");
        root.setProperty ("presetName", name, nullptr);
        root.setProperty ("presetBaseline", juce::String (name) + "-baseline", nullptr);
        root.setProperty ("presetSource", "factory", nullptr);
        root.setProperty ("presetFactoryId", active == 0 ? "default" : "gentleWidth", nullptr);
        param.setProperty ("id", pid::width, nullptr); param.setProperty ("value", width, nullptr); sound.appendChild (param, nullptr);
        root.appendChild (sound, nullptr);
        for (const auto& field : anamorph::InternalState::settings())
        {
            auto value = field.defaultValue;
            if (field.id == anamorph::iid::oversample) value = oversample;
            else if (field.id == anamorph::iid::uiScale) value = active == 0 ? 2 : 4;
            else if (field.id == anamorph::iid::scopePersist) value = active == 0 ? 0.2 : 0.8;
            else value = active != 0;
            settings.setProperty (field.id, value, nullptr);
        }
        root.appendChild (settings, nullptr);
        ab.setProperty ("active", active, nullptr);
        for (int slot = 0; slot < 2; ++slot)
        {
            auto payload = sound.createCopy();
            payload.getChild (0).setProperty ("value", width + (slot == 0 ? 0.1f : -0.1f), nullptr);
            const auto prefix = juce::String (slot == 0 ? "slotA" : "slotB");
            ab.setProperty (prefix + "Params", payload.toXmlString(), nullptr);
            ab.setProperty (prefix + "Name", juce::String (name) + prefix, nullptr);
            ab.setProperty (prefix + "Base", juce::String (name) + prefix + "-base", nullptr);
            ab.setProperty (prefix + "Source", "factory", nullptr);
            ab.setProperty (prefix + "FactoryId", slot == 0 ? "default" : "gentleWidth", nullptr);
        }
        root.appendChild (ab, nullptr);
        return root;
    };
    const auto older = makeRoot ("older", 1.6f, 2, 0), newer = makeRoot ("newer", 0.6f, 3, 1);
    const auto encode = [] (const juce::ValueTree& root) { return frameHostChunk (root.toXmlString()); };
    const auto oldBlob = encode (older), newBlob = encode (newer);
    const auto verify = [] (AnamorphAudioProcessor& p, const juce::ValueTree& expected,
                           const juce::MemoryBlock* alreadySaved = nullptr)
    {
        juce::MemoryBlock saved;
        if (alreadySaved != nullptr) saved = *alreadySaved;
        else p.getStateInformation (saved);
        const auto xml = BlobCodec::unwrap (saved);
        check (xml != nullptr, "latency callback save is valid XML");
        if (xml == nullptr) return;
        const auto root = juce::ValueTree::fromXml (*xml);
        for (const char* key : { "presetName", "presetBaseline", "presetSource", "presetFactoryId" })
            checkStr (root[key].toString(), expected[key].toString(), "callback save has one restore's complete preset metadata");
        checkNear (plainOf (p, pid::width), (double) expected.getChildWithName ("ANAMORPH").getChild (0)["value"], 1e-5,
                   "callback save and live sound belong to the same restore");
        checkNear ((double) root.getChildWithName ("ANAMORPH").getChildWithProperty ("id", pid::width)["value"],
                   (double) expected.getChildWithName ("ANAMORPH").getChild (0)["value"], 1e-5,
                   "the actual saved sound belongs to the same restore as its metadata");
        const auto settings = root.getChildWithName ("ANAMORPH_INTERNAL");
        for (const auto& field : anamorph::InternalState::settings())
            checkNear ((double) settings[field.id], (double) expected.getChildWithName ("ANAMORPH_INTERNAL")[field.id],
                       1e-6, "all Settings fields are committed before the callback");
        const auto ab = root.getChildWithName ("AB"), expectedAb = expected.getChildWithName ("AB");
        check ((int) ab["active"] == (int) expectedAb["active"], "the callback observes the committed active A/B slot");
        for (const char* prefix : { "slotA", "slotB" })
        {
            for (const char* suffix : { "Name", "Base", "Source", "FactoryId" })
                checkStr (ab[juce::String (prefix) + suffix].toString(), expectedAb[juce::String (prefix) + suffix].toString(),
                          "both A/B slots retain the current restore's metadata");
            const auto payload = juce::ValueTree::fromXml (ab[juce::String (prefix) + "Params"].toString());
            const auto expectedPayload = juce::ValueTree::fromXml (expectedAb[juce::String (prefix) + "Params"].toString());
            checkNear ((double) payload.getChildWithProperty ("id", pid::width)["value"], (double) expectedPayload.getChild (0)["value"],
                       1e-5, "both callback-saved A/B sounds match their metadata");
        }
        check (! p.canUndo(), "a fully committed restore has cleared obsolete Undo state");
        check (p.getPresets().isDirty(), "the supplied nonempty dirty baseline survives callback saves");
    };
    for (bool outerBackground : { false, true })
        for (bool innerBackground : { false, true })
            for (bool pump : { false, true })
            {
                auto p = std::make_unique<AnamorphAudioProcessor>();
                p->prepareToPlay (48000.0, 64);
                for (bool nested : { true, false, true })
                {
                    struct Host final : juce::AudioProcessorListener
                    {
                        std::function<void()> callback;
                        bool armed = true, fired = false;
                        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
                        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
                        { if (details.latencyChanged && armed) { armed = false; fired = true; callback(); } }
                    } host;
                    host.callback = [&]
                    {
                        verify (*p, older); // No newer restore yet: older must already be complete.
                        if (! nested) return;
                        if (innerBackground)
                        {
                            std::thread next ([&] { p->setStateInformation (newBlob.getData(), (int) newBlob.getSize()); });
                            next.join();
                        }
                        else p->setStateInformation (newBlob.getData(), (int) newBlob.getSize());
                        if (pump) p->pollUndoCoalesceFromTimer();
                        verify (*p, newer); // A save itself must drain when no timer was pumped.
                    };
                    // Force a changed latency on every cycle, including a pending older
                    // handoff that an inline restore drains before decoding its own bytes.
                    p->setStateInformation (newBlob.getData(), (int) newBlob.getSize());
                    if (! outerBackground && pump)
                    {
                        std::thread prior ([&] { p->setStateInformation (oldBlob.getData(), (int) oldBlob.getSize()); });
                        prior.join();
                    }
                    p->addListener (&host);
                    if (outerBackground)
                    {
                        std::thread first ([&] { p->setStateInformation (oldBlob.getData(), (int) oldBlob.getSize()); });
                        first.join(); p->pollUndoCoalesceFromTimer();
                    }
                    else p->setStateInformation (oldBlob.getData(), (int) oldBlob.getSize());
                    p->removeListener (&host);
                    check (host.fired, "each repeated schedule enters the real latency callback");
                    verify (*p, nested ? newer : older);
                    auto* width = p->getAPVTS().getParameter (pid::width);
                    const float before = width->getValue();
                    width->beginChangeGesture(); width->setValueNotifyingHost (0.41f);
                    p->noteOwnedParamWrite (width, before, width->getValue()); width->endChangeGesture();
                    p->pollUndoCoalesce(); check (p->canUndo(), "the next owned edit starts a new Undo boundary");
                    p->undo();
                    checkNear (width->getValue(), before, 1e-6, "Undo restores the latest completed restore's sound");
                    const int active = p->abActiveSlot(), other = active == 0 ? 1 : 0;
                    p->abSwitchTo (other);
                    const auto expected = nested ? newer : older;
                    checkStr (p->getPresets().currentName(), expected.getChildWithName ("AB")[other == 0 ? "slotAName" : "slotBName"].toString(),
                              "A/B switch uses the latest restore's other-slot metadata");
                    p->abSwitchTo (active);
                    checkNear (width->getValue(), before, 1e-6, "A/B return preserves the current restore after a later edit/Undo");
                }
            }

    // A callback may only publish a background restore and RETURN: no nested
    // pump/save supplies adoption on its behalf. The OUTER save/drain must still
    // reach a fixed point after the notification before reading program state.
    for (bool saveDoor : { true, false })
    {
        auto p = std::make_unique<AnamorphAudioProcessor>();
        p->prepareToPlay (48000.0, 64);
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            p->setStateInformation (newBlob.getData(), (int) newBlob.getSize());
            struct UnpumpedHost final : juce::AudioProcessorListener
            {
                std::function<void()> callback;
                bool fired = false;
                void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
                void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
                {
                    if (details.latencyChanged && ! fired)
                    {
                        fired = true;
                        callback();
                    }
                }
            } host;
            host.callback = [&]
            {
                std::thread newerHost ([&] { p->setStateInformation (newBlob.getData(), (int) newBlob.getSize()); });
                newerHost.join(); // intentionally no pump or callback save
            };
            p->addListener (&host);
            std::thread olderHost ([&] { p->setStateInformation (oldBlob.getData(), (int) oldBlob.getSize()); });
            olderHost.join();
            juce::MemoryBlock saved;
            if (saveDoor) p->getStateInformation (saved);
            else p->pollUndoCoalesceFromTimer();
            p->removeListener (&host);
            check (host.fired, "each outer save/timer drain reaches the unpumped callback");
            checkStr (p->getPresets().currentName(), "newer",
                      "the outer drain adopts a callback-published restore before its caller reads metadata");
            if (! saveDoor) p->getStateInformation (saved);
            verify (*p, newer, &saved); // never replace already-captured bytes with a later repaired save
        }
    }

    // Reproduce the late-arrival edge from the real preset admission gate. A
    // callback must not run while an OUTER command still owns the lock, even
    // after the adoption's own lock has ended. Witness with tryEnter, never wait.
    auto p = std::make_unique<AnamorphAudioProcessor>();
    p->prepareToPlay (48000.0, 64);
    auto& presets = p->getPresets();
    const auto* originalHooks = presets.stateCommand;
    auto hooks = *originalHooks;
    bool injected = false, notified = false, lockAvailable = false;
    hooks.drainToFixedPoint = [&]
    {
        const bool drained = originalHooks->drainToFixedPoint();
        if (! injected)
        {
            injected = true;
            std::thread host ([&] { p->setStateInformation (oldBlob.getData(), (int) oldBlob.getSize()); });
            host.join();
        }
        return drained;
    };
    struct LockWitness final : juce::AudioProcessorListener
    {
        std::function<void()> callback;
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
        { if (details.latencyChanged) callback(); }
    } witness;
    witness.callback = [&]
    {
        notified = true;
        std::thread probe ([&]
        {
            const juce::ScopedTryLock attempt (*originalHooks->soundReplacement);
            lockAvailable = attempt.isLocked();
        });
        probe.join();
        verify (*p, older);
    };
    p->addListener (&witness); presets.stateCommand = &hooks;
    {
        const auto admission = presets.stateCommandAdmission ([] {});
        check (admission.admitted(), "the actual preset gate admits the controlled late-arrival schedule");
        if (admission.admitted())
        {
            presets.onAboutToSave();
            p->pollUndoCoalesceFromTimer();
            check (! notified, "neither adoption nor a nested timer door calls the host under the outer command lock");
        }
    }
    presets.stateCommand = originalHooks;
    p->pollUndoCoalesceFromTimer();
    check (notified && lockAvailable, "the pending report is delivered after the command and replacement lock have ended");
    p->removeListener (&witness);
}
