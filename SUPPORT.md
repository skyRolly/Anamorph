# Anamorph — internal testing guide

**For internal testers, beta testers and pre-release validation users.**

Anamorph is currently a **pre-release build**. It is not a released product, it is not on sale,
and this document is not a customer-support channel — it is the working guide for the people
testing the build, and the definition of what a usable test report contains.

---

## 1. What you have, and what you don't

**Anamorph is closed-source commercial software.** © 2026 RollyTech. All rights reserved.

Having a test build gives you permission to **install it, use it, and report what you find** —
nothing else. Specifically:

- **No source-code rights.** You do not receive Anamorph's source code, and holding a test build
  gives you no right to receive, read, request or reverse engineer it. The technical documentation
  in this repository is not a source-code licence.
- **No redistribution.** Do not pass builds, installers, links or credentials to anyone who was
  not given them by the project owner.
- **Evaluation only.** Your permission lasts as long as the owner runs the testing programme, and
  ends when they withdraw the build or the programme.
- **Disclosure.** The default reporting channel is the **public** issue tracker (§3), so ordinary
  reporting is public by design. Beyond that, no confidentiality or embargo condition is recorded
  in this repository — if the owner gave you the build under one, it came from them directly and
  not from this document.

The draft terms are in [`EULA.md`](https://github.com/skyRolly/Anamorph/blob/main/EULA.md)
§2.1 (the evaluation grant), §3 (the restrictions) and §10 (how long it lasts) — marked as a
draft; no approved licence exists yet, see
[`docs/COMMERCIAL_STATUS.md`](https://github.com/skyRolly/Anamorph/blob/main/docs/COMMERCIAL_STATUS.md).

**Treat it as pre-release.** Every automated gate passing means the build is *"ready to audition,"
not final* — do not put it on production work you cannot afford to redo. Known limitations are
listed in [`docs/KNOWN_ISSUES.md`](https://github.com/skyRolly/Anamorph/blob/main/docs/KNOWN_ISSUES.md).

Anamorph collects nothing and contacts no server; the verified detail is in
[`PRIVACY.md`](https://github.com/skyRolly/Anamorph/blob/main/PRIVACY.md).

## 2. Before you file anything — three checks

Roughly half of what gets reported is one of these.

1. **Rescan your plug-ins.** A stale host cache or a blocklist entry from an earlier failed scan
   explains most "it doesn't appear" reports. Per-host steps are in the
   [user manual FAQ](https://github.com/skyRolly/Anamorph/blob/main/docs/user/USER_MANUAL.md#9-faq--troubleshooting).
2. **Compare against unprocessed audio.** Press **Bypass** in the top bar, or switch to
   **Advanced** and set the OUTPUT panel's **Mix** to 0 % — at which point the output is
   bit-exactly the input. If the problem is still there, it is not coming from Anamorph.
3. **Check it isn't already known.**
   [Known issues](https://github.com/skyRolly/Anamorph/blob/main/docs/KNOWN_ISSUES.md) lists the
   confirmed limitations and their workarounds.

### Where the answers already are

| Question | Document |
|---|---|
| How do I install it? Where do the files go? | [Installation guide](https://github.com/skyRolly/Anamorph/blob/main/docs/user/INSTALLATION.md) |
| How does a control work? What does this panel do? | [User manual](https://github.com/skyRolly/Anamorph/blob/main/docs/user/USER_MANUAL.md) |
| It won't load / it doesn't appear / CPU is high | [User manual §FAQ & troubleshooting](https://github.com/skyRolly/Anamorph/blob/main/docs/user/USER_MANUAL.md#9-faq--troubleshooting) |
| Is this already known? | [Known issues](https://github.com/skyRolly/Anamorph/blob/main/docs/KNOWN_ISSUES.md) |
| What changed in this build? | [Changelog](https://github.com/skyRolly/Anamorph/blob/main/CHANGELOG.md) |

**Testing offline?** The user manual, this guide and the third-party attribution are attached to
every release as `Anamorph-<version>-UserManual.md`, `Anamorph-<version>-SUPPORT.md`,
`Anamorph-<version>-NOTICE.txt` and `Anamorph-<version>-THIRD_PARTY_LICENSES.md`. Per-platform
install steps ship inside the package as `INSTALL.txt`.

## 3. Where to send a test report

Send it through the **project's testing channel**:

- **Default channel — the issue tracker:** <https://github.com/skyRolly/Anamorph/issues>, using the
  **Test report — bug** form. It asks for exactly the fields below.
  ⚠ **This tracker is public.** Attach nothing you are not willing to publish.
- **Direct to the project owner** — if you were given a private channel for this testing round,
  use it, and use it for anything you should not post publicly.
  *`[OWNER]` — no private testing-channel address is recorded in this repository. Testers use the
  address they were given directly.*

There is no support desk, no ticket queue and no response-time commitment. Reports are triaged by
the project owner as testing capacity allows.

## 4. What a test report must contain

A report missing these usually costs two or three rounds of questions before anyone can act on it.
All six are required.

| # | Field | How to get it |
|---|---|---|
| 1 | **Anamorph version *and* build number** | Click the **ANAMORPH** title to open the About screen: `Version x.y.z   build N`. Both matter — the build number identifies the exact CI run the binary came from. |
| 2 | **Operating system and version** | e.g. Windows 11 24H2, macOS 15.3, Ubuntu 24.04. On macOS also say **Apple Silicon or Intel**, and whether the host runs natively or under Rosetta — some issues appear in only one. |
| 3 | **DAW / host and version** | e.g. REAPER 7.22, Ableton Live 12.1, Logic Pro 11.1. Most confirmed issues are host-specific, so the exact version matters. Say "Standalone" if you are not in a host. |
| 4 | **Plug-in format** | VST3, AU (macOS only), or the Standalone application. |
| 5 | **Reproduction steps** | The shortest sequence that shows it, **starting from a freshly inserted instance**, plus what you expected instead. If it needs a specific preset or session, say which. |
| 6 | **Logs / screenshots, where applicable** | See §5 — Anamorph writes no log file. For anything visual, attach a screenshot or a short screen recording; for a crash, attach the host's crash report if you have one. |

The report form additionally requires **how you installed** (installer or manual zip). Also worth
including when relevant: **sample rate and buffer size** (any audio problem), the **Oversampling**
setting from the Settings overlay (the single biggest influence on CPU and on whether latency is
reported, and it is session state that cannot be recovered from an attachment), and whether the
**Standalone** reproduces it — that one answer often decides where to look first.

## 5. Logs, crashes and symbols

**Anamorph writes no log file and produces no diagnostic output.** There is nothing to attach, so
don't go looking for one — reproduction steps and the version/build number are what make a report
actionable. This is a deliberate property of the build, not an omission
([`PRIVACY.md`](https://github.com/skyRolly/Anamorph/blob/main/PRIVACY.md) §2).

For a **crash**: say what you were doing, and whether it repeats. Host crash reports (macOS
Console, Windows Event Viewer) help if you have them. Test builds are **stripped**, so a stack
trace from your machine will not be symbolicated — but debug symbols are produced per CI build as
separate artifacts, so symbolication is often possible on our side. Two caveats: CI artifacts
expire, and macOS dSYM capture is best-effort under the release build configuration, so it can be
unavailable for a given build. Reproduction steps still beat a trace.

## 6. Security

Anamorph makes no network connections. It does parse untrusted input in one place: `.anamorph`
preset files and host session state are XML, so a malformed or hostile file is the realistic
attack surface.

If you think you have found a security problem, **do not open a public issue**. Use GitHub's
private vulnerability reporting on the repository if the *Security* tab offers it; otherwise open
an issue saying only that you have a security report and asking for a private contact — no details
in the public thread.

## 7. Scope of this testing programme

This covers the Anamorph plug-in and Standalone application only. Third-party DAW bugs, operating
system problems and audio-interface driver issues are out of scope — though reporting them is
still useful, since a workaround may belong in
[known issues](https://github.com/skyRolly/Anamorph/blob/main/docs/KNOWN_ISSUES.md).

Anamorph's source code, its build process and its internal documentation are **not** part of what
testers evaluate and are not open for contribution.
