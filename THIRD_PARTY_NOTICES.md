# Third-party components

JUCE 8.0.6, commit 51a8a6d7aeae7326956d747737ccf1575e61e209:
https://github.com/juce-framework/JUCE/tree/8.0.6
Dual-licensed commercial / AGPLv3; this source distribution uses the AGPL option.
JUCE's dependency licenses and bundled VST3 SDK notices are reproduced under
third_party/JUCE-LICENSE.md. JUCE source is fetched by CMake from the pinned upstream release. Review licensing before proprietary release.

System frameworks (Windows SDK, SwiftUI, Network, AudioToolbox) are platform
components and are not redistributed. Windows builds use the static MSVC runtime.
Inno Setup compiler is build tooling, not a shipped dependency.
