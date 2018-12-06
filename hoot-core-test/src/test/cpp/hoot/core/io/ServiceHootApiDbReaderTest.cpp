/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. DigitalGlobe
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2013, 2014, 2016, 2017, 2018 DigitalGlobe (http://www.digitalglobe.com/)
 */

// CPP Unit
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>

// Hoot
#include <hoot/core/elements/OsmMap.h>
#include <hoot/core/TestUtils.h>
#include <hoot/core/elements/ElementAttributeType.h>
#include <hoot/core/io/HootApiDb.h>
#include <hoot/core/io/HootApiDbReader.h>
#include <hoot/core/io/HootApiDbWriter.h>
#include <hoot/core/io/OsmMapReaderFactory.h>
#include <hoot/core/io/OsmMapWriterFactory.h>
#include <hoot/core/io/OsmXmlWriter.h>
#include <hoot/core/io/ServicesDbTestUtils.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/Log.h>
#include <hoot/core/schema/MetadataTags.h>
#include <hoot/core/util/MapProjector.h>
#include <hoot/core/visitors/RemoveAttributesVisitor.h>

using namespace std;

namespace hoot
{

class ServiceHootApiDbReaderTest : public HootTestFixture
{
  CPPUNIT_TEST_SUITE(ServiceHootApiDbReaderTest);
  CPPUNIT_TEST(runCalculateBoundsTest);
  CPPUNIT_TEST(runElementIdTest);
  CPPUNIT_TEST(runUrlMissingMapIdTest);
  CPPUNIT_TEST(runUrlInvalidMapIdTest);
  CPPUNIT_TEST(runReadTest);
  CPPUNIT_TEST(runPartialReadTest);
  CPPUNIT_TEST(runFactoryReadTest);
  CPPUNIT_TEST(runReadWithElemTest);
  CPPUNIT_TEST(runReadByBoundsTest);
  CPPUNIT_TEST(runAccessPublicMapWithoutEmailTest);
  CPPUNIT_TEST(runAccessPrivateMapWithoutEmailTest);
  //TODO: fix
  //CPPUNIT_TEST(runInvalidUserTest);
  //CPPUNIT_TEST(runMultipleMapsSameNameSameUserTest);
  CPPUNIT_TEST(runMultipleMapsSameNameDifferentUsersTest);
  //TODO: fix
  //CPPUNIT_TEST(runMultipleMapsSameNameNoUserTest);
  CPPUNIT_TEST_SUITE_END();

public:

  QString userEmail()
  { return QString("%1.ServiceHootApiDbReaderTest@hoottestcpp.org").arg(testName); }

  long mapId;
  QString testName;

  ServiceHootApiDbReaderTest()
  {
    setResetType(ResetAll);
    TestUtils::mkpath("test-output/io/ServiceHootApiDbReaderTest");
  }

  void setUpTest(const QString& test_name)
  {
    mapId = -1;
    testName = test_name;
    ServicesDbTestUtils::deleteUser(userEmail());
    HootApiDb database;

    database.open(ServicesDbTestUtils::getDbModifyUrl());
    database.getOrCreateUser(userEmail(), QString("%1.ServiceHootApiDbReaderTest").arg(testName));
    database.close();
  }

  virtual void tearDown()
  {
    ServicesDbTestUtils::deleteUser(userEmail());

    if (mapId != -1)
    {
      HootApiDb database;
      database.open(ServicesDbTestUtils::getDbModifyUrl());

      database.deleteFolderMapMappingsByMapId(mapId);
      database.deleteFolders(database.getFolderIdsAssociatedWithMap(mapId));

      database.deleteMap(mapId);

      database.close();
    }
  }

  long populateMap(const bool putInFolder = false, const bool folderIsPublic = false)
  {
    LOG_DEBUG("Populating test map...");

    OsmMapPtr map = ServicesDbTestUtils::createServiceTestMap();

    HootApiDbWriter writer;
    writer.setUserEmail(userEmail());
    writer.setRemap(false);
    writer.setIncludeDebug(true);
    writer.open(ServicesDbTestUtils::getDbModifyUrl(testName).toString());
    writer.write(map);
    writer.close();

    if (putInFolder)
    {
      LOG_DEBUG("Adding test data folder...");
      HootApiDb db;
      db.open(ServicesDbTestUtils::getDbModifyUrl(testName).toString());
      db.insertFolderMapMapping(
        writer.getMapId(),
        db.insertFolder(testName, -1, db.getUserId(userEmail(), true), folderIsPublic));
      db.close();
    }

    return writer.getMapId();
  }

  long insertDataForBoundTest()
  {
    OsmMapPtr map(new OsmMap());
    OsmMapReaderFactory::read(
      map, "test-files/io/ServiceHootApiDbReaderTest/runReadByBoundsTestInput.osm", false,
      Status::Unknown1);

    HootApiDbWriter writer;
    writer.setUserEmail(userEmail());
    writer.setRemap(true);
    writer.open(ServicesDbTestUtils::getDbModifyUrl(testName).toString());
    writer.write(map);
    writer.close();
    return writer.getMapId();
  }

  template<typename T>
  vector<long> getKeys(T begin, T end)
  {
    vector<long> result;
    for (;begin != end; ++begin)
    {
      result.push_back(begin->first);
    }
    return result;
  }

  void runCalculateBoundsTest()
  {
    setUpTest("runCalculateBoundsTest");
    mapId = populateMap();

    HootApiDbReader reader;
    reader.setUserEmail(userEmail());
    QString url = ServicesDbTestUtils::getDbReadUrl(mapId).toString();
    reader.open(url);
    HOOT_STR_EQUALS("Env[0:0.4,0:0]", reader.calculateEnvelope().toString());
  }

  void runElementIdTest()
  {
    setUpTest("runElementIdTest");
    mapId = populateMap();

    HootApiDbReader reader;
    reader.setUserEmail(userEmail());
    // make sure all the element ids start with -1
    OsmMapPtr map(new OsmMap());
    reader.setUseDataSourceIds(false);
    reader.open(ServicesDbTestUtils::getDbReadUrl(mapId).toString());
    reader.read(map);

    HOOT_STR_EQUALS("[5]{-5, -4, -3, -2, -1}",
      getKeys(map->getNodes().begin(), map->getNodes().end()));
    HOOT_STR_EQUALS("[2]{-2, -1}",
      getKeys(map->getRelations().begin(), map->getRelations().end()));
    HOOT_STR_EQUALS("[3]{-3, -2, -1}", getKeys(map->getWays().begin(), map->getWays().end()));

    HOOT_STR_EQUALS("[1]{-2}", map->getWay(-3)->getNodeIds());
    HOOT_STR_EQUALS("[2]{-2, -3}", map->getWay(-2)->getNodeIds());
    HOOT_STR_EQUALS("[2]{-1, -2}", map->getWay(-1)->getNodeIds());
    HOOT_STR_EQUALS("[1]{Entry: role: n2, eid: Node:-2}", map->getRelation(-2)->getMembers());
    HOOT_STR_EQUALS("[2]{Entry: role: n1, eid: Node:-1, Entry: role: w1, eid: Way:-1}",
      map->getRelation(-1)->getMembers());
  }

  void runUrlMissingMapIdTest()
  {
    setUpTest("runUrlMissingMapIdTest");
    // temporarily disable logging to avoid isValid warning
    DisableLog dl;

    HootApiDbReader reader;
    reader.setUserEmail(userEmail());
    QString exceptionMsg("");
    try
    {
      reader.open(
        ServicesDbTestUtils::getDbReadUrl(mapId).toString().replace("/" + QString::number(mapId), ""));
    }
    catch (const HootException& e)
    {
      exceptionMsg = e.what();
    }

    //I would rather this return: "URL does not contain valid map ID." from
    //HootApiDbReader::open
    CPPUNIT_ASSERT(exceptionMsg.contains("An unsupported URL was passed in"));
  }

  void runUrlInvalidMapIdTest()
  {
    setUpTest("runUrlInvalidMapIdTest");
    HootApiDbReader reader;
    reader.setUserEmail(userEmail());
    QString exceptionMsg("");
    const long invalidMapId = mapId + 1;
    try
    {
      reader.open(
        ServicesDbTestUtils::getDbReadUrl(mapId).toString().replace(
          "/" + QString::number(mapId), "/" + QString::number(invalidMapId)));
    }
    catch (const HootException& e)
    {
      exceptionMsg = e.what();
    }
    CPPUNIT_ASSERT_EQUAL(
      QString("No map exists with requested ID: " +
      QString::number(invalidMapId)).toStdString(), exceptionMsg.toStdString());
  }

  void verifyFullReadOutput(OsmMapPtr map)
  {
    //nodes

    CPPUNIT_ASSERT_EQUAL(5, (int)map->getNodes().size());

    NodePtr node = map->getNode(1);
    HOOT_STR_EQUALS(Status::Unknown1, node->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)1, node->getId());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getX());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getY());
    CPPUNIT_ASSERT_EQUAL(10.0, node->getCircularError());
    CPPUNIT_ASSERT_EQUAL(1, node->getTags().size());
    CPPUNIT_ASSERT_EQUAL((long)1, node->getVersion());
    CPPUNIT_ASSERT(node->getTimestamp() != ElementData::TIMESTAMP_EMPTY);
    HOOT_STR_EQUALS("1", node->getTags().get(MetadataTags::HootId()));

    node = map->getNode(2);
    HOOT_STR_EQUALS(Status::Unknown2, node->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)2, node->getId());
    CPPUNIT_ASSERT_EQUAL(0.1, node->getX());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getY());
    CPPUNIT_ASSERT_EQUAL(11.0, node->getCircularError());
    CPPUNIT_ASSERT_EQUAL(2, node->getTags().size());
    HOOT_STR_EQUALS("n2b", node->getTags().get("noteb"));
    CPPUNIT_ASSERT_EQUAL((long)1, node->getVersion());
    CPPUNIT_ASSERT(node->getTimestamp() != ElementData::TIMESTAMP_EMPTY);
    HOOT_STR_EQUALS("2", node->getTags().get(MetadataTags::HootId()));

    node = map->getNode(3);
    HOOT_STR_EQUALS(Status::Conflated, node->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)3, node->getId());
    CPPUNIT_ASSERT_EQUAL(0.2, node->getX());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getY());
    CPPUNIT_ASSERT_EQUAL(12.0, node->getCircularError());
    CPPUNIT_ASSERT_EQUAL(2, node->getTags().size());
    HOOT_STR_EQUALS("n3", node->getTags().get("note"));
    CPPUNIT_ASSERT_EQUAL((long)1, node->getVersion());
    CPPUNIT_ASSERT(node->getTimestamp() != ElementData::TIMESTAMP_EMPTY);
    HOOT_STR_EQUALS("3", node->getTags().get(MetadataTags::HootId()));

    node = map->getNode(4);
    HOOT_STR_EQUALS(Status::Conflated, node->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)4, node->getId());
    CPPUNIT_ASSERT_EQUAL(0.3, node->getX());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getY());
    CPPUNIT_ASSERT_EQUAL(13.0, node->getCircularError());
    CPPUNIT_ASSERT_EQUAL(2, node->getTags().size());
    HOOT_STR_EQUALS("n4", node->getTags().get("note"));
    CPPUNIT_ASSERT_EQUAL((long)1, node->getVersion());
    CPPUNIT_ASSERT(node->getTimestamp() != ElementData::TIMESTAMP_EMPTY);
    HOOT_STR_EQUALS("4", node->getTags().get(MetadataTags::HootId()));

    node = map->getNode(5);
    HOOT_STR_EQUALS(Status::Invalid, node->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)5, node->getId());
    CPPUNIT_ASSERT_EQUAL(0.4, node->getX());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getY());
    CPPUNIT_ASSERT_EQUAL(14.0, node->getCircularError());
    CPPUNIT_ASSERT_EQUAL(1, node->getTags().size());
    CPPUNIT_ASSERT_EQUAL((long)1, node->getVersion());
    CPPUNIT_ASSERT(node->getTimestamp() != ElementData::TIMESTAMP_EMPTY);
    HOOT_STR_EQUALS("5", node->getTags().get(MetadataTags::HootId()));

    //ways

    CPPUNIT_ASSERT_EQUAL(3, (int)map->getWays().size());

    WayPtr way = map->getWay(1);
    HOOT_STR_EQUALS(Status::Unknown1, way->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)1, way->getId());
    CPPUNIT_ASSERT_EQUAL(15.0, way->getCircularError());
    CPPUNIT_ASSERT(way->hasNode(1));
    CPPUNIT_ASSERT(way->hasNode(2));
    CPPUNIT_ASSERT_EQUAL(2, way->getTags().size());
    HOOT_STR_EQUALS("w1b", way->getTags().get("noteb"));
    CPPUNIT_ASSERT_EQUAL((long)1, way->getVersion());
    CPPUNIT_ASSERT(way->getTimestamp() != ElementData::TIMESTAMP_EMPTY);
    HOOT_STR_EQUALS("1", way->getTags().get(MetadataTags::HootId()));

    way = map->getWay(2);
    HOOT_STR_EQUALS(Status::Unknown2, way->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)2, way->getId());
    CPPUNIT_ASSERT_EQUAL(16.0, way->getCircularError());
    CPPUNIT_ASSERT(way->hasNode(2));
    CPPUNIT_ASSERT(way->hasNode(3));
    CPPUNIT_ASSERT_EQUAL(2, way->getTags().size());
    HOOT_STR_EQUALS("w2", way->getTags().get("note"));
    CPPUNIT_ASSERT_EQUAL((long)1, way->getVersion());
    CPPUNIT_ASSERT(way->getTimestamp() != ElementData::TIMESTAMP_EMPTY);
    HOOT_STR_EQUALS("2", way->getTags().get(MetadataTags::HootId()));

    way = map->getWay(3);
    HOOT_STR_EQUALS(Status::Unknown2, way->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)3, way->getId());
    CPPUNIT_ASSERT_EQUAL(17.0, way->getCircularError());
    CPPUNIT_ASSERT(way->hasNode(2));
    CPPUNIT_ASSERT_EQUAL(1, way->getTags().size());
    CPPUNIT_ASSERT_EQUAL((long)1, way->getVersion());
    CPPUNIT_ASSERT(way->getTimestamp() != ElementData::TIMESTAMP_EMPTY);
    HOOT_STR_EQUALS("3", way->getTags().get(MetadataTags::HootId()));

    //relations

    CPPUNIT_ASSERT_EQUAL(2, (int)map->getRelations().size());

    RelationPtr relation = map->getRelation(1);
    HOOT_STR_EQUALS(Status::Unknown1, relation->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)1, relation->getId());
    CPPUNIT_ASSERT_EQUAL(18.1, relation->getCircularError());
    HOOT_STR_EQUALS(MetadataTags::RelationCollection(), relation->getType());
    vector<RelationData::Entry> relationMembers = relation->getMembers();
    CPPUNIT_ASSERT_EQUAL(size_t(2), relationMembers.size());
    CPPUNIT_ASSERT(relation->contains(ElementId::node(1)));
    CPPUNIT_ASSERT(relation->contains(ElementId::way(1)));
    RelationData::Entry member = relationMembers.at(0);
    HOOT_STR_EQUALS("n1", member.role);
    CPPUNIT_ASSERT_EQUAL((long)1, member.getElementId().getId());
    member = relationMembers.at(1);
    HOOT_STR_EQUALS("w1", member.role);
    CPPUNIT_ASSERT_EQUAL((long)1, member.getElementId().getId());
    CPPUNIT_ASSERT_EQUAL(2, relation->getTags().size());
    HOOT_STR_EQUALS("r1", relation->getTags().get("note"));
    CPPUNIT_ASSERT_EQUAL((long)1, relation->getVersion());
    CPPUNIT_ASSERT(relation->getTimestamp() != ElementData::TIMESTAMP_EMPTY);
    HOOT_STR_EQUALS("1", relation->getTags().get(MetadataTags::HootId()));

    relation = map->getRelation(2);
    HOOT_STR_EQUALS(Status::Unknown1, relation->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)2, relation->getId());
    CPPUNIT_ASSERT_EQUAL(15.0, relation->getCircularError());
    HOOT_STR_EQUALS("", relation->getType());
    CPPUNIT_ASSERT(relation->contains(ElementId::node(2)));
    CPPUNIT_ASSERT_EQUAL(size_t(1), relation->getMembers().size());
    member = relation->getMembers().at(0);
    HOOT_STR_EQUALS("n2", member.role);
    CPPUNIT_ASSERT_EQUAL((long)2, member.getElementId().getId());
    CPPUNIT_ASSERT_EQUAL(1, relation->getTags().size());
    CPPUNIT_ASSERT_EQUAL((long)1, relation->getVersion());
    CPPUNIT_ASSERT(relation->getTimestamp() != ElementData::TIMESTAMP_EMPTY);
    HOOT_STR_EQUALS("2", relation->getTags().get(MetadataTags::HootId()));
  }

  void verifySingleReadOutput(OsmMapPtr map)
  {
    //nodes

    CPPUNIT_ASSERT_EQUAL(5, (int)map->getNodes().size());

    NodePtr node = map->getNode(3);
    HOOT_STR_EQUALS(Status::Conflated, node->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)3, node->getId());
    CPPUNIT_ASSERT_EQUAL(0.2, node->getX());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getY());
    CPPUNIT_ASSERT_EQUAL(12.0, node->getCircularError());
    CPPUNIT_ASSERT_EQUAL(2, node->getTags().size());
    HOOT_STR_EQUALS("n3", node->getTags().get("note"));
    HOOT_STR_EQUALS("3", node->getTags().get(MetadataTags::HootId()));
  }

  void runReadTest()
  {
    setUpTest("runReadTest");
    mapId = populateMap();

    HootApiDbReader reader;
    reader.setUserEmail(userEmail());
    OsmMapPtr map(new OsmMap());
    reader.open(ServicesDbTestUtils::getDbReadUrl(mapId).toString());
    reader.read(map);
    verifyFullReadOutput(map);
    reader.close();
  }

  void runReadWithElemTest()
  {
    setUpTest("runReadWithElemTest");
    mapId = populateMap();

    HootApiDbReader reader;
    reader.setUserEmail(userEmail());
    OsmMapPtr map(new OsmMap());
    reader.open(ServicesDbTestUtils::getDbReadUrl(mapId,3,"node").toString());
    reader.read(map);
    verifySingleReadOutput(map);
    reader.close();
  }

  void runFactoryReadTest()
  {
    setUpTest("runFactoryReadTest");
    mapId = populateMap();

    OsmMapPtr map(new OsmMap());
    conf().set("api.db.email", userEmail());
    OsmMapReaderFactory::read(map, ServicesDbTestUtils::getDbReadUrl(mapId).toString());
    verifyFullReadOutput(map);

    TestUtils::resetEnvironment();
  }

  void runPartialReadTest()
  {
    setUpTest("runPartialReadTest");
    mapId = populateMap();

    HootApiDbReader reader;
    reader.setUserEmail(userEmail());
    const int chunkSize = 3;
    reader.setMaxElementsPerMap(chunkSize);
    reader.open(ServicesDbTestUtils::getDbReadUrl(mapId).toString());
    reader.initializePartial();

    int ctr = 0;
    OsmMapPtr map(new OsmMap());

    //3 nodes

    CPPUNIT_ASSERT(reader.hasMoreElements());
    reader.readPartial(map);

    CPPUNIT_ASSERT_EQUAL(3, (int)map->getNodes().size());
    CPPUNIT_ASSERT_EQUAL(0, (int)map->getWays().size());
    CPPUNIT_ASSERT_EQUAL(0, (int)map->getRelations().size());

    NodePtr node = map->getNode(1);
    HOOT_STR_EQUALS(Status::Unknown1, node->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)1, node->getId());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getX());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getY());
    CPPUNIT_ASSERT_EQUAL(10.0, node->getCircularError());
    CPPUNIT_ASSERT_EQUAL(1, node->getTags().size());
    HOOT_STR_EQUALS("1", node->getTags().get(MetadataTags::HootId()));

    node = map->getNode(2);
    HOOT_STR_EQUALS(Status::Unknown2, node->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)2, node->getId());
    CPPUNIT_ASSERT_EQUAL(0.1, node->getX());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getY());
    CPPUNIT_ASSERT_EQUAL(11.0, node->getCircularError());
    CPPUNIT_ASSERT_EQUAL(2, node->getTags().size());
    HOOT_STR_EQUALS("n2b", node->getTags().get("noteb"));
    HOOT_STR_EQUALS("2", node->getTags().get(MetadataTags::HootId()));

    node = map->getNode(3);
    HOOT_STR_EQUALS(Status::Conflated, node->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)3, node->getId());
    CPPUNIT_ASSERT_EQUAL(0.2, node->getX());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getY());
    CPPUNIT_ASSERT_EQUAL(12.0, node->getCircularError());
    CPPUNIT_ASSERT_EQUAL(2, node->getTags().size());
    HOOT_STR_EQUALS("n3", node->getTags().get("note"));
    HOOT_STR_EQUALS("3", node->getTags().get(MetadataTags::HootId()));

    ctr++;

    //2 nodes, 1 way

    map.reset(new OsmMap());
    CPPUNIT_ASSERT(reader.hasMoreElements());
    reader.readPartial(map);
    CPPUNIT_ASSERT_EQUAL(2, (int)map->getNodes().size());
    CPPUNIT_ASSERT_EQUAL(1, (int)map->getWays().size());
    CPPUNIT_ASSERT_EQUAL(0, (int)map->getRelations().size());

    node = map->getNode(4);
    HOOT_STR_EQUALS(Status::Conflated, node->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)4, node->getId());
    CPPUNIT_ASSERT_EQUAL(0.3, node->getX());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getY());
    CPPUNIT_ASSERT_EQUAL(13.0, node->getCircularError());
    CPPUNIT_ASSERT_EQUAL(2, node->getTags().size());
    HOOT_STR_EQUALS("n4", node->getTags().get("note"));
    HOOT_STR_EQUALS("4", node->getTags().get(MetadataTags::HootId()));

    node = map->getNode(5);
    HOOT_STR_EQUALS(Status::Invalid, node->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)5, node->getId());
    CPPUNIT_ASSERT_EQUAL(0.4, node->getX());
    CPPUNIT_ASSERT_EQUAL(0.0, node->getY());
    CPPUNIT_ASSERT_EQUAL(14.0, node->getCircularError());
    CPPUNIT_ASSERT_EQUAL(1, node->getTags().size());
    HOOT_STR_EQUALS("5", node->getTags().get(MetadataTags::HootId()));

    WayPtr way = map->getWay(1);
    HOOT_STR_EQUALS(Status::Unknown1, way->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)1, way->getId());
    CPPUNIT_ASSERT_EQUAL(15.0, way->getCircularError());
    CPPUNIT_ASSERT(way->hasNode(1));
    CPPUNIT_ASSERT(way->hasNode(2));
    CPPUNIT_ASSERT_EQUAL(2, way->getTags().size());
    HOOT_STR_EQUALS("w1b", way->getTags().get("noteb"));
    HOOT_STR_EQUALS("1", way->getTags().get(MetadataTags::HootId()));

    ctr++;

    //2 ways, 1 relation

    map.reset(new OsmMap());
    CPPUNIT_ASSERT(reader.hasMoreElements());
    reader.readPartial(map);
    CPPUNIT_ASSERT_EQUAL(0, (int)map->getNodes().size());
    CPPUNIT_ASSERT_EQUAL(2, (int)map->getWays().size());
    CPPUNIT_ASSERT_EQUAL(1, (int)map->getRelations().size());

    way = map->getWay(2);
    HOOT_STR_EQUALS(Status::Unknown2, way->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)2, way->getId());
    CPPUNIT_ASSERT_EQUAL(16.0, way->getCircularError());
    CPPUNIT_ASSERT(way->hasNode(2));
    CPPUNIT_ASSERT(way->hasNode(3));
    CPPUNIT_ASSERT_EQUAL(2, way->getTags().size());
    HOOT_STR_EQUALS("w2", way->getTags().get("note"));
    HOOT_STR_EQUALS("2", way->getTags().get(MetadataTags::HootId()));

    way = map->getWay(3);
    HOOT_STR_EQUALS(Status::Unknown2, way->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)3, way->getId());
    CPPUNIT_ASSERT_EQUAL(17.0, way->getCircularError());
    CPPUNIT_ASSERT(way->hasNode(2));
    CPPUNIT_ASSERT_EQUAL(1, way->getTags().size());
    HOOT_STR_EQUALS("3", way->getTags().get(MetadataTags::HootId()));

    RelationPtr relation = map->getRelation(1);
    HOOT_STR_EQUALS(Status::Unknown1, relation->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)1, relation->getId());
    CPPUNIT_ASSERT_EQUAL(18.1, relation->getCircularError());
    HOOT_STR_EQUALS(MetadataTags::RelationCollection(), relation->getType());
    CPPUNIT_ASSERT_EQUAL(size_t(2), relation->getMembers().size());
    CPPUNIT_ASSERT(relation->contains(ElementId::node(1)));
    CPPUNIT_ASSERT(relation->contains(ElementId::way(1)));
    RelationData::Entry member = relation->getMembers().at(0);
    HOOT_STR_EQUALS("n1", member.role);
    CPPUNIT_ASSERT_EQUAL((long)1, member.getElementId().getId());
    member = relation->getMembers().at(1);
    HOOT_STR_EQUALS("w1", member.role);
    CPPUNIT_ASSERT_EQUAL((long)1, member.getElementId().getId());
    CPPUNIT_ASSERT_EQUAL(2, relation->getTags().size());
    HOOT_STR_EQUALS("r1", relation->getTags().get("note"));
    HOOT_STR_EQUALS("1", relation->getTags().get(MetadataTags::HootId()));

    ctr++;

    //1 relation

    map.reset(new OsmMap());
    CPPUNIT_ASSERT(reader.hasMoreElements());
    reader.readPartial(map);
    CPPUNIT_ASSERT_EQUAL(0, (int)map->getNodes().size());
    CPPUNIT_ASSERT_EQUAL(0, (int)map->getWays().size());
    CPPUNIT_ASSERT_EQUAL(1, (int)map->getRelations().size());

    relation = map->getRelation(2);
    HOOT_STR_EQUALS(Status::Unknown1, relation->getStatus().getEnum());
    CPPUNIT_ASSERT_EQUAL((long)2, relation->getId());
    CPPUNIT_ASSERT_EQUAL(15.0, relation->getCircularError());
    HOOT_STR_EQUALS("", relation->getType());
    CPPUNIT_ASSERT(relation->contains(ElementId::node(2)));
    CPPUNIT_ASSERT_EQUAL(size_t(1), relation->getMembers().size());
    member = relation->getMembers().at(0);
    HOOT_STR_EQUALS("n2", member.role);
    CPPUNIT_ASSERT_EQUAL((long)2, member.getElementId().getId());
    CPPUNIT_ASSERT_EQUAL(1, relation->getTags().size());
    HOOT_STR_EQUALS("2", relation->getTags().get(MetadataTags::HootId()));

    ctr++;

    CPPUNIT_ASSERT(!reader.hasMoreElements());
    reader.finalizePartial();

    CPPUNIT_ASSERT_EQUAL(4, ctr);
  }

  void runReadByBoundsTest()
  {
    setUpTest("runReadByBoundsTest");
    mapId = insertDataForBoundTest();

    HootApiDbReader reader;
    reader.setUserEmail(userEmail());
    OsmMapPtr map(new OsmMap());
    reader.open(ServicesDbTestUtils::getDbReadUrl(mapId).toString());

    reader.setBoundingBox("-88.1,28.91,-88.0,28.89");
    reader.read(map);

    //quick check to see if the element counts are off...consult the test output for more detail

    //See explanations for these assertions in ServiceOsmApiDbReaderTest::runReadByBoundsTest
    //(exact same input data)
    CPPUNIT_ASSERT_EQUAL(5, (int)map->getNodes().size());
    CPPUNIT_ASSERT_EQUAL(2, (int)map->getWays().size());
    CPPUNIT_ASSERT_EQUAL(2, (int)map->getRelations().size());

    //We need to drop to set all the element changeset tags here to empty, which will cause them
    //to be dropped from the file output.  If they aren't dropped, they will increment with each
    //test and cause the output comparison to fail.
    QList<ElementAttributeType> types;
    types.append(ElementAttributeType(ElementAttributeType::Changeset));
    types.append(ElementAttributeType(ElementAttributeType::Timestamp));
    RemoveAttributesVisitor attrVis(types);
    map->visitRw(attrVis);

    MapProjector::projectToWgs84(map);

    OsmXmlWriter writer;
    writer.setIncludeCompatibilityTags(false);
    writer.write(
      map, "test-output/io/ServiceHootApiDbReaderTest/runReadByBoundsTestOutput.osm");
    HOOT_STR_EQUALS(
      TestUtils::readFile("test-files/io/ServiceHootApiDbReaderTest/runReadByBoundsTestOutput.osm"),
      TestUtils::readFile("test-output/io/ServiceHootApiDbReaderTest/runReadByBoundsTestOutput.osm"));

    //just want to make sure I can read against the same data twice in a row w/o crashing and also
    //make sure I don't get the same result again for a different bounds
    reader.setBoundingBox("-1,-1,1,1");
    map.reset(new OsmMap());
    reader.read(map);

    CPPUNIT_ASSERT_EQUAL(0, (int)map->getNodes().size());
    CPPUNIT_ASSERT_EQUAL(0, (int)map->getWays().size());
    CPPUNIT_ASSERT_EQUAL(0, (int)map->getRelations().size());

    reader.close();
  }

  void runAccessPublicMapWithoutEmailTest()
  {
    setUpTest("runAccessPublicMapWithoutEmailTest");
    mapId = populateMap(true, true);

    // Even though we have no configured user, the map is public, so we should be able to read it.

    HootApiDbReader reader;
    reader.setUserEmail("");
    OsmMapPtr map(new OsmMap());
    reader.open(ServicesDbTestUtils::getDbReadUrl(mapId).toString());
    reader.read(map);
    verifyFullReadOutput(map);
    reader.close();
  }

  void runInvalidUserTest()
  {
    setUpTest("runInvalidUserTest");
    // Write a private map.
    mapId = populateMap(true, false);

    // Create a user different than the one who wrote the map.
    HootApiDb db;
    db.open(ServicesDbTestUtils::getDbModifyUrl(testName).toString());
    const QString differentUserEmail = "blah@blah.com";
    const long differentUserId = db.insertUser(differentUserEmail, differentUserEmail);

    // Set the reader up with the different user.
    HootApiDbReader reader;
    reader.setUserEmail(differentUserEmail);

    // The different user should not be able to access the other user's private map.
    QString exceptionMsg("");
    try
    {
      reader.open(ServicesDbTestUtils::getDbReadUrl(mapId).toString());
    }
    catch (const HootException& e)
    {
      exceptionMsg = e.what();
      reader.close();
    }
    LOG_VARD(exceptionMsg);
    CPPUNIT_ASSERT(exceptionMsg.contains("access to map with ID"));

    db.deleteUser(differentUserId);
    db.close();
  }

  void runAccessPrivateMapWithoutEmailTest()
  {
    setUpTest("runAccessPrivateMapWithoutEmailTest");
    // Write a private map.
    mapId = populateMap(true, false);

    // Configure no user with the reader.
    HootApiDbReader reader;
    reader.setUserEmail("");

    // The reader should not be able to read the private map without a configured user.
    QString exceptionMsg("");
    try
    {
      reader.open(ServicesDbTestUtils::getDbReadUrl(mapId).toString());
    }
    catch (const HootException& e)
    {
      exceptionMsg = e.what();
      reader.close();
    }
    LOG_VARD(exceptionMsg);
    CPPUNIT_ASSERT(exceptionMsg.contains("not available for public"));
  }

  void runMultipleMapsSameNameSameUserTest()
  {
    setUpTest("runMultipleMapsSameNameSameUserTest");
    // Create a map.
    mapId = populateMap();

    // Create another map with the same name.
    const long differentMapId = populateMap();

    // Try to access the map by name (not ID).
    HootApiDbReader reader;
    reader.setUserEmail(userEmail());
    QString url = ServicesDbTestUtils::getDbReadUrl(mapId).toString();
    url = url.replace(QString::number(mapId), testName);

    // Access should fail, since there are multiple maps created by this user with the same name.

    // This restriction may be found to be too strict at some point in the future, but this
    // particular logic has been in here a long and this test simply verifies the behavior.

    QString exceptionMsg("");
    try
    {
      reader.open(url);
    }
    catch (const HootException& e)
    {
      exceptionMsg = e.what();
      reader.close();
    }
    LOG_VARD(exceptionMsg);
    HOOT_STR_EQUALS(
      "Expected 1 map with the name: " + testName + " but found %2 maps.", exceptionMsg);

    HootApiDb db;
    db.open(ServicesDbTestUtils::getDbModifyUrl(testName).toString());
    db.deleteMap(differentMapId);
    db.close();
  }

  void runMultipleMapsSameNameDifferentUsersTest()
  {
    setUpTest("runMultipleMapsSameNameDifferentUsersTest");
    // Create a map.
    mapId = populateMap();

    // Create a user different than the one who wrote the map.
    HootApiDb db;
    db.open(ServicesDbTestUtils::getDbModifyUrl(testName).toString());
    const QString differentUserEmail = "blah@blah.com";
    const long differentUserId = db.insertUser(differentUserEmail, differentUserEmail);

    // Write a map for the different user with the same name as the original map.
    // TODO: delete
    HootApiDbWriter writer;
    writer.setUserEmail(differentUserEmail);
    writer.setRemap(false);
    writer.setIncludeDebug(true);
    writer.open(ServicesDbTestUtils::getDbModifyUrl(testName).toString());
    writer.write(ServicesDbTestUtils::createServiceTestMap());
    writer.close();

    // Configure the reader for the original user, and we should be able to read out the map.
    // There are two maps with the same name, but they are owned by different users.
    HootApiDbReader reader;
    reader.setUserEmail(userEmail());
    QString url = ServicesDbTestUtils::getDbReadUrl(mapId).toString();
    url = url.replace(QString::number(mapId), testName);
    reader.open(url);
    OsmMapPtr map(new OsmMap());
    reader.read(map);
    verifyFullReadOutput(map);
    reader.close();

    // One of the maps is still left.
    db.deleteMap(db.getMapIdByName(testName));

    db.deleteUser(differentUserId);
    db.close();
  }

  void runMultipleMapsSameNameNoUserTest()
  {
    setUpTest("runMultipleMapsSameNameNoUserTest");
    // Create a map.
    mapId = populateMap();

    // Create another map with the same name and original user.
    // TODO: delete
    const long differentMapId = populateMap();

    // Configure the reader with no user.
    HootApiDbReader reader;
    reader.setUserEmail("");
    QString url = ServicesDbTestUtils::getDbReadUrl(mapId).toString();
    url = url.replace(QString::number(mapId), testName);

    // Reading should fail, since there are multiple maps with the same name.  See related note in
    // runMultipleMapsSameNameSameUserTest.
    QString exceptionMsg("");
    try
    {
      reader.open(url);
    }
    catch (const HootException& e)
    {
      exceptionMsg = e.what();
      reader.close();
    }
    LOG_VARD(exceptionMsg);
    HOOT_STR_EQUALS(
      "Expected 1 map with the name: " + testName + " but found %2 maps.", exceptionMsg);

    HootApiDb db;
    db.open(ServicesDbTestUtils::getDbModifyUrl(testName).toString());
    db.deleteMap(differentMapId);
    db.close();
  }
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ServiceHootApiDbReaderTest, "slow");

}
