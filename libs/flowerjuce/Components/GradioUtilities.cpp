#include "GradioUtilities.h"
#include "../Engine/MultiTrackLooperEngine.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace Shared
{

juce::Result saveTrackBufferToWavFile(
    MultiTrackLooperEngine& engine,
    int trackIndex,
    juce::File& outputFile,
    const juce::String& filePrefix)
{
    auto& track = engine.getTrack(trackIndex);
    
    const juce::ScopedLock sl(track.tapeLoop.m_lock);
    const auto& buffer = track.tapeLoop.get_buffer();
    
    if (buffer.empty())
    {
        return juce::Result::fail("Buffer is empty");
    }

    // Get wrapPos to determine how much to save
    size_t wrapPos = track.writeHead.get_wrap_pos();
    if (wrapPos == 0)
    {
        wrapPos = track.tapeLoop.m_recorded_length.load();
    }
    if (wrapPos == 0)
    {
        wrapPos = buffer.size(); // Fallback to full buffer
    }
    
    // Clamp wrapPos to buffer size
    wrapPos = juce::jmin(wrapPos, buffer.size());
    
    if (wrapPos == 0)
    {
        return juce::Result::fail("No audio data to save");
    }

    // Get sample rate
    double sampleRate = track.writeHead.get_sample_rate();
    if (sampleRate <= 0)
    {
        sampleRate = 44100.0; // Default sample rate
    }

    // Create temporary file
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    outputFile = tempDir.getChildFile(filePrefix + "_" + juce::Uuid().toString() + ".wav");

    // Create output stream
    outputFile.deleteFile();
    std::unique_ptr<juce::OutputStream> fileStream(outputFile.createOutputStream());
    if (fileStream == nullptr)
    {
        return juce::Result::fail("Failed to create output file: " + outputFile.getFullPathName());
    }
    
    // Check if it's a FileOutputStream and verify it opened successfully
    auto* fileOutputStream = dynamic_cast<juce::FileOutputStream*>(fileStream.get());
    if (fileOutputStream != nullptr && !fileOutputStream->openedOk())
    {
        return juce::Result::fail("Failed to open output file: " + outputFile.getFullPathName());
    }

    // Create WAV writer
    juce::WavAudioFormat wavFormat;
    using Opts = juce::AudioFormatWriterOptions;
    auto options = Opts{}.withSampleRate(sampleRate)
                          .withNumChannels(1)  // Mono
                          .withBitsPerSample(16);

    // Writer takes ownership of the stream (pass by reference)
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(fileStream, options));
    if (writer == nullptr)
    {
        return juce::Result::fail("Failed to create WAV writer");
    }

    // Write audio data (cropped to wrapPos)
    // Convert float buffer to AudioBuffer for writing
    juce::AudioBuffer<float> audioBuffer(1, static_cast<int>(wrapPos));
    const float* source = buffer.data();
    float* dest = audioBuffer.getWritePointer(0);
    
    for (size_t i = 0; i < wrapPos; ++i)
    {
        dest[i] = source[i];
    }

    // Write the buffer
    if (!writer->writeFromAudioSampleBuffer(audioBuffer, 0, audioBuffer.getNumSamples()))
    {
        return juce::Result::fail("Failed to write audio data to file");
    }

    // Writer will flush and close when destroyed
    writer.reset();

    DBG("GradioUtilities: Saved " + juce::String(wrapPos) + " samples to " + outputFile.getFullPathName());
    return juce::Result::ok();
}

