#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "core/FretModel.h"
#include "core/TuningLibrary.h"
#include "core/ChordShapeLibrary.h"
#include "core/StrumScheduler.h"

#include <array>
#include <atomic>
#include <limits>

namespace virtualfret
{

/**
    MIDI-thru processor that merges fretboard-generated notes into the
    output stream. All sounding notes go through the per-string ledger,
    which enforces one note per string (the real-guitar constraint) and
    guarantees note-offs on panic, tuning changes and string swaps.

    Threading (NoteNamer pattern):
      - state (tuning/latch/settings) is owned by the message thread;
        the audio thread never reads it. Guarded by stateLock.
      - GUI -> audio: lock-free NoteCommand FIFO; velocity/channel are
        baked into each command at enqueue time.
      - audio -> GUI: atomics polled by the editor's timer.
      - Strum timing: commands carry stroke-relative milliseconds; the
        StrumScheduler turns them into in-block sample offsets and
        carries late events into following blocks (SPEC §5).
*/
class VirtualFretProcessor : public juce::AudioProcessor,
                             public juce::ChangeBroadcaster
{
public:
    VirtualFretProcessor();
    ~VirtualFretProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                  { return true; }

    const juce::String getName() const override      { return JucePlugin_Name; }
    bool acceptsMidi() const override                { return true; }
    bool producesMidi() const override               { return true; }
    bool isMidiEffect() const override               { return true; }

    /** Infinite tail keeps Live's plug-in auto-suspend off. Live otherwise
        stops calling processBlock on a silent instrument track, which would
        strand queued notes (the GUI->FIFO->processBlock path). */
    double getTailLengthSeconds() const override
    {
        return std::numeric_limits<double>::infinity();
    }

    int getNumPrograms() override                    { return 1; }
    int getCurrentProgram() override                 { return 0; }
    void setCurrentProgram (int) override            {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // --- model access (message thread) -----------------------------------
    VirtualFretState& getModel() noexcept            { return state; }
    juce::CriticalSection& getStateLock() noexcept   { return stateLock; }
    TuningLibrary& getTunings() noexcept             { return tunings; }
    ChordShapeLibrary& getChordShapes() noexcept     { return chordShapes; }

    // --- note triggering (called from the GUI thread) ---------------------
    /** Immediate note-on on a string; the string's previous note is
        released first (mono per string). Velocity/channel come from the
        current settings. */
    void playNote (int stringIndex, int note);

    /** Immediate note-on with an explicit velocity (strum velocity). */
    void playNote (int stringIndex, int note, int velocity);

    /** Releases whatever the string is currently sounding. */
    void releaseString (int stringIndex);

    /** Strum hit: scheduled `relMs` into the stroke `strokeId`. */
    void strumNote (int stringIndex, int note, int velocity, float relMs, juce::uint8 strokeId);

    /** Force-releases everything (panic: Esc, mute button, editor close,
        tuning/string-count changes). */
    void requestAllNotesOff();

    // --- highlight state (polled by the GUI) -------------------------------
    /** Note currently sounding per string (internal index), -1 = silent. */
    std::array<std::atomic<int>, kMaxStrings> soundingNote;
    /** Notes held in the incoming MIDI stream. */
    std::array<std::atomic<bool>, 128> inputActive;

private:
    void enqueue (const NoteCommand& cmd);
    void processMidi (juce::MidiBuffer& midi, int numSamples);
    void executeCommand (const NoteCommand& cmd, int sampleOffset, juce::MidiBuffer& midi);
    void clearHighlights();

    VirtualFretState state;
    juce::CriticalSection stateLock;
    TuningLibrary tunings;
    ChordShapeLibrary chordShapes;

    juce::AudioParameterBool* bypassParam = nullptr;

    static constexpr int fifoSize = 512;
    juce::AbstractFifo commandFifo { fifoSize };
    std::array<NoteCommand, fifoSize> commands;
    std::atomic<bool> allOffRequested { false };

    StrumScheduler scheduler;

    // Per-string sounding ledger, audio thread only. Every generated
    // note lives on a string, so 9 slots cover the whole panic sweep.
    struct StringVoice { int note = -1; int channel = 1; };
    StringVoice voices[kMaxStrings];

    static BusesProperties makeBusesProperties();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VirtualFretProcessor)
};

} // namespace virtualfret
