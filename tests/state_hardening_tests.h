// Included by state_tests.cpp after its shared fixtures. No separate test target.
#include "SerializedXml.h"

static void testANaNSessionSavePreservesTheExistingEncoding()
{
    std::printf ("State test 142: NaN session saves preserve the encoding without undefined conversion\n");
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    juce::ValueTree finite ("ANAMORPH");
    for (double value : { -0.0, 0.0, 1.0e-30, -24.0, 0.37, 1.0, 1.0e30 })
    {
        juce::ValueTree param ("PARAM");
        param.setProperty ("id", "finite", nullptr);
        param.setProperty ("value", value, nullptr);
        finite.appendChild (param, nullptr);
    }
    finite.setProperty ("integer", 7, nullptr);
    finite.setProperty ("boolean", true, nullptr);
    finite.setProperty ("text", "nan & <finite>", nullptr);
    checkStr (anamorph::toXmlStringWithSafeNumbers (finite), finite.toXmlString(),
              "the serialization helper is byte-identical for finite numbers and other properties");
    auto p = std::make_unique<AnamorphAudioProcessor>();
    for (float value : { 0.37f, nan, -nan, 0.81f, nan })
    {
        setRaw (*p, pid::velvetDensity, value);
        p->abCopyToOther(); // Both the live tree and separately serialized slot copies carry NaN.
        for (bool background : { false, true })
        {
            const float beforeSave = rawOf (*p, pid::velvetDensity);
            juce::MemoryBlock blob;
            if (background)
            {
                std::thread host ([&] { p->getStateInformation (blob); });
                host.join();
            }
            else p->getStateInformation (blob);
            const auto xml = BlobCodec::unwrap (blob);
            check (xml != nullptr, "the saved session is XML on either host state thread");
            if (xml == nullptr) continue;
            const auto root = juce::ValueTree::fromXml (*xml);
            const auto node = root.getChildWithName ("ANAMORPH")
                                  .getChildWithProperty ("id", pid::velvetDensity);
            const bool bad = std::isnan (value);
            const auto spelling = juce::String ((double) value, 15, true);
            check (bad ? std::isnan (rawOf (*p, pid::velvetDensity))
                       : juce::exactlyEqual (rawOf (*p, pid::velvetDensity), beforeSave),
                   "saving never changes the live host parameter");
            if (bad)
            {
                checkStr (node["value"].toString(), spelling, "NaN value keeps the platform writer's spelling");
                checkStr (node["raw"].toString(), spelling, "NaN raw keeps the platform writer's spelling");
            }
            for (const char* key : { "slotAParams", "slotBParams" })
            {
                const auto slot = juce::ValueTree::fromXml (root.getChildWithName ("AB")[key].toString());
                check (slot.isValid(), "each separately written A/B payload is XML");
                if (bad)
                    checkStr (slot.getChildWithProperty ("id", pid::velvetDensity)["raw"].toString(),
                              spelling, "a cached A/B NaN retains its spelling too");
            }
            auto restored = std::make_unique<AnamorphAudioProcessor>();
            restored->setStateInformation (blob.getData(), (int) blob.getSize());
            const float expected = bad ? 0.5f : value; // The EXISTING invalid-serialized-value rule.
            checkNear (rawOf (*restored, pid::velvetDensity), expected, 1e-6,
                       "restore keeps the existing finite/default resolution");
            restored->abSwitchTo (1);
            restored->abSwitchTo (0);
            checkNear (rawOf (*restored, pid::velvetDensity), expected, 1e-6,
                       "the A/B return leg uses the same existing resolution");
        }
        const auto file = anamorph::PresetManager::presetDirectory()
                              .getNonexistentChildFile ("__AnamorphNaNSave__", anamorph::PresetManager::fileSuffix());
        check (opCompleted (p->getPresets().saveUser (file.getFileNameWithoutExtension())),
               "a preset can be saved while the live parameter is NaN");
        const auto saved = juce::ValueTree::fromXml (file.loadFileAsString());
        check (saved.isValid(), "the saved preset is XML");
        if (std::isnan (value))
            checkStr (saved.getChildWithProperty ("id", pid::velvetDensity)["value"].toString(),
                      juce::String ((double) value, 15, true), "preset NaN preserves the existing spelling");
        check (opCompleted (p->getPresets().loadFile (file)), "the saved preset still loads");
        checkNear (rawOf (*p, pid::velvetDensity), std::isnan (value) ? 0.5 : (double) value, 1e-6,
                   "preset loading keeps its existing value/default rule");
        file.deleteFile();
    }
}