// Overload for VampNetMultiTrackLooperEngine
juce::Result saveTrackBufferToWavFile(
    VampNetMultiTrackLooperEngine& engine,
    int trackIndex,
    juce::File& outputFile,
    const juce::String& filePrefix)
{
    auto& track = engine.getTrack(trackIndex);
    
    const juce::ScopedLock sl(track.recordBuffer.m_lock);
    const auto& buffer = track.recordBuffer.get_buffer();
    
    if (buffer.empty())
    {
        return juce::Result::fail("Buffer is empty");
    }

    // Get wrapPos to determine how much to save
    size_t wrapPos = track.writeHead.get_wrap_pos();
    if (wrapPos == 0)
    {
        wrapPos = track.recordBuffer.m_recorded_length.load();
    }
    if (wrapPos == 0)
    {
        wrapPos = buffer.size(); // Fallback to full buffer
    }
    
    // Clamp wrapPos to buffer size
    wrapPos = juce::jmin(wrapPos, buffer.size());
    
    if (wrapPos == 0)
    {
        return juce::Result::fail("No audio data to save");
    }

    // Get sample rate
    double sampleRate = track.writeHead.get_sample_rate();
    if (sampleRate <= 0)
    {
        sampleRate = 44100.0; // Default sample rate
    }

    // Create temporary file
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    outputFile = tempDir.getChildFile(filePrefix + "_" + juce::Uuid().toString() + ".wav");

    // Create output stream
    outputFile.deleteFile();
    std::unique_ptr<juce::OutputStream> fileStream(outputFile.createOutputStream());
    if (fileStream == nullptr)
    {
        return juce::Result::fail("Failed to create output file: " + outputFile.getFullPathName());
    }
    
    // Check if it's a FileOutputStream and verify it opened successfully
    auto* fileOutputStream = dynamic_cast<juce::FileOutputStream*>(fileStream.get());
    if (fileOutputStream != nullptr && !fileOutputStream->openedOk())
    {
        return juce::Result::fail("Failed to open output file: " + outputFile.getFullPathName());
    }

    // Create WAV writer
    juce::WavAudioFormat wavFormat;
    using Opts = juce::AudioFormatWriterOptions;
    auto options = Opts{}.withSampleRate(sampleRate)
                          .withNumChannels(1)  // Mono
                          .withBitsPerSample(16);

    // Writer takes ownership of the stream (pass by reference)
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(fileStream, options));
    if (writer == nullptr)
    {
        return juce::Result::fail("Failed to create WAV writer");
    }

    // Write audio data (cropped to wrapPos)
    // Convert float buffer to AudioBuffer for writing
    juce::AudioBuffer<float> audioBuffer(1, static_cast<int>(wrapPos));
    const float* source = buffer.data();
    float* dest = audioBuffer.getWritePointer(0);
    
    for (size_t i = 0; i < wrapPos; ++i)
    {
        dest[i] = source[i];
    }

    // Write the buffer
    if (!writer->writeFromAudioSampleBuffer(audioBuffer, 0, audioBuffer.getNumSamples()))
    {
        return juce::Result::fail("Failed to write audio data to file");
    }

    // Writer will flush and close when destroyed
    writer.reset();

    DBG("GradioUtilities: Saved " + juce::String(wrapPos) + " samples to " + outputFile.getFullPathName());
    return juce::Result::ok();
}

// Save VampNet output buffer to WAV file
juce::Result saveVampNetOutputBufferToWavFile(
    VampNetMultiTrackLooperEngine& engine,
    int trackIndex,
    juce::File& outputFile,
    const juce::String& filePrefix)
{
    auto& track = engine.getTrack(trackIndex);
    
    const juce::ScopedLock sl(track.outputBuffer.m_lock);
    const auto& buffer = track.outputBuffer.get_buffer();
    
    if (buffer.empty())
    {
        return juce::Result::fail("Output buffer is empty");
    }

    // Get recorded length to determine how much to save
    size_t recordedLength = track.outputBuffer.m_recorded_length.load();
    if (recordedLength == 0)
    {
        recordedLength = buffer.size(); // Fallback to full buffer
    }
    
    // Clamp recordedLength to buffer size
    recordedLength = juce::jmin(recordedLength, buffer.size());
    
    if (recordedLength == 0)
    {
        return juce::Result::fail("No audio data in output buffer");
    }

    // Get sample rate from write head (should be same for all buffers)
    double sampleRate = track.writeHead.get_sample_rate();
    if (sampleRate <= 0)
    {
        sampleRate = 44100.0; // Default sample rate
    }

    // Create temporary file
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    outputFile = tempDir.getChildFile(filePrefix + "_" + juce::Uuid().toString() + ".wav");

    // Create output stream
    outputFile.deleteFile();
    std::unique_ptr<juce::OutputStream> fileStream(outputFile.createOutputStream());
    if (fileStream == nullptr)
    {
        return juce::Result::fail("Failed to create output file: " + outputFile.getFullPathName());
    }
    
    // Check if it's a FileOutputStream and verify it opened successfully
    auto* fileOutputStream = dynamic_cast<juce::FileOutputStream*>(fileStream.get());
    if (fileOutputStream != nullptr && !fileOutputStream->openedOk())
    {
        return juce::Result::fail("Failed to open output file: " + outputFile.getFullPathName());
    }

    // Create WAV writer
    juce::WavAudioFormat wavFormat;
    using Opts = juce::AudioFormatWriterOptions;
    auto options = Opts{}.withSampleRate(sampleRate)
                          .withNumChannels(1)  // Mono
                          .withBitsPerSample(16);

    // Writer takes ownership of the stream (pass by reference)
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(fileStream, options));
    if (writer == nullptr)
    {
        return juce::Result::fail("Failed to create WAV writer");
    }

    // Write audio data (cropped to recordedLength)
    // Convert float buffer to AudioBuffer for writing
    juce::AudioBuffer<float> audioBuffer(1, static_cast<int>(recordedLength));
    const float* source = buffer.data();
    float* dest = audioBuffer.getWritePointer(0);
    
    for (size_t i = 0; i < recordedLength; ++i)
    {
        dest[i] = source[i];
    }

    // Write the buffer
    if (!writer->writeFromAudioSampleBuffer(audioBuffer, 0, audioBuffer.getNumSamples()))
    {
        return juce::Result::fail("Failed to write audio data to file");
    }

    // Writer will flush and close when destroyed
    writer.reset();

    DBG("GradioUtilities: Saved " + juce::String(recordedLength) + " samples from output buffer to " + outputFile.getFullPathName());
    return juce::Result::ok();
}

