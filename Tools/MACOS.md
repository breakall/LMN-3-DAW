# macOS compatibility patches

Run `bash Tools/build-macos.sh` from the repository root. The script applies
the patches idempotently before configuring and building a native Release app.
Patch files preserve the pinned dependency's CRLF context through `.gitattributes`.
Dependency commit pointers remain unchanged. These compatibility changes are
independent of the tape editor UI.

## CoreAudio

`coreaudio-compat.patch` targets the pinned JUCE revision
`b0a8bc0ce2a9241c05f47c3a2b9b6b37e8f70513` in `tracktion_engine/modules/juce`.
It contains three fixes:

1. Backport JUCE's upstream device-start deadlock fix
   [455c865d372a138eec4de64d38ce8074d542190e](https://github.com/juce-framework/JUCE/commit/455c865d372a138eec4de64d38ce8074d542190e).
   Release the callback lock around `AudioDeviceStart` and manage the IO callback
   registration with a scoped owner. The backport adapts context to this pinned
   revision, which lacks the later `CoreAudioTimeConversions` member.
2. Local fix: reallocate scratch buffers under the callback lock after assigning
   the requested buffer size. CoreAudio can report a stale size during reopening;
   retaining the earlier allocation caused a heap-buffer overflow confirmed with
   AddressSanitizer on the development Mac.
3. Local fix: defer combined-device teardown to the existing restart timer rather
   than closing devices inside a CoreAudio property notification. This avoids a
   lock cycle involving CoreAudio's HAL locks and the combiner's shutdown locks.
   The existing timer restarts with the callback retained by `start()`.

## Window snapshots

The existing `patch_macos_juce.py` script replaces the unavailable
`CGWindowListCreateImage` call when compiling with macOS 15+ SDKs. Its fallback
captures the local window view using AppKit. Older SDKs retain the original
implementation; the two capture paths do not have identical screen-capture
semantics.

## Validation and scope

Both native Release builds, the DAW's 88 tests, and its rendered tape-control
checks passed on the development Mac. The repaired diagnostic DAW also ran with
AddressSanitizer without repeating the startup buffer error. This is not a test
matrix across all macOS versions, audio interfaces, device changes or sleep/wake.

After a build, Git may show the JUCE dependency as dirty because these committed
patches have been applied to its checkout. This is expected; there is no new
third-party submodule commit to publish. Reassess the patches when updating JUCE,
since newer revisions may already include equivalent fixes.
