#pragma once

#include <juce_core/juce_core.h>

namespace Shared
{

struct GitInfo
{
    juce::String branch;
    juce::String commit;
    juce::String timestamp;
    juce::String error;
    
    bool isValid() const
    {
        return branch.isNotEmpty() && commit.isNotEmpty() && error.isEmpty();
    }
};

class GitInfoProvider
{
public:
    static GitInfo query();

private:
    static juce::File findRepoRoot();
    static juce::String runGitCommand(const juce::File& repoRoot, const juce::StringArray& args);
};

} // namespace Shared