template <typename T>
static T* hardeningChild (juce::Component& parent, const std::function<bool (T&)>& matches)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* typed = dynamic_cast<T*> (child); typed != nullptr && matches (*typed)) return typed;
        if (auto* found = hardeningChild<T> (*child, matches)) return found;
    }
    return nullptr;
}

static void testAReleaseCannotClearABandMoveDuringItsStartup()
{
    std::printf ("State test 143: a nested release cannot clear a half-open band move (F10)\n");
#if JUCE_LINUX || JUCE_BSD
    auto p = std::make_unique<AnamorphAudioProcessor>();
    p->prepareToPlay (48000.0, 256);
    setRaw (*p, pid::advancedMode, 1.0f);
    setRaw (*p, pid::mbEnable, 1.0f);
    setPlain (*p, pid::mbBands, 4.0f);
    setPlain (*p, pid::mbFreqLow, 200.0f);
    setPlain (*p, pid::mbFreqMid, 2000.0f);
    setPlain (*p, pid::mbFreqHigh, 10000.0f);
    auto* ed = p->createEditor();
    auto* im = hardeningChild<anamorph::gui::SpectrumImager> (*ed, [] (auto&) { return true; });
    check (im != nullptr, "the real editor supplies the band-move surface");
    if (im == nullptr) { p->editorBeingDeleted (ed); delete ed; return; }
    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    auto event = [&] (float x, float down, bool dragged)
    {
        return juce::MouseEvent (source, { x, 11.0f }, juce::ModifierKeys::leftButtonModifier,
                                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, im, im, juce::Time::getCurrentTime(),
                                { down, 11.0f }, juce::Time::getCurrentTime(), 1, dragged);
    };
    // Discover the second headphone through production hit testing. It has TWO edge pins.
    float x = -1.0f;
    int runs = 0;
    bool inRun = false;
    for (float at = 2.0f; at < (float) im->getWidth() - 2.0f; at += 1.0f)
    {
        im->mouseMove (event (at, at, false));
        const bool hit = im->getTooltip() == "Solo this band";
        if (hit && ! inRun) ++runs;
        if (hit && runs == 2) { x = at + 2.0f; break; }
        inRun = hit;
    }
    check (x >= 0.0f, "a two-pin band-move handle is found");
    if (x >= 0.0f)
        for (bool wheel : { false, true })
            for (bool nested : { false, true, false, true })
            {
                setPlain (*p, pid::mbFreqLow, 200.0f);
                setPlain (*p, pid::mbFreqMid, 2000.0f);
                setPlain (*p, pid::mbFreqHigh, 10000.0f);
                setPlain (*p, pid::mbSolo, 0.0f);
                struct ReleaseOnOpen final : juce::AudioProcessorParameter::Listener
                {
                    std::function<void()> release;
                    bool fired = false;
                    void parameterValueChanged (int, float) override {}
                    void parameterGestureChanged (int, bool start) override
                    { if (start && ! fired && release) { fired = true; release(); } }
                } host;
                CountBothWays left, right;
                auto* lo = p->getAPVTS().getParameter (pid::mbFreqLow);
                auto* mid = p->getAPVTS().getParameter (pid::mbFreqMid);
                lo->addListener (&left); mid->addListener (&right);
                if (nested) host.release = [&] { im->mouseUp (event (x + 8.0f, x, true)); };
                lo->addListener (&host);
                im->mouseDown (event (x, x, false));
                if (wheel)
                {
                    juce::MouseWheelDetails notch {}; notch.deltaY = 0.1f;
                    im->mouseWheelMove (event (x, x, false), notch);
                }
                else im->mouseDrag (event (x + 8.0f, x, true));
                check (! nested || host.fired, "the host delivered the nested release from the first pin open");
                im->mouseUp (event (x + 8.0f, x, true));
                im->cancelActiveDrag(); // Also exercises the existing lost-release reconcile.
                check (left.opens == 1 && right.opens == 1, "both move pins are opened exactly once");
                check (left.closes == 1 && right.closes == 1, "both move pins are closed exactly once");
                check (left.closesWithNoOpen == 0 && right.closesWithNoOpen == 0,
                       "no close precedes the corresponding open");
                lo->removeListener (&host); lo->removeListener (&left); mid->removeListener (&right);
                checkNear (plainOf (*p, pid::mbSolo), 0.0, 1e-6, "a move never turns into a solo click");
            }
    p->editorBeingDeleted (ed); delete ed;
#else
    std::printf ("  SKIPPED: the established headless-editor surface is Linux/BSD only\n");
#endif
}

