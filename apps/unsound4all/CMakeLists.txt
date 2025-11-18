# Unsound4All app

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png)
    juce_add_gui_app(Unsound4AllApp
        COMPANY_NAME "Unsound"
        PRODUCT_NAME "Unsound4All Tape Looper"
        VERSION 1.0.0
        ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png
        ICON_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png
    )
else()
    juce_add_gui_app(Unsound4AllApp
        COMPANY_NAME "Unsound"
        PRODUCT_NAME "Unsound4All Tape Looper"
        VERSION 1.0.0
    )
endif()

# App source files
target_sources(Unsound4AllApp PRIVATE
    Main.cpp
    StartupDialog.cpp
    StartupDialog.h
    MainComponent.cpp
    MainComponent.h
    LooperTrack.cpp
    LooperTrack.h
    CLAP/ONNXModelManager.cpp
    CLAP/ONNXModelManager.h
    CLAP/RobertaTokenizer.cpp
    CLAP/RobertaTokenizer.h
    CLAP/SoundPaletteManager.cpp
    CLAP/SoundPaletteManager.h
    CLAP/SoundPaletteCreator.cpp
    CLAP/SoundPaletteCreator.h
    CLAP/CLAPSearchWorkerThread.cpp
    CLAP/CLAPSearchWorkerThread.h
    CLAP/PaletteCreationProgressWindow.cpp
    CLAP/PaletteCreationProgressWindow.h
)

# Link flowerjuce library
target_link_libraries(Unsound4AllApp PRIVATE
    flowerjuce
)

# Link JUCE modules
target_link_libraries(Unsound4AllApp PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# ONNX Runtime C++ integration
# Auto-download if not found, or use existing installation
set(ONNXRUNTIME_VERSION "1.23.2")
set(ONNXRUNTIME_ROOT "${CMAKE_SOURCE_DIR}/third_party/onnxruntime")

# Detect platform and architecture for download
if(APPLE)
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64|ARM64")
        set(ONNXRUNTIME_PLATFORM "osx-arm64")
    elseif(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64|X86_64")
        set(ONNXRUNTIME_PLATFORM "osx-x86_64")
    else()
        # Universal binary (both arm64 and x86_64)
        set(ONNXRUNTIME_PLATFORM "osx-universal2")
    endif()
    set(ONNXRUNTIME_EXT "tgz")
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(ONNXRUNTIME_PLATFORM "linux-aarch64")
    else()
        set(ONNXRUNTIME_PLATFORM "linux-x64")
    endif()
    set(ONNXRUNTIME_EXT "tgz")
elseif(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64")
        set(ONNXRUNTIME_PLATFORM "win-arm64")
    else()
        set(ONNXRUNTIME_PLATFORM "win-x64")
    endif()
    set(ONNXRUNTIME_EXT "zip")
endif()

set(ONNXRUNTIME_ARCHIVE_NAME "onnxruntime-${ONNXRUNTIME_PLATFORM}-${ONNXRUNTIME_VERSION}.${ONNXRUNTIME_EXT}")
set(ONNXRUNTIME_ARCHIVE_PATH "${CMAKE_SOURCE_DIR}/third_party/${ONNXRUNTIME_ARCHIVE_NAME}")
set(ONNXRUNTIME_EXTRACT_DIR "${CMAKE_SOURCE_DIR}/third_party/onnxruntime-${ONNXRUNTIME_PLATFORM}-${ONNXRUNTIME_VERSION}")

# Ensure third_party directory exists
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party")

# Check if already extracted
if(EXISTS "${ONNXRUNTIME_EXTRACT_DIR}/include/onnxruntime_cxx_api.h")
    set(ONNXRUNTIME_FOUND TRUE)
    set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_EXTRACT_DIR}/include")
    set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_EXTRACT_DIR}/lib")
    message(STATUS "Found ONNX Runtime at: ${ONNXRUNTIME_EXTRACT_DIR}")
elseif(EXISTS "${ONNXRUNTIME_ARCHIVE_PATH}")
    # Archive exists but not extracted - extract it
    message(STATUS "Extracting ONNX Runtime from ${ONNXRUNTIME_ARCHIVE_NAME}...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${ONNXRUNTIME_ARCHIVE_PATH}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party"
        RESULT_VARIABLE EXTRACT_RESULT
        ERROR_VARIABLE EXTRACT_ERROR
        OUTPUT_VARIABLE EXTRACT_OUTPUT
    )
    if(EXTRACT_RESULT EQUAL 0 AND EXISTS "${ONNXRUNTIME_EXTRACT_DIR}/include/onnxruntime_cxx_api.h")
        set(ONNXRUNTIME_FOUND TRUE)
        set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_EXTRACT_DIR}/include")
        set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_EXTRACT_DIR}/lib")
        message(STATUS "Extracted ONNX Runtime to: ${ONNXRUNTIME_EXTRACT_DIR}")
    else()
        message(WARNING "Failed to extract ONNX Runtime archive (result: ${EXTRACT_RESULT})")
        if(EXTRACT_ERROR)
            message(WARNING "Extraction error: ${EXTRACT_ERROR}")
        endif()
        if(EXTRACT_OUTPUT)
            message(WARNING "Extraction output: ${EXTRACT_OUTPUT}")
        endif()
        # Try alternative extraction method
        message(STATUS "Trying alternative extraction method...")
        find_program(TAR_COMMAND tar)
        if(TAR_COMMAND)
            execute_process(
                COMMAND ${TAR_COMMAND} -xzf "${ONNXRUNTIME_ARCHIVE_PATH}"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party"
                RESULT_VARIABLE TAR_RESULT
            )
            if(TAR_RESULT EQUAL 0 AND EXISTS "${ONNXRUNTIME_EXTRACT_DIR}/include/onnxruntime_cxx_api.h")
                set(ONNXRUNTIME_FOUND TRUE)
                set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_EXTRACT_DIR}/include")
                set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_EXTRACT_DIR}/lib")
                message(STATUS "Successfully extracted ONNX Runtime using tar command")
            endif()
        endif()
    endif()
