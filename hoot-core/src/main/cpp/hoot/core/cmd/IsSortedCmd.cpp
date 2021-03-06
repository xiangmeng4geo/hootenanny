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
 * @copyright Copyright (C) 2015, 2016, 2017, 2018 DigitalGlobe (http://www.digitalglobe.com/)
 */

// Hoot
#include <hoot/core/util/Factory.h>
#include <hoot/core/cmd/BaseCommand.h>
#include <hoot/core/io/OsmPbfReader.h>
#include <hoot/core/io/OsmMapReaderFactory.h>
#include <hoot/core/visitors/IsSortedVisitor.h>

// Qt
#include <QFile>
#include <QFileInfo>

namespace hoot
{

class IsSortedCmd : public BaseCommand
{
public:

  static std::string className() { return "hoot::IsSortedCmd"; }

  IsSortedCmd() {}

  virtual QString getName() const { return "is-sorted"; }

  virtual QString getDescription() const
  { return "Determines if a map is sorted to the OSM standard"; }

  int runSimple(QStringList args)
  {
    if (args.size() != 1)
    {
      std::cout << getHelp() << std::endl << std::endl;
      throw HootException(QString("%1 takes one parameter.").arg(getName()));
    }

    const QString input = args[0];
    QFileInfo fileInfo(input);
    if (!fileInfo.exists())
    {
      throw HootException("Specified input: " + input + " does not exist.");
    }

    bool result = true;
    if (OsmPbfReader().isSupported(input))
    {
      result = OsmPbfReader().isSorted(input);
    }
    else
    {
      boost::shared_ptr<PartialOsmMapReader> reader =
        boost::dynamic_pointer_cast<PartialOsmMapReader>(
          OsmMapReaderFactory::getInstance().createReader(input));
      reader->setUseDataSourceIds(true);
      reader->open(input);
      reader->initializePartial();

      IsSortedVisitor vis;
      while (reader->hasMoreElements())
      {
        ElementPtr element = reader->readNextElement();
        if (element)
        {
          vis.visit(element);
          if (!vis.getIsSorted())
          {
            result = false;
            LOG_VART(result);
            break;
          }
        }
      }

      reader->finalizePartial();
      reader->close();
    }

    if (result)
    {
      std::cout << input << " is sorted." << std::endl;
    }
    else
    {
      std::cout << input << " is not sorted." << std::endl;
    }

    return 0;
  }
};

HOOT_FACTORY_REGISTER(Command, IsSortedCmd)

}
