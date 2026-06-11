#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "VirtualFretAssets.h"

namespace virtualfret
{

juce::AudioProcessor::BusesProperties VirtualFretProcessor::makeBusesProperties()
{
    // Always expose a silent stereo output. The VST3 is registered as an
    // instrument (Live refuses audio-effect VST3s without audio inputs and
    // won't load instruments without outputs), and a fixed layout keeps the
    // plugin identical across hosts and validators.
    return BusesProperties().withOutput ("out", juce::AudioChannelSet::stereo());
}

VirtualFretProcessor::VirtualFretProcessor()
    : AudioProcessor (makeBusesProperties())
{
    for (auto& s : soundingNote) s.store (-1, std::memory_order_relaxed);
    for (auto& a : inputActive)  a.store (false, std::memory_order_relaxed);

    tunings.loadBuiltins (VirtualFretAssets::tunings_json, VirtualFretAssets::tunings_jsonSize);
    tunings.loadUserTunings (TuningLibrary::defaultUserFile());
    chordShapes.loadBuiltins (VirtualFretAssets::chordshapes_json, VirtualFretAssets::chordshapes_jsonSize);
    chordShapes.loadUserChords (ChordShapeLibrary::defaultUserChordFile());

    // Hosts dislike parameterless plugins; a bypass flag is the standard filler.
    bypassParam = new juce::AudioParameterBool ({ "bypass", 1 }, "Bypass", false);
    addParameter (bypassParam);
}

VirtualFretProcessor::~VirtualFretProcessor() = default;

void VirtualFretProcessor::prepareToPlay (double sampleRate, int)
{
    // A restart invalidates anything we believed was sounding.
    for (auto& v : voices)
        v = {};
    scheduler.prepare (sampleRate);
    clearHighlights();
}

void VirtualFretProcessor::releaseResources()
{
    // processBlock may never run again, so the best we can do for any
    // still-sounding note is forget it; the host resets downstream
    // instruments on transport stop anyway.
    for (auto& v : voices)
        v = {};
    scheduler.clear();
    clearHighlights();
}

void VirtualFretProcessor::clearHighlights()
{
    for (auto& s : soundingNote) s.store (-1, std::memory_order_relaxed);
    for (auto& a : inputActive)  a.store (false, std::memory_order_relaxed);
}

void VirtualFretProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    audio.clear();
    processMidi (midi, audio.getNumSamples());
}

void VirtualFretProcessor::processBlock (juce::AudioBuffer<double>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    audio.clear();
    processMidi (midi, audio.getNumSamples());
}

void VirtualFretProcessor::executeCommand (const NoteCommand& cmd, int sampleOffset, juce::MidiBuffer& midi)
{
    const int stringIndex = juce::jmin ((int) cmd.string, kMaxStrings - 1);
    auto& voice = voices[stringIndex];

    // Mono per string: anything the string still sounds goes off first.
    // The off uses the ledger's note/channel — i.e. whatever was actually
    // started — so tuning or channel changes can never strand a note.
    if (voice.note >= 0)
    {
        midi.addEvent (juce::MidiMessage::noteOff (voice.channel, voice.note), sampleOffset);
        voice = {};
        soundingNote[(size_t) stringIndex].store (-1, std::memory_order_relaxed);
    }

    if (cmd.type == NoteCommand::noteOn)
    {
        const int note = cmd.note & 0x7f;
        const int channel = juce::jlimit (1, 16, (int) cmd.channel);
        const auto velocity = (juce::uint8) juce::jlimit (1, 127, (int) cmd.velocity);

        midi.addEvent (juce::MidiMessage::noteOn (channel, note, velocity), sampleOffset);
        voice.note = note;
        voice.channel = channel;
        soundingNote[(size_t) stringIndex].store (note, std::memory_order_relaxed);
    }
}

