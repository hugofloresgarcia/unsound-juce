// Simple test runner for Panner unit tests
#include "PannerTests.cpp"
#include <juce_core/juce_core.h>
#include <iostream>

class ConsoleLogger final : public juce::Logger
{
    void logMessage(const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};

class ConsoleTestRunner : public juce::UnitTestRunner
{
public:
    void logMessage(const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};

int main(int argc, char* argv[])
{
    ConsoleLogger logger;
    juce::Logger::setCurrentLogger(&logger);
    
    ConsoleTestRunner runner;
    runner.setPassesAreLogged(true);
    
    // Run tests in the "Panners" category
    runner.runTestsInCategory("Panners");
    
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "Total test suites: " << runner.getNumResults() << std::endl;
    
    int totalPasses = 0;
    int totalFailures = 0;
    
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* result = runner.getResult(i);
        if (result != nullptr)
        {
            std::cout << "\nTest: " << result->unitTestName << " / " << result->subcategoryName << std::endl;
            std::cout << "  Passes: " << result->passes << std::endl;
            std::cout << "  Failures: " << result->failures << std::endl;
            
            totalPasses += result->passes;
            totalFailures += result->failures;
            
            if (result->failures > 0)
            {
                std::cout << "  Failure messages:" << std::endl;
                for (const auto& msg : result->messages)
                    std::cout << "    - " << msg << std::endl;
            }
        }
    }
    
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total passes: " << totalPasses << std::endl;
    std::cout << "Total failures: " << totalFailures << std::endl;
    
    juce::Logger::setCurrentLogger(nullptr);
    
    return (totalFailures == 0) ? 0 : 1;
}

