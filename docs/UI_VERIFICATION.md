# Native render verification

These images come from the implemented JUCE components. They are not image
mockups or invented phone connections.

![Installed desktop, actual empty service state](DesktopPreview.png)

The desktop was launched from the actual installer output. The background
process reported ready with no audio source and no authorized receivers.
The pictured pairing PIN expired when that test service exited.

![Actual streaming to explicitly labeled software test receivers](DesktopTestReceivers.png)

Five TEST-labeled software receivers were authenticated over real TCP sockets
while the stress tool generated PCM into actual shared memory. Service packet
counts and meters were live. The receivers do not implement phone telemetry,
so the dashboard accurately shows diagnostics pending. They are **not iPhones**.
The device list scrolls; only the visible cards appear in the image.

![Actual plugin processor editor under offline-service test](PluginPreview.png)

The editor rendered during the processor test with actual test-generated input
samples. The service was offline and the normal pass-through signal remained
bit-identical. This is a component render, not a commercial-DAW compatibility test.

Native clicks, alternate display scaling, accessibility, iOS appearance and
embedding the editor in the named DAWs remain manual/hardware checks.
