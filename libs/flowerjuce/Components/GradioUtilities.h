#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include "../LooperEngine/MultiTrackLooperEngine.h"

namespace Shared
{

// Save a track's audio buffer to a WAV file
// Used by both Text2Sound and VampNet worker threads
juce::Result saveTrackBufferToWavFile(
    MultiTrackLooperEngine& engine,
    int trackIndex,
    juce::File& outputFile,
    const juce::String& filePrefix = "gradio_input"
);

// Overload for VampNetMultiTrackLooperEngine (saves from recordBuffer) - commented out since VampNetTrackEngine doesn't exist
// juce::Result saveTrackBufferToWavFile(
//     VampNetMultiTrackLooperEngine& engine,
//     int trackIndex,
//     juce::File& outputFile,
//     const juce::String& filePrefix = "gradio_input"
// );

// Save VampNet output buffer to WAV file - commented out since VampNetTrackEngine doesn't exist
// juce::Result saveVampNetOutputBufferToWavFile(
//     VampNetMultiTrackLooperEngine& engine,
//     int trackIndex,
//     juce::File& outputFile,
//     const juce::String& filePrefix = "gradio_input"
// );

// Parse a Server-Sent Events (SSE) stream from a Gradio API
// Returns the complete response data line when successful
// shouldAbort: optional callback to check if parsing should be aborted (e.g., thread stop requested)
juce::Result parseSSEStream(
    juce::InputStream* stream,
    juce::String& completeResponse,
    std::function<bool()> shouldAbort = nullptr
);

// Upload a file to a Gradio API endpoint
// Returns the uploaded file path on the server
juce::Result uploadFileToGradio(
    const juce::String& gradioBaseUrl,
    const juce::File& fileToUpload,
    juce::String& uploadedFilePath,
    int timeoutMs = 30000
);

// Download a file from a URL (typically a Gradio output file)
juce::Result downloadFileFromURL(
    const juce::URL& fileURL,
    juce::File& downloadedFile,
    int timeoutMs = 30000
);

} // namespace Shared

