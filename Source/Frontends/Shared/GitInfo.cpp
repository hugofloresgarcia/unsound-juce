#include "GitInfo.h"

namespace Shared
{

juce::File GitInfoProvider::findRepoRoot()
{
    auto dir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    for (int i = 0; i < 8; ++i)
    {
        if (dir.getChildFile(".git").exists())
            return dir;
        dir = dir.getParentDirectory();
        if (!dir.exists())
            break;
    }
    return {};
}

juce::String GitInfoProvider::runGitCommand(const juce::File& repoRoot, const juce::StringArray& args)
{
    if (!repoRoot.exists())
        return {};
    
    juce::String command = "git -C \"" + repoRoot.getFullPathName() + "\"";
    for (const auto& arg : args)
        command += " " + arg;
    
    juce::ChildProcess process;
    if (!process.start(command))
        return {};
    
    juce::String output = process.readAllProcessOutput().trim();
    process.waitForProcessToFinish(3000);
    return output;
}

GitInfo GitInfoProvider::query()
{
    GitInfo info;
    auto repoRoot = findRepoRoot();
    if (!repoRoot.exists())
    {
        info.error = "Repository root not found";
        return info;
    }
    
    info.branch = runGitCommand(repoRoot, {"rev-parse", "--abbrev-ref", "HEAD"});
    info.commit = runGitCommand(repoRoot, {"rev-parse", "--short", "HEAD"});
    info.timestamp = runGitCommand(repoRoot, {"show", "-s", "--format=%ci", "HEAD"});
    
    if (info.branch.isEmpty() || info.commit.isEmpty())
        info.error = "Unable to read git metadata";
    
    return info;
}

} // namespace Shared


