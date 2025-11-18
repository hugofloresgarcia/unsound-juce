#include "GradioClient.h"
#include "../Components/GradioUtilities.h"
#include <juce_audio_formats/juce_audio_formats.h>

GradioClient::GradioClient()
{
    // Default space info - updated to new API endpoint
    spaceInfo.gradio = "http://localhost:7860/";
}

juce::Result GradioClient::processRequest(const juce::File& inputAudioFile,
                                          const juce::String& textPrompt,
                                          juce::File& outputFile,
                                          const juce::var& customParams)
{
    // Step 1: Upload the input audio file (if provided)
    juce::String uploadedFilePath;
    bool hasAudio = inputAudioFile != juce::File() && inputAudioFile.existsAsFile();
    
    if (hasAudio)
    {
        auto uploadResult = uploadFileRequest(inputAudioFile, uploadedFilePath);
        if (uploadResult.failed())
        {
            return juce::Result::fail("Failed to upload audio file: " + uploadResult.getErrorMessage());
        }
    }

    // Step 2: Prepare the JSON payload
    // New API format with 7 parameters:
    // "data": [
    //   "Hello!!",  // [0] text prompt
    //   {"path":"..."} or null,  // [1] audio file path (or null if no audio)
    //   3,  // [2] seed (number)
    //   0,  // [3] median filter length (number)
    //   -24, // [4] normalize dB (number)
    //   0,  // [5] duration in seconds (number)
    //   "print('Hello World')"  // [6] inference parameters (string/code)
    // ]
    
    juce::Array<juce::var> dataItems;
    
    // [0] Text prompt
    dataItems.add(juce::var(textPrompt));
    
    // [1] Audio file object - null if no audio, otherwise file object
    if (hasAudio)
    {
        juce::DynamicObject::Ptr fileObj = new juce::DynamicObject();
        fileObj->setProperty("path", juce::var(uploadedFilePath));
        
        juce::DynamicObject::Ptr metaObj = new juce::DynamicObject();
        metaObj->setProperty("_type", juce::var("gradio.FileData"));
        fileObj->setProperty("meta", juce::var(metaObj));
        
        dataItems.add(juce::var(fileObj));
    }
    else
    {
        // Add null for no audio input
        dataItems.add(juce::var());
    }
    
    // Other parameters - customParams should always be valid (caller ensures this)
    auto* obj = customParams.getDynamicObject();
    if (obj != nullptr)
    {
        dataItems.add(obj->getProperty("seed"));                    // [2]
        dataItems.add(obj->getProperty("median_filter_length"));    // [3]
        dataItems.add(obj->getProperty("normalize_db"));            // [4]
        dataItems.add(obj->getProperty("duration"));                // [5]
        dataItems.add(obj->getProperty("inference_params"));        // [6]
    }
    
    juce::DynamicObject::Ptr payloadObj = new juce::DynamicObject();
    payloadObj->setProperty("data", juce::var(dataItems));
    
    juce::String jsonBody = juce::JSON::toString(juce::var(payloadObj), false);
    
    DBG("GradioClient: POST payload: " + jsonBody);

    // Step 3: Make POST request to get event ID
    juce::String eventId;
    auto postResult = makePostRequestForEventID("generate_with_params", eventId, jsonBody);
    if (postResult.failed())
    {
        return juce::Result::fail("Failed to make POST request: " + postResult.getErrorMessage());
    }

    DBG("GradioClient: Got event ID: " + eventId);

    // Step 4: Poll for response
    juce::String response;
    auto getResult = getResponseFromEventID("generate_with_params", eventId, response);
    if (getResult.failed())
    {
        return juce::Result::fail("Failed to get response: " + getResult.getErrorMessage());
    }

    DBG("GradioClient: Got response: " + response);

    // Step 5: Extract data from response
    juce::String responseData;
    auto extractResult = extractKeyFromResponse(response, responseData, "data: ");
    if (extractResult.failed())
    {
        return juce::Result::fail("Failed to extract data from response: " + extractResult.getErrorMessage());
    }

    // Step 6: Parse JSON and extract file URL
    juce::var parsedData;
    auto parseResult = juce::JSON::parse(responseData, parsedData);
    if (parseResult.failed())
    {
        return juce::Result::fail("Failed to parse JSON response: " + parseResult.getErrorMessage());
    }

    if (!parsedData.isObject())
    {
        return juce::Result::fail("Failed to parse the 'data' key of the received JSON.");
    }

    if (!parsedData.isArray())
    {
        return juce::Result::fail("Parsed data field should be an array.");
    }

    juce::Array<juce::var>* dataArray = parsedData.getArray();
    if (dataArray == nullptr)
    {
        return juce::Result::fail("The data array is empty.");
    }

    // Get the first element which should be the output file
    juce::var firstElement = dataArray->getFirst();
    if (!firstElement.isObject())
    {
        return juce::Result::fail("First element is not an object");
    }

    juce::DynamicObject* fileObj = firstElement.getDynamicObject();
    if (fileObj == nullptr || !fileObj->hasProperty("url"))
    {
        return juce::Result::fail("Response object does not have 'url' property");
    }

    juce::String fileURL = fileObj->getProperty("url").toString();
    DBG("GradioClient: Output file URL: " + fileURL);

    // Step 7: Download the output file
    juce::URL outputURL(fileURL);
    auto downloadResult = downloadFileFromURL(outputURL, outputFile);
    if (downloadResult.failed())
    {
        return juce::Result::fail("Failed to download output file: " + downloadResult.getErrorMessage());
    }

    return juce::Result::ok();
}

