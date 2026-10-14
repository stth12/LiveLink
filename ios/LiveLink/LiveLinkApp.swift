import SwiftUI
@main struct LiveLinkApp: App {
    @StateObject private var model = ReceiverModel()
    var body: some Scene { WindowGroup { ContentView().environmentObject(model).preferredColorScheme(.dark) } }
}
