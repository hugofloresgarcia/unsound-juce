#pragma once

#include <juce_core/juce_core.h>
#include "../Components/GradioUtilities.h"

class GradioClient
{
public:
    GradioClient();
    ~GradioClient() = default;

    struct SpaceInfo
    {
        juce::String gradio;  // Base Gradio URL (e.g., "https://opensound-ezaudio-controlnet.hf.space/")
        
        juce::String toString() const
        {
            return "Gradio URL: " + gradio;
        }
    };

    // Set the Gradio space info
    void setSpaceInfo(const SpaceInfo& info) { spaceInfo = info; }
    const SpaceInfo& getSpaceInfo() const { return spaceInfo; }

    // Process request - simplified version that just calls generate_audio
    // Returns the downloaded output file path
    // If inputAudioFile is empty/File(), it will be treated as null (no audio input)
    // customParams: optional custom parameters (if invalid/empty, uses defaults)
    juce::Result processRequest(const juce::File& inputAudioFile,
                                const juce::String& textPrompt,
                                juce::File& outputFile,
                                const juce::var& customParams = juce::var());
    
    // Process request and return all output files (for variations)
    // Returns all files found in the response array
    juce::Result processRequestMultiple(const juce::File& inputAudioFile,
                                       const juce::String& textPrompt,
                                       juce::Array<juce::File>& outputFiles,
                                       const juce::var& customParams = juce::var());

    // Process request for generate_audio API (new simplified API)
    // API signature: [textPrompt (string), duration (number)]
    // Returns: [audio1, audio2, audio3, audio4, status]
    juce::Result processRequestGenerateAudio(const juce::String& textPrompt,
                                             int durationSeconds,
                                             juce::Array<juce::File>& outputFiles);

private:
    SpaceInfo spaceInfo;

    // Make POST request to get event ID
    juce::Result makePostRequestForEventID(const juce::String& endpoint,
                                           juce::String& eventID,
                                           const juce::String& jsonBody,
                                           int timeoutMs = 30000) const;

    // Get response from event ID (polling)
    juce::Result getResponseFromEventID(const juce::String& callID,
                                        const juce::String& eventID,
                                        juce::String& response,
                                        int timeoutMs = 30000) const;

    // Extract key from response (e.g., "data: ")
    juce::Result extractKeyFromResponse(const juce::String& response,
                                       juce::String& responseKey,
                                       const juce::String& key) const;

    // Upload file to Gradio server
    juce::Result uploadFileRequest(const juce::File& fileToUpload,
                                   juce::String& uploadedFilePath,
                                   int timeoutMs = 30000) const;

    // Download file from URL
    juce::Result downloadFileFromURL(const juce::URL& fileURL,
                                    juce::File& downloadedFile,
                                    int timeoutMs = 30000) const;

    // Create common headers for requests (as formatted string)
    juce::String createCommonHeaders() const;

    // Create SSE headers for streaming requests (as formatted string)
    juce::String createSSEHeaders() const;

    // Create JSON headers for POST requests (as formatted string)
    juce::String createJsonHeaders() const;
};