juce::Result GradioClient::processRequestMultiple(const juce::File& inputAudioFile,
                                                  const juce::String& textPrompt,
                                                  juce::Array<juce::File>& outputFiles,
                                                  const juce::var& customParams)
{
    // Step 1: Upload the input audio file (if provided)
    juce::String uploadedFilePath;
    bool hasAudio = inputAudioFile != juce::File() && inputAudioFile.existsAsFile();
    
    if (hasAudio)
    {
        auto uploadResult = uploadFileRequest(inputAudioFile, uploadedFilePath);
        if (uploadResult.failed())
        {
            return juce::Result::fail("Failed to upload audio file: " + uploadResult.getErrorMessage());
        }
    }

    // Step 2: Prepare the JSON payload (same as processRequest)
    juce::Array<juce::var> dataItems;
    
    dataItems.add(juce::var(textPrompt));
    
    if (hasAudio)
    {
        juce::DynamicObject::Ptr fileObj = new juce::DynamicObject();
        fileObj->setProperty("path", juce::var(uploadedFilePath));
        
        juce::DynamicObject::Ptr metaObj = new juce::DynamicObject();
        metaObj->setProperty("_type", juce::var("gradio.FileData"));
        fileObj->setProperty("meta", juce::var(metaObj));
        
        dataItems.add(juce::var(fileObj));
    }
    else
    {
        dataItems.add(juce::var());
    }
    
    auto* obj = customParams.getDynamicObject();
    if (obj != nullptr)
    {
        dataItems.add(obj->getProperty("seed"));
        dataItems.add(obj->getProperty("median_filter_length"));
        dataItems.add(obj->getProperty("normalize_db"));
        dataItems.add(obj->getProperty("duration"));
        dataItems.add(obj->getProperty("inference_params"));
    }
    
    juce::DynamicObject::Ptr payloadObj = new juce::DynamicObject();
    payloadObj->setProperty("data", juce::var(dataItems));
    
    juce::String jsonBody = juce::JSON::toString(juce::var(payloadObj), false);
    
    DBG("GradioClient: POST payload: " + jsonBody);

    // Step 3: Make POST request to get event ID
    juce::String eventId;
    auto postResult = makePostRequestForEventID("generate_with_params", eventId, jsonBody);
    if (postResult.failed())
    {
        return juce::Result::fail("Failed to make POST request: " + postResult.getErrorMessage());
    }

    DBG("GradioClient: Got event ID: " + eventId);

    // Step 4: Poll for response
    juce::String response;
    auto getResult = getResponseFromEventID("generate_with_params", eventId, response);
    if (getResult.failed())
    {
        return juce::Result::fail("Failed to get response: " + getResult.getErrorMessage());
    }

    DBG("GradioClient: Got response: " + response);

    // Step 5: Extract data from response
    juce::String responseData;
    auto extractResult = extractKeyFromResponse(response, responseData, "data: ");
    if (extractResult.failed())
    {
        return juce::Result::fail("Failed to extract data from response: " + extractResult.getErrorMessage());
    }

    // Step 6: Parse JSON and extract all file URLs
    juce::var parsedData;
    auto parseResult = juce::JSON::parse(responseData, parsedData);
    if (parseResult.failed())
    {
        return juce::Result::fail("Failed to parse JSON response: " + parseResult.getErrorMessage());
    }

    if (!parsedData.isArray())
    {
        return juce::Result::fail("Parsed data field should be an array.");
    }

    juce::Array<juce::var>* dataArray = parsedData.getArray();
    if (dataArray == nullptr)
    {
        return juce::Result::fail("The data array is empty.");
    }

    // Extract all file objects from the array
    outputFiles.clear();
    for (int i = 0; i < dataArray->size(); ++i)
    {
        juce::var element = (*dataArray)[i];
        if (element.isObject())
        {
            juce::DynamicObject* fileObj = element.getDynamicObject();
            if (fileObj != nullptr && fileObj->hasProperty("url"))
            {
                juce::String fileURL = fileObj->getProperty("url").toString();
                DBG("GradioClient: Found output file URL [" + juce::String(i) + "]: " + fileURL);
                
                juce::File outputFile;
                juce::URL outputURL(fileURL);
                auto downloadResult = downloadFileFromURL(outputURL, outputFile);
                if (!downloadResult.failed())
                {
                    outputFiles.add(outputFile);
                }
                else
                {
                    DBG("GradioClient: Failed to download file [" + juce::String(i) + "]: " + downloadResult.getErrorMessage());
                }
            }
        }
    }

    if (outputFiles.isEmpty())
    {
        return juce::Result::fail("No valid output files found in response");
    }

    DBG("GradioClient: Successfully downloaded " + juce::String(outputFiles.size()) + " variation(s)");
    return juce::Result::ok();
}

