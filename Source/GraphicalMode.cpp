#include "GraphicalMode.h"

GraphicalMode::GraphicalMode()
    : nodes (std::make_shared<const NodeList>())
{
}

void GraphicalMode::clear()
{
    const juce::ScopedLock scopedLock (lock);
    storeSnapshot (std::make_shared<const NodeList>());
}

int GraphicalMode::addNode (double time, float hz)
{
    const juce::ScopedLock scopedLock (lock);

    const auto id = nextId++;
    auto newNodes = std::make_shared<NodeList> (*loadSnapshot());
    newNodes->push_back ({ id, juce::jmax (0.0, time), juce::jmax (0.0f, hz) });
    sortNodes (*newNodes);
    storeSnapshot (newNodes);
    return id;
}

void GraphicalMode::moveNode (int id, double time, float hz)
{
    const juce::ScopedLock scopedLock (lock);
    auto newNodes = std::make_shared<NodeList> (*loadSnapshot());

    for (auto& node : *newNodes)
    {
        if (node.id == id)
        {
            node.time = juce::jmax (0.0, time);
            node.hz = juce::jmax (0.0f, hz);
            sortNodes (*newNodes);
            storeSnapshot (newNodes);
            return;
        }
    }
}

void GraphicalMode::deleteNode (int id)
{
    const juce::ScopedLock scopedLock (lock);
    auto newNodes = std::make_shared<NodeList> (*loadSnapshot());

    newNodes->erase (std::remove_if (newNodes->begin(), newNodes->end(),
                                     [id] (const auto& node) { return node.id == id; }),
                     newNodes->end());
    storeSnapshot (newNodes);
}

float GraphicalMode::getInterpolatedHz (double time) const
{
    const auto snapshot = loadSnapshot();

    if (snapshot->empty())
        return 0.0f;

    if (time <= snapshot->front().time)
        return snapshot->front().hz;

    if (time >= snapshot->back().time)
        return snapshot->back().hz;

    for (size_t i = 1; i < snapshot->size(); ++i)
    {
        const auto& previous = (*snapshot)[i - 1];
        const auto& next = (*snapshot)[i];

        if (time <= next.time)
        {
            const auto span = juce::jmax (1.0e-9, next.time - previous.time);
            const auto alpha = juce::jlimit (0.0, 1.0, (time - previous.time) / span);
            return previous.hz + static_cast<float> (alpha) * (next.hz - previous.hz);
        }
    }

    return snapshot->back().hz;
}

std::vector<GraphicalNode> GraphicalMode::getNodesCopy() const
{
    return *loadSnapshot();
}

std::unique_ptr<juce::XmlElement> GraphicalMode::createXml() const
{
    const juce::ScopedLock scopedLock (lock);
    const auto snapshot = loadSnapshot();
    auto xml = std::make_unique<juce::XmlElement> ("GRAPHICAL_MODE");
    xml->setAttribute ("nextId", nextId);

    for (const auto& node : *snapshot)
    {
        auto* child = xml->createNewChildElement ("NODE");
        child->setAttribute ("id", node.id);
        child->setAttribute ("time", node.time);
        child->setAttribute ("hz", static_cast<double> (node.hz));
    }

    return xml;
}

void GraphicalMode::restoreFromXml (const juce::XmlElement* xml)
{
    const juce::ScopedLock scopedLock (lock);
    auto newNodes = std::make_shared<NodeList>();
    nextId = 1;

    if (xml == nullptr)
    {
        storeSnapshot (newNodes);
        return;
    }

    nextId = juce::jmax (1, xml->getIntAttribute ("nextId", 1));

    for (const auto* child : xml->getChildIterator())
    {
        if (! child->hasTagName ("NODE"))
            continue;

        GraphicalNode node;
        node.id = child->getIntAttribute ("id");
        node.time = child->getDoubleAttribute ("time");
        node.hz = static_cast<float> (child->getDoubleAttribute ("hz"));
        newNodes->push_back (node);
        nextId = juce::jmax (nextId, node.id + 1);
    }

    sortNodes (*newNodes);
    storeSnapshot (newNodes);
}

void GraphicalMode::sortNodes (NodeList& nodesToSort)
{
    std::sort (nodesToSort.begin(), nodesToSort.end(),
               [] (const auto& a, const auto& b)
               {
                   if (std::abs (a.time - b.time) < 1.0e-9)
                       return a.id < b.id;

                   return a.time < b.time;
               });
}

std::shared_ptr<const GraphicalMode::NodeList> GraphicalMode::loadSnapshot() const noexcept
{
    auto snapshot = std::atomic_load_explicit (&nodes, std::memory_order_acquire);

    if (snapshot == nullptr)
        return std::make_shared<const NodeList>();

    return snapshot;
}

void GraphicalMode::storeSnapshot (std::shared_ptr<const NodeList> snapshot) noexcept
{
    std::atomic_store_explicit (&nodes, std::move (snapshot), std::memory_order_release);
}