else()
    # Try to download
    set(ONNXRUNTIME_DOWNLOAD_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/${ONNXRUNTIME_ARCHIVE_NAME}")
    message(STATUS "ONNX Runtime not found. Attempting to download from ${ONNXRUNTIME_DOWNLOAD_URL}...")
    
    file(DOWNLOAD
        ${ONNXRUNTIME_DOWNLOAD_URL}
        ${ONNXRUNTIME_ARCHIVE_PATH}
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
        TIMEOUT 300
    )
    
    list(GET DOWNLOAD_STATUS 0 DOWNLOAD_RESULT)
    if(DOWNLOAD_RESULT EQUAL 0)
        message(STATUS "Downloaded ONNX Runtime. Extracting...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xzf "${ONNXRUNTIME_ARCHIVE_PATH}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party"
            RESULT_VARIABLE EXTRACT_RESULT
            ERROR_VARIABLE EXTRACT_ERROR
            OUTPUT_VARIABLE EXTRACT_OUTPUT
        )
        if(EXTRACT_RESULT EQUAL 0 AND EXISTS "${ONNXRUNTIME_EXTRACT_DIR}/include/onnxruntime_cxx_api.h")
            set(ONNXRUNTIME_FOUND TRUE)
            set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_EXTRACT_DIR}/include")
            set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_EXTRACT_DIR}/lib")
            message(STATUS "Successfully downloaded and extracted ONNX Runtime")
        else()
            message(WARNING "Failed to extract ONNX Runtime archive (result: ${EXTRACT_RESULT})")
            if(EXTRACT_ERROR)
                message(WARNING "Extraction error: ${EXTRACT_ERROR}")
            endif()
            # Try alternative extraction method
            find_program(TAR_COMMAND tar)
            if(TAR_COMMAND)
                execute_process(
                    COMMAND ${TAR_COMMAND} -xzf "${ONNXRUNTIME_ARCHIVE_PATH}"
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party"
                    RESULT_VARIABLE TAR_RESULT
                )
                if(TAR_RESULT EQUAL 0 AND EXISTS "${ONNXRUNTIME_EXTRACT_DIR}/include/onnxruntime_cxx_api.h")
                    set(ONNXRUNTIME_FOUND TRUE)
                    set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_EXTRACT_DIR}/include")
                    set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_EXTRACT_DIR}/lib")
                    message(STATUS "Successfully extracted ONNX Runtime using tar command")
                endif()
            endif()
        endif()
    else()
        list(GET DOWNLOAD_STATUS 1 ERROR_MESSAGE)
        message(WARNING "Failed to download ONNX Runtime: ${ERROR_MESSAGE}")
        message(WARNING "You may need to run CMake again after ensuring network connectivity, or manually download:")
        message(WARNING "  ${ONNXRUNTIME_DOWNLOAD_URL}")
        message(WARNING "  Place it in: ${CMAKE_SOURCE_DIR}/third_party/")
    endif()
endif()

