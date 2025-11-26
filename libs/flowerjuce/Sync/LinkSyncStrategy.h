#pragma once

#include "SyncInterface.h"
#include <ableton/Link.hpp>
#include <ableton/link/HostTimeFilter.hpp>
#include <atomic>
#include <memory>
#include <optional>

namespace flower
{

class LinkSyncStrategy : public SyncInterface
{
public:
    explicit LinkSyncStrategy(double initial_bpm);
    ~LinkSyncStrategy() override;

    void prepare(double sample_rate, int block_size) override;
    double get_current_beat() override;
    double get_tempo() const override;
    void set_tempo(double bpm) override;
    bool is_playing() const override;
    void set_playing(bool playing) override;
    void request_reset() override;
    void process(int num_samples, double sample_rate) override;
    double get_phase(double quantum) const override;
    
    // Link specific
    void enable_link(bool enabled) override;
    bool is_link_enabled() const override;
    int get_num_peers() const override;

private:
    void calculate_output_time(double sample_rate, int buffer_size);
    void commit_timeline_changes();

    std::unique_ptr<ableton::Link> m_link;
    ableton::link::HostTimeFilter<ableton::link::platform::Clock> m_host_time_filter;
    std::unique_ptr<ableton::Link::SessionState> m_session_state;
    
    std::chrono::microseconds m_output_time;
    uint64_t m_total_samples{0};
    
    // Buffered changes
    struct Request
    {
        std::optional<double> bpm;
        std::optional<bool> playing;
        bool reset{false};
    };
    Request m_pending_request;
    
    // Cached values for the current block
    double m_block_start_beat{0.0};
    double m_cached_bpm{120.0};
    bool m_cached_playing{false};
};

} // namespace flower
