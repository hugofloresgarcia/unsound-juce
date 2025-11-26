#include <juce_core/juce_core.h>
#include <flowerjuce/Panners/DSP/PathPlayer.h>
#include "TestUtils.h"

class PathPlayerTests : public juce::UnitTest
{
public:
    PathPlayerTests() : juce::UnitTest("PathPlayerTests") {}

    void runTest() override
    {
        beginTest("Basic Playback Flow");
        testBasicPlayback();

        beginTest("Scaling");
        testScaling();

        beginTest("Offset");
        testOffset();

        beginTest("Speed");
        testSpeed();
        
        beginTest("Smoothing");
        testSmoothing();
    }

    void testBasicPlayback()
    {
        PathPlayer player;
        
        // Create a simple trajectory: (0,0) -> (1,1) -> (0.5, 0.5)
        std::vector<PathPlayer::TrajectoryPoint> points;
        points.push_back({0.0f, 0.0f, 0.0});
        points.push_back({1.0f, 1.0f, 1.0});
        points.push_back({0.5f, 0.5f, 2.0});
        
        player.set_trajectory(points);
        
        expect(!player.is_playing(), "Should not be playing initially");
        
        // No smoothing for this test to check raw points
        player.set_smoothing_time(0.0, 100.0);
        
        player.start_playback();
        expect(player.is_playing(), "Should be playing after start");
        
        // Force update to settle any smoothing (even if 0 time)
        player.update();

        // Initial position should be first point
        auto pos = player.get_current_position();
        // With 0 smoothing, it might still take one update or depend on implementation
        // PathPlayer::start_playback updates targets.
        // SmoothedValue::setTargetValue doesn't change current value immediately unless time is 0?
        // Actually, if time is 0, reset is called in set_smoothing_time with 0.
        // SmoothedValue behavior with time 0: usually snaps instantly on getNextValue.
        
        // Let's check initial values
        // The constructor sets current to 0.5.
        // If we start playback, target becomes 0.0.
        // If smoothing is 0, getNextValue (called in update) should return 0.0.
        
        expectWithinAbsoluteError(pos.first, 0.0f, 0.001f, "Initial X");
        expectWithinAbsoluteError(pos.second, 0.0f, 0.001f, "Initial Y");
        
        player.advance();
        player.update(); // Need to update to get new target into current value
        
        pos = player.get_current_position();
        expectWithinAbsoluteError(pos.first, 1.0f, 0.001f, "Second point X");
        expectWithinAbsoluteError(pos.second, 1.0f, 0.001f, "Second point Y");
        
        player.advance();
        player.update();
        
        pos = player.get_current_position();
        expectWithinAbsoluteError(pos.first, 0.5f, 0.001f, "Third point X");
        expectWithinAbsoluteError(pos.second, 0.5f, 0.001f, "Third point Y");
        
        // Loop back
        player.advance();
        player.update();
        
        pos = player.get_current_position();
        expectWithinAbsoluteError(pos.first, 0.0f, 0.001f, "Loop back to first X");
        expectWithinAbsoluteError(pos.second, 0.0f, 0.001f, "Loop back to first Y");
        
        player.stop_playback();
        expect(!player.is_playing(), "Should stop");
    }

    void testScaling()
    {
        PathPlayer player;
        
        // Square around center: (0.25, 0.25) -> (0.75, 0.75)
        // Center is 0.5, 0.5
        std::vector<PathPlayer::TrajectoryPoint> points;
        points.push_back({0.25f, 0.25f, 0.0}); // -0.25 from center
        points.push_back({0.75f, 0.75f, 0.0}); // +0.25 from center
        
        player.set_trajectory(points);
        player.set_smoothing_time(0.0, 100.0);
        player.start_playback();
        player.update();
        
        // Scale 1.0
        auto pos = player.get_current_position();
        expectWithinAbsoluteError(pos.first, 0.25f, 0.001f, "Scale 1.0 X");
        
        // Scale 2.0 -> should be (0.5 - 0.25*2) = 0.0
        player.set_scale(2.0f);
        // update needed to reflect new targets
        player.update(); 
        
        pos = player.get_current_position();
        expectWithinAbsoluteError(pos.first, 0.0f, 0.001f, "Scale 2.0 X");
        
        player.advance();
        player.update();
        
        // Second point: 0.5 + 0.25*2 = 1.0
        pos = player.get_current_position();
        expectWithinAbsoluteError(pos.first, 1.0f, 0.001f, "Scale 2.0 X second point");
        
        // Scale 0.0 -> all center
        player.set_scale(0.0f);
        player.update();
        pos = player.get_current_position();
        expectWithinAbsoluteError(pos.first, 0.5f, 0.001f, "Scale 0.0 X");
    }

    void testOffset()
    {
        PathPlayer player;
        std::vector<PathPlayer::TrajectoryPoint> points;
        points.push_back({0.5f, 0.5f, 0.0});
        
        player.set_trajectory(points);
        player.set_smoothing_time(0.0, 100.0);
        player.start_playback();
        player.update();
        
        auto pos = player.get_current_position();
        expectWithinAbsoluteError(pos.first, 0.5f, 0.001f);
        
        player.set_offset(0.1f, -0.1f);
        player.update(); 
        
        pos = player.get_current_position();
        expectWithinAbsoluteError(pos.first, 0.6f, 0.001f, "Offset X");
        expectWithinAbsoluteError(pos.second, 0.4f, 0.001f, "Offset Y");
        
        // Clamp check
        player.set_offset(0.6f, 0.0f); // 0.5 + 0.6 = 1.1 -> clamped to 1.0
        player.update();
        
        pos = player.get_current_position();
        expectWithinAbsoluteError(pos.first, 1.0f, 0.001f, "Clamped Offset X");
    }
    
    void testSpeed()
    {
        PathPlayer player;
        player.set_playback_speed(2.0f);
        expectWithinAbsoluteError(player.get_playback_speed(), 2.0f, 0.001f);
        
        player.set_playback_speed(0.05f); // Clamp min
        expectWithinAbsoluteError(player.get_playback_speed(), 0.1f, 0.001f); // Min 0.1
        
        player.set_playback_speed(3.0f); // Clamp max
        expectWithinAbsoluteError(player.get_playback_speed(), 2.0f, 0.001f); // Max 2.0
    }
    
    void testSmoothing()
    {
        PathPlayer player;
        std::vector<PathPlayer::TrajectoryPoint> points;
        points.push_back({0.0f, 0.0f, 0.0});
        points.push_back({1.0f, 1.0f, 1.0});
        
        player.set_trajectory(points);
        
        // Set smoothing: 1 second at 100Hz
        player.set_smoothing_time(1.0, 100.0);
        
        player.start_playback();
        // Start sets target to 0.0. Initial is 0.5 (default ctor).
        
        player.update(); 
        auto pos = player.get_current_position();
        
        // Should be moving from 0.5 towards 0.0
        expectLessThan(pos.first, 0.5f, "Should move down from 0.5");
        expectGreaterThan(pos.first, 0.0f, "Should not be at 0.0 yet");
        
        // Advance to 1.0
        player.advance();
        // Target is now 1.0. Current is somewhere between 0.0 and 0.5.
        
        // Run for a bit
        for(int i=0; i<50; ++i) player.update();
        
        pos = player.get_current_position();
        expectGreaterThan(pos.first, 0.5f, "Should be moving up towards 1.0");
    }
};

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;
    PathPlayerTests tests;
    juce::UnitTestRunner runner;
    runner.runTests({&tests});
    return 0;
}

