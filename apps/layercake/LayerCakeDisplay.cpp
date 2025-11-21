#include "LayerCakeDisplay.h"
#include <algorithm>

namespace
{
constexpr int kWaveformPoints = 512;
constexpr int kDisplaySize = 500;
constexpr float kHighlightAlpha = 0.35f;
constexpr float kPlayheadSway = 6.0f;
constexpr float kBaseSquiggleCycles = 2.0f;
constexpr float kLaneSpacing = 10.0f;
}

LayerCakeDisplay::LayerCakeDisplay(LayerCakeEngine& engine)
    : m_engine(engine)
{
    m_palette = {
        juce::Colour(0xfff7e4c6),
        juce::Colour(0xfff27d72),
        juce::Colour(0xffa5d9ff),
        juce::Colour(0xffd7bce8),
        juce::Colour(0xff8dd18c),
        juce::Colour(0xffe9f19c),
        juce::Colour(0xfffcb879),
        juce::Colour(0xffe06666)
    };

    m_invaders.reserve(6);
    for (int i = 0; i < 6; ++i)
    {
        Invader inv;
        inv.position = { juce::Random::getSystemRandom().nextFloat() * kDisplaySize,
                         juce::Random::getSystemRandom().nextFloat() * kDisplaySize };
        inv.velocity = { juce::Random::getSystemRandom().nextFloat() * 0.6f + 0.2f,
                         juce::Random::getSystemRandom().nextFloat() * 0.6f + 0.2f };
        if (juce::Random::getSystemRandom().nextBool())
            inv.velocity.x *= -1.0f;
        if (juce::Random::getSystemRandom().nextBool())
            inv.velocity.y *= -1.0f;
        m_invaders.push_back(inv);
    }

    refresh_waveforms();
    refresh_grains();
    startTimerHz(30);
}

