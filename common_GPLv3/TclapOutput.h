/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 08.01.2020

  Copyright (C) 2020 Authors and www.dsp-crowd.com

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TCLAP_OUTPUT_H
#define TCLAP_OUTPUT_H

#include <tclap/CmdLine.h>

// http://tclap.sourceforge.net/
class TclapOutput : public TCLAP::StdOutput
{

public:

	virtual void usage(TCLAP::CmdLineInterface& c)
	{
		std::string tmp;

		std::cout << std::endl << dPackageName << std::endl;
		std::cout << "Application: " << dAppName << std::endl << std::endl;
		std::cout << "Usage: " << dAppName << " [OPTION]" << std::endl;
		std::cout << std::endl;
		std::cout << "Required" << std::endl;
		std::cout << std::endl;

		std::list<TCLAP::Arg*> args = c.getArgList();
		for (TCLAP::ArgListIterator it = args.begin(); it != args.end(); it++) {
			tmp = (*it)->longID();
			if ((*it)->isRequired() and tmp.find(",") != std::string::npos) {
				tmp.erase(2, tmp.find(",") - 2);
				std::cout << std::setw(2) << " ";
				std::cout << std::setw(35) << std::left << tmp;
				tmp = (*it)->getDescription();
				tmp.erase(0, 12);
				std::cout << tmp << std::endl;
			}
		}
		for (TCLAP::ArgListIterator it = args.begin(); it != args.end(); it++) {
			tmp = (*it)->longID();
			if ((*it)->isRequired() and tmp.find(",") == std::string::npos) {
				std::cout << std::setw(7) << " ";
				std::cout << std::setw(30) << std::left << tmp;
				tmp = (*it)->getDescription();
				tmp.erase(0, 12);
				std::cout << tmp << std::endl;
			}
		}

		std::cout << std::endl;
		std::cout << "Optional" << std::endl;
		std::cout << std::endl;

		for (TCLAP::ArgListIterator it = args.begin(); it != args.end(); it++) {
			tmp = (*it)->longID();
			if (!(*it)->isRequired() and tmp.find(",") != std::string::npos) {
				tmp.erase(2, tmp.find(",") - 2);
				std::cout << std::setw(2) << " ";
				std::cout << std::setw(35) << std::left << tmp;
				std::cout << (*it)->getDescription() << std::endl;
			}
		}
		for (TCLAP::ArgListIterator it = args.begin(); it != args.end(); it++) {
			tmp = (*it)->longID();
			if (!(*it)->isRequired() and tmp.find(",") == std::string::npos) {
				std::cout << std::setw(7) << " ";
				std::cout << std::setw(30) << std::left << tmp;
				std::cout << (*it)->getDescription() << std::endl;
			}
		}
		std::cout << std::endl;

		printAppCommands();
	}

private:

	virtual void printAppCommands()
	{
	}

};

#endif

