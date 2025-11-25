/*
  ==============================================================================

    FocusRegistry.h
    Created: 25 Nov 2025
    Author:  LayerCake

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include "FocusableTarget.h"

namespace layercake {

class FocusRegistry : public juce::ChangeBroadcaster
{
public:
    FocusRegistry() = default;
    ~FocusRegistry() override = default;

    void registerTarget(FocusableTarget* target)
    {
        if (target == nullptr) return;
        targets.add(target);
    }

    void unregisterTarget(FocusableTarget* target)
    {
        if (currentTarget == target)
            setFocus(nullptr);
        targets.removeFirstMatchingValue(target);
    }

    void setFocus(FocusableTarget* target)
    {
        if (currentTarget == target) return;

        if (currentTarget)
            currentTarget->onFocusLost();

        currentTarget = target;

        if (currentTarget)
            currentTarget->onFocusGain();

        sendChangeMessage();
    }
    
    void setFocusByID(const juce::String& id)
    {
        for (auto* t : targets)
        {
            if (t->getFocusID() == id)
            {
                setFocus(t);
                return;
            }
        }
    }

    FocusableTarget* getCurrentFocus() const { return currentTarget; }

    const juce::Array<FocusableTarget*>& getAllTargets() const { return targets; }
    
    // Fuzzy search for targets
    juce::Array<FocusableTarget*> findTargets(const juce::String& query) const
    {
        juce::Array<FocusableTarget*> results;
        auto search = query.toLowerCase();
        
        for (auto* t : targets)
        {
            if (t->getDisplayName().toLowerCase().contains(search) || 
                t->getFocusID().toLowerCase().contains(search))
            {
                results.add(t);
            }
        }
        return results;
    }

private:
    juce::Array<FocusableTarget*> targets;
    FocusableTarget* currentTarget = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FocusRegistry)
};

} // namespace layercake

