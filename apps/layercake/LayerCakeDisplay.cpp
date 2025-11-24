#include "LayerCakeDisplay.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr int kWaveformPoints = 512;
constexpr float kReferenceDisplaySize = 560.0f;
constexpr float kHighlightAlpha = 0.35f;
constexpr float kPlayheadSway = 6.0f;
constexpr float kBaseSquiggleCycles = 2.0f;
constexpr float kLaneSpacing = 10.0f;
constexpr float kDisplayMarginRatio = 0.06f;
constexpr float kFrameMarginRatio = 0.08f;
const juce::Colour kSoftWhite(0xfff4f4f2);
constexpr double kNoisePhaseDelta = 0.0125;

float layered_noise(float x, float y, float phase, float freqA, float freqB)
{
    const float waveA = std::sin((x * freqA) + (y * freqB) + phase);
    const float waveB = std::sin((x * freqB * 0.6f) - (y * freqA * 0.35f) - phase * 1.3f);
    const float waveC = std::sin((x * freqA * 0.45f) + (y * freqB * 1.1f) + phase * 0.65f);
    return (waveA + waveB + waveC) / 3.0f;
}

float wrap_value(float value, float maxValue)
{
    if (maxValue <= 0.0f)
    {
        DBG("LayerCakeDisplay::wrap_value early return (invalid max)");
        return 0.0f;
    }

    float wrapped = std::fmod(value, maxValue);
    if (wrapped < 0.0f)
        wrapped += maxValue;
    return wrapped;
}
}