juce::Result GradioClient::processRequestGenerateAudio(const juce::String& textPrompt,
                                                        int durationSeconds,
                                                        juce::Array<juce::File>& outputFiles)
{
    // Step 1: Prepare the JSON payload
    // API signature: [textPrompt (string), durationSeconds (number)]
    juce::Array<juce::var> dataItems;
    dataItems.add(juce::var(textPrompt));
    dataItems.add(juce::var(durationSeconds));
    
    juce::DynamicObject::Ptr payloadObj = new juce::DynamicObject();
    payloadObj->setProperty("data", juce::var(dataItems));
    
    juce::String jsonBody = juce::JSON::toString(juce::var(payloadObj), false);
    
    DBG("GradioClient: POST payload for generate_audio: " + jsonBody);

    // Step 2: Make POST request to get event ID
    juce::String eventId;
    auto postResult = makePostRequestForEventID("generate_audio", eventId, jsonBody);
    if (postResult.failed())
    {
        return juce::Result::fail("Failed to make POST request: " + postResult.getErrorMessage());
    }

    DBG("GradioClient: Got event ID: " + eventId);

    // Step 3: Poll for response
    juce::String response;
    auto getResult = getResponseFromEventID("generate_audio", eventId, response);
    if (getResult.failed())
    {
        return juce::Result::fail("Failed to get response: " + getResult.getErrorMessage());
    }

    DBG("GradioClient: Got response: " + response);

    // Step 4: Extract data from response
    juce::String responseData;
    auto extractResult = extractKeyFromResponse(response, responseData, "data: ");
    if (extractResult.failed())
    {
        return juce::Result::fail("Failed to extract data from response: " + extractResult.getErrorMessage());
    }

    // Step 5: Parse JSON and extract file URLs
    juce::var parsedData;
    auto parseResult = juce::JSON::parse(responseData, parsedData);
    if (parseResult.failed())
    {
        return juce::Result::fail("Failed to parse JSON response: " + parseResult.getErrorMessage());
    }

    if (!parsedData.isArray())
    {
        return juce::Result::fail("Parsed data field should be an array.");
    }

    juce::Array<juce::var>* dataArray = parsedData.getArray();
    if (dataArray == nullptr)
    {
        return juce::Result::fail("The data array is empty.");
    }

    // Extract first 4 elements as audio files (ignore the 5th element which is status string)
    outputFiles.clear();
    int maxFiles = juce::jmin(4, dataArray->size() - 1); // -1 to skip status string
    
    for (int i = 0; i < maxFiles; ++i)
    {
        juce::var element = (*dataArray)[i];
        if (element.isObject())
        {
            juce::DynamicObject* fileObj = element.getDynamicObject();
            if (fileObj != nullptr && fileObj->hasProperty("url"))
            {
                juce::String fileURL = fileObj->getProperty("url").toString();
                DBG("GradioClient: Found output file URL [" + juce::String(i) + "]: " + fileURL);
                
                juce::File outputFile;
                juce::URL outputURL(fileURL);
                auto downloadResult = downloadFileFromURL(outputURL, outputFile);
                if (!downloadResult.failed())
                {
                    outputFiles.add(outputFile);
                }
                else
                {
                    DBG("GradioClient: Failed to download file [" + juce::String(i) + "]: " + downloadResult.getErrorMessage());
                }
            }
        }
    }

    if (outputFiles.isEmpty())
    {
        return juce::Result::fail("No valid output files found in response");
    }

    DBG("GradioClient: Successfully downloaded " + juce::String(outputFiles.size()) + " audio variation(s)");
    return juce::Result::ok();
}

