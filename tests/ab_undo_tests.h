// NR-022 / ADR-0008: a completed edit belongs to its outgoing A/B slot even
// when the editor's coalescer timer has not run. No message pumping or sleeps.
static void testAbSwitchPreservesACompletedUnpolledEdit()
{
    std::printf ("State test 149: completed user edits survive direct A/B switches before the poll\n");

    for (const bool toggle : { false, true })
        for (const bool polled : { false, true })
        {
            auto owner = std::make_unique<AnamorphAudioProcessor>();
            auto& p = *owner;
            auto* width = p.getAPVTS().getParameter (pid::width);
            check (width != nullptr, "149: width exists");
            if (width == nullptr) continue;
            const float initial = width->getValue();
            const auto edit = [&] (float value)
            {
                width->beginChangeGesture();
                width->setValueNotifyingHost (value);
                width->endChangeGesture();
            };
            const auto changeSlot = [&] (int slot)
            {
                if (toggle) p.abToggle();
                else        p.abSwitchTo (slot);
            };

            // Give B a distinct sound and an independent history before the
            // outgoing A edit. A false global/shared-stack fix cannot pass this.
            p.abSwitchTo (1);
            edit (0.81f);
            p.pollUndoCoalesce();
            check (p.canUndo() && ! p.canRedo(), "149 control: B owns one committed edit");
            p.abSwitchTo (0);
            checkNear (rawOf (p, pid::width), initial, 1.0e-6, "149 control: A starts independently");
            check (! p.canUndo() && ! p.canRedo(), "149 control: A owns no B history");

            const float values[] { 0.23f, 0.67f };
            for (int cycle = 0; cycle < 2; ++cycle)
            {
                edit (values[cycle]);
                // Host automation after the gesture is outside the user step.
                // Whole-state snapshots manufactured at the switch would undo it.
                const float drive = cycle == 0 ? 0.31f : 0.42f;
                setRaw (p, pid::drive, drive);
                if (cycle == 0)
                {
                    p.abSwitchTo (0); // selecting the same slot is an inert input
                    check (! p.canUndo(), "149 control: same-slot selection does not commit a pending edit");
                }
                if (polled) p.pollUndoCoalesce(); // positive control on original code
                changeSlot (1);
                checkNear (rawOf (p, pid::width), 0.81f, 1.0e-6, "149: switch adopts B sound");
                check (p.canUndo() && ! p.canRedo(), "149: B retains its independent history");
                p.undo();
                checkNear (rawOf (p, pid::width), initial, 1.0e-6, "149: B undo reaches its own before-value");
                check (! p.canUndo(), "149: switching did not add a B undo step");
                p.redo();
                checkNear (rawOf (p, pid::width), 0.81f, 1.0e-6, "149: B redo reaches its own after-value");
                changeSlot (0);
                checkNear (rawOf (p, pid::width), values[cycle], 1.0e-6, "149: A sound survives the detour");
                check (p.canUndo() && ! p.canRedo(), "149: A's completed edit survives an unpolled switch");
                p.undo();
                checkNear (rawOf (p, pid::width), cycle == 0 ? initial : values[0], 1.0e-6,
                           "149: A undo restores the completed gesture's before-value");
                checkNear (rawOf (p, pid::drive), drive, 1.0e-6,
                           "149: A undo preserves a later host write outside the gesture");
                if (cycle == 0) check (! p.canUndo(), "149: A/B switching itself is not undoable");
                p.redo();
                checkNear (rawOf (p, pid::width), values[cycle], 1.0e-6,
                           "149: A redo restores the completed gesture's after-value");
                check (! p.canRedo(), "149: a completed gesture creates exactly one redo entry");
            }
            p.undo(); p.undo();
            checkNear (rawOf (p, pid::width), initial, 1.0e-6, "149: repeated cycles retain both A edits");
            check (! p.canUndo(), "149: exactly the two A edits are in A's history");
        }

    // Neither a bare host write nor an empty gesture is an undoable user edit.
    // Always pushing a step, or pushing the A/B switch itself, fails these controls.
    for (const bool emptyGesture : { false, true })
    {
        auto p = std::make_unique<AnamorphAudioProcessor>();
        setRaw (*p, pid::width, 0.77f);
        if (emptyGesture)
            if (auto* width = p->getAPVTS().getParameter (pid::width))
            {
                width->beginChangeGesture();
                width->endChangeGesture();
            }
        p->abSwitchTo (1);
        check (! p->canUndo() && ! p->canRedo(), "149 control: an untouched B has no switch history");
        p->abSwitchTo (0);
        checkNear (rawOf (*p, pid::width), 0.77f, 1.0e-6, "149 control: an automated sound still survives A/B");
        check (! p->canUndo() && ! p->canRedo(), "149 control: automation/empty gestures create no undo entry");
    }
}
