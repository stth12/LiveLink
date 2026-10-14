# Contributing

Describe the problem, expected behavior and reproduction steps before making
large changes. Keep audio callbacks free of allocation, locks and network I/O.
Maintain protocol compatibility or document version changes explicitly.

Run the Windows build and relevant tests described in docs/BUILD_WINDOWS.md.
For receiver changes, build the iOS target and test on a physical device when
possible. Report toolchain versions and distinguish automated results from
hardware observations. Keep credentials and machine-specific paths out of commits.

Contributions are accepted under AGPL-3.0-or-later. Preserve dependency notices.