juce::Result GradioClient::makePostRequestForEventID(const juce::String& endpoint,
                                                     juce::String& eventID,
                                                     const juce::String& jsonBody,
                                                     int timeoutMs) const
{
    juce::URL gradioEndpoint(spaceInfo.gradio);
    juce::URL requestEndpoint = gradioEndpoint.getChildURL("gradio_api")
                                       .getChildURL("call")
                                       .getChildURL(endpoint);

    juce::URL postEndpoint = requestEndpoint.withPOSTData(jsonBody);

    juce::String curlPostCommand = "curl -X POST '" + requestEndpoint.toString(true) + "' "
                                   "-H 'Content-Type: application/json' "
                                   "-d '" + jsonBody.replace("'", "\\'") + "'";
    DBG("GradioClient: Equivalent POST curl:\n" + curlPostCommand);

    DBG("GradioClient: POST URL: " + postEndpoint.toString(true));
    DBG("GradioClient: JSON body: " + jsonBody);

    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders(createJsonHeaders())
                       .withConnectionTimeoutMs(timeoutMs)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(5)
                       .withHttpRequestCmd("POST");

    std::unique_ptr<juce::InputStream> stream(postEndpoint.createInputStream(options));

    if (stream == nullptr)
    {
        return juce::Result::fail("Failed to create input stream for POST request. Status code: " + juce::String(statusCode));
    }

    juce::String response = stream->readEntireStreamAsString();

    if (statusCode != 200)
    {
        return juce::Result::fail("POST request failed with status code: " + juce::String(statusCode) + "\nResponse: " + response);
    }

    // Parse response to get event_id
    juce::var parsedResponse;
    auto parseResult = juce::JSON::parse(response, parsedResponse);
    if (parseResult.failed() || !parsedResponse.isObject())
    {
        return juce::Result::fail("Failed to parse JSON response from POST request");
    }

    juce::DynamicObject* obj = parsedResponse.getDynamicObject();
    if (obj == nullptr || !obj->hasProperty("event_id"))
    {
        return juce::Result::fail("Response does not contain 'event_id'");
    }

    eventID = obj->getProperty("event_id").toString();
    if (eventID.isEmpty())
    {
        return juce::Result::fail("event_id is empty");
    }

    return juce::Result::ok();
}

juce::Result GradioClient::getResponseFromEventID(const juce::String& callID,
                                                  const juce::String& eventID,
                                                  juce::String& response,
                                                  int timeoutMs) const
{
    juce::URL gradioEndpoint(spaceInfo.gradio);
    juce::URL getEndpoint = gradioEndpoint.getChildURL("gradio_api")
                                  .getChildURL("call")
                                  .getChildURL(callID)
                                  .getChildURL(eventID);

    // Print curl equivalent for polling request
    DBG("=== CURL EQUIVALENT FOR POLLING REQUEST ===");
    DBG("curl -N \\");
    DBG("  -H \"Accept: text/event-stream\" \\");
    DBG("  -H \"Cache-Control: no-cache\" \\");
    DBG("  -H \"Connection: keep-alive\" \\");
    DBG("  \"" + getEndpoint.toString(false) + "\"");
    DBG("===========================================");

    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    
    // Use SSE-specific headers for streaming
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(createSSEHeaders())
                       .withConnectionTimeoutMs(timeoutMs)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(5)
                       .withHttpRequestCmd("GET");

    DBG("GradioClient: Creating streaming connection...");
    std::unique_ptr<juce::InputStream> stream(getEndpoint.createInputStream(options));
    
    DBG("GradioClient: Status code: " + juce::String(statusCode));
    
    // Log response headers
    DBG("GradioClient: Response headers:");
    for (int i = 0; i < responseHeaders.size(); ++i)
    {
        DBG("  " + responseHeaders.getAllKeys()[i] + ": " + responseHeaders.getAllValues()[i]);
    }

    if (stream == nullptr)
    {
        return juce::Result::fail("Failed to create input stream for GET request to " + callID + "/" + eventID + ". Status code: " + juce::String(statusCode));
    }
    
    // Check if we got a valid status code
    if (statusCode != 0 && statusCode != 200)
    {
        DBG("GradioClient: Non-200 status code: " + juce::String(statusCode));
        // Don't fail immediately - SSE might still work
    }

    // Use shared SSE parsing utility
    auto parseResult = Shared::parseSSEStream(stream.get(), response);
    if (parseResult.failed())
        return parseResult;

    return juce::Result::ok();
}