static juce::MemoryBlock hardeningFrameBytes (const std::string& bytes, int stated = -1)
{
    juce::MemoryBlock blob (bytes.size() + 9, true);
    auto* out = static_cast<juce::uint8*> (blob.getData());
    const juce::uint32 words[] { 0x21324356u, (juce::uint32) (stated < 0 ? bytes.size() : (size_t) stated) };
    for (int w = 0; w < 2; ++w)
        for (int i = 0; i < 4; ++i) out[4 * w + i] = (juce::uint8) ((words[w] >> (8 * i)) & 0xffu);
    std::memcpy (out + 8, bytes.data(), bytes.size());
    return blob;
}

static void testTheCurrentHostStateAcceptanceAtBothParserSurfaces()
{
    std::printf ("State test 144: original host bytes, decoded text and separately parsed A/B payloads (RISK-015)\n");
    using namespace anamorph::xmlBoundary;
    const std::string sound = "<ANAMORPH><PARAM id=\"drive\" value=\"0.9\"/></ANAMORPH>";
    const std::string outer = "<AnamorphRoot presetName=\"first\">" + sound + "</AnamorphRoot>";
    const std::string second = "<AnamorphRoot><ANAMORPH><PARAM id=\"drive\" value=\"0.1\"/></ANAMORPH></AnamorphRoot>";
    const std::string slot = "<ANAMORPH><PARAM id=\"width\" value=\"1.6\"/></ANAMORPH>";
    struct Row { const char* name; std::string suffix; };
    const Row tails[] { { "two documents", second }, { "prose", "prose" },
                        { "another element", "<JUNK/>" }, { "binary", std::string ("\x01\x02\x03", 3) },
                        { "NUL and document", std::string (1, '\0') + second } };
    auto p = std::make_unique<AnamorphAudioProcessor>();
    auto apply = [&] (const char* name, const juce::MemoryBlock& blob, bool accepted)
    {
        // Cycle on the SAME instance. An accepted prefix must apply; a refusal must leave it alone.
        for (int repeat = 0; repeat < 2; ++repeat)
        {
            setPlain (*p, pid::drive, 4.0f);
            p->setStateInformation (blob.getData(), (int) blob.getSize());
            checkNear (plainOf (*p, pid::drive), accepted ? 0.9 : 4.0, 1e-5, name);
        }
    };
    for (const auto& row : tails)
    {
        const auto bytes = outer + row.suffix;
        const auto blob = hardeningFrameBytes (bytes);
        const auto decoded = juce::String::fromUTF8 (bytes.data(), (int) bytes.size());
        check (textIsAdmissible (decoded, maxDocumentDepth, DocumentRule::parserSafetyOnly), row.name);
        const bool strict = textIsAdmissible (decoded, maxDocumentDepth, DocumentRule::oneWellFormedDocument);
        check (strict == (row.suffix.front() == '\0'), "a text-only rule cannot see bytes behind NUL");
        apply (row.name, blob, true);
        std::printf ("  outer %-18s bytes %zu, decoded %zu, strict-text %d: applied first drive\n",
                     row.name, bytes.size(), decoded.getNumBytesAsUTF8(), strict ? 1 : 0);

        // The NUL case must use a numeric entity: a literal zero byte belongs to the OUTER decoder.
        auto innerText = sessionCarrying (juce::String (slot.c_str()) + "@TAIL@");
        std::string inner (innerText.toRawUTF8());
        auto tail = row.suffix.front() == '\0' ? std::string ("&#0;") + "&lt;ANAMORPH/&gt;"
                                             : juce::String (row.suffix.c_str()).replace ("&", "&amp;")
                                                   .replace ("<", "&lt;").replace (">", "&gt;")
                                                   .replace ("\"", "&quot;").toStdString();
        inner.replace (inner.find ("@TAIL@"), 6, tail);
        const auto innerBlob = hardeningFrameBytes (inner);
        apply (row.name, innerBlob, true);
        p->abSwitchTo (0);
        checkNear (plainOf (*p, pid::width), 1.6, 1e-5, "the first inner document is actually applied");
        p->abSwitchTo (1); p->abSwitchTo (0);
        checkNear (plainOf (*p, pid::width), 1.6, 1e-5, "the parsed slot survives its return leg");
    }
    // Invalid UTF-8 placed INSIDE an attribute, with surrounding sound syntax intact.
    std::string bad = outer;
    bad.replace (bad.find ("first"), 5, std::string ("x\xc3(y", 4));
    check (! juce::CharPointer_UTF8::isValidString (bad.data(), (int) bad.size()), "the original bytes are invalid UTF-8");
    const auto decoded = juce::String::fromUTF8 (bad.data(), (int) bad.size());
    check (textIsAdmissible (decoded, maxDocumentDepth, DocumentRule::oneWellFormedDocument),
           "one-document scanning is not an original-byte encoding validator");
    apply ("invalid UTF-8 attribute", hardeningFrameBytes (bad), true);
    std::printf ("  invalid UTF-8 name retained as raw hex %s; counted UTF-8 bytes %zu\n",
                 juce::String::toHexString (p->getPresets().currentName().toRawUTF8(),
                                           (int) std::strlen (p->getPresets().currentName().toRawUTF8())).toRawUTF8(),
                 p->getPresets().currentName().getNumBytesAsUTF8());
    std::string badSlot (sessionCarrying (juce::String (slot.c_str()).replace ("<ANAMORPH>", "<ANAMORPH note=\"@BAD@\">"))
                             .toRawUTF8());
    badSlot.replace (badSlot.find ("@BAD@"), 5, std::string ("x\xc3(y", 4));
    apply ("invalid UTF-8 in inner attribute", hardeningFrameBytes (badSlot), true);
    p->abSwitchTo (0);
    checkNear (plainOf (*p, pid::width), 1.6, 1e-5, "invalid original UTF-8 does not refuse the inner payload either");

    auto half = frameHostChunk ("<AnamorphRoot><ANAMORPH><PARAM id=\"drive\" value=\"0.9\"/>"
                               "</ANAMORPH><AB active=\"0\" slotAParams=\"&lt;ANAMORPH/&gt;\"/></AnamorphRoot>");
    half.setSize (half.getSize() / 2);
    apply ("the historical half chunk still applies its prefix", half, true);
    const auto halfText = juce::String::fromUTF8 (static_cast<const char*> (half.getData()) + 8, (int) half.getSize() - 8);
    check (! textIsAdmissible (halfText, maxDocumentDepth, DocumentRule::oneWellFormedDocument),
           "strict text would newly refuse the historical parseable truncation");
    const auto partialSlot = juce::String (slot.c_str()).dropLastCharacters (5); // Ends in </ANAM.
    const auto partial = frameHostChunk (sessionCarrying (partialSlot));
    apply ("a complete outer document carrying an unclosed inner document", partial, true);
    p->abSwitchTo (0);
    checkNear (plainOf (*p, pid::width), 1.6, 1e-5, "the parseable truncated inner document applies too");
    const auto brokenInner = frameHostChunk (sessionCarrying ("<ANAMORPH><PARAM id=\"width\" value=\"1.6"));
    apply ("a refused inner payload still permits its outer session", brokenInner, true);
    p->abSwitchTo (0);
    checkNear (plainOf (*p, pid::width), 1.0, 1e-5, "a refused inner payload is re-seeded from live sound");
    apply ("an open quote in the outer document refuses the whole restore",
           frameHostChunk ("<AnamorphRoot><ANAMORPH><PARAM id=\"drive\" value=\"0.9"), false);

    apply ("an oversized declared length still parses the available complete document",
           hardeningFrameBytes (outer, (int) outer.size() + 1024), true);
    apply ("bytes beyond the declared length remain invisible",
           hardeningFrameBytes (outer + second, (int) outer.size()), true);

    // The inner size cap IS reachable under the outer cap. With the pinned native UTF-8 String,
    // invalid bytes survive the byte copy, but getNumBytesAsUTF8 counts U+FFFD for each one.
    // This fixture reaches the existing payload size guard; it is not an oversized outer test.
    const auto shell = sessionCarrying ("<ANAMORPH note=\"@PAD@\"><PARAM id=\"width\" value=\"1.6\"/></ANAMORPH>");
    std::string padded (shell.toRawUTF8());
    padded.replace (padded.find ("@PAD@"), 5, std::string (90000, (char) 0x80));
    const auto bounded = hardeningFrameBytes (padded);
    const auto outerXml = BlobCodec::unwrap (bounded);
    check (outerXml != nullptr, "the under-cap outer document carrying invalid UTF-8 parses");
    if (outerXml != nullptr)
    {
        const auto payload = outerXml->getChildByName ("AB")->getStringAttribute ("slotAParams");
        check ((juce::int64) bounded.getSize() < maxDocumentBytes, "the ORIGINAL outer chunk is below 256 KB");
        check (payload.getNumBytesAsUTF8() > (size_t) maxDocumentBytes, "the separately checked payload exceeds 256 KB");
        std::printf ("  reachable inner cap: outer %zu B, inner stored %zu B, inner counted %zu B\n",
                     bounded.getSize(), std::strlen (payload.toRawUTF8()), payload.getNumBytesAsUTF8());
        apply ("the outer session applies despite the oversized inner byte count", bounded, true);
        p->abSwitchTo (0);
        checkNear (plainOf (*p, pid::width), 1.0, 1e-5, "the INNER size guard refuses the payload and re-seeds its slot");
    }
}

