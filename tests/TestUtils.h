#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <fstream>
#include <vector>
#include <string>
#include <memory>

namespace TestUtils
{

// Helper class to write test data to CSV for plotting
class CsvWriter
{
public:
    CsvWriter(const juce::String& filename, const std::vector<std::string>& headers)
        : m_filename(filename)
    {
        // Ensure output directory exists
        juce::File outputDir = juce::File::getCurrentWorkingDirectory().getChildFile("tests/output");
        if (!outputDir.exists())
            outputDir.createDirectory();
            
        juce::File csvFile = outputDir.getChildFile(filename + ".csv");
        m_ofs.open(csvFile.getFullPathName().toStdString());
        
        // Write headers
        for (size_t i = 0; i < headers.size(); ++i)
        {
            m_ofs << headers[i];
            if (i < headers.size() - 1)
                m_ofs << ",";
        }
        m_ofs << "\n";
    }

    ~CsvWriter()
    {
        if (m_ofs.is_open())
            m_ofs.close();
    }

    template<typename... Args>
    void writeRow(Args... args)
    {
        writeRowImpl(args...);
        m_ofs << "\n";
    }

private:
    template<typename T, typename... Args>
    void writeRowImpl(T first, Args... rest)
    {
        m_ofs << first;
        if constexpr (sizeof...(rest) > 0)
        {
            m_ofs << ",";
            writeRowImpl(rest...);
        }
    }
    
    void writeRowImpl() {}

    juce::String m_filename;
    std::ofstream m_ofs;
};

// Simple WAV writer
class AudioWriter
{
public:
    AudioWriter(const juce::String& filename, double sampleRate = 44100.0)
        : m_sampleRate(sampleRate)
    {
        juce::File outputDir = juce::File::getCurrentWorkingDirectory().getChildFile("tests/output");
        if (!outputDir.exists())
            outputDir.createDirectory();
        m_file = outputDir.getChildFile(filename + ".wav");
        m_file.deleteFile(); // overwrite
    }

    void write(const std::vector<float>& samples)
    {
        // Simple manual WAV writing to avoid module dependencies if possible
        // but relying on juce_audio_formats is cleaner if linked.
        // Let's do manual simple 16-bit mono WAV to be safe and dependency-light
        // effectively replicating what typical raw writers do.
        
        std::ofstream f(m_file.getFullPathName().toStdString(), std::ios::binary);
        
        int numSamples = static_cast<int>(samples.size());
        int numChannels = 1;
        int bitsPerSample = 16;
        int byteRate = static_cast<int>(m_sampleRate * numChannels * bitsPerSample / 8);
        int blockAlign = numChannels * bitsPerSample / 8;
        int dataSize = numSamples * numChannels * bitsPerSample / 8;
        int chunkSize = 36 + dataSize;

        // Header
        f.write("RIFF", 4);
        writeInternal<int32_t>(f, chunkSize);
        f.write("WAVE", 4);
        f.write("fmt ", 4);
        writeInternal<int32_t>(f, 16); // Subchunk1Size (16 for PCM)
        writeInternal<int16_t>(f, 1); // AudioFormat (1 for PCM)
        writeInternal<int16_t>(f, (int16_t)numChannels);
        writeInternal<int32_t>(f, (int32_t)m_sampleRate);
        writeInternal<int32_t>(f, byteRate);
        writeInternal<int16_t>(f, (int16_t)blockAlign);
        writeInternal<int16_t>(f, (int16_t)bitsPerSample);
        f.write("data", 4);
        writeInternal<int32_t>(f, dataSize);

        // Data
        for (float s : samples)
        {
            // Clip and convert to 16-bit
            float clipped = juce::jlimit(-1.0f, 1.0f, s);
            int16_t pcm = static_cast<int16_t>(clipped * 32767.0f);
            writeInternal<int16_t>(f, pcm);
        }
        
        f.close();
    }

private:
    template<typename T>
    void writeInternal(std::ofstream& f, T value)
    {
        f.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }

    juce::File m_file;
    double m_sampleRate;
};

// Helper for floating point comparisons
inline bool almostEqual(float a, float b, float epsilon = 1e-4f)
{
    return std::abs(a - b) < epsilon;
}

} // namespace TestUtils
