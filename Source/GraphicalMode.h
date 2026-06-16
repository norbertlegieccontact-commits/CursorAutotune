#pragma once

#include <JuceHeader.h>
#include <memory>

struct GraphicalNode
{
    int id = 0;
    double time = 0.0;
    float hz = 0.0f;
};

class GraphicalMode
{
public:
    GraphicalMode();

    void clear();
    int addNode (double time, float hz);
    void moveNode (int id, double time, float hz);
    void deleteNode (int id);

    float getInterpolatedHz (double time) const;
    std::vector<GraphicalNode> getNodesCopy() const;

    std::unique_ptr<juce::XmlElement> createXml() const;
    void restoreFromXml (const juce::XmlElement* xml);

private:
    using NodeList = std::vector<GraphicalNode>;

    static void sortNodes (NodeList& nodes);
    std::shared_ptr<const NodeList> loadSnapshot() const noexcept;
    void storeSnapshot (std::shared_ptr<const NodeList> snapshot) noexcept;

    mutable juce::CriticalSection lock;
    std::shared_ptr<const NodeList> nodes;
    int nextId = 1;
};