LayerCakeDisplay::LayerCakeDisplay(LayerCakeEngine& engine)
    : m_engine(engine)
{
    m_palette = {
        juce::Colour(0xfffd5e53).darker(0.35f), // coral
        juce::Colour(0xff35c0ff).darker(0.3f),  // cyan
        juce::Colour(0xfff2b950).darker(0.35f), // amber
        juce::Colour(0xff7d6bff).darker(0.3f),  // indigo
        juce::Colour(0xff63ff87).darker(0.25f), // mint
        juce::Colour(0xfff45bff).darker(0.3f),  // magenta
        juce::Colour(0xffff8154).darker(0.35f), // tangerine
        juce::Colour(0xff50f2d4).darker(0.3f)   // teal
    };

    m_invaders.reserve(6);
    for (int i = 0; i < 6; ++i)
    {
        Invader inv;
        inv.position = { juce::Random::getSystemRandom().nextFloat() * kReferenceDisplaySize,
                         juce::Random::getSystemRandom().nextFloat() * kReferenceDisplaySize };
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

    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
        return;

    const float horizontalMargin = juce::jmax(16.0f, bounds.getWidth() * kDisplayMarginRatio);
    const float verticalMargin = juce::jmax(12.0f, bounds.getHeight() * kDisplayMarginRatio);
    auto display = bounds.reduced(horizontalMargin, verticalMargin);
    if (display.isEmpty())
        return;

    const float scale = display.getHeight() / kReferenceDisplaySize;
    const float frame_corner_radius = 30.0f * scale;
    const float screen_corner_radius = 18.0f * scale;
    const float lane_corner_radius = 10.0f * scale;
    const float lane_spacing = kLaneSpacing * scale;
    const float lane_inner_padding = juce::jmax(2.0f, 4.0f * scale);
    const float indicator_column_width = juce::jmax(24.0f, 34.0f * scale);
    const float indicator_corner_radius = 5.0f * scale;
    const float indicator_vertical_padding = 6.0f * scale;
    const float separator_thickness = juce::jmax(0.6f, 1.0f * scale);
    const float playhead_sway = kPlayheadSway * scale;

    g.fillAll(juce::Colour(0xffffcccc));

    const float frameMargin = juce::jmax(18.0f, bounds.getHeight() * kFrameMarginRatio);
    auto frame = display.expanded(frameMargin);
    g.setColour(juce::Colour(0xff101010));
    g.fillRoundedRectangle(frame, frame_corner_radius);
    g.setColour(kSoftWhite.withAlpha(0.35f));
    g.drawRoundedRectangle(frame, frame_corner_radius, 3.0f);

    const int texWidth = juce::jmax(32, static_cast<int>(std::ceil(display.getWidth())));
    const int texHeight = juce::jmax(32, static_cast<int>(std::ceil(display.getHeight())));
    if (m_funfetti_texture.isNull()
        || m_funfetti_texture.getWidth() != texWidth
        || m_funfetti_texture.getHeight() != texHeight)
    {
        regenerate_funfetti_texture(texWidth, texHeight);
    }
    {
        juce::Graphics::ScopedSaveState state(g);
        g.setTiledImageFill(m_funfetti_texture, display.getX(), display.getY(), 1.0f);
        g.fillRoundedRectangle(display, screen_corner_radius);
    }

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
        juce::Colour layer_colour = m_palette[static_cast<size_t>(layer) % m_palette.size()];
        juce::Colour lane_colour = layer_colour.darker(is_record_layer ? 0.35f : 0.5f).withAlpha(0.9f);
        if (is_record_layer)
            lane_colour = lane_colour.brighter(0.25f);
        g.setColour(lane_colour);
        g.fillRoundedRectangle(lane, lane_corner_radius);

        juce::Rectangle<float> inner_lane = lane.reduced(lane_inner_padding, lane_inner_padding);
        juce::Rectangle<float> indicator_area = inner_lane.removeFromLeft(indicator_column_width);
        waveform_bounds[static_cast<size_t>(layer)] = inner_lane;

        auto indicator_rect = indicator_area.reduced(4.0f, indicator_vertical_padding);
        const juce::Colour indicatorColour = layer_colour;
        g.setColour(is_record_layer ? indicatorColour.brighter(0.2f) : indicatorColour.withAlpha(0.55f));
        g.fillRoundedRectangle(indicator_rect, indicator_corner_radius);
        g.setColour(juce::Colour(0xfff6f1d3));
        g.drawRoundedRectangle(indicator_rect, indicator_corner_radius, 1.5f);
        juce::String indicator_text = is_record_layer ? "r" : juce::String(layer + 1);
        g.drawText(indicator_text, indicator_rect, juce::Justification::centred);
    }

    g.setColour(kSoftWhite.withAlpha(0.08f));
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

        juce::Colour wave_colour = juce::Colours::darkgrey.withAlpha(0.8f);
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

        const auto base_colour = colour_for_voice(grain.voice_index);
        const auto grain_colour = base_colour.darker(0.35f);
        g.setColour(grain_colour.withAlpha(kHighlightAlpha));
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
            const float sway = std::sin(juce::MathConstants<float>::twoPi * (t * cycles)) * playhead_sway;
            const float x = juce::jlimit(lane_area.getX(),
                                         lane_area.getRight(),
                                         playhead_x + sway);
            if (i == 0)
                squiggle.startNewSubPath(x, y);
            else
                squiggle.lineTo(x, y);
        }
        g.setColour(grain_colour.withAlpha(0.85f));
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

    if (!m_funfetti_texture.isNull())
    {
        const float texWidth = static_cast<float>(m_funfetti_texture.getWidth());
        const float texHeight = static_cast<float>(m_funfetti_texture.getHeight());
        if (texWidth > 0.0f && texHeight > 0.0f)
        {
            m_noise_scroll.x = wrap_value(m_noise_scroll.x + m_noise_velocity.x, texWidth);
            m_noise_scroll.y = wrap_value(m_noise_scroll.y + m_noise_velocity.y, texHeight);
            m_noise_phase = std::fmod(m_noise_phase + kNoisePhaseDelta,
                                      juce::MathConstants<double>::twoPi * 4096.0);
            animate_funfetti_texture();
        }
    }

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
    const float horizontalMargin = juce::jmax(16.0f, bounds.getWidth() * kDisplayMarginRatio);
    const float verticalMargin = juce::jmax(12.0f, bounds.getHeight() * kDisplayMarginRatio);
    auto display = bounds.reduced(horizontalMargin, verticalMargin);
    return display;
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

