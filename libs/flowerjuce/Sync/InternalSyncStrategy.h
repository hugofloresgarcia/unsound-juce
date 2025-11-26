#pragma once

#include "SyncInterface.h"
#include <atomic>
#include <juce_core/juce_core.h>

namespace flower
{

class InternalSyncStrategy : public SyncInterface
{
public:
    InternalSyncStrategy();
    ~InternalSyncStrategy() override = default;
    
    void prepare(double sample_rate, int block_size) override;
    double get_current_beat() override;
    double get_tempo() const override;
    void set_tempo(double bpm) override;
    bool is_playing() const override;
    void set_playing(bool playing) override;
    void request_reset() override;
    void process(int num_samples, double sample_rate) override;
    double get_phase(double quantum) const override;

private:
    std::atomic<double> m_bpm{120.0};
    std::atomic<double> m_current_beat{0.0};
    std::atomic<bool> m_playing{false};
};

} // namespace flower