// Runs LAST: this uses the real JUCE event queue and ends its dispatch loop on completion.
// It does not enable modal loops or add a production scheduling path.
static void testHostAdvancedModeWritesWithTheRealEditor()
{
    std::printf ("State test 145: advancedMode host writes, resize re-entry and editor lifetime (F12)\n");
#if JUCE_LINUX || JUCE_BSD
    auto p = std::make_unique<AnamorphAudioProcessor>();
    auto* advanced = p->getAPVTS().getParameter (pid::advancedMode);
    check (advanced != nullptr && ! advanced->isAutomatable(), "advancedMode stays registered and non-automatable");
    auto hostWrite = [&] (float value)
    {
        std::thread host ([&] { advanced->setValueNotifyingHost (value); });
        host.join();
    };
    hostWrite (1.0f);
    auto* ed = p->createEditor();
    const int tall = ed->getHeight();
    check (tall > 0, "a closed-editor host write is read at construction");
    struct ResizeRecorder final : juce::ComponentListener
    {
        bool wrongThread = false, armed = false, reentered = false;
        std::function<void()> write;
        void componentMovedOrResized (juce::Component&, bool, bool resized) override
        {
            if (! resized) return;
            wrongThread |= ! juce::MessageManager::getInstance()->isThisTheMessageThread();
            if (armed) { armed = false; reentered = true; write(); }
        }
    } resized;
    ed->addComponentListener (&resized);
    hostWrite (0.0f);
    check (ed->getHeight() == tall, "off-thread host writes defer UI work to JUCE's message queue");
    int shortHeight = 0;
    bool finished = false;
    std::function<void()> phase2, phase3, phase4;
    auto close = [&]
    {
        ed->removeComponentListener (&resized);
        p->editorBeingDeleted (ed); delete ed; ed = nullptr;
    };
    phase4 = [&]
    {
        p->pollUndoCoalesceFromTimer();
        check (ed->getHeight() == tall, "an editor built during host restore keeps the restored mode");
        check (! resized.wrongThread, "every observed resize ran on the message thread");
        close();
        finished = true;
        juce::MessageManager::getInstance()->stopDispatchLoop();
    };
    phase3 = [&]
    {
        check (ed->getHeight() == shortHeight, "recreation reads the latest host write after pending update cancellation");
        close();
        auto author = std::make_unique<AnamorphAudioProcessor>();
        setRaw (*author, pid::advancedMode, 1.0f);
        juce::MemoryBlock blob; author->getStateInformation (blob);
        // Park AFTER the host has installed the APVTS, BEFORE publishing the metadata tail.
        // Construction overlaps the actual off-thread restore without inventing a lock order.
        std::mutex mutex; std::condition_variable cv;
        bool installed = false, release = false;
        p->seams.beforeRestorePut = [&]
        {
            std::unique_lock<std::mutex> lock (mutex);
            installed = true; cv.notify_one();
            cv.wait (lock, [&] { return release; });
        };
        std::thread restore ([&] { p->setStateInformation (blob.getData(), (int) blob.getSize()); });
        {
            std::unique_lock<std::mutex> lock (mutex);
            cv.wait (lock, [&] { return installed; });
        }
        ed = p->createEditor();
        ed->addComponentListener (&resized);
        check (ed->getHeight() == tall, "construction sees the APVTS already installed by the overlapping restore");
        { std::lock_guard<std::mutex> lock (mutex); release = true; }
        cv.notify_one(); restore.join();
        p->seams.beforeRestorePut = {};
        juce::MessageManager::callAsync (phase4);
    };
    phase2 = [&]
    {
        check (ed->getHeight() == shortHeight, "the open editor applies a background host update through the queue");
        // A synchronous host write in an existing resize must leave the latest mode consistent.
        resized.write = [&] { advanced->setValueNotifyingHost (0.0f); };
        resized.armed = true;
        advanced->setValueNotifyingHost (1.0f);
        check (resized.reentered, "the host wrote advancedMode from inside the first resize");
        check (ed->getHeight() == shortHeight, "the reentrant resize settles at the latest host mode");
        // Queue a UI update, then destroy its attachment before it can be delivered.
        hostWrite (1.0f); close();
        hostWrite (0.0f);
        ed = p->createEditor(); ed->addComponentListener (&resized);
        juce::MessageManager::callAsync (phase3);
    };
    juce::MessageManager::callAsync ([&]
    {
        shortHeight = ed->getHeight();
        check (shortHeight < tall, "the queued host update actually shrinks the editor");
        hostWrite (1.0f);
        hostWrite (0.0f); // Coalescing two pending writes must keep the latest value.
        juce::MessageManager::callAsync (phase2);
    });
    juce::MessageManager::getInstance()->runDispatchLoop();
    check (finished, "all host-write and lifetime phases completed");
#else
    std::printf ("  SKIPPED: real-editor headless dispatch is verified on Linux/BSD only\n");
#endif
}

