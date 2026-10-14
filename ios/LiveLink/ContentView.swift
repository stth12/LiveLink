import SwiftUI
import Network
struct ContentView: View {
    @EnvironmentObject var model: ReceiverModel
    @State private var pin = ""
    @State private var host = ""
    private let pink = Color(red:1,green:0.176,blue:0.553)
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment:.leading,spacing:24) {
                    Text("LIVE LINK").font(.system(size:34,weight:.bold,design:.rounded))
                    Text("Your Sound. Everywhere.").foregroundStyle(.secondary)
                    VStack(alignment:.leading,spacing:14) {
                        Label(model.status,systemImage:"waveform").foregroundStyle(pink).font(.headline)
                        if !model.computer.isEmpty { Text(model.computer).font(.title2) }
                        Text("\(model.rate) Hz · Stereo PCM · Wi-Fi")
                        ProgressView(value:Double(min(1,max(0,model.peak)))).tint(pink)
                        Text(String(format:"Buffer estimate %.1f ms · jitter %.1f ms", model.rate > 0 ? Double(model.occupancy)*1000/Double(model.rate) : 0,model.jitterMs)).font(.caption).foregroundStyle(.secondary)
                        Text("Loss \(model.loss) · Underruns \(model.underruns)").font(.caption)
                        Text("Buffer estimate excludes network and device output latency.").font(.caption2).foregroundStyle(.secondary)
                    }.padding(20).frame(maxWidth:.infinity,alignment:.leading).background(Color.white.opacity(0.04),in:RoundedRectangle(cornerRadius:18))
                    if ["CONNECTED","STREAMING","PAUSED"].contains(model.status) {
                        Slider(value:$model.volume,in:0...1,onEditingChanged:{ editing in if !editing { model.controlsChanged() } }).tint(pink)
                        Toggle("Mute",isOn:$model.muted).onChange(of:model.muted) { _ in model.controlsChanged() }
                        Toggle("Mono",isOn:$model.mono).onChange(of:model.mono) { _ in model.controlsChanged() }
                        Picker("Buffer",selection:$model.mode) { Text("Ultra low").tag(0); Text("Balanced").tag(1); Text("Stable").tag(2) }.pickerStyle(.segmented).onChange(of:model.mode) { _ in model.controlsChanged() }
                        if model.status == "PAUSED" { Button("RESUME AUDIO") { model.resumePlayback() }.buttonStyle(.borderedProminent).tint(pink) }
                        Button("DISCONNECT",role:.destructive) { model.disconnect() }.buttonStyle(.borderedProminent).tint(pink)
                    } else {
                        TextField("Pairing PIN from your computer",text:$pin).keyboardType(.numberPad).textFieldStyle(.roundedBorder)
                        Text("AVAILABLE COMPUTERS").font(.caption).foregroundStyle(.secondary)
                        ForEach(model.computers,id:\.endpoint) { result in Button(model.displayName(for:result.endpoint)) { model.connect(to:result.endpoint,pin:pin,name:model.displayName(for:result.endpoint)) }.buttonStyle(.bordered).disabled(pin.count != 8) }
                        if model.computers.isEmpty { Text("Searching the local network…").foregroundStyle(.secondary) }
                        TextField("Computer IPv4 address",text:$host).keyboardType(.decimalPad).textFieldStyle(.roundedBorder)
                        Button("CONNECT BY ADDRESS") { model.manualConnect(host:host,pin:pin) }.buttonStyle(.borderedProminent).tint(pink).disabled(host.isEmpty || pin.count != 8)
                    }
                    Toggle("Automatic reconnection",isOn:$model.autoReconnect).onChange(of:model.autoReconnect) { _ in model.controlsChanged() }
                    if !model.error.isEmpty { Text(model.error).foregroundStyle(.orange).font(.caption) }
                    Text("USB unavailable · Independent playback").font(.caption).foregroundStyle(.secondary)
                }.padding(24)
            }.background(Color(red:0.031,green:0.031,blue:0.043)).tint(pink)
        }
    }
}
