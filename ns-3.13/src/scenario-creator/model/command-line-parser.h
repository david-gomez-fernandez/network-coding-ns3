/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Universidad de Cantabria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: David Gómez Fernández <dgomez@tlmat.unican.es>
 *         Ramón Agüero Calvo <ramon@tlmat.unican.es>
 */


#ifndef COMMAND_LINE_PARSER_H_
#define COMMAND_LINE_PARSER_H_

#include "configure-scenario.h"

namespace ns3 {
/*
 * Helper class that will handle all those command-line prompted parameters which cannot
 * be directly configured through the attribute system. It is worth highlighting that
 * this class will be tightly linked to the ConfigureScenario singleton, since this object
 * will be invoked by it
 */
class CommandLineParser
{
public:
	/*
	 * Default constructor
	 */
	CommandLineParser ();
	/*
	 * Default destructor
	 */
	~CommandLineParser ();
	/*
	 * Parse the options from the command line
	 */
	void Parse (int argc, char *argv[]);

private:
	CommandLine m_cmd;
};
}  //End namespace ns3


namespace ns3 {

CommandLineParser::CommandLineParser ()
{
}

CommandLineParser::~CommandLineParser ()
{
}

void CommandLineParser::Parse (int argc, char *argv[])
{
	double fer = -1.0;
	u_int16_t runOffset = 0;

	m_cmd.AddValue ("Fer", "FER value", fer);
	m_cmd.AddValue ("RunOffset", "Run offset", runOffset);

	m_cmd.Parse(argc, argv);

	//Command-line options enabler
	if (fer != -1.0)
	{
		SimulationSingleton <ConfigureScenario>::Get ()->m_fer = fer;
		SimulationSingleton <ConfigureScenario>::Get ()->m_propTracing->GetTraceInfo().fer = fer;
	}

	if (runOffset)
	{
		SimulationSingleton <ConfigureScenario>::Get ()->m_propTracing->GetTraceInfo().runOffset = runOffset;
	}

}


}  //End namespace ns3

#endif /* COMMAND_LINE_PARSER_H_ */
