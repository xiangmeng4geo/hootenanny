#!/bin/bash
set -e

mkdir -p test-output/NonDestructiveTest/

# First run the Congo network conflation
hoot conflate -C Network.conf -D reader.conflate.use.data.source.ids.1=true \
 -D writer.include.debug.tags=true \
 test-files/Congo_MGCP_Roads_Bridges_subset.osm \
 test-files/Congo_OSM_Roads_Bridges_subset.osm \
 test-output/NonDestructiveTest/output.osm


# Derive the changeset for the Congo conflation
hoot changeset-derive --stats \
 test-files/Congo_MGCP_Roads_Bridges_subset.osm \
 test-output/NonDestructiveTest/output.osm \
 test-output/NonDestructiveTest/changeset.osc

# Check the output against the expected output
hoot diff \
  test-output/NonDestructiveTest/output.osm \
  test-files/NonDestructiveTest/Expected.osm || \
  diff \
  test-output/NonDestructiveTest/output.osm \
  test-files/NonDestructiveTest/Expected.osm

# Old stats
# ---------
#
# Data Counts
#
# |        | Node | Relation | Way |
# |  REF1  | 8126 |        1 |  82 |
# |  REF2  | 1032 |        0 | 218 |
#
# @b2ef060 - Baseline
#
# |        | Node | Relation | Way |
# | Create |  856 |        0 | 900 |
# | Delete |    2 |        1 |  59 |
# | Modify |    0 |        0 |   4 |
#
# @bb953fb - WaySplitter keep IDs
#
# |        | Node | Relation | Way |
# | Create |  859 |        0 | 860 |
# | Delete |    1 |        1 |  17 |
# | Modify |    0 |        0 |  45 |
#
# @ - WayJoiner checking tags
#
# |        | Node | Relation | Way |
# | Create |  859 |        0 | 670 |
# | Delete |    1 |        1 |  17 |
# | Modify |    0 |        0 |  33 |
#
# @ - WayJoiner no tag checking
#
# |        | Node | Relation | Way |
# | Create |  859 |        0 | 474 |
# | Delete |    1 |        1 |  17 |
# | Modify |    0 |        0 |  10 |
#
# @ - WayJoiner rejoin siblings
#
# |        | Node | Relation | Way | 
# | Create |  859 |        0 | 312 | 
# | Delete |    1 |        1 |  18 | 
# | Modify |    0 |        0 |   9 | 
#
# @ - WayJoiner join at node
#
# |        | Node | Relation | Way |
# | Create |  859 |        0 | 309 |
# | Delete |    1 |        1 |  17 |
# | Modify |    0 |        0 |   9 |
#
# @ - WaySplitter::createSplits change
#
# |        | Node | Relation | Way |
# | Create |  859 |        0 | 282 |
# | Delete |    1 |        1 |  17 |
# | Modify |    0 |        0 |   9 |
#

