#include "WayString.h"

// hoot
#include <hoot/core/schema/TagMergerFactory.h>

namespace hoot
{

// if the difference is smaller than this we consider it to be equivalent.
Meters WayString::_epsilon = 1e-9;

WayString::WayString()
{
}

Meters WayString::_aggregateCircularError() const
{
  // I considered averaging the circular errors, but I think that could cause undesireable behaviour
  // if it is then used for comparison. This too could cause issues, but I think they'll be a little
  // more acceptable. At some point it would be good to only assign circular errors to nodes, but
  // that is a problem for the distant future.
  Meters result = 0.0;

  foreach (const WaySubline& subline, _sublines)
  {
    result = max(result, subline.getWay()->getCircularError());
  }

  return result;
}

void WayString::append(const WaySubline& subline)
{
  LOG_VAR(subline);
  if (_sublines.size() > 0)
  {
    if (back().getWay() == subline.getWay() &&
      back().getEnd() != subline.getStart())
    {
      LOG_VARW(back());
      LOG_VARW(subline);
      throw IllegalArgumentException("All consecutive ways must connect end to start in a "
        "WayString.");
    }
    else
    {
      if (subline.getStart().isExtreme(WayLocation::SLOPPY_EPSILON) == false ||
        back().getEnd().isExtreme(WayLocation::SLOPPY_EPSILON) == false)
      {
        throw IllegalArgumentException("If ways are different they must connect at an extreme "
          "node.");
      }
      if (back().getEnd().getNode(WayLocation::SLOPPY_EPSILON) !=
        subline.getStart().getNode(WayLocation::SLOPPY_EPSILON))
      {
        LOG_VARW(back());
        LOG_VARW(back().getWay());
        LOG_VARW(subline);
        LOG_VARW(subline.getWay());
        LOG_WARN("Nodes don't match: "
          << back().getEnd().getNode(WayLocation::SLOPPY_EPSILON)->getElementId()
          << " vs. " << subline.getStart().getNode(WayLocation::SLOPPY_EPSILON)->getElementId());
        throw IllegalArgumentException("Ways must connect at a node in the WayString.");
      }
    }
  }
  _sublines.append(subline);
}

WayLocation WayString::calculateLocationFromStart(Meters distance, ElementId preferredEid) const
{
  if (distance <= 0.0)
  {
    return _sublines[0].getStart();
  }

  Meters soFar = 0.0;
  for (int i = 0; i < _sublines.size(); ++i)
  {
    Meters ls = _sublines[i].calculateLength();
    if (distance <= soFar + ls)
    {
      ConstOsmMapPtr map = _sublines[i].getStart().getMap();
      ConstWayPtr w = _sublines[i].getStart().getWay();
      Meters offset = _sublines[i].getStart().calculateDistanceOnWay();
      if (_sublines[i].isBackwards())
      {
        offset -= distance - soFar;
      }
      else
      {
        offset += distance - soFar;
      }
      // if the offset isn't expected (while allowing for floating point rounding)
      if (offset < _sublines[i].getFormer().calculateDistanceOnWay() - _epsilon ||
        offset > _sublines[i].getLatter().calculateDistanceOnWay() + _epsilon)
      {
        throw InternalErrorException("Expected offset to be in the bounds of the subline. "
          "Logic error?");
      }
      WayLocation result(map, w, offset);
      return _changeToPreferred(i, result, preferredEid);
    }
    soFar += ls;
  }

  return _sublines.back().getEnd();
}

Meters WayString::calculateDistanceOnString(const WayLocation& l) const
{
  Meters d = 0.0;

  for (int i = 0; i < _sublines.size(); ++i)
  {
    if (_sublines[i].contains(l))
    {
      d += fabs(l.calculateDistanceOnWay() - _sublines[i].getStart().calculateDistanceOnWay());
      return d;
    }
    else
    {
      d += _sublines[i].calculateLength();
    }
  }

  throw IllegalArgumentException("Way location was not found in this way string.");
}

Meters WayString::calculateLength() const
{
  Meters result = 0.0;
  for (int i = 0; i < _sublines.size(); ++i)
  {
    result += _sublines[i].getLength();
  }

  return result;
}

WayLocation WayString::_changeToPreferred(int index, const WayLocation& wl, ElementId preferredEid)
  const
{
  WayLocation result = wl;

  if (preferredEid.isNull() || wl.getWay()->getElementId() == preferredEid)
  {
    result = wl;
  }
  else if (index >= 1 &&
    _sublines[index - 1].getWay()->getElementId() == preferredEid &&
    fabs(calculateDistanceOnString(wl) - calculateDistanceOnString(_sublines[index - 1].getEnd()))
      < _epsilon)
  {
    result = _sublines[index - 1].getEnd();
  }
  else if (index < _sublines.size() - 1 &&
    _sublines[index + 1].getWay()->getElementId() == preferredEid &&
    fabs(calculateDistanceOnString(wl) - calculateDistanceOnString(_sublines[index + 1].getStart()))
      < _epsilon)
  {
    result = _sublines[index + 1].getStart();
  }

  return result;
}

WayPtr WayString::copySimplifiedWayIntoMap(const ElementProvider& map, OsmMapPtr destination)
{
  ConstWayPtr w = _sublines.front().getWay();
  Meters ce = _aggregateCircularError();
  WayPtr newWay(new Way(w->getStatus(), destination->createNextWayId(), ce));

  Tags newTags;

  // go through each subline.
  foreach (const WaySubline& subline, _sublines)
  {
    // first, create a vector that contains all the node IDs in ascending order.
    ConstWayPtr oldWay = subline.getWay();

    newTags = TagMergerFactory::getInstance().mergeTags(newTags, oldWay->getTags(),
      ElementType::Way);

    // figure out which node is the first node, if we're between nodes, then create a new node to
    // add.
    size_t formeri;
    vector<long> newNids;
    if (subline.getFormer().isNode() == false)
    {
      NodePtr n = NodePtr(new Node(w->getStatus(), destination->createNextWayId(),
        subline.getFormer().getCoordinate(), ce));
      destination->addNode(n);
      newNids.push_back(n->getId());
      formeri = subline.getStart().getSegmentIndex() + 1;
    }
    else
    {
      formeri = subline.getStart().getSegmentIndex();
    }

    // which is the last node that we can directly add.
    size_t latteri = subline.getLatter().getSegmentIndex();

    // add all the pre-existing nodes that we can.
    for (size_t i = formeri; i <= latteri; ++i)
    {
      long nid = oldWay->getNodeId(i);
      newNids.push_back(nid);
      destination->addNode(NodePtr(new Node(*map.getNode(nid))));
    }

    // if the last locaiton isn't on a node, create a new node for it
    if (subline.getLatter().isNode() == false)
    {
      NodePtr n = NodePtr(new Node(w->getStatus(), destination->createNextWayId(),
        subline.getLatter().getCoordinate(), ce));
      destination->addNode(n);
      newNids.push_back(n->getId());
    }

    // if the subline is backwards reverse the nodes so we can add them in the correct order.
    if (subline.isBackwards())
    {
      std::reverse(newNids.begin(), newNids.end());
    }

    LOG_VAR(newNids);
    // add each node to the new way. If the node is a duplicate (could happen with adjoining
    // sublines) then just don't add it.
    foreach (long nid, newNids)
    {
      if (newWay->getNodeCount() == 0 || newWay->getLastNodeId() != nid)
      {
        newWay->addNode(nid);
      }
    }
  }

  newWay->setTags(newTags);
  destination->addWay(newWay);

  return newWay;
}

Meters WayString::getMaxCircularError() const
{
  Meters result = 0.0;
  foreach (const WaySubline& ws, _sublines)
  {
    result = max(ws.getWay()->getCircularError(), result);
  }
  return result;
}

void WayString::visitRo(const ElementProvider &map, ElementVisitor& v) const
{
  // go through each subline and call visitRw on the subline. The sublines should only visit the
  // nodes that intersect the subline.
  for (int i = 0; i < _sublines.size(); ++i)
  {
    _sublines[i].visitRo(map, v);
  }
}

}