static int runRestoreAdoptionLockProbe()
{
    auto p = std::make_unique<AnamorphAudioProcessor>();
    auto& pm = p->getPresets();
    const auto originalHooks = pm.stateCommand;
    auto hooks = *originalHooks;
    const auto blob = frameHostChunk ("<AnamorphRoot presetName=\"late restore\"><ANAMORPH>"
                                     "<PARAM id=\"drive\" value=\"0.9\"/></ANAMORPH></AnamorphRoot>");
    bool injected = false, observed = false, held = false;
    hooks.drainToFixedPoint = [&]
    {
        const bool drained = originalHooks->drainToFixedPoint();
        if (! injected)
        {
            injected = true;
            // Exactly the gap AFTER the gate's final drain and BEFORE it takes the lock.
            std::thread host ([&] { p->setStateInformation (blob.getData(), (int) blob.getSize()); });
            host.join();
        }
        return drained;
    };
    pm.stateCommand = &hooks;
    p->seams.afterRestoreTake = [&]
    {
        observed = true;
        // This acquisition cannot wait or pump. It measures the remaining OUTER ownership
        // after adoptPendingHostState's own scoped acquisition has already ended.
        std::thread witness ([&]
        {
            const juce::ScopedTryLock attempt (*hooks.soundReplacement);
            held = ! attempt.isLocked();
        });
        witness.join();
    };
    {
        const auto admission = pm.stateCommandAdmission ([] {});
        check (admission.admitted(), "the production preset command gate admits after its drain");
        if (admission.admitted()) pm.onAboutToSave(); // The actual saveUser/writeUserPreset callback.
    }
    pm.stateCommand = originalHooks;
    p->seams.afterRestoreTake = {};
    check (injected && observed, "the late restore reached the production adoption door");
    std::printf ("F9: late restore adopted while an outer command owns soundReplacement: %s\n",
                 held ? "yes (lock edge established; no real-host deadlock claimed)" : "no");
    // The related F9 window: a real latency notification pumps another adoption while the
    // first tail is applying Settings. Host state calls themselves remain serialized.
    p->prepareToPlay (48000.0, 256);
    const auto older = frameHostChunk ("<AnamorphRoot presetName=\"older\"><ANAMORPH>"
                                      "<PARAM id=\"width\" value=\"1.6\"/></ANAMORPH>"
                                      "<ANAMORPH_INTERNAL int_oversample=\"2\"/></AnamorphRoot>");
    const auto newer = frameHostChunk ("<AnamorphRoot presetName=\"newer\"><ANAMORPH>"
                                      "<PARAM id=\"width\" value=\"0.6\"/></ANAMORPH>"
                                      "<ANAMORPH_INTERNAL int_oversample=\"3\"/></AnamorphRoot>");
    struct PumpOnLatency final : juce::AudioProcessorListener
    {
        bool armed = true, fired = false;
        std::function<void()> pump;
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
        {
            if (armed && details.latencyChanged) { armed = false; fired = true; pump(); }
        }
    } host;
    host.pump = [&]
    {
        std::thread restore ([&] { p->setStateInformation (newer.getData(), (int) newer.getSize()); });
        restore.join();
        p->pollUndoCoalesceFromTimer();
    };
    p->addListener (&host);
    std::thread restore ([&] { p->setStateInformation (older.getData(), (int) older.getSize()); });
    restore.join();
    p->pollUndoCoalesceFromTimer();
    p->removeListener (&host);
    std::printf ("F9: latency callout fired=%d, final width=%g, final preset=%s\n", host.fired ? 1 : 0,
                 (double) plainOf (*p, pid::width), pm.currentName().toRawUTF8());
    return failures == 0 ? 0 : 1;
}