# Try to find library
if(ONNXRUNTIME_FOUND)
    find_library(ONNXRUNTIME_LIB
        NAMES onnxruntime
        PATHS
            ${ONNXRUNTIME_LIB_DIR}
            ${ONNXRUNTIME_EXTRACT_DIR}/lib
        NO_DEFAULT_PATH
    )
    
    if(ONNXRUNTIME_LIB)
        message(STATUS "Found ONNX Runtime library: ${ONNXRUNTIME_LIB}")
        target_include_directories(Unsound4AllApp PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
        target_link_libraries(Unsound4AllApp PRIVATE ${ONNXRUNTIME_LIB})
        target_compile_definitions(Unsound4AllApp PRIVATE HAVE_ONNXRUNTIME=1)
        
        # On macOS, copy dylib to app bundle and set rpath
        if(APPLE)
            # Copy the versioned library first
            add_custom_command(TARGET Unsound4AllApp POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ONNXRUNTIME_LIB_DIR}/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib"
                    "$<TARGET_BUNDLE_DIR:Unsound4AllApp>/Contents/MacOS/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib"
                COMMENT "Copying ONNX Runtime versioned library to app bundle"
            )
            # Fix the install name of the copied library to use @rpath
            add_custom_command(TARGET Unsound4AllApp POST_BUILD
                COMMAND install_name_tool -id "@rpath/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib"
                    "$<TARGET_BUNDLE_DIR:Unsound4AllApp>/Contents/MacOS/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib"
                COMMENT "Fixing ONNX Runtime library install name"
            )
            # Fix the executable's reference to the library to use @rpath
            # Note: @executable_path is already in rpath via INSTALL_RPATH setting
            add_custom_command(TARGET Unsound4AllApp POST_BUILD
                COMMAND install_name_tool -change "${ONNXRUNTIME_LIB_DIR}/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib" "@rpath/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib"
                    "$<TARGET_BUNDLE_DIR:Unsound4AllApp>/Contents/MacOS/Unsound4All Tape Looper"
                COMMENT "Fixing executable's library reference to use @rpath"
            )
            # Also try to fix any reference to the non-versioned library name
            add_custom_command(TARGET Unsound4AllApp POST_BUILD
                COMMAND install_name_tool -change "${ONNXRUNTIME_LIB_DIR}/libonnxruntime.dylib" "@rpath/libonnxruntime.${ONNXRUNTIME_VERSION}.dylib"
                    "$<TARGET_BUNDLE_DIR:Unsound4AllApp>/Contents/MacOS/Unsound4All Tape Looper"
                COMMENT "Fixing executable's library reference (non-versioned) to use @rpath"
            )
            # Also copy the non-versioned symlink if it exists (for compatibility)
            add_custom_command(TARGET Unsound4AllApp POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ONNXRUNTIME_LIB_DIR}/libonnxruntime.dylib"
                    "$<TARGET_BUNDLE_DIR:Unsound4AllApp>/Contents/MacOS/libonnxruntime.dylib"
                COMMENT "Copying ONNX Runtime library symlink to app bundle"
            )
            # Set rpath to find the library in the app bundle
            set_target_properties(Unsound4AllApp PROPERTIES
                INSTALL_RPATH "@executable_path"
                BUILD_WITH_INSTALL_RPATH TRUE
            )
        endif()
        
        # Copy ONNX model files and data files to app bundle
        set(ONNX_MODEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
        if(EXISTS "${ONNX_MODEL_DIR}/clap_audio_encoder.onnx" AND EXISTS "${ONNX_MODEL_DIR}/clap_text_encoder.onnx")
            if(APPLE)
                # On macOS, copy to Resources folder in app bundle
                add_custom_command(TARGET Unsound4AllApp POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory
                        "$<TARGET_BUNDLE_DIR:Unsound4AllApp>/Contents/Resources"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${ONNX_MODEL_DIR}/clap_audio_encoder.onnx"
                        "${ONNX_MODEL_DIR}/clap_text_encoder.onnx"
                        "${ONNX_MODEL_DIR}/clap_audio_encoder.onnx.data"
                        "${ONNX_MODEL_DIR}/clap_text_encoder.onnx.data"
                        "${ONNX_MODEL_DIR}/roberta_vocab.json"
                        "${ONNX_MODEL_DIR}/roberta_merges.json"
                        "${ONNX_MODEL_DIR}/roberta_special_tokens.json"
                        "$<TARGET_BUNDLE_DIR:Unsound4AllApp>/Contents/Resources/"
                    COMMENT "Copying ONNX model files, data files, and tokenizer files to app bundle"
                )
            else()
                # On other platforms, copy to executable directory
                add_custom_command(TARGET Unsound4AllApp POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${ONNX_MODEL_DIR}/clap_audio_encoder.onnx"
                        "${ONNX_MODEL_DIR}/clap_text_encoder.onnx"
                        "${ONNX_MODEL_DIR}/clap_audio_encoder.onnx.data"
                        "${ONNX_MODEL_DIR}/clap_text_encoder.onnx.data"
                        "${ONNX_MODEL_DIR}/roberta_vocab.json"
                        "${ONNX_MODEL_DIR}/roberta_merges.json"
                        "${ONNX_MODEL_DIR}/roberta_special_tokens.json"
                        "$<TARGET_FILE_DIR:Unsound4AllApp>/"
                    COMMENT "Copying ONNX model files, data files, and tokenizer files to executable directory"
                )
            endif()
        else()
            message(WARNING "ONNX model files not found in ${ONNX_MODEL_DIR}")
        endif()
    else()
        message(WARNING "ONNX Runtime include found but library not found")
        target_compile_definitions(Unsound4AllApp PRIVATE HAVE_ONNXRUNTIME=0)
    endif()
else()
    # Fallback: try system installation
    find_path(ONNXRUNTIME_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        PATHS
            /usr/local/include/onnxruntime
            /opt/homebrew/include/onnxruntime
            ENV ONNXRUNTIME_INCLUDE_DIR
    )
    
    find_library(ONNXRUNTIME_LIB
        NAMES onnxruntime
        PATHS
            /usr/local/lib
            /opt/homebrew/lib
            ENV ONNXRUNTIME_LIB_DIR
    )
    
    if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIB)
        message(STATUS "Found ONNX Runtime in system: ${ONNXRUNTIME_INCLUDE_DIR}")
        target_include_directories(Unsound4AllApp PRIVATE ${ONNXRUNTIME_INCLUDE_DIR})
        target_link_libraries(Unsound4AllApp PRIVATE ${ONNXRUNTIME_LIB})
        target_compile_definitions(Unsound4AllApp PRIVATE HAVE_ONNXRUNTIME=1)
    else()
        message(WARNING "ONNX Runtime not found. CLAP functionality will be disabled.")
        message(STATUS "To enable ONNX Runtime:")
        message(STATUS "  1. Download from: https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/${ONNXRUNTIME_ARCHIVE_NAME}")
        message(STATUS "  2. Place in: ${CMAKE_SOURCE_DIR}/third_party/")
        message(STATUS "  3. Or set ONNXRUNTIME_INCLUDE_DIR and ONNXRUNTIME_LIB_DIR environment variables")
        target_compile_definitions(Unsound4AllApp PRIVATE HAVE_ONNXRUNTIME=0)
    endif()