juce::Result GradioClient::extractKeyFromResponse(const juce::String& response,
                                                  juce::String& responseKey,
                                                  const juce::String& key) const
{
    int keyIndex = response.indexOf(key);
    if (keyIndex == -1)
    {
        return juce::Result::fail("Key '" + key + "' not found in response");
    }

    responseKey = response.substring(keyIndex + key.length()).trim();
    return juce::Result::ok();
}

juce::Result GradioClient::uploadFileRequest(const juce::File& fileToUpload,
                                            juce::String& uploadedFilePath,
                                            int timeoutMs) const
{
    juce::URL gradioEndpoint(spaceInfo.gradio);
    juce::URL uploadEndpoint = gradioEndpoint.getChildURL("gradio_api")
                                     .getChildURL("upload");

    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    juce::String mimeType = "audio/wav";

    // Use withFileToUpload to handle multipart/form-data
    auto postEndpoint = uploadEndpoint.withFileToUpload("files", fileToUpload, mimeType);

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders(createCommonHeaders())
                       .withConnectionTimeoutMs(timeoutMs)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(5)
                       .withHttpRequestCmd("POST");

    std::unique_ptr<juce::InputStream> stream(postEndpoint.createInputStream(options));

    if (stream == nullptr)
    {
        return juce::Result::fail("Failed to create input stream for file upload. Status code: " + juce::String(statusCode));
    }

    juce::String response = stream->readEntireStreamAsString();

    if (statusCode != 200)
    {
        return juce::Result::fail("File upload failed with status code: " + juce::String(statusCode));
    }

    // Parse response
    juce::var parsedResponse;
    auto parseResult = juce::JSON::parse(response, parsedResponse);
    if (parseResult.failed() || !parsedResponse.isObject())
    {
        return juce::Result::fail("Failed to parse JSON response.");
    }

    // Get the array from the parsed response
    juce::Array<juce::var>* responseArray = parsedResponse.getArray();
    if (responseArray == nullptr || responseArray->isEmpty())
    {
        return juce::Result::fail("Parsed JSON does not contain the expected array.");
    }

    uploadedFilePath = responseArray->getFirst().toString();
    if (uploadedFilePath.isEmpty())
    {
        return juce::Result::fail("Uploaded file path is empty");
    }

    DBG("GradioClient: File uploaded successfully, path: " + uploadedFilePath);
    return juce::Result::ok();
}

juce::Result GradioClient::downloadFileFromURL(const juce::URL& fileURL,
                                               juce::File& downloadedFile,
                                               int timeoutMs) const
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

    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(createCommonHeaders())
                       .withConnectionTimeoutMs(timeoutMs)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(5);

    std::unique_ptr<juce::InputStream> stream(fileURL.createInputStream(options));

    if (stream == nullptr)
    {
        return juce::Result::fail("Failed to create input stream for file download");
    }

    if (statusCode != 200)
    {
        return juce::Result::fail("File download failed with status code: " + juce::String(statusCode));
    }

    // Remove file if it already exists
    downloadedFile.deleteFile();

    // Create output stream to save the file
    std::unique_ptr<juce::FileOutputStream> fileOutput(downloadedFile.createOutputStream());
    if (fileOutput == nullptr || !fileOutput->openedOk())
    {
        return juce::Result::fail("Failed to create output stream for file: " + downloadedFile.getFullPathName());
    }

    // Copy data from input stream to output stream
    fileOutput->writeFromInputStream(*stream, stream->getTotalLength());

    DBG("GradioClient: File downloaded successfully to: " + downloadedFile.getFullPathName());
    return juce::Result::ok();
}

juce::String GradioClient::createCommonHeaders() const
{
    return "User-Agent: JUCE-GradioClient/1.0\r\n";
}

juce::String GradioClient::createSSEHeaders() const
{
    return "Accept: text/event-stream\r\n"
           "Cache-Control: no-cache\r\n"
           "Connection: keep-alive\r\n";
}

juce::String GradioClient::createJsonHeaders() const
{
    return "User-Agent: JUCE-GradioClient/1.0\r\n"
           "Content-Type: application/json\r\n";
}