void VirtualFretProcessor::processMidi (juce::MidiBuffer& midi, int numSamples)
{
    // 1. Track the incoming stream for the all-positions highlight. The
    //    events themselves stay in the buffer untouched (full thru).
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            inputActive[(size_t) msg.getNoteNumber()].store (true, std::memory_order_relaxed);
        else if (msg.isNoteOff())
            inputActive[(size_t) msg.getNoteNumber()].store (false, std::memory_order_relaxed);
    }

    // 2. Panic: release everything we started and drop whatever is still
    //    queued. The FIFO is drained (not reset) because reset() must not
    //    race a concurrent GUI-thread write.
    if (allOffRequested.exchange (false, std::memory_order_acq_rel))
    {
        for (;;)
        {
            const auto scope = commandFifo.read (1);
            if (scope.blockSize1 + scope.blockSize2 == 0)
                break;  // discard stale commands
        }
        scheduler.clear();

        for (int s = 0; s < kMaxStrings; ++s)
        {
            if (voices[s].note >= 0)
            {
                midi.addEvent (juce::MidiMessage::noteOff (voices[s].channel, voices[s].note), 0);
                voices[s] = {};
            }
            soundingNote[(size_t) s].store (-1, std::memory_order_relaxed);
        }
    }

    // 3. Drain the FIFO: immediate commands execute at the block top,
    //    strum commands go through the sample-offset scheduler.
    for (;;)
    {
        const auto scope = commandFifo.read (1);
        if (scope.blockSize1 + scope.blockSize2 == 0)
            break;

        const auto& cmd = commands[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1
                                                                  : scope.startIndex2)];
        if (cmd.relMs >= 0.0f)
        {
            if (! scheduler.push (cmd))
                executeCommand (cmd, 0, midi);   // scheduler full: degrade to immediate
        }
        else
        {
            executeCommand (cmd, 0, midi);
        }
    }

    // 4. Emit everything due inside this block at its sample offset.
    scheduler.processBlock (numSamples, [this, &midi] (const NoteCommand& cmd, int offset)
    {
        executeCommand (cmd, offset, midi);
    });
}

void VirtualFretProcessor::enqueue (const NoteCommand& cmd)
{
    const auto scope = commandFifo.write (1);
    if (scope.blockSize1 > 0)
        commands[(size_t) scope.startIndex1] = cmd;
    else if (scope.blockSize2 > 0)
        commands[(size_t) scope.startIndex2] = cmd;
}

void VirtualFretProcessor::playNote (int stringIndex, int note)
{
    int velocity = 100;
    {
        const juce::ScopedLock sl (stateLock);
        velocity = state.velocity;
    }
    playNote (stringIndex, note, velocity);
}

void VirtualFretProcessor::playNote (int stringIndex, int note, int velocity)
{
    if (! juce::isPositiveAndBelow (note, 128)
        || ! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    NoteCommand cmd;
    cmd.type = NoteCommand::noteOn;
    cmd.string = (juce::uint8) stringIndex;
    cmd.note = (juce::uint8) note;
    cmd.velocity = (juce::uint8) juce::jlimit (1, 127, velocity);
    cmd.relMs = -1.0f;
    {
        const juce::ScopedLock sl (stateLock);
        cmd.channel = (juce::uint8) state.channelForString (stringIndex);
    }
    enqueue (cmd);
}

void VirtualFretProcessor::releaseString (int stringIndex)
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    NoteCommand cmd;
    cmd.type = NoteCommand::stringOff;
    cmd.string = (juce::uint8) stringIndex;
    cmd.relMs = -1.0f;
    enqueue (cmd);
}

void VirtualFretProcessor::strumNote (int stringIndex, int note, int velocity,
                                      float relMs, juce::uint8 strokeId)
{
    if (! juce::isPositiveAndBelow (note, 128)
        || ! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    NoteCommand cmd;
    cmd.type = NoteCommand::noteOn;
    cmd.string = (juce::uint8) stringIndex;
    cmd.note = (juce::uint8) note;
    cmd.velocity = (juce::uint8) juce::jlimit (1, 127, velocity);
    cmd.strokeId = strokeId;
    cmd.relMs = juce::jmax (0.0f, relMs);
    {
        const juce::ScopedLock sl (stateLock);
        cmd.channel = (juce::uint8) state.channelForString (stringIndex);
    }
    enqueue (cmd);
}

void VirtualFretProcessor::requestAllNotesOff()
{
    allOffRequested.store (true, std::memory_order_release);
}

void VirtualFretProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::String json;
    {
        const juce::ScopedLock sl (stateLock);
        json = state.toStateJson();
    }
    destData.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void VirtualFretProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto json = juce::String::fromUTF8 (static_cast<const char*> (data), sizeInBytes);
    bool changed = false;
    {
        const juce::ScopedLock sl (stateLock);
        changed = state.restoreFromStateJson (json);
    }
    if (changed)
    {
        requestAllNotesOff();   // the restored world starts silent
        sendChangeMessage();    // async; the editor refreshes on the message thread
    }
}

juce::AudioProcessorEditor* VirtualFretProcessor::createEditor()
{
    return new VirtualFretEditor (*this);
}

} // namespace virtualfret

// JUCE plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new virtualfret::VirtualFretProcessor();
}
