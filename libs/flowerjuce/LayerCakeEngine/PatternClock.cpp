#include "PatternClock.h"
#include "LayerCakeEngine.h"

namespace
{
constexpr int kMaxPatternLength = 128;
}

PatternClock::PatternClock(LayerCakeEngine& engine)
    : m_engine(engine)
{
    clear_pattern();
}

void PatternClock::prepare(double sample_rate)
{
    m_metro.prepare(sample_rate);
}

void PatternClock::set_enabled(bool enabled)
{
    m_enabled.store(enabled);
}

void PatternClock::set_pattern_length(int length)
{
    m_pattern_length = juce::jlimit(1, kMaxPatternLength, length);
    if (m_current_step >= m_pattern_length)
        m_current_step = 0;
}

void PatternClock::set_skip_probability(float probability)
{
    m_skip_probability = juce::jlimit(0.0f, 1.0f, probability);
}

void PatternClock::set_period_ms(float period_ms)
{
    m_metro.set_period_ms(period_ms);
}

void PatternClock::set_bpm(float bpm)
{
    m_metro.set_period_ms(Metro::bpm_to_period_ms(bpm));
}

float PatternClock::get_bpm() const
{
    return Metro::period_ms_to_bpm(m_metro.get_period_ms());
}

void PatternClock::reset()
{
    m_current_step = 0;
    m_recorded_steps = 0;
    m_metro.reset();
}

void PatternClock::process_sample()
{
    m_metro.process_sample();
    if (m_metro.consume_tick()){
        if (!m_enabled.load()){
            // DBG("PatternClock: clock disabled;");
        } else {
            advance_step();
        }
    }
}

void PatternClock::capture_step_grain(const GrainState& state)
{
    if (!m_enabled.load() || m_mode != Mode::Recording)
        return;

    m_pending_record_state = state;
    m_pending_record_state.should_trigger = true;
}

void PatternClock::advance_step()
{
    switch (m_mode)
    {
        case Mode::Recording:
            handle_record_step();
            break;
        case Mode::Playback:
            handle_playback_step();
            break;
        case Mode::Idle:
            DBG("PatternClock: handling idle step;");
            handle_idle_step();
        default:
            break;
    }

    m_current_step = (m_current_step + 1) % m_pattern_length;
}

void PatternClock::handle_record_step()
{
    m_pattern_steps[m_current_step] = m_grain_builder();
    // m_pattern_steps[m_current_step].skip_randomization = true;
    ++m_recorded_steps;

    const auto recorded_state = m_pattern_steps[m_current_step];
    trigger_step_state(recorded_state);

    if (m_recorded_steps >= m_pattern_length)
    {
        m_mode = Mode::Playback;
        m_recorded_steps = 0;
        DBG("PatternClock switching to playback");
    }
}

void PatternClock::handle_playback_step()
{
    const auto& step_state = m_pattern_steps[m_current_step];
    trigger_step_state(step_state);
}

void PatternClock::handle_idle_step()
{
    // TODO: here, we need to sample a NEW auto fire state from the engine.
    m_auto_fire_state = m_grain_builder();
    trigger_step_state(m_auto_fire_state);
}

void PatternClock::trigger_step_state(const GrainState& state)
{
    if (!m_enabled.load()){
        DBG("PatternClock: not enabled. will not trigger step");
        return;
    }

    if (!state.should_trigger){
        DBG("PatternClock: rskip skipping skep.");
        return;
    }

    GrainState local_state = state;

    if (!local_state.should_trigger && m_auto_fire_enabled.load())
    {
        const juce::SpinLock::ScopedLockType lock(m_auto_state_lock);
        local_state = m_auto_fire_state;
        local_state.should_trigger = true;
    }

    if (!local_state.should_trigger){
        DBG("PatternClock: local state suggested we shouldn't trigger.");
        return;
    }

    m_engine.trigger_grain(local_state, true);
}

void PatternClock::clear_pattern()
{
    for (auto& step : m_pattern_steps)
    {
        step = GrainState{};
        step.should_trigger = false;
    }
}

bool PatternClock::should_skip_step()
{
    if (m_skip_probability <= 0.0f)
        return false;
    return m_random.nextFloat() < m_skip_probability;
}

void PatternClock::set_mode(const PatternClock::Mode mode) {
    m_mode = mode;
    if (m_mode == Mode::Recording)
    {
        m_current_step = 0;
        m_recorded_steps = 0;
        clear_pattern();
        m_metro.reset();
        DBG("PatternClock armed: recording " + juce::String(m_pattern_length) + " steps");
    }
    else
    {
        m_current_step = 0;
        m_recorded_steps = 0;
        DBG("PatternClock disarmed");
    }
}

void PatternClock::set_auto_fire_enabled(bool enabled)
{
    m_auto_fire_enabled.store(enabled);
}

void PatternClock::set_auto_fire_state(const GrainState& state)
{
    const juce::SpinLock::ScopedLockType lock(m_auto_state_lock);
    m_auto_fire_state = state;
}

void PatternClock::get_snapshot(PatternSnapshot& snapshot) const
{
    snapshot.pattern_length = m_pattern_length;
    snapshot.skip_probability = m_skip_probability;
    snapshot.period_ms = m_metro.get_period_ms();
    snapshot.enabled = m_enabled.load();
    snapshot.steps = m_pattern_steps;
}

void PatternClock::apply_snapshot(const PatternSnapshot& snapshot)
{
    m_pattern_length = juce::jlimit(1, kMaxPatternLength, snapshot.pattern_length);
    m_skip_probability = juce::jlimit(0.0f, 1.0f, snapshot.skip_probability);
    m_metro.set_period_ms(snapshot.period_ms);
    m_pattern_steps = snapshot.steps;
    m_enabled.store(snapshot.enabled);
    m_mode = snapshot.enabled ? Mode::Playback : Mode::Idle;
    m_current_step = 0;
    m_recorded_steps = 0;
    m_metro.reset();
}