juce::Result parseSSEStream(
    juce::InputStream* stream,
    juce::String& completeResponse,
    std::function<bool()> shouldAbort)
{
    if (stream == nullptr)
    {
        return juce::Result::fail("Stream is null");
    }

    juce::String lastDataLine;
    juce::String currentEventType;
    int lineCount = 0;
    
    DBG("GradioUtilities: Starting to read SSE stream...");
    
    while (!stream->isExhausted())
    {
        // Check if we should abort
        if (shouldAbort && shouldAbort())
        {
            DBG("GradioUtilities: Abort requested");
            return juce::Result::fail("Stream parsing aborted");
        }
        
        juce::String line = stream->readNextLine();
        lineCount++;
        
        // Skip empty lines (SSE uses blank lines as message separators)
        if (line.trim().isEmpty())
        {
            DBG("GradioUtilities: Empty line (message separator)");
            continue;
        }
        
        DBG("GradioUtilities: SSE line #" + juce::String(lineCount) + ": " + line);

        // Check for event type
        if (line.startsWith("event:"))
        {
            currentEventType = line.substring(6).trim();
            DBG("GradioUtilities: Event type: " + currentEventType);
        }
        else if (line.startsWith("data:"))
        {
            juce::String dataContent = line.substring(5).trim();
            lastDataLine = line;
            
            DBG("GradioUtilities: Data content: " + dataContent.substring(0, juce::jmin(200, dataContent.length())) + "...");
            
            // Check if we just got a complete or error event
            if (currentEventType == "complete")
            {
                completeResponse = line;
                DBG("GradioUtilities: Got complete event with data");
                break;
            }
            else if (currentEventType == "error")
            {
                DBG("GradioUtilities: Got error event with data: " + dataContent);
                
                // Try to read any additional error details
                juce::String additionalInfo;
                while (!stream->isExhausted())
                {
                    juce::String extraLine = stream->readNextLine();
                    if (extraLine.isNotEmpty())
                    {
                        additionalInfo += extraLine + "\n";
                        DBG("GradioUtilities: Additional error info: " + extraLine);
                    }
                    if (additionalInfo.length() > 1000) break; // Don't read too much
                }
                
                juce::String errorMsg = "Gradio API returned error";
                if (dataContent != "null" && dataContent.isNotEmpty())
                    errorMsg += ": " + dataContent;
                if (additionalInfo.isNotEmpty())
                    errorMsg += "\nAdditional info: " + additionalInfo;
                    
                return juce::Result::fail(errorMsg);
            }
            
            // Clear the event type after processing
            currentEventType = "";
        }
        // Legacy: check if the line itself contains complete/error
        else if (line.contains("complete"))
        {
            completeResponse = stream->readNextLine();
            DBG("GradioUtilities: Complete response line (legacy): " + completeResponse);
            break;
        }
        else if (line.contains("error"))
        {
            juce::String errorPayload = stream->readEntireStreamAsString();
            DBG("GradioUtilities: Error payload (legacy): " + errorPayload);
            return juce::Result::fail("Gradio API error: " + errorPayload);
        }
    }
    
    DBG("GradioUtilities: Finished reading SSE stream. Total lines: " + juce::String(lineCount));
    
    // If we didn't get completeResponse from event:complete, use the last data line
    if (completeResponse.isEmpty() && lastDataLine.isNotEmpty())
    {
        completeResponse = lastDataLine;
        DBG("GradioUtilities: Using last data line as response");
    }
    
    if (completeResponse.isEmpty())
    {
        DBG("GradioUtilities: No valid response received from stream");
        return juce::Result::fail("No response received from Gradio API");
    }

    return juce::Result::ok();
}