static int runNonFiniteIngressProbe()
{
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float inf = std::numeric_limits<float>::infinity();
    for (const char* id : { pid::width, pid::velvetDensity, pid::monoMakerFreq, pid::advancedMode })
    {
        auto p = std::make_unique<AnamorphAudioProcessor>();
        auto* parameter = p->getAPVTS().getParameter (id);
        for (float value : { nan, inf, -inf })
        {
            parameter->setValueNotifyingHost (value);
            std::printf ("host %-16s input=%g getValue=%g atomic=%g\n", id, (double) value,
                         (double) parameter->getValue(), (double) p->getAPVTS().getRawParameterValue (id)->load());
        }
        for (const char* text : { "nan", "inf", "-inf" })
            std::printf ("text %-16s %-4s -> normalised=%g\n", id, text, (double) parameter->getValueForText (text));
    }
    struct ObserveNaN final : juce::AudioProcessorParameter::Listener
    {
        int nanNotifications = 0;
        void parameterValueChanged (int, float value) override { if (std::isnan (value)) ++nanNotifications; }
        void parameterGestureChanged (int, bool) override {}
    } observed;
    auto p = std::make_unique<AnamorphAudioProcessor>();
    auto* width = p->getAPVTS().getParameter (pid::width);
    width->addListener (&observed);
    for (const char* raw : { "nan", "0.71", "inf", "0.71" })
    {
        const auto text = juce::String ("<AnamorphRoot><ANAMORPH><PARAM id=\"width\" value=\"nan\" raw=\"")
                          + raw + "\"/></ANAMORPH></AnamorphRoot>";
        const auto blob = frameHostChunk (text);
        observed.nanNotifications = 0;
        p->setStateInformation (blob.getData(), (int) blob.getSize());
        juce::MemoryBlock saved; p->getStateInformation (saved);
        const auto xml = BlobCodec::unwrap (saved);
        const auto node = juce::ValueTree::fromXml (*xml).getChildWithName ("ANAMORPH")
                              .getChildWithProperty ("id", pid::width);
        std::printf ("restore value=nan raw=%s -> getValue=%g, NaN notifications=%d, saved value=%s raw=%s\n",
                     raw, (double) width->getValue(), observed.nanNotifications,
                     node["value"].toString().toRawUTF8(), node["raw"].toString().toRawUTF8());
    }
    width->removeListener (&observed);
    return 0;
}

