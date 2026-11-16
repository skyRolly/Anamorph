# N2-07 PR predecessor: fresh full-source replay

A new Linux/Ninja build used predecessor PR #245 head
`f78b53208623fbd7702194e41bd931d708a837fa`, tree
`e42206512fbf54a7c354afc8fbf1f22962d72cd6`, pinned JUCE
`d60b7a78030661574a615b586d9325ba7a7fa7dd`, GCC 14.2.0 and committed
specification `17ce424197603a8100cbae05a114b9d717a64728`. The Docker container
exit was independently observed as 0 after the original command session became
unavailable. All five recorded executable exits are 0; a companion check
rehashed their retained stdout/stderr against the receipt. The original outer
shell-wrapper exit was not observed and is not asserted.

| Selection | Checks | Failures |
| --- | ---: | ---: |
| Drive endpoint | 13 | 0 |
| Multiband endpoint | 392 | 0 |
| Ordinary duck controls | 720 | 0 |
| Full DSP | 5,627 | 0 |
| Full State, Xvfb | 16,555 | 0 |

The current PR head `44ef025aab46e09a8be7069184ce79071b7d5a09` changes only
citation declarations and the documentation coverage record relative to this
head; `git diff` confirms no change in `src`, `tests`, CMakeLists or `.github`.
The exact predecessor identity is retained: this is not a fresh complete-head
replay of `44ef025a`. Its hosted checks and the predecessor's earlier citation
self-test failure are reported separately. This is automated Linux evidence,
not native DAW, listening or release acceptance.
