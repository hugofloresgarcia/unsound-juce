#include "GradioClient.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>

GradioClient::GradioClient()
{
    // Default space info
    spaceInfo.gradio = "https://opensound-ezaudio-controlnet.hf.space/";
}

juce::Result GradioClient::processRequest(const juce::File& inputAudioFile,
                                          const juce::String& textPrompt,
                                          juce::File& outputFile)
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
    // Based on the curl example:
    // "data": [
    //   "Hello!!",  // text prompt
    //   {"path":"..."} or null,  // audio file path (or null if no audio)
    //   0, 1, 0, 25, 0, 0, 0, true  // other parameters
    // ]
    
    juce::Array<juce::var> dataItems;
    
    // Text prompt
    dataItems.add(juce::var(textPrompt));
    
    // Audio file object - null if no audio, otherwise file object
    if (hasAudio)
    {
        juce::DynamicObject::Ptr fileObj = new juce::DynamicObject();
        fileObj->setProperty("path", juce::var(uploadedFilePath));
        dataItems.add(juce::var(fileObj));
    }
    else
    {
        // Add null for no audio input
        dataItems.add(juce::var());
    }
    
    // Other parameters (using defaults from the example)
    dataItems.add(juce::var(0));   // param 3
    dataItems.add(juce::var(1));   // param 4
    dataItems.add(juce::var(0));   // param 5
    dataItems.add(juce::var(25));  // param 6
    dataItems.add(juce::var(0));   // param 7
    dataItems.add(juce::var(0));   // param 8
    dataItems.add(juce::var(0));   // param 9
    dataItems.add(juce::var(true)); // param 10
    
    juce::DynamicObject::Ptr payloadObj = new juce::DynamicObject();
    payloadObj->setProperty("data", juce::var(dataItems));
    
    juce::String jsonBody = juce::JSON::toString(juce::var(payloadObj), false);
    
    DBG("GradioClient: POST payload: " + jsonBody);

    // Step 3: Make POST request to get event ID
    juce::String eventId;
    auto postResult = makePostRequestForEventID("generate_audio", eventId, jsonBody);
    if (postResult.failed())
    {
        return juce::Result::fail("Failed to make POST request: " + postResult.getErrorMessage());
    }

    DBG("GradioClient: Got event ID: " + eventId);

    // Step 4: Poll for response
    juce::String response;
    auto getResult = getResponseFromEventID("generate_audio", eventId, response);
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

    juce::DynamicObject* obj = firstElement.getDynamicObject();
    if (obj == nullptr || !obj->hasProperty("url"))
    {
        return juce::Result::fail("Response object does not have 'url' property");
    }

    juce::String fileURL = obj->getProperty("url").toString();
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

    juce::String curlGetCommand = "curl -N '" + getEndpoint.toString(true) + "'";
    DBG("GradioClient: Equivalent GET curl:\n" + curlGetCommand);

    DBG("GradioClient: GET URL: " + getEndpoint.toString(true));

    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(createCommonHeaders())
                       .withConnectionTimeoutMs(timeoutMs)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(5);

    std::unique_ptr<juce::InputStream> stream(getEndpoint.createInputStream(options));

    DBG("GradioClient: Input stream created");
    DBG("GradioClient: Status code: " + juce::String(statusCode));

    if (stream == nullptr)
    {
        return juce::Result::fail("Failed to create input stream for GET request to " + callID + "/" + eventID + ". Status code: " + juce::String(statusCode));
    }

    // Stream the response line by line until we get "complete" event
    while (!stream->isExhausted())
    {
        response = stream->readNextLine();
        
        DBG("GradioClient: Event ID: " + eventID);
        DBG("GradioClient: Response line: " + response);
        DBG("GradioClient: Line length: " + juce::String(response.length()));

        // Check for completion event - Gradio sends "event: complete"
        if (response.contains("complete"))
        {
            // Read the next line which should contain the data
            response = stream->readNextLine();
            break;
        }
        // Check for error event
        else if (response.contains("error"))
        {
            juce::String errorPayload = stream->readNextLine();
            DBG("GradioClient: Error payload: " + errorPayload);

            juce::String detailedMessage;

            std::function<juce::String(const juce::var&)> extractErrorText;
            extractErrorText = [&extractErrorText](const juce::var& value) -> juce::String
            {
                if (value.isString())
                    return value.toString();

                if (value.isObject())
                {
                    if (auto* obj = value.getDynamicObject())
                    {
                        if (obj->hasProperty("detail"))
                            return obj->getProperty("detail").toString();
                        if (obj->hasProperty("error"))
                            return obj->getProperty("error").toString();
                        if (obj->hasProperty("message"))
                            return obj->getProperty("message").toString();
                        // Fall back to first property value
                        const auto& properties = obj->getProperties();
                        for (const auto& prop : properties)
                        {
                            auto text = extractErrorText(prop.value);
                            if (text.isNotEmpty())
                                return text;
                        }
                    }
                }
                else if (value.isArray())
                {
                    if (auto* arr = value.getArray())
                    {
                        for (const auto& element : *arr)
                        {
                            auto text = extractErrorText(element);
                            if (text.isNotEmpty())
                                return text;
                        }
                    }
                }

                return juce::JSON::toString(value);
            };

            if (errorPayload.startsWith("data:"))
            {
                juce::String dataSection = errorPayload.fromFirstOccurrenceOf("data:", false, false).trim();

                if (dataSection.isNotEmpty())
                {
                    juce::var parsedError;
                    auto parseResult = juce::JSON::parse(dataSection, parsedError);
                    if (parseResult.wasOk())
                        detailedMessage = extractErrorText(parsedError).trim();
                    else
                        detailedMessage = dataSection;
                }
            }

            if (detailedMessage.isEmpty())
                detailedMessage = errorPayload;

            return juce::Result::fail("Gradio API error: " + detailedMessage);
        }
    }

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

juce::String GradioClient::createJsonHeaders() const
{
    return "User-Agent: JUCE-GradioClient/1.0\r\n"
           "Content-Type: application/json\r\n";
}