static void probeRestoreTailReentry()
{
    std::printf ("F9 diagnostic (opt-in, unresolved): nested restore adoption and callback-save coherence\n");
    for (bool nested : { false, true, false, true })
    {
        auto p = std::make_unique<AnamorphAudioProcessor>();
        p->prepareToPlay (48000.0, 256);
        const auto older = frameHostChunk ("<AnamorphRoot presetName=\"older\"><ANAMORPH>"
                                          "<PARAM id=\"width\" value=\"1.6\"/></ANAMORPH>"
                                          "<ANAMORPH_INTERNAL int_oversample=\"2\"/></AnamorphRoot>");
        const auto newer = frameHostChunk ("<AnamorphRoot presetName=\"newer\"><ANAMORPH>"
                                          "<PARAM id=\"width\" value=\"0.6\"/></ANAMORPH>"
                                          "<ANAMORPH_INTERNAL int_oversample=\"3\"/></AnamorphRoot>");
        struct Host final : juce::AudioProcessorListener
        {
            bool armed = false, fired = false, messageThread = true;
            std::function<void()> pump;
            void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
            void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
            {
                if (! details.latencyChanged) return;
                messageThread &= juce::MessageManager::getInstance()->isThisTheMessageThread();
                if (armed) { armed = false; fired = true; pump(); }
            }
        } host;
        host.armed = nested;
        host.pump = [&]
        {
            std::thread restore ([&] { p->setStateInformation (newer.getData(), (int) newer.getSize()); });
            restore.join(); // The first HOST state call has already returned: no concurrent host calls.
            p->pollUndoCoalesceFromTimer(); // The real timer door, pumped inside the latency callout.
            juce::MemoryBlock saved;
            p->getStateInformation (saved);
            const auto xml = BlobCodec::unwrap (saved);
            check (xml != nullptr, "the callback save produces a session");
            if (xml != nullptr)
            {
                checkStr (xml->getStringAttribute ("presetName"), "newer",
                          "a callback save must not combine old metadata with the newer sound");
                const auto sound = juce::ValueTree::fromXml (*xml).getChildWithName ("ANAMORPH");
                const auto width = sound.getChildWithProperty ("id", pid::width);
                checkNear ((double) width["value"], 0.6, 1e-5, "the callback save contains the newer sound");
            }
        };
        p->addListener (&host);
        std::thread restore ([&] { p->setStateInformation (older.getData(), (int) older.getSize()); });
        restore.join();
        p->pollUndoCoalesceFromTimer();
        check (host.fired == nested, "the nested and control legs exercise different adoption schedules");
        check (host.messageThread, "the real latency callout remains on the message thread");
        checkNear (plainOf (*p, pid::width), nested ? 0.6 : 1.6, 1e-5, "the latest installed sound remains live");
        checkStr (p->getPresets().currentName(), nested ? "newer" : "older",
                  "the final metadata belongs to that same restore");
        check ((int) p->getInternal().oversampleValue().getValue() == (nested ? 3 : 2),
               "the latest restore's Settings tree and sound agree");
        p->pollUndoCoalesceFromTimer();
        checkStr (p->getPresets().currentName(), nested ? "newer" : "older",
                  "the next poll does not revive an older tail");
        p->removeListener (&host);
    }
}
