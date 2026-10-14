// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "LiveLink",
    platforms: [.iOS(.v16)],
    products: [.library(name: "LiveLink", targets: ["LiveLink"])],
    targets: [
        .target(
            name: "LLAudio",
            path: "ios/LiveLink",
            exclude: ["LiveLinkApp.swift", "ContentView.swift", "ReceiverModel.swift",
                      "USBProbe.swift", "Info.plist", "Assets.xcassets", "LiveLink-Bridging-Header.h"],
            sources: ["AudioEngine.mm"],
            publicHeadersPath: "include",
            cxxSettings: [.headerSearchPath("../..")],
            linkerSettings: [.linkedFramework("AudioToolbox"), .linkedLibrary("c++")]
        ),
        .target(
            name: "LiveLink",
            dependencies: ["LLAudio"],
            path: "ios/LiveLink",
            exclude: ["AudioEngine.mm", "AudioEngine.h", "include", "LiveLink-Bridging-Header.h",
                      "USBProbe.swift", "Info.plist", "Assets.xcassets"],
            sources: ["LiveLinkApp.swift", "ContentView.swift", "ReceiverModel.swift"]
        )
    ],
    swiftLanguageModes: [.v5],
    cxxLanguageStandard: .cxx20
)
