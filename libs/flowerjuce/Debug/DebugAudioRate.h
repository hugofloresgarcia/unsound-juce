#pragma once

#include <juce_core/juce_core.h>

/**
 * DBG_AUDIO_RATE(interval_ms, block)
 *
 * Executes the provided code block immediately on the first call,
 * and then periodically at the specified interval (in milliseconds).
 * Useful for logging inside high-frequency audio callbacks without flooding the console.
 *
 * usage:
 *   DBG_AUDIO_RATE(2000, {
 *       DBG("current gain: " << gain);
 *   });
 *
 * @param interval_ms  The minimum time in milliseconds between executions of the block.
 *                     If <= 0, the block is never executed.
 * @param block        The code to execute.
 */
#define DBG_AUDIO_RATE(interval_ms, block) \
    do { \
        if ((interval_ms) > 0) \
        { \
            static double _dbg_last_time_ms = 0; \
            static bool _dbg_first_call = true; \
            double _dbg_now_ms = juce::Time::getMillisecondCounterHiRes(); \
            if (_dbg_first_call || (_dbg_now_ms - _dbg_last_time_ms >= (interval_ms))) \
            { \
                _dbg_last_time_ms = _dbg_now_ms; \
                _dbg_first_call = false; \
                block \
            } \
        } \
    } while (0)