void LayerCakeDisplay::paint(juce::Graphics& g)
{
    const int num_layers = static_cast<int>(LayerCakeEngine::kNumLayers);
    const float frame_corner_radius = 30.0f;
    const float screen_corner_radius = 18.0f;
    const float lane_corner_radius = 10.0f;
    const float lane_spacing = kLaneSpacing;
    const float lane_inner_padding = 8.0f;
    const float indicator_column_width = 34.0f;
    const float indicator_corner_radius = 5.0f;
    const float indicator_vertical_padding = 6.0f;
    const float separator_thickness = 1.0f;

    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff050505));

    auto frame = bounds.withSizeKeepingCentre(static_cast<float>(kDisplaySize + 80),
                                              static_cast<float>(kDisplaySize + 90));
    g.setColour(juce::Colour(0xff202020));
    g.fillRoundedRectangle(frame, frame_corner_radius);
    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawRoundedRectangle(frame, frame_corner_radius, 4.0f);

    auto display = get_display_area();
    g.setColour(juce::Colour(0xff050505));
    g.fillRoundedRectangle(display, screen_corner_radius);

    const float position_indicator = m_position_indicator.load();
    const bool show_position = position_indicator >= 0.0f && position_indicator <= 1.0f;

    const float total_lane_spacing = lane_spacing * (num_layers - 1);
    const float lane_height = (display.getHeight() - total_lane_spacing) / static_cast<float>(num_layers);

    std::array<juce::Rectangle<float>, LayerCakeEngine::kNumLayers> waveform_bounds{};

    for (int layer = 0; layer < num_layers; ++layer)
    {
        juce::Rectangle<float> lane(display.getX(),
                                    display.getY() + layer * (lane_height + lane_spacing),
                                    display.getWidth(),
                                    lane_height);

        const bool is_record_layer = layer == m_record_layer;
        const float layer_mix = num_layers > 1 ? static_cast<float>(layer) / static_cast<float>(num_layers - 1)
                                               : 0.0f;
        juce::Colour lane_colour = juce::Colours::black
                                       .interpolatedWith(juce::Colour(0xffbbeeff), 0.35f + 0.35f * layer_mix);
        if (is_record_layer)
            lane_colour = lane_colour.brighter(0.2f);
        g.setColour(lane_colour);
        g.fillRoundedRectangle(lane, lane_corner_radius);

        juce::Rectangle<float> inner_lane = lane.reduced(lane_inner_padding, lane_inner_padding);
        juce::Rectangle<float> indicator_area = inner_lane.removeFromLeft(indicator_column_width);
        waveform_bounds[static_cast<size_t>(layer)] = inner_lane;

        auto indicator_rect = indicator_area.reduced(4.0f, indicator_vertical_padding);
        g.setColour(is_record_layer ? juce::Colour(0xffd83c3c) : juce::Colour(0xff2b2b2b));
        g.fillRoundedRectangle(indicator_rect, indicator_corner_radius);
        g.setColour(juce::Colour(0xfff6f1d3));
        g.drawRoundedRectangle(indicator_rect, indicator_corner_radius, 1.5f);
        juce::String indicator_text = is_record_layer ? "r" : juce::String(layer + 1);
        g.drawText(indicator_text, indicator_rect, juce::Justification::centred);
    }

    g.setColour(juce::Colour(0x22101010));
    for (int layer = 1; layer < num_layers; ++layer)
    {
        const float y = display.getY() + layer * (lane_height + lane_spacing) - lane_spacing * 0.5f;
        g.drawLine(display.getX(), y, display.getRight(), y, separator_thickness);
    }

    // Waveforms per lane
    for (size_t layer = 0; layer < m_waveform_cache.size(); ++layer)
    {
        const auto& samples = m_waveform_cache[layer];
        if (samples.empty())
            continue;

        const auto& area = waveform_bounds[layer];
        if (area.isEmpty())
            continue;

        juce::Colour wave_colour = juce::Colour(0xfff1e8c8).withAlpha(0.25f);
        g.setColour(wave_colour);

        juce::Path path;
        const float dx = area.getWidth() / juce::jmax(1, static_cast<int>(samples.size() - 1));
        const float center_y = area.getCentreY();
        const float height_scale = area.getHeight() * 0.45f;
        path.startNewSubPath(area.getX(), center_y - samples.front() * height_scale);
        for (size_t i = 1; i < samples.size(); ++i)
        {
            const float x = area.getX() + dx * static_cast<float>(i);
            const float y = center_y - samples[i] * height_scale;
            path.lineTo(x, y);
        }
        g.strokePath(path, juce::PathStrokeType(1.4f));
    }

    // Grain highlights
    for (const auto& grain : m_grain_states)
    {
        if (!grain.is_active || grain.recorded_length_samples <= 0.0f)
            continue;
        if (!juce::isPositiveAndBelow(grain.layer, static_cast<int>(waveform_bounds.size())))
            continue;

        const auto& lane_area = waveform_bounds[static_cast<size_t>(grain.layer)];
        if (lane_area.isEmpty())
            continue;

        const float start_norm = juce::jlimit(0.0f, 1.0f, grain.loop_start_samples / grain.recorded_length_samples);
        const float end_norm = juce::jlimit(0.0f, 1.0f, grain.loop_end_samples / grain.recorded_length_samples);
        const float width_norm = juce::jmax(0.01f, end_norm - start_norm);
        juce::Rectangle<float> highlight{
            lane_area.getX() + lane_area.getWidth() * start_norm,
            lane_area.getY() + lane_area.getHeight() * 0.1f,
            lane_area.getWidth() * width_norm,
            lane_area.getHeight() * 0.8f };

        auto colour = colour_for_voice(grain.voice_index);
        g.setColour(colour.withAlpha(kHighlightAlpha));
        g.fillRoundedRectangle(highlight, 6.0f);

        // Squiggle playhead
        const float playhead_x = highlight.getX() + highlight.getWidth() * grain.normalized_position;
        const float squiggle_height = highlight.getHeight();
        const float start_y = highlight.getY();
        const float cycles = kBaseSquiggleCycles * juce::jlimit(0.5f, 3.5f, std::pow(2.0f, grain.rate_semitones / 12.0f));

        juce::Path squiggle;
        const int segments = 24;
        for (int i = 0; i <= segments; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(segments);
            const float y = start_y + t * squiggle_height;
            const float sway = std::sin(juce::MathConstants<float>::twoPi * (t * cycles)) * kPlayheadSway;
            const float x = juce::jlimit(lane_area.getX(),
                                         lane_area.getRight(),
                                         playhead_x + sway);
            if (i == 0)
                squiggle.startNewSubPath(x, y);
            else
                squiggle.lineTo(x, y);
        }
        g.setColour(colour.withAlpha(0.95f));
        g.strokePath(squiggle, juce::PathStrokeType(2.2f));
    }

    if (show_position)
    {
        const float x = display.getX() + display.getWidth() * position_indicator;
        g.setColour(juce::Colour(0xfff7e4c6).withAlpha(0.8f));
        g.drawLine(x, display.getY(), x, display.getBottom(), 2.0f);
        g.setColour(juce::Colour(0xffe06666));
        g.fillEllipse(x - 4.0f, display.getY() - 6.0f, 8.0f, 8.0f);
    }

    // Invaders / decorations
    g.setColour(juce::Colour(0xff4cffd7));
    for (const auto& inv : m_invaders)
    {
        auto invRect = juce::Rectangle<float>(8.0f, 8.0f)
                           .withCentre({ display.getX() + inv.position.x, display.getY() + inv.position.y });
        g.fillRect(invRect);
    }
}

void LayerCakeDisplay::resized()
{
    refresh_waveforms();
}

void LayerCakeDisplay::timerCallback()
{
    if (++m_waveform_counter % 4 == 0)
        refresh_waveforms();
    refresh_grains();

    auto display = get_display_area();
    update_invaders(display.getWidth(), display.getHeight());
    repaint();
}

