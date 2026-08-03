#include "PeopleRouteMotion.h"

#include <cmath>
#include <vector>

namespace {

const double kSegmentEpsilon = 1e-9;
const double kArrivalEpsilon = 1e-6;

bool FiniteVector(const CFVector3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

double Dot(const CFVector3 &left, const CFVector3 &right, bool horizontal)
{
    return left.x * right.x + left.z * right.z +
           (horizontal ? 0.0 : left.y * right.y);
}

double LengthSquared(const CFVector3 &value, bool horizontal)
{
    return Dot(value, value, horizontal);
}

CFVector3 Along(const CFVector3 &start, const CFVector3 &direction,
                double distance, bool horizontal, double originalY)
{
    CFVector3 value = start + direction * distance;
    if (horizontal)
        value.y = originalY;
    return value;
}

void CorrectToCorridor(CFVector3 *candidate, const CFVector3 &start,
                       const CFVector3 &end, bool horizontal,
                       double maximumDistance, double movementDistance,
                       bool centerToRoute, double *corridorDistance,
                       int *outsideCorridor, int *centeredToRoute)
{
    if (corridorDistance != NULL)
        *corridorDistance = 0.0;
    if (outsideCorridor != NULL)
        *outsideCorridor = 0;
    if (centeredToRoute != NULL)
        *centeredToRoute = 0;
    if (candidate == NULL || !FiniteVector(*candidate) ||
        !std::isfinite(maximumDistance) || maximumDistance <= 0.0)
        return;

    CFVector3 segment = end - start;
    if (horizontal)
        segment.y = 0.0;
    const double lengthSquared = LengthSquared(segment, horizontal);
    double parameter = 0.0;
    if (lengthSquared > kSegmentEpsilon * kSegmentEpsilon)
    {
        CFVector3 relative = *candidate - start;
        if (horizontal)
            relative.y = 0.0;
        parameter = Dot(relative, segment, horizontal) / lengthSquared;
        if (parameter < 0.0)
            parameter = 0.0;
        else if (parameter > 1.0)
            parameter = 1.0;
    }

    CFVector3 nearest = start + segment * parameter;
    CFVector3 offset = *candidate - nearest;
    if (horizontal)
    {
        nearest.y = candidate->y;
        offset.y = 0.0;
    }
    const double distance = std::sqrt(LengthSquared(offset, horizontal));
    if (corridorDistance != NULL)
        *corridorDistance = distance;
    if (distance > maximumDistance && distance > kSegmentEpsilon)
    {
        if (outsideCorridor != NULL)
            *outsideCorridor = 1;
        const double scale = maximumDistance / distance;
        candidate->x = nearest.x + offset.x * scale;
        candidate->z = nearest.z + offset.z * scale;
        if (!horizontal)
            candidate->y = nearest.y + offset.y * scale;
    }
    else if (centerToRoute && distance > kSegmentEpsilon &&
             std::isfinite(movementDistance) && movementDistance > 0.0)
    {
        double step = movementDistance * 0.5;
        if (step > distance)
            step = distance;
        const double scale = (distance - step) / distance;
        candidate->x = nearest.x + offset.x * scale;
        candidate->z = nearest.z + offset.z * scale;
        if (!horizontal)
            candidate->y = nearest.y + offset.y * scale;
        if (centeredToRoute != NULL)
            *centeredToRoute = 1;
    }
}

bool AdvanceCursor(int nodeCount, int backSpaceNode, int *previousNode,
                   int *currentNode, bool *stopped)
{
    if (previousNode == NULL || currentNode == NULL || stopped == NULL ||
        nodeCount < 2 || *currentNode < 0 || *currentNode >= nodeCount)
        return false;

    *previousNode = *currentNode;
    int nextNode = *currentNode + 1;
    if (nextNode >= nodeCount)
    {
        if (backSpaceNode > 0)
            nextNode -= backSpaceNode + 1;
        else if (backSpaceNode == 0)
        {
            nextNode = *currentNode;
            *stopped = true;
        }
        else
            nextNode = 0;
    }
    if (nextNode < 0)
        nextNode = 0;
    else if (nextNode >= nodeCount)
        nextNode = nodeCount - 1;
    *currentNode = nextNode;
    return true;
}

class VectorRouteSource : public IPeopleRouteNodeSource
{
public:
    explicit VectorRouteSource(const std::vector<CFVector3> &nodes)
        : nodes_(nodes) {}

    int NodeCount() override
    {
        return static_cast<int>(nodes_.size());
    }

    CFVector3 Node(int index) override
    {
        return nodes_[static_cast<std::size_t>(index)];
    }

private:
    const std::vector<CFVector3> &nodes_;
};

bool Near(double left, double right)
{
    return std::fabs(left - right) <= 1e-6;
}

} // namespace

bool PeopleRouteMotion_Advance(IPeopleRouteNodeSource *source,
                               const SPeopleRouteMotionRequest &request,
                               SPeopleRouteMotionResult *result)
{
    if (source == NULL || result == NULL ||
        !FiniteVector(request.candidate))
        return false;

    const int nodeCount = source->NodeCount();
    if (nodeCount < 2 || request.previousNode < 0 ||
        request.previousNode >= nodeCount || request.currentNode < 0 ||
        request.currentNode >= nodeCount || request.maximumSegments < 0)
        return false;

    const bool horizontal = request.horizontal != 0;
    result->position = request.candidate;
    result->previousNode = request.previousNode;
    result->currentNode = request.currentNode;
    result->segmentsConsumed = 0;
    result->stopped = 0;
    result->traversalLimitReached = 0;
    result->degenerateSegmentSeen = 0;
    result->corridorDistance = 0.0;
    result->outsideCorridor = 0;
    result->centeredToRoute = 0;

    while (true)
    {
        const CFVector3 start = source->Node(result->previousNode);
        const CFVector3 target = source->Node(result->currentNode);
        if (!FiniteVector(start) || !FiniteVector(target))
            return false;

        CFVector3 segment = target - start;
        if (horizontal)
            segment.y = 0.0;
        const double lengthSquared = LengthSquared(segment, horizontal);
        const double length = std::sqrt(lengthSquared);

        if (length <= kSegmentEpsilon)
        {
            result->degenerateSegmentSeen = 1;
            if (result->segmentsConsumed >= request.maximumSegments)
            {
                result->traversalLimitReached = 1;
                break;
            }

            bool stopped = false;
            if (!AdvanceCursor(nodeCount, request.backSpaceNode,
                               &result->previousNode, &result->currentNode,
                               &stopped))
                return false;
            ++result->segmentsConsumed;
            result->position = target;
            if (horizontal)
                result->position.y = request.candidate.y;
            if (stopped)
            {
                result->stopped = 1;
                break;
            }
            continue;
        }

        CFVector3 relative = result->position - start;
        if (horizontal)
            relative.y = 0.0;
        const CFVector3 direction = segment * (1.0 / length);
        const double longitudinal = Dot(relative, direction, horizontal);
        if (longitudinal <= length + kArrivalEpsilon)
            break;
        if (result->segmentsConsumed >= request.maximumSegments)
        {
            result->traversalLimitReached = 1;
            break;
        }

        const double remainder = longitudinal - length;
        bool stopped = false;
        if (!AdvanceCursor(nodeCount, request.backSpaceNode,
                           &result->previousNode, &result->currentNode,
                           &stopped))
            return false;
        ++result->segmentsConsumed;
        if (stopped)
        {
            result->position = target;
            if (horizontal)
                result->position.y = request.candidate.y;
            result->stopped = 1;
            break;
        }

        const CFVector3 nextStart = source->Node(result->previousNode);
        const CFVector3 nextTarget = source->Node(result->currentNode);
        CFVector3 nextSegment = nextTarget - nextStart;
        if (horizontal)
            nextSegment.y = 0.0;
        const double nextLength = std::sqrt(
            LengthSquared(nextSegment, horizontal));
        if (nextLength <= kSegmentEpsilon)
        {
            result->position = nextStart;
            if (horizontal)
                result->position.y = request.candidate.y;
        }
        else
        {
            result->position = Along(nextStart,
                nextSegment * (1.0 / nextLength), remainder, horizontal,
                request.candidate.y);
        }
    }

    const CFVector3 corridorStart = source->Node(result->previousNode);
    const CFVector3 corridorEnd = source->Node(result->currentNode);
    CorrectToCorridor(&result->position, corridorStart, corridorEnd, horizontal,
                      request.maximumCorridorDistance,
                      request.movementDistance,
                      request.centerToRoute != 0,
                      &result->corridorDistance,
                      &result->outsideCorridor,
                      &result->centeredToRoute);
    return FiniteVector(result->position);
}

bool PeopleRouteMotion_Probe()
{
    std::vector<CFVector3> straight;
    for (int index = 0; index < 14; ++index)
        straight.push_back(CFVector3(index * 10.0, 0.0, 0.0));
    VectorRouteSource straightSource(straight);

    SPeopleRouteMotionRequest request = {
        CFVector3(25.0, 7.0, 0.0), 0, 1, -1, 1, 10, 10.0, 0.0, 0};
    SPeopleRouteMotionResult result = {};
    const bool multiSegment = PeopleRouteMotion_Advance(
        &straightSource, request, &result) &&
        result.previousNode == 2 && result.currentNode == 3 &&
        result.segmentsConsumed == 2 && !result.stopped &&
        Near(result.position.x, 25.0) && Near(result.position.y, 7.0);

    request.candidate = CFVector3(135.0, 0.0, 0.0);
    request.previousNode = 12;
    request.currentNode = 13;
    request.backSpaceNode = 0;
    const bool stop = PeopleRouteMotion_Advance(
        &straightSource, request, &result) && result.stopped &&
        result.previousNode == 13 && result.currentNode == 13 &&
        Near(result.position.x, 130.0);

    std::vector<CFVector3> terminalNodes;
    terminalNodes.push_back(CFVector3(0.0, 0.0, 0.0));
    terminalNodes.push_back(CFVector3(10.0, 0.0, 0.0));
    terminalNodes.push_back(CFVector3(20.0, 0.0, 0.0));
    terminalNodes.push_back(CFVector3(30.0, 0.0, 0.0));
    VectorRouteSource terminalSource(terminalNodes);
    request.candidate = CFVector3(35.0, 0.0, 0.0);
    request.previousNode = 2;
    request.currentNode = 3;
    request.backSpaceNode = -1;
    const bool loop = PeopleRouteMotion_Advance(
        &terminalSource, request, &result) && !result.stopped &&
        result.previousNode == 3 && result.currentNode == 0 &&
        Near(result.position.x, 25.0);
    request.backSpaceNode = 1;
    const bool rewind = PeopleRouteMotion_Advance(
        &terminalSource, request, &result) && !result.stopped &&
        result.previousNode == 3 && result.currentNode == 2 &&
        Near(result.position.x, 25.0);

    request.candidate = CFVector3(5.0, 3.0, 100.0);
    request.previousNode = 0;
    request.currentNode = 1;
    request.backSpaceNode = -1;
    request.maximumCorridorDistance = 10.0;
    const bool corridor = PeopleRouteMotion_Advance(
        &terminalSource, request, &result) &&
        Near(result.position.x, 5.0) && Near(result.position.y, 3.0) &&
        Near(result.position.z, 10.0) && result.outsideCorridor &&
        Near(result.corridorDistance, 100.0);

    request.candidate = CFVector3(5.0, 7.0, 8.0);
    request.movementDistance = 4.0;
    request.centerToRoute = 1;
    const bool centered = PeopleRouteMotion_Advance(
        &terminalSource, request, &result) &&
        Near(result.position.x, 5.0) && Near(result.position.y, 7.0) &&
        Near(result.position.z, 6.0) && !result.outsideCorridor &&
        result.centeredToRoute && Near(result.corridorDistance, 8.0);

    request.movementDistance = 100.0;
    const bool centeredAtLine = PeopleRouteMotion_Advance(
        &terminalSource, request, &result) && Near(result.position.z, 0.0) &&
        result.centeredToRoute;

    request.movementDistance = 4.0;
    request.centerToRoute = 0;
    const bool collisionBypass = PeopleRouteMotion_Advance(
        &terminalSource, request, &result) && Near(result.position.z, 8.0) &&
        !result.outsideCorridor && !result.centeredToRoute;

    std::vector<CFVector3> degenerateNodes;
    degenerateNodes.push_back(CFVector3(0.0, 0.0, 0.0));
    degenerateNodes.push_back(CFVector3(0.0, 0.0, 0.0));
    degenerateNodes.push_back(CFVector3(10.0, 0.0, 0.0));
    VectorRouteSource degenerateSource(degenerateNodes);
    request.candidate = CFVector3(2.0, 0.0, 0.0);
    request.previousNode = 0;
    request.currentNode = 1;
    const bool degenerate = PeopleRouteMotion_Advance(
        &degenerateSource, request, &result) &&
        result.degenerateSegmentSeen && result.segmentsConsumed == 1 &&
        result.previousNode == 1 && result.currentNode == 2 &&
        FiniteVector(result.position);

    request.candidate = CFVector3(200.0, 0.0, 0.0);
    request.previousNode = 0;
    request.currentNode = 1;
    request.maximumSegments = 10;
    const bool capped = PeopleRouteMotion_Advance(
        &straightSource, request, &result) &&
        result.segmentsConsumed == 10 && result.traversalLimitReached &&
        result.previousNode == 10 && result.currentNode == 11 &&
        FiniteVector(result.position);

    return multiSegment && stop && loop && rewind && corridor && centered &&
           centeredAtLine && collisionBypass && degenerate && capped;
}
