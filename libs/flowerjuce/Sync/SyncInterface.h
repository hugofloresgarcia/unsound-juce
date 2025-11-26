#pragma once

namespace flower
{

class SyncInterface
{
public:
    virtual ~SyncInterface() = default;

    // Prepare the sync strategy.
    virtual void prepare(double sample_rate, int block_size) = 0;

    // Get the current beat position.
    // For Link, this is mapped from the current system time.
    // For Internal, this is incremented by the engine.
    virtual double get_current_beat() = 0;

    // Get the current tempo in BPM.
    virtual double get_tempo() const = 0;

    // Set the tempo (if supported).
    virtual void set_tempo(double bpm) = 0;

    // Check if transport is playing.
    virtual bool is_playing() const = 0;

    // Start/Stop transport.
    virtual void set_playing(bool playing) = 0;

    // Reset transport (e.g. to beat 0).
    virtual void request_reset() = 0;

    // Process any recurring tasks (e.g. updating beat time based on sample count).
    // This should be called once per audio block.
    // @param num_samples: number of samples in this block
    // @param sample_rate: current sample rate
    virtual void process(int num_samples, double sample_rate) = 0;
    
    // Get the current phase for a given quantum (e.g. 4 beats).
    virtual double get_phase(double quantum) const = 0;

    // Link specific (optional support)
    virtual void enable_link(bool enabled) {}
    virtual bool is_link_enabled() const { return false; }
    virtual int get_num_peers() const { return 0; }
};

} // namespace flower
