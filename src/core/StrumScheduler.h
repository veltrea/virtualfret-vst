#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>

namespace virtualfret
{

/** One GUI note event heading for the audio thread (POD, FIFO-safe).
    Velocity/channel are baked in at enqueue time so the audio thread
    never reads the settings model. */
struct NoteCommand
{
    enum Type : juce::uint8
    {
        noteOn = 0,     // sound `note` on `string` (releasing that string's old note first)
        stringOff = 1   // release whatever `string` is currently sounding
    };

    juce::uint8 type = noteOn;
    juce::uint8 string = 0;        // internal index, 0 = lowest string
    juce::uint8 note = 0;
    juce::uint8 velocity = 100;
    juce::uint8 channel = 1;       // 1-based
    juce::uint8 strokeId = 0;      // strum stroke group (wraps)
    float relMs = -1.0f;           // >= 0: time within the stroke; < 0: immediate
};

/**
    Audio-thread-only mapper from stroke-relative event times to
    per-block sample offsets (SPEC §5: per-string strum spacing must
    survive large audio buffers instead of collapsing onto one block
    boundary).

    The first event of a stroke (a new strokeId) anchors that stroke to
    the start of the current block; every later event of the same stroke
    is placed `relMs - firstRelMs` after the anchor and carried over to
    following blocks when it lands beyond the current one.

    Call order per block (all on the audio thread):
    push() for every drained FIFO command, then processBlock().
*/
class StrumScheduler
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        clear();
    }

    /** Drops everything pending (panic). */
    void clear()
    {
        count = 0;
        haveBase = false;
    }

    int pendingCount() const noexcept { return count; }

    /** Queue a command. relMs < 0 schedules at the top of the current
        block. Returns false (command dropped) only when full. */
    bool push (const NoteCommand& cmd)
    {
        if (count >= capacity)
            return false;

        juce::int64 due = blockStart;
        if (cmd.relMs >= 0.0f)
        {
            if (! haveBase || cmd.strokeId != baseStrokeId)
            {
                haveBase = true;
                baseStrokeId = cmd.strokeId;
                baseRelMs = cmd.relMs;
                baseSample = blockStart;
            }
            due = baseSample
                  + (juce::int64) ((double) (cmd.relMs - baseRelMs) * 0.001 * sampleRate);
            if (due < blockStart)       // late arrival: play at the block top
                due = blockStart;
        }

        slots[count++] = { cmd, due, serial++ };
        return true;
    }

    /** Emits fn(cmd, sampleOffset) for everything due inside this block,
        ordered by due time (FIFO order within a tie), and keeps the rest. */
    template <typename Fn>
    void processBlock (int numSamples, Fn&& fn)
    {
        const juce::int64 blockEnd = blockStart + numSamples;

        int dueIndex[capacity];
        int numDue = 0;
        for (int i = 0; i < count; ++i)
            if (slots[i].due < blockEnd)
                dueIndex[numDue++] = i;

        // std::sort is allocation-free; the serial number keeps ties stable.
        std::sort (dueIndex, dueIndex + numDue, [this] (int a, int b)
        {
            if (slots[a].due != slots[b].due)
                return slots[a].due < slots[b].due;
            return slots[a].serial < slots[b].serial;
        });

        for (int k = 0; k < numDue; ++k)
        {
            const auto& slot = slots[dueIndex[k]];
            fn (slot.cmd, (int) (slot.due - blockStart));
        }

        int write = 0;
        for (int i = 0; i < count; ++i)
            if (slots[i].due >= blockEnd)
                slots[write++] = slots[i];
        count = write;

        blockStart = blockEnd;
    }

private:
    struct Slot
    {
        NoteCommand cmd;
        juce::int64 due = 0;
        juce::uint32 serial = 0;
    };

    static constexpr int capacity = 128;
    Slot slots[capacity];
    int count = 0;
    juce::uint32 serial = 0;

    double sampleRate = 48000.0;
    juce::int64 blockStart = 0;

    bool haveBase = false;
    juce::uint8 baseStrokeId = 0;
    float baseRelMs = 0.0f;
    juce::int64 baseSample = 0;
};

} // namespace virtualfret