endif()

# FAISS integration
# Try to find FAISS library
find_path(FAISS_INCLUDE_DIR
    NAMES faiss/Index.h
    PATHS
        ${CMAKE_SOURCE_DIR}/third_party/faiss/include
        /usr/local/include
        /opt/homebrew/include
        ENV FAISS_INCLUDE_DIR
)

find_library(FAISS_LIB
    NAMES faiss
    PATHS
        ${CMAKE_SOURCE_DIR}/third_party/faiss/lib
        /usr/local/lib
        /opt/homebrew/lib
        ENV FAISS_LIB_DIR
)

if(FAISS_INCLUDE_DIR AND FAISS_LIB)
    message(STATUS "Found FAISS: ${FAISS_INCLUDE_DIR}")
    target_include_directories(Unsound4AllApp PRIVATE ${FAISS_INCLUDE_DIR})
    target_link_libraries(Unsound4AllApp PRIVATE ${FAISS_LIB})
    target_compile_definitions(Unsound4AllApp PRIVATE HAVE_FAISS=1)
else()
    message(WARNING "FAISS not found. Vector search will use simple linear search.")
    target_compile_definitions(Unsound4AllApp PRIVATE HAVE_FAISS=0)
endif()

# Embed binary resources
set(BINARY_DATA_SOURCES)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png)
    list(APPEND BINARY_DATA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png)
endif()

if(BINARY_DATA_SOURCES)
    juce_add_binary_data(Unsound4AllAppBinaryData
        SOURCES ${BINARY_DATA_SOURCES}
    )
    target_link_libraries(Unsound4AllApp PRIVATE Unsound4AllAppBinaryData)
endif()