void LayerCakeDisplay::refresh_waveforms()
{
    auto& layers = m_engine.get_layers();
    for (size_t i = 0; i < layers.size(); ++i)
    {
        std::vector<float> points;
        points.reserve(kWaveformPoints);

        auto& loop = layers[i];
        const juce::ScopedLock sl(loop.m_lock);
        const auto& buffer = loop.get_buffer();
        const size_t recorded = loop.m_recorded_length.load();
        if (buffer.empty() || recorded == 0)
        {
            m_waveform_cache[i].clear();
            continue;
        }

        const size_t length = juce::jmin(recorded, buffer.size());
        const double stride = static_cast<double>(length) / static_cast<double>(kWaveformPoints);
        for (int p = 0; p < kWaveformPoints; ++p)
        {
            const size_t index = static_cast<size_t>(p * stride);
            points.push_back(buffer[index]);
        }
        m_waveform_cache[i] = std::move(points);
    }
}

void LayerCakeDisplay::refresh_grains()
{
    m_engine.get_active_grains(m_grain_states);
    for (const auto& state : m_grain_states)
        colour_for_voice(state.voice_index);
}

void LayerCakeDisplay::set_position_indicator(float normalized_position)
{
    if (normalized_position < 0.0f)
    {
        m_position_indicator.store(-1.0f);
    }
    else
    {
        const float clamped = juce::jlimit(0.0f, 1.0f, normalized_position);
        m_position_indicator.store(clamped);
    }
    repaint();
}

juce::Rectangle<float> LayerCakeDisplay::get_display_area() const
{
    auto bounds = getLocalBounds().toFloat();
    auto display = bounds.withSizeKeepingCentre(static_cast<float>(kDisplaySize),
                                                static_cast<float>(kDisplaySize));
    return display.reduced(20.0f);
}

juce::Colour LayerCakeDisplay::colour_for_voice(size_t voice_index)
{
    const auto it = m_voice_colours.find(voice_index);
    if (it != m_voice_colours.end())
        return it->second;

    juce::Colour colour = m_palette[voice_index % m_palette.size()];
    m_voice_colours[voice_index] = colour;
    return colour;
}

void LayerCakeDisplay::update_invaders(float width, float height)
{
    for (auto& inv : m_invaders)
    {
        inv.position += inv.velocity;
        if (inv.position.x < 0.0f || inv.position.x > width)
            inv.velocity.x *= -1.0f;
        if (inv.position.y < 0.0f || inv.position.y > height)
            inv.velocity.y *= -1.0f;
        inv.position.x = juce::jlimit(0.0f, width, inv.position.x);
        inv.position.y = juce::jlimit(0.0f, height, inv.position.y);
    }
}

bool LayerCakeDisplay::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        juce::File file(path);
        if (file.existsAsFile() && has_supported_audio_extension(file))
            return true;
    }
    return false;
}

void LayerCakeDisplay::filesDropped(const juce::StringArray& files, int x, int y)
{
    if (files.isEmpty())
    {
        DBG("LayerCakeDisplay::filesDropped early return (no files)");
        return;
    }

    juce::File drop_file;
    for (const auto& path : files)
    {
        juce::File candidate(path);
        if (candidate.existsAsFile() && has_supported_audio_extension(candidate))
        {
            drop_file = candidate;
            break;
        }
    }

    if (!drop_file.existsAsFile())
    {
        DBG("LayerCakeDisplay::filesDropped early return (no supported audio files)");
        return;
    }

    const int layer_index = layer_at_point({ x, y });
    if (!juce::isPositiveAndBelow(layer_index, static_cast<int>(LayerCakeEngine::kNumLayers)))
    {
        DBG("LayerCakeDisplay::filesDropped early return (point outside lanes)");
        return;
    }

    if (!m_engine.load_layer_from_file(layer_index, drop_file))
    {
        DBG("LayerCakeDisplay::filesDropped failed to load file=" + drop_file.getFileName());
        return;
    }

    DBG("LayerCakeDisplay::filesDropped loaded file=" + drop_file.getFileName()
        + " layer=" + juce::String(layer_index + 1));
    refresh_waveforms();
    repaint();
}

juce::Rectangle<float> LayerCakeDisplay::lane_bounds_for_index(int layer_index) const
{
    if (!juce::isPositiveAndBelow(layer_index, static_cast<int>(LayerCakeEngine::kNumLayers)))
        return {};

    auto display = get_display_area();
    const float total_spacing = kLaneSpacing * (LayerCakeEngine::kNumLayers - 1);
    const float lane_height = (display.getHeight() - total_spacing) / static_cast<float>(LayerCakeEngine::kNumLayers);

    return {
        display.getX(),
        display.getY() + static_cast<float>(layer_index) * (lane_height + kLaneSpacing),
        display.getWidth(),
        lane_height
    };
}

int LayerCakeDisplay::layer_at_point(juce::Point<int> point) const
{
    const auto target = point.toFloat();
    for (int layer = 0; layer < static_cast<int>(LayerCakeEngine::kNumLayers); ++layer)
    {
        if (lane_bounds_for_index(layer).contains(target))
            return layer;
    }
    return -1;
}

bool LayerCakeDisplay::has_supported_audio_extension(const juce::File& file) const
{
    static const std::array<juce::String, 6> kExtensions{
        ".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg"
    };
    const auto ext = file.getFileExtension().toLowerCase();
    return std::any_of(kExtensions.begin(), kExtensions.end(),
                       [&ext](const juce::String& allowed) { return ext == allowed; });
}


