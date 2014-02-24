/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Universidad de Cantabria
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
 *		   Ramón Agüero Calvo <ramon@tlmat.unican.es>
 */

#include "scratch-logging.h"				//Set the LOGGING options
#include "ns3/scenario-creator-module.h"
#include "ns3/simulation-singleton.h"
#include <ctime>

using namespace ns3;
using namespace std;

u_int32_t GetNumberOfSimulations (string fileName);

/**
 * Simple script to test the scenario-creator handler. User only need the following stuff:
 * 		- Raw file name where we have stored the scenario configuration values. NOTE: it MUST have the ".config" extension, but MUST NOT be included in the variable declaration.
 *
 * To run the script, just prompt a command similar to this one: ./waf --run "scratch/test-scenario"
 * You can init every attribute you want at the command line as well, for instance: ./waf --run "scratch/test-scenario --ns3::OnOffApplication::DataRate=11Mbps"
 *
 * Please refer to the scenario-creator module documentation to get a quick overview of its possibilities
 *
 * ENJOY!!
 */

int main (int argc, char *argv[])
{
	CommandLineParser parser;		//Advanced attributed (i.e. fer, run offset, etc.)
	clock_t begin, end;
	char output [FILENAME_MAX];

	//Configuration file
	string configuration = "network-coding-scenario";

	//Random variable generation (Random seed)
	SeedManager::SetSeed (3);

	//Activate the logging  (from the library scratch-logging.h, just modify there those LOGGERS as wanted)
	EnableLogging ();

	for (u_int32_t runCounter = 1; runCounter <= GetNumberOfSimulations (configuration); runCounter ++)
	{
		begin = clock();

		//Create the scenario (auto-configured by the ConfigureScenario object)
		SimulationSingleton <ConfigureScenario>::Get ()->ParseConfigurationFile (configuration);
		SimulationSingleton <ConfigureScenario>::Get ()->SetAttributes();

		//Parse the command line options
		parser.Parse(argc, argv);

		//Change the seed for each simulation run
		SimulationSingleton <ConfigureScenario>::Get ()->m_propTracing->GetTraceInfo().run = runCounter +
				SimulationSingleton <ConfigureScenario>::Get ()->m_propTracing->GetTraceInfo().runOffset;
		SeedManager::SetRun (SimulationSingleton <ConfigureScenario>::Get ()->m_propTracing->GetTraceInfo().run);

		//Initialize the scenario
		SimulationSingleton <ConfigureScenario>::Get ()->Init ();

		//Run the simulation
		Simulator::Stop (Seconds (1000.0));
		Simulator::Run ();
		end = clock ();

		//Print the duration of the simulation
		sprintf(output, "[%04.5f sec] - ", (double) (end - begin) / CLOCKS_PER_SEC);
		printf("%s", output);

		//Print the statistics and accordingly close the trace files
		SimulationSingleton <ConfigureScenario>::Get ()->m_propTracing->PrintStatistics();
		Simulator::Destroy ();

	} // end for

	return 0;
} 	//end main

/**
 * Read the configuration file in order to get the number of simulations to create the main loop
 */
u_int32_t GetNumberOfSimulations (string fileName)
{
	ConfigurationFile config;
	string temp;
	config.LoadConfig (config.SetConfigFileName("/src/scenario-creator/config/", fileName));
	config.GetKeyValue("SCENARIO", "RUN", temp);

	return (u_int32_t) atoi (temp.c_str());
}