juce::Result uploadFileToGradio(
    const juce::String& gradioBaseUrl,
    const juce::File& fileToUpload,
    juce::String& uploadedFilePath,
    int timeoutMs)
{
    juce::URL gradioEndpoint(gradioBaseUrl);
    juce::URL uploadEndpoint = gradioEndpoint.getChildURL("gradio_api")
                                     .getChildURL("upload");

    // Print curl equivalent for upload request
    DBG("=== CURL EQUIVALENT FOR UPLOAD ===");
    DBG("curl -X POST \\");
    DBG("  -H \"User-Agent: JUCE-Gradio/1.0\" \\");
    DBG("  -F \"files=@" + fileToUpload.getFullPathName() + "\" \\");
    DBG("  \"" + uploadEndpoint.toString(false) + "\"");
    DBG("===================================");

    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    juce::String mimeType = "audio/wav";

    // Use withFileToUpload to handle multipart/form-data
    auto postEndpoint = uploadEndpoint.withFileToUpload("files", fileToUpload, mimeType);

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders("User-Agent: JUCE-Gradio/1.0\r\n")
                       .withConnectionTimeoutMs(timeoutMs)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(5)
                       .withHttpRequestCmd("POST");

    std::unique_ptr<juce::InputStream> stream(postEndpoint.createInputStream(options));

    if (stream == nullptr || statusCode != 200)
    {
        return juce::Result::fail("Failed to upload file. Status: " + juce::String(statusCode));
    }

    juce::String response = stream->readEntireStreamAsString();
    DBG("GradioUtilities: Upload response: " + response);

    // Parse response
    juce::var parsedResponse;
    auto parseResult = juce::JSON::parse(response, parsedResponse);
    if (parseResult.failed() || !parsedResponse.isArray())
    {
        return juce::Result::fail("Failed to parse upload response: " + parseResult.getErrorMessage());
    }

    juce::Array<juce::var>* responseArray = parsedResponse.getArray();
    if (responseArray == nullptr || responseArray->isEmpty())
    {
        return juce::Result::fail("Upload response is empty");
    }

    uploadedFilePath = responseArray->getFirst().toString();
    if (uploadedFilePath.isEmpty())
    {
        return juce::Result::fail("Uploaded file path is empty");
    }

    DBG("GradioUtilities: File uploaded successfully. Path: " + uploadedFilePath);
    return juce::Result::ok();
}

juce::Result downloadFileFromURL(
    const juce::URL& fileURL,
    juce::File& downloadedFile,
    int timeoutMs)
{
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::String fileName = fileURL.getFileName();
    
    juce::String baseName = juce::File::createFileWithoutCheckingPath(fileName).getFileNameWithoutExtension();
    juce::String extension = juce::File::createFileWithoutCheckingPath(fileName).getFileExtension();
    if (extension.isEmpty())
    {
        extension = ".wav"; // Default to .wav if no extension
    }
    
    downloadedFile = tempDir.getChildFile(baseName + "_" + juce::Uuid().toString() + extension);

    // Print curl equivalent for download request
    DBG("=== CURL EQUIVALENT FOR DOWNLOAD REQUEST ===");
    DBG("curl -X GET \\");
    DBG("  -H \"User-Agent: JUCE-Gradio/1.0\" \\");
    DBG("  -o \"" + downloadedFile.getFullPathName() + "\" \\");
    DBG("  \"" + fileURL.toString(false) + "\"");
    DBG("=============================================");

    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders("User-Agent: JUCE-Gradio/1.0\r\n")
                       .withConnectionTimeoutMs(timeoutMs)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(5);

    std::unique_ptr<juce::InputStream> stream(fileURL.createInputStream(options));

    if (stream == nullptr || statusCode != 200)
    {
        return juce::Result::fail("Failed to download file. Status: " + juce::String(statusCode));
    }

    // Remove file if it already exists
    downloadedFile.deleteFile();

    // Create output stream to save the file
    std::unique_ptr<juce::FileOutputStream> fileOutput(downloadedFile.createOutputStream());
    if (fileOutput == nullptr || !fileOutput->openedOk())
    {
        return juce::Result::fail("Failed to create output file: " + downloadedFile.getFullPathName());
    }

    // Copy data from input stream to output stream
    fileOutput->writeFromInputStream(*stream, stream->getTotalLength());

    DBG("GradioUtilities: File downloaded successfully to: " + downloadedFile.getFullPathName());
    return juce::Result::ok();
}

} // namespace Shared