void LayerCakeDisplay::regenerate_funfetti_texture(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        DBG("LayerCakeDisplay::regenerate_funfetti_texture early return (invalid size)");
        return;
    }

    m_funfetti_texture = juce::Image(juce::Image::ARGB, width, height, true);

    auto& rng = juce::Random::getSystemRandom();
    m_noise_phase = rng.nextDouble() * juce::MathConstants<double>::twoPi;
    m_noise_scroll = {
        rng.nextFloat() * static_cast<float>(width),
        rng.nextFloat() * static_cast<float>(height)
    };

    const float minSpeed = 0.05f;
    const float maxSpeed = 0.45f;
    const float speedX = minSpeed + rng.nextFloat() * (maxSpeed - minSpeed);
    const float speedY = minSpeed + rng.nextFloat() * (maxSpeed - minSpeed);
    m_noise_velocity = {
        rng.nextBool() ? speedX : -speedX,
        rng.nextBool() ? speedY : -speedY
    };

    animate_funfetti_texture();
}

void LayerCakeDisplay::animate_funfetti_texture()
{
    if (m_funfetti_texture.isNull())
    {
        DBG("LayerCakeDisplay::animate_funfetti_texture early return (texture unavailable)");
        return;
    }

    const int width = m_funfetti_texture.getWidth();
    const int height = m_funfetti_texture.getHeight();
    if (width <= 0 || height <= 0)
    {
        DBG("LayerCakeDisplay::animate_funfetti_texture early return (invalid size)");
        return;
    }

    juce::Image::BitmapData data(m_funfetti_texture, juce::Image::BitmapData::readWrite);
    const float invWidth = 1.0f / static_cast<float>(width);
    const float invHeight = 1.0f / static_cast<float>(height);
    const float phase = static_cast<float>(m_noise_phase);

    for (int y = 0; y < height; ++y)
    {
        auto* line = reinterpret_cast<juce::PixelARGB*>(data.getLinePointer(y));
        if (line == nullptr)
            continue;

        const float ny = (static_cast<float>(y) + m_noise_scroll.y) * invHeight;
        for (int x = 0; x < width; ++x)
        {
            const float nx = (static_cast<float>(x) + m_noise_scroll.x) * invWidth;

            const float redNoise = layered_noise(nx * 60.0f, ny * 40.0f, phase, 28.0f, 19.0f);
            const float greenNoise = layered_noise(nx * 48.0f, ny * 32.0f, phase * 0.85f + 0.8f, 22.0f, 17.0f);
            const float blueNoise = layered_noise(nx * 36.0f, ny * 52.0f, phase * 1.25f + 1.6f, 31.0f, 13.0f);
            const float flicker = layered_noise(nx * 12.0f, ny * 18.0f, phase * 0.35f, 9.0f, 7.0f);

            const float r = juce::jlimit(0.0f, 1.0f, 0.55f + 0.35f * redNoise + 0.05f * flicker);
            const float g = juce::jlimit(0.0f, 1.0f, 0.5f + 0.35f * greenNoise - 0.04f * flicker);
            const float b = juce::jlimit(0.0f, 1.0f, 0.6f + 0.35f * blueNoise + 0.03f * flicker);

            line[x].setARGB(255,
                            static_cast<juce::uint8>(r * 255.0f),
                            static_cast<juce::uint8>(g * 255.0f),
                            static_cast<juce::uint8>(b * 255.0f));
        }
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
    const float scale = display.getHeight() / kReferenceDisplaySize;
    const float lane_spacing = kLaneSpacing * scale;
    const float total_spacing = lane_spacing * (LayerCakeEngine::kNumLayers - 1);
    const float lane_height = (display.getHeight() - total_spacing) / static_cast<float>(LayerCakeEngine::kNumLayers);

    return {
        display.getX(),
        display.getY() + static_cast<float>(layer_index) * (lane_height + lane_spacing),
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


