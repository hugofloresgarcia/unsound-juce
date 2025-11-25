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
constexpr float kDisplayMarginRatio = 0.03f;
constexpr float kFrameMarginRatio = 0.05f;
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

    generate_ant_sprite_sheet();

    m_ants.reserve(6);
    for (int i = 0; i < 6; ++i)
    {
        Ant ant;
        ant.position = { juce::Random::getSystemRandom().nextFloat() * kReferenceDisplaySize,
                         juce::Random::getSystemRandom().nextFloat() * kReferenceDisplaySize };
        ant.velocity = { juce::Random::getSystemRandom().nextFloat() * 0.6f + 0.2f,
                         juce::Random::getSystemRandom().nextFloat() * 0.6f + 0.2f };
        if (juce::Random::getSystemRandom().nextBool())
            ant.velocity.x *= -1.0f;
        if (juce::Random::getSystemRandom().nextBool())
            ant.velocity.y *= -1.0f;
        ant.frame = juce::Random::getSystemRandom().nextInt(kAntFrameCount);
        m_ants.push_back(ant);
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

    // NES-style sizing - use integer pixels, sharp edges
    const int margin = 8;
    auto display = bounds.reduced(static_cast<float>(margin)).toNearestInt().toFloat();
    if (display.isEmpty())
        return;

    const int border_thickness = 2;
    const int lane_gap = 4;
    const int indicator_width = 20;
    const int indicator_margin = 2;

    // Outer frame - solid black with white pixel border
    auto frame = display.expanded(4.0f);
    g.setColour(juce::Colour(0xff000000));
    g.fillRect(frame);
    g.setColour(juce::Colour(0xfffcfcfc));
    g.drawRect(frame, border_thickness);

    // Inner screen area - dark blue-ish NES background
    g.setColour(juce::Colour(0xff101820));
    g.fillRect(display);

    const float position_indicator = m_position_indicator.load();
    const bool show_position = position_indicator >= 0.0f && position_indicator <= 1.0f;

    const float total_lane_gap = static_cast<float>(lane_gap * (num_layers - 1));
    const float lane_height = (display.getHeight() - total_lane_gap) / static_cast<float>(num_layers);

    std::array<juce::Rectangle<float>, LayerCakeEngine::kNumLayers> waveform_bounds{};

    // NES-style palette (more saturated, limited colors)
    const std::array<juce::Colour, 8> nes_palette = {
        juce::Colour(0xfffc4040), // NES red
        juce::Colour(0xff00b8f8), // NES cyan
        juce::Colour(0xfff8b800), // NES yellow/gold
        juce::Colour(0xff6888fc), // NES blue
        juce::Colour(0xff58f858), // NES green
        juce::Colour(0xfff878f8), // NES magenta
        juce::Colour(0xfff87858), // NES orange
        juce::Colour(0xff00e8d8)  // NES teal
    };

    for (int layer = 0; layer < num_layers; ++layer)
    {
        // Snap to integer pixels
        const int lane_y = static_cast<int>(display.getY() + layer * (lane_height + lane_gap));
        const int lane_h = static_cast<int>(lane_height);
        juce::Rectangle<int> lane(static_cast<int>(display.getX()),
                                   lane_y,
                                   static_cast<int>(display.getWidth()),
                                   lane_h);

        const bool is_record_layer = layer == m_record_layer;
        juce::Colour layer_colour = nes_palette[static_cast<size_t>(layer) % nes_palette.size()];
        
        // Dark lane background
        juce::Colour lane_bg = layer_colour.darker(0.7f);
        g.setColour(lane_bg);
        g.fillRect(lane);

        // Pixel border around lane
        g.setColour(layer_colour.darker(0.3f));
        g.drawRect(lane, 1);

        // Indicator box on left
        juce::Rectangle<int> indicator_rect(lane.getX() + indicator_margin,
                                             lane.getY() + indicator_margin,
                                             indicator_width,
                                             lane.getHeight() - indicator_margin * 2);
        
        g.setColour(is_record_layer ? layer_colour : layer_colour.darker(0.4f));
        g.fillRect(indicator_rect);
        g.setColour(juce::Colour(0xfffcfcfc));
        g.drawRect(indicator_rect, 1);
        
        // Layer number text
        juce::String indicator_text = is_record_layer ? "R" : juce::String(layer + 1);
        g.setColour(is_record_layer ? juce::Colour(0xff000000) : juce::Colour(0xfffcfcfc));
        g.drawText(indicator_text, indicator_rect.toFloat(), juce::Justification::centred);

        // Waveform area
        juce::Rectangle<int> wave_area(lane.getX() + indicator_width + indicator_margin * 2,
                                        lane.getY() + 2,
                                        lane.getWidth() - indicator_width - indicator_margin * 3,
                                        lane.getHeight() - 4);
        waveform_bounds[static_cast<size_t>(layer)] = wave_area.toFloat();
    }

    // Waveforms - NES style stepped/blocky
    for (size_t layer = 0; layer < m_waveform_cache.size(); ++layer)
    {
        const auto& samples = m_waveform_cache[layer];
        if (samples.empty())
            continue;

        const auto& area = waveform_bounds[layer];
        if (area.isEmpty())
            continue;

        juce::Colour layer_colour = nes_palette[layer % nes_palette.size()];
        g.setColour(layer_colour);

        const int num_bars = juce::jmin(64, static_cast<int>(samples.size())); // Limit to 64 bars for chunky look
        const float bar_width = area.getWidth() / static_cast<float>(num_bars);
        const float center_y = area.getCentreY();
        const float height_scale = area.getHeight() * 0.4f;
        const int samples_per_bar = juce::jmax(1, static_cast<int>(samples.size()) / num_bars);

        for (int bar = 0; bar < num_bars; ++bar)
        {
            // Get max sample value for this bar
            float max_val = 0.0f;
            const int start_idx = bar * samples_per_bar;
            const int end_idx = juce::jmin(start_idx + samples_per_bar, static_cast<int>(samples.size()));
            for (int i = start_idx; i < end_idx; ++i)
            {
                max_val = juce::jmax(max_val, std::abs(samples[static_cast<size_t>(i)]));
            }

            // Draw mirrored bar (like classic visualizer)
            const int bar_x = static_cast<int>(area.getX() + bar * bar_width);
            const int bar_h = juce::jmax(1, static_cast<int>(max_val * height_scale));
            const int bar_w = juce::jmax(1, static_cast<int>(bar_width) - 1);

            // Top half
            g.fillRect(bar_x, static_cast<int>(center_y) - bar_h, bar_w, bar_h);
            // Bottom half (mirror)
            g.fillRect(bar_x, static_cast<int>(center_y), bar_w, bar_h);
        }
    }

    // Grain highlights - sharp rectangles
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
        
        juce::Rectangle<int> highlight{
            static_cast<int>(lane_area.getX() + lane_area.getWidth() * start_norm),
            static_cast<int>(lane_area.getY() + 1),
            juce::jmax(2, static_cast<int>(lane_area.getWidth() * width_norm)),
            static_cast<int>(lane_area.getHeight() - 2) };

        const auto base_colour = nes_palette[grain.voice_index % nes_palette.size()];
        
        // Solid highlight box with dithered pattern
        g.setColour(base_colour.withAlpha(0.5f));
        g.fillRect(highlight);
        g.setColour(base_colour);
        g.drawRect(highlight, 1);

        // Simple vertical playhead bar (no squiggle - NES style)
        const int playhead_x = highlight.getX() + static_cast<int>(highlight.getWidth() * grain.normalized_position);
        g.setColour(juce::Colour(0xfffcfcfc));
        g.fillRect(playhead_x, highlight.getY(), 2, highlight.getHeight());
    }

    if (show_position)
    {
        const int x = static_cast<int>(display.getX() + display.getWidth() * position_indicator);
        g.setColour(juce::Colour(0xfffcfcfc));
        g.fillRect(x, static_cast<int>(display.getY()), 2, static_cast<int>(display.getHeight()));
        // Simple square marker instead of circle
        g.setColour(juce::Colour(0xfffc4040));
        g.fillRect(x - 3, static_cast<int>(display.getY()) - 6, 8, 8);
    }

    // Ants / decorations
    if (!m_ant_sprite_sheet.isNull())
    {
        for (const auto& ant : m_ants)
        {
            const int ant_x = static_cast<int>(display.getX() + ant.position.x) - kAntSpriteSize / 2;
            const int ant_y = static_cast<int>(display.getY() + ant.position.y) - kAntSpriteSize / 2;
            const int src_x = ant.frame * kAntSpriteSize;
            const int src_y = ant.direction * kAntSpriteSize;
            g.drawImage(m_ant_sprite_sheet,
                        ant_x, ant_y,
                        kAntSpriteSize, kAntSpriteSize,
                        src_x, src_y,
                        kAntSpriteSize, kAntSpriteSize);
        }
    }

    // ========== CRT OVERLAY EFFECTS (drawn last, on top of everything) ==========

    // Heavy CRT scanlines - alternating dark bands
    for (int y = static_cast<int>(display.getY()); y < static_cast<int>(display.getBottom()); y += 2)
    {
        g.setColour(juce::Colour(0x60000000));
        g.drawHorizontalLine(y, display.getX(), display.getRight());
    }

    // CRT noise/static effect - random pixels that flicker
    {
        auto& rng = juce::Random::getSystemRandom();
        const int noise_density = 200; // more pixels of noise
        for (int i = 0; i < noise_density; ++i)
        {
            const int nx = static_cast<int>(display.getX()) + rng.nextInt(static_cast<int>(display.getWidth()));
            const int ny = static_cast<int>(display.getY()) + rng.nextInt(static_cast<int>(display.getHeight()));
            const juce::uint8 brightness = static_cast<juce::uint8>(rng.nextInt(80) + 40);
            g.setColour(juce::Colour(brightness, brightness, brightness, static_cast<juce::uint8>(rng.nextInt(100) + 50)));
            g.fillRect(nx, ny, 1, 1);
        }
    }

    // Horizontal interference line (like a bad signal) - moves over time
    {
        const int display_h = static_cast<int>(display.getHeight());
        if (display_h > 0)
        {
            const int interference_y = static_cast<int>(display.getY()) + 
                (static_cast<int>(m_noise_phase * 50.0) % display_h);
            g.setColour(juce::Colour(0x40ffffff));
            g.fillRect(static_cast<int>(display.getX()), interference_y, static_cast<int>(display.getWidth()), 3);
            // Secondary fainter line
            const int interference_y2 = static_cast<int>(display.getY()) + 
                ((static_cast<int>(m_noise_phase * 80.0) + display_h / 3) % display_h);
            g.setColour(juce::Colour(0x25ffffff));
            g.fillRect(static_cast<int>(display.getX()), interference_y2, static_cast<int>(display.getWidth()), 2);
        }
    }

    // Screen edge glow / bloom (subtle bright edge on the frame)
    g.setColour(juce::Colour(0x15ffffff));
    g.drawRect(display.toNearestInt(), 1);
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
    update_ants(display.getWidth(), display.getHeight());

    // Animate ants every 4 frames (approximately 7.5 fps animation at 30 fps timer)
    if (++m_animation_counter % 4 == 0)
    {
        for (auto& ant : m_ants)
            ant.frame = (ant.frame + 1) % kAntFrameCount;
    }

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

