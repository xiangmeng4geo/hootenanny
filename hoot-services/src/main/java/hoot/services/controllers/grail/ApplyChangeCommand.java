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
 * @copyright Copyright (C) 2016, 2017, 2018 DigitalGlobe (http://www.digitalglobe.com/)
 */
package hoot.services.controllers.grail;

import java.io.File;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import hoot.services.command.CommandResult;
import hoot.services.command.ExternalCommand;
import hoot.services.geo.BoundingBox;


class ApplyChangesetCommand extends ExternalCommand {
    private static final Logger logger = LoggerFactory.getLogger(RunDiffCommand.class);

    ApplyChangesetCommand(String jobId, File input, String apiUrl, String userName, String debugLevel, Class<?> caller) {
        super(jobId);

        logger.info("Starting to push the change diff");

        // String comment = "-D changeset.description='Hootenanny differential changeset generated by " + userName + "'";
        
        List<String> options = new LinkedList<>();
        options.add("changeset.description=\"Hootenanny differential changeset generated by " + userName + "\"");

        List<String> hootOptions = toHootOptions(options);

        Map<String, Object> substitutionMap = new HashMap<>();
        substitutionMap.put("INPUT", input.getAbsolutePath());
        substitutionMap.put("API_URL", apiUrl);
        substitutionMap.put("HOOT_OPTIONS", hootOptions);
        substitutionMap.put("DEBUG_LEVEL", debugLevel);

        // hoot changeset-apply -D changeset.description='Raw NZ OSM data' $x "http://test:hoottest@192.168.0.31:3000" 
        String command = "hoot changeset-apply --${DEBUG_LEVEL} ${HOOT_OPTIONS} ${INPUT} ${API_URL} --stats --progress";

        super.configureCommand(command, substitutionMap, caller);
    }
}
