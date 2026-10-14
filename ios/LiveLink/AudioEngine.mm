#include "AudioEngine.h"
#include "../../shared/audio/Receiver.h"
#include <AudioToolbox/AudioToolbox.h>
#include <cstring>
struct Engine {
    ll::Receiver receiver;
    AudioUnit unit = nullptr;
    double deviceRate = 48000;
    static OSStatus callback(void *context, AudioUnitRenderActionFlags *, const AudioTimeStamp *,
                             UInt32, UInt32 frames, AudioBufferList *output) {
        auto &e = *static_cast<Engine *>(context);
        if (output->mNumberBuffers == 2 && output->mBuffers[0].mData && output->mBuffers[1].mData)
            e.receiver.render(static_cast<float *>(output->mBuffers[0].mData),
                              static_cast<float *>(output->mBuffers[1].mData), frames,
                              e.deviceRate);
        else
            for (UInt32 b = 0; b < output->mNumberBuffers; ++b)
                if (output->mBuffers[b].mData)
                    memset(output->mBuffers[b].mData, 0, output->mBuffers[b].mDataByteSize);
        return noErr;
    }
    void stop() {
        if (unit) {
            AudioOutputUnitStop(unit);
            AudioUnitUninitialize(unit);
            AudioComponentInstanceDispose(unit);
            unit = nullptr;
        }
        receiver.discardBufferedPlayback();
    }
    ~Engine() { stop(); }
};
extern "C" {
LLReceiverRef ll_create() { return new Engine; }
void ll_destroy(LLReceiverRef ref) { delete static_cast<Engine *>(ref); }
void ll_stop(LLReceiverRef ref) { static_cast<Engine *>(ref)->stop(); }
int ll_start(LLReceiverRef ref, double rate) {
    auto &e = *static_cast<Engine *>(ref);
    e.stop();
    e.deviceRate = rate;
    AudioComponentDescription d{};
    d.componentType = kAudioUnitType_Output;
    d.componentSubType = kAudioUnitSubType_RemoteIO;
    d.componentManufacturer = kAudioUnitManufacturer_Apple;
    auto component = AudioComponentFindNext(nullptr, &d);
    if (!component)
        return -1;
    OSStatus status = AudioComponentInstanceNew(component, &e.unit);
    if (status)
        return status;
    AudioStreamBasicDescription format{};
    format.mSampleRate = rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags =
        kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
    format.mBytesPerPacket = 4;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = 4;
    format.mChannelsPerFrame = 2;
    format.mBitsPerChannel = 32;
    AURenderCallbackStruct callback{Engine::callback, &e};
    status = AudioUnitSetProperty(e.unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                                  &format, sizeof(format));
    if (!status)
        status = AudioUnitSetProperty(e.unit, kAudioUnitProperty_SetRenderCallback,
                                      kAudioUnitScope_Input, 0, &callback, sizeof(callback));
    if (!status)
        status = AudioUnitInitialize(e.unit);
    if (!status)
        status = AudioOutputUnitStart(e.unit);
    if (status)
        e.stop();
    return status;
}
void ll_authorize(LLReceiverRef ref, uint64_t token) {
    static_cast<Engine *>(ref)->receiver.authorize(token);
}
void ll_mode(LLReceiverRef ref, int mode) { static_cast<Engine *>(ref)->receiver.mode(mode); }
int ll_packet(LLReceiverRef ref, const uint8_t *bytes, int n) {
    return static_cast<Engine *>(ref)->receiver.receive(std::span(bytes, size_t(std::max(0, n))))
               ? 1
               : 0;
}
uint32_t ll_loss(LLReceiverRef ref) { return static_cast<Engine *>(ref)->receiver.loss.load(); }
uint32_t ll_underruns(LLReceiverRef ref) {
    return static_cast<Engine *>(ref)->receiver.underruns.load();
}
uint32_t ll_occupancy(LLReceiverRef ref) {
    return static_cast<Engine *>(ref)->receiver.occupancy.load();
}
uint32_t ll_rate(LLReceiverRef ref) { return static_cast<Engine *>(ref)->receiver.sampleRate(); }
float ll_peak(LLReceiverRef ref) { return static_cast<Engine *>(ref)->receiver.peak.load(); }
}