void LayerCakeDisplay::update_ants(float width, float height)
{
    for (auto& ant : m_ants)
    {
        ant.position += ant.velocity;
        if (ant.position.x < 0.0f || ant.position.x > width)
            ant.velocity.x *= -1.0f;
        if (ant.position.y < 0.0f || ant.position.y > height)
            ant.velocity.y *= -1.0f;
        ant.position.x = juce::jlimit(0.0f, width, ant.position.x);
        ant.position.y = juce::jlimit(0.0f, height, ant.position.y);

        // Update direction based on velocity (0=right, 1=down, 2=left, 3=up)
        if (std::abs(ant.velocity.x) > std::abs(ant.velocity.y))
            ant.direction = ant.velocity.x > 0.0f ? 0 : 2;
        else
            ant.direction = ant.velocity.y > 0.0f ? 1 : 3;
    }
}

void LayerCakeDisplay::generate_ant_sprite_sheet()
{
    const int sheet_width = kAntSpriteSize * kAntFrameCount;
    const int sheet_height = kAntSpriteSize * kAntDirectionCount;
    m_ant_sprite_sheet = juce::Image(juce::Image::ARGB, sheet_width, sheet_height, true);

    juce::Image::BitmapData data(m_ant_sprite_sheet, juce::Image::BitmapData::writeOnly);

    // NES-style limited palette
    const juce::PixelARGB body_dark(255, 48, 24, 16);    // Dark brown
    const juce::PixelARGB body_mid(255, 96, 56, 32);     // Mid brown  
    const juce::PixelARGB body_light(255, 144, 88, 48);  // Highlight
    const juce::PixelARGB leg_col(255, 32, 16, 8);       // Very dark legs
    const juce::PixelARGB transparent(0, 0, 0, 0);

    // Helper to set a pixel with bounds checking
    auto set_pixel = [&data, sheet_width, sheet_height](int x, int y, juce::PixelARGB col) {
        if (x >= 0 && x < sheet_width && y >= 0 && y < sheet_height)
        {
            auto* pixel = reinterpret_cast<juce::PixelARGB*>(data.getPixelPointer(x, y));
            if (pixel) *pixel = col;
        }
    };

    // Pixel art ant patterns (8x8 centered in 16x16 with room for legs)
    // Pattern for ant facing RIGHT (direction 0): head on right, abdomen on left
    // Each row represents y, each char represents a pixel type
    // . = transparent, D = dark body, M = mid body, L = light (highlight), X = leg
    
    // Frame patterns differ by leg positions
    // Ant body is roughly: [abdomen 3px][thorax 2px][head 2px] = 7px wide
    
    for (int dir = 0; dir < kAntDirectionCount; ++dir)
    {
        for (int frame = 0; frame < kAntFrameCount; ++frame)
        {
            const int base_x = frame * kAntSpriteSize;
            const int base_y = dir * kAntSpriteSize;

            // Leg animation: alternate between two positions
            const bool legs_up = (frame % 2 == 0);

            // Lambda to transform coordinates based on direction
            // dir 0 = right, 1 = down, 2 = left, 3 = up
            auto plot = [&](int local_x, int local_y, juce::PixelARGB col) {
                int tx, ty;
                switch (dir)
                {
                    case 0: // right: no transform
                        tx = base_x + 4 + local_x;
                        ty = base_y + 8 + local_y;
                        break;
                    case 1: // down: rotate 90 CW
                        tx = base_x + 8 - local_y;
                        ty = base_y + 4 + local_x;
                        break;
                    case 2: // left: rotate 180
                        tx = base_x + 12 - local_x;
                        ty = base_y + 8 - local_y;
                        break;
                    case 3: // up: rotate 90 CCW
                        tx = base_x + 8 + local_y;
                        ty = base_y + 12 - local_x;
                        break;
                    default:
                        tx = base_x + 4 + local_x;
                        ty = base_y + 8 + local_y;
                }
                set_pixel(tx, ty, col);
            };

            // Draw ant body (facing right in local coords)
            // Body is 4px (abdomen) + 3px (thorax) + 3px (head) = 10px wide
            
            // Abdomen (back/left side): 4x3 block
            plot(-5, -1, body_dark);
            plot(-4, -1, body_dark);
            plot(-3, -1, body_mid);
            plot(-2, -1, body_mid);
            plot(-5,  0, body_dark);
            plot(-4,  0, body_mid);
            plot(-3,  0, body_mid);
            plot(-2,  0, body_light); // highlight
            plot(-5,  1, body_dark);
            plot(-4,  1, body_dark);
            plot(-3,  1, body_dark);
            plot(-2,  1, body_mid);

            // Thorax (middle): 3x3 block  
            plot(-1, -1, body_mid);
            plot( 0, -1, body_mid);
            plot( 1, -1, body_mid);
            plot(-1,  0, body_mid);
            plot( 0,  0, body_mid);
            plot( 1,  0, body_mid);
            plot(-1,  1, body_dark);
            plot( 0,  1, body_dark);
            plot( 1,  1, body_dark);

            // Head (front/right): 3x3 block
            plot(2, -1, body_mid);
            plot(3, -1, body_mid);
            plot(4, -1, body_mid);
            plot(2,  0, body_mid);
            plot(3,  0, body_light); // eye highlight
            plot(4,  0, body_mid);
            plot(2,  1, body_dark);
            plot(3,  1, body_dark);
            plot(4,  1, body_dark);

            // Antennae (1 pixel each, short diagonal from head)
            plot(5, -2, leg_col);
            plot(5,  2, leg_col);

            // Legs - 6 legs, tripod gait animation
            if (legs_up)
            {
                // Tripod A up: front-left, mid-right, back-left up
                // Front legs (at head)
                plot(3, -3, leg_col);  // front-left UP
                plot(3,  3, leg_col);  // front-right DOWN
                
                // Middle legs (at thorax) 
                plot(0, -2, leg_col);  // mid-left DOWN
                plot(0,  3, leg_col);  // mid-right UP
                
                // Back legs (at abdomen)
                plot(-4, -3, leg_col); // back-left UP
                plot(-4,  3, leg_col); // back-right DOWN
            }
            else
            {
                // Tripod B up: front-right, mid-left, back-right up
                // Front legs
                plot(3, -2, leg_col);  // front-left DOWN
                plot(3,  4, leg_col);  // front-right UP
                
                // Middle legs
                plot(0, -3, leg_col);  // mid-left UP
                plot(0,  2, leg_col);  // mid-right DOWN
                
                // Back legs
                plot(-4, -2, leg_col); // back-left DOWN
                plot(-4,  4, leg_col); // back-right UP
            }
        }
    }

    DBG("LayerCakeDisplay::generate_ant_sprite_sheet created " 
        + juce::String(sheet_width) + "x" + juce::String(sheet_height) + " NES-style sprite sheet");
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


