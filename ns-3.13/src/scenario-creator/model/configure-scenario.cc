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

#include "configure-scenario.h"
#include "network-monitor.h"

//Network Coding implementation
#include "ns3/network-coding-helper.h"
#include "ns3/network-coding-l4-protocol.h"
#include "ns3/inter-flow-network-coding-protocol.h"
#include "ns3/intra-flow-network-coding-protocol.h"

#include <sstream>
#include <stdio.h>
#include <sys/types.h>
#include <assert.h>

using namespace ns3;
using namespace std;

//const bool g_debug = true;  							//Temporal solution (only for debugging)

NS_LOG_COMPONENT_DEFINE("ConfigureScenario");
NS_OBJECT_ENSURE_REGISTERED(ConfigureScenario);


ConfigureScenario::ConfigureScenario()
{
    NS_LOG_FUNCTION(this);
    if (m_nodesVector.size())
        m_nodesVector.clear();
    if (m_channelFer.size())
        m_channelFer.clear();

//    m_scenarioObjectContainer = CreateObject<ScenarioObjectContainer > ();
    m_configurationFile = CreateObject<ConfigurationFile> ();
    m_deployment = FILE_BASED;
    m_simulationChannel = SIM_RATE_ERROR;
    m_routingProtocol = RT_OLSR_PROTOCOL;
    m_transportProtocol = TCP_PROTOCOL;
    m_fer = 0.0;
    m_ferStatic = 0.0;
    m_propTracing = CreateObject<ProprietaryTracing > ();
    m_nodesNumber = 0;
    m_distance = 0.0;

    m_scriptedBufferConfiguration = false;
    m_scriptedAckBufferConfiguration = false;

}

ConfigureScenario::~ConfigureScenario()
{
    NS_LOG_FUNCTION(this);

    if (m_nodesVector.size())
    {
        m_nodesVector.clear();
    }
    if (m_channelFer.size())
    {
        m_channelFer.clear();
    }

    //Reset the NetworkMonitor
    NetworkMonitor::Instance().Reset();

}

TypeId
ConfigureScenario::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::ConfigureScenario")
            .SetParent<Object > ()
            .AddConstructor<ConfigureScenario > ()
    		.AddAttribute ("Fer",
    				"FER value used to configure the links",
    				DoubleValue (0.0),
    				MakeDoubleAccessor (&ConfigureScenario::m_fer),
    				MakeDoubleChecker <double>())
    		;
    return tid;
}

bool ConfigureScenario::ParseConfigurationFile(string confFile)
{
    NS_LOG_FUNCTION_NOARGS();
    string value;

    assert (m_configurationFile->LoadConfig(m_configurationFile->SetConfigFileName("/src/scenario-creator/config/", confFile)) != -1);

    //Get the scenario configuration from the file
    assert(m_configurationFile->GetKeyValue("SCENARIO", "RUN", value) >= 0);
    m_propTracing->GetTraceInfo().totalRuns = atoi(value.c_str());
    assert(m_configurationFile->GetKeyValue("SCENARIO", "RUN_OFFSET", value) >= 0);
    m_propTracing->GetTraceInfo().runOffset = atoi(value.c_str());

    assert(m_configurationFile->GetKeyValue("SCENARIO", "PACKET_LENGTH", value) >= 0);
    m_propTracing->GetTraceInfo().packetLength = atoi(value.c_str());
    assert(m_configurationFile->GetKeyValue("SCENARIO", "FER", value) >= 0);
    m_fer = atof(value.c_str());
    m_propTracing->GetTraceInfo().fer = m_fer;

    assert(m_configurationFile->GetKeyValue("SCENARIO", "FER_STATIC", value) >= 0);
    m_ferStatic = atof(value.c_str());

    //Get the stack configuration from the file
    assert (m_configurationFile->GetKeyValue("STACK", "TRANSPORT_PROTOCOL", value) >= 0);

    if (!value.compare("TCP"))
    {
        m_transportProtocol = TCP_PROTOCOL;
        m_propTracing->GetTraceInfo().transport = "TCP";
    }
    else if (!value.compare("UDP"))
    {
        m_transportProtocol = UDP_PROTOCOL;
        m_propTracing->GetTraceInfo().transport = "UDP";
    }
    else if (!value.compare("NSC"))
    {
        m_transportProtocol = NSC_TCP_PROTOCOL;
        m_propTracing->GetTraceInfo().transport = "NSC";
    }
    else
    {
    	NS_ABORT_MSG("Incorrect transport protocol " << value << " . Please fix the configuration file");
    }

    //New routing protocols parsing
    assert (m_configurationFile->GetKeyValue("STACK", "ROUTING_PROTOCOL", value) >= 0);
    if (!value.compare("POPULATE"))
    {
    	m_routingProtocol = RT_POPULATE_ROUTING_TABLES;
    }
    else if (!value.compare("STATIC"))
    {
    	m_routingProtocol = RT_STATIC_ROUTING_PROTOCOL;
    }
    else if (!value.compare("STATIC_GRAPH"))
    {
    	m_routingProtocol = RT_STATIC_GRAPH_ROUTING_PROTOCOL;
    }
    else if (!value.compare("OLSR"))
    {
    	m_routingProtocol = RT_OLSR_PROTOCOL;
    }
    else if (!value.compare("AODV"))
    {
    	m_routingProtocol = RT_AODV_PROTOCOL;
    }
    else
    {
    	NS_ABORT_MSG("Incorrect routing protocol. Please fix the configuration file");
    }

    assert(m_configurationFile->GetKeyValue("STACK", "PROPAGATION_LOSS_MODEL", value) >= 0);
    if (!value.compare("RATE"))
    {
        m_simulationChannel = SIM_RATE_ERROR;
        m_propTracing->GetTraceInfo().channel = "RATE";
    }
    else if (!value.compare("DEFAULT"))
    {
        m_simulationChannel = SIM_DEFAULT_MODEL;
        m_propTracing->GetTraceInfo().channel = "DEFAULT";
    }
    else if (!value.compare("HMM"))
    {
        m_simulationChannel = SIM_HMM_MODEL;
        m_propTracing->GetTraceInfo().channel = "HMM";
    }
    else if (!value.compare("BEAR"))
    {
        m_simulationChannel = SIM_BEAR_MODEL;
        m_propTracing->GetTraceInfo().channel = "BEAR";
    }
    else if (!value.compare("MANUAL"))
    {
    	m_simulationChannel = SIM_MANUAL_MODEL;
    	m_propTracing->GetTraceInfo().channel = "MANUAL";
    }
    else if (!value.compare("SIMPLE"))
    {
    	m_simulationChannel = SIM_SIMPLE_MODEL;
    	m_propTracing->GetTraceInfo().channel = "SIMPLE";
    }
    else
    {
        NS_ABORT_MSG("Incorrect channel model. " << value << " Please fix the configuration file");
    }

    assert(m_configurationFile->GetKeyValue ("STACK", "NODE_DEPLOYMENT", value) >= 0);
    if (!value.compare ("CODE")) //Nodes deployed though the code... To be implemented
    {
        m_deployment = CODE_BASED;
        m_propTracing->GetTraceInfo().deployment += "CODE_BASED_";
    }
    else if (!value.compare ("FILE")) //Nodes deployed by means of an extern file. In this case, we will read the needed files and call the method which will create the corresponding scenario.
    {
        string scenarioConfig, channelConfig, temp;
        m_deployment = FILE_BASED;
        assert (m_configurationFile->GetKeyValue ("STACK", "SCENARIO_DESCRIPTION", scenarioConfig) >= 0);
        assert (m_configurationFile->GetKeyValue ("STACK", "CHANNEL_CONFIGURATION", channelConfig) >= 0);
        ParseScenarioDescriptionFile (scenarioConfig, channelConfig);

        //Handle scenario description file name to compose the trace file name
        temp = scenarioConfig;

        assert (temp.find("-scenario.conf"));
        temp = temp.erase(temp.find("-scenario.conf"));
        for (std::string::iterator p = temp.begin(); temp.end() != p; ++p)
        {
        	*p = std::toupper((unsigned char) *p);
        }
        m_propTracing->GetTraceInfo().deployment = temp;

    }
    else if (!value.compare ("RANDOM"))
    {
        int16_t nodesNumber, dataFlows;
        double maxX, maxY;
        m_deployment = RANDOM_DEPLOYMENT;
        m_propTracing->GetTraceInfo().deployment = "RANDOM_";

        assert (m_configurationFile->GetKeyValue ("STACK", "NODES_NUMBER", value) >= 0);
        nodesNumber = atoi(value.c_str());
        m_propTracing->GetTraceInfo().deployment += value;

        assert (m_configurationFile->GetKeyValue ("STACK", "MAX_X", value) >= 0);
        maxX = atof(value.c_str());
        m_propTracing->GetTraceInfo().deployment += value;

        assert (m_configurationFile->GetKeyValue ("STACK", "MAX_Y", value) >= 0);
        maxY = atof(value.c_str());
        m_propTracing->GetTraceInfo().deployment += value;

        assert (m_configurationFile->GetKeyValue ("STACK", "DATA_FLOWS", value) >= 0);
        dataFlows = atoi(value.c_str());


        GenerateRandomScenario (nodesNumber, maxX, maxY, dataFlows);
    }
    else if (!value.compare ("LINE"))
    {
    	m_deployment = LINE_TOPOLOGY;
    	m_propTracing->GetTraceInfo().deployment = "LINE_";

    	if (!m_nodesNumber)
    	{
    		assert (m_configurationFile->GetKeyValue("STACK", "NODES_NUMBER", value) >= 0);
    		m_nodesNumber = atoi(value.c_str());
    		m_propTracing->GetTraceInfo().deployment += value + "_";
    	}
    	if (!m_distance)
    	{
    		assert (m_configurationFile->GetKeyValue("STACK", "DISTANCE", value) >= 0);
    		m_distance = atoi(value.c_str());
    		m_propTracing->GetTraceInfo().deployment += "DISTANCE_" + value + "_";
    	}

    	GenerateLineTopology ();
    }
    return true;
}

bool ConfigureScenario::ParseScenarioDescriptionFile (string confFile, string channelFile)
{
    NS_LOG_FUNCTION_NOARGS();
    string pathFile = "/src/scenario-creator/scenarios/";
    string lineString;
    fstream scenarioConfFile, scenarioChannelFile;
    //File handle variables
    int i;
    char cwdBuf [FILENAME_MAX];
    char line[256];
    u_int8_t lineNumber = 0;
    u_int8_t ferValue;
    vector<u_int8_t> ferVector;
    NodeDescription_t *nodeDescriptor;

    confFile = std::string(getcwd(cwdBuf, FILENAME_MAX)) + pathFile + confFile;
    channelFile = std::string(getcwd(cwdBuf, FILENAME_MAX)) + pathFile + channelFile;

    NS_LOG_DEBUG("Node description file: " << confFile << "\nChannel description file: " << channelFile);

    scenarioConfFile.open((const char *) confFile.c_str(), ios::in);
    scenarioChannelFile.open((const char *) channelFile.c_str(), ios::in);

    //Parsing the scenario description (deployment of the nodes) from the configuration file
    //File format
    //#No.	X	Y	Z	TX	RX	RT	CN
    //  1	0	0	0	 6	 0 	 0	 1

    NS_ASSERT_MSG(scenarioConfFile, "File (Channel FER file) " << channelFile << " not found: Please fix");

    while (scenarioConfFile.getline(line, 256))
    {
        lineString = string(line);
        if ((lineString.find('#') == string::npos) || (lineString.find('#') != 0)) //Ignore those lines which begins with the '#' character at its beginning
        {
            int nodeId, destNodeId, receiver, codingRouter, forwarder;
            float xCoord, yCoord, zCoord;

            sscanf(lineString.c_str(), "%d %f %f %f %d %d %d %d", &nodeId, &xCoord, &yCoord, &zCoord, &destNodeId, &receiver, &codingRouter, &forwarder);

            //Search for the node ID into the vector; if it is not found, create the element; otherwise, just add the new flow
            if (m_nodesVector.size()) //No elements --> Create add the first one
            {
                bool found = false;
                //Check if the node is already located into the vector; if so, just add the new flow destination

                for (i = 0; i < (int) m_nodesVector.size(); i++)
                {
                    if (nodeId - 1 == (int) m_nodesVector[i].nodeId) //If the node is repeated, we only update the destination container
                    {
                        found = true;
                        m_nodesVector[i].destinations.insert(destNodeId - 1);
                        continue;
                    }
                }

                if (found)
                {
                    continue;
                }
                else
                {
                    goto insert_element;
                }

            }
            else
            {
                goto insert_element;
            }

insert_element:
            //If the node ID is brand new information, we will create a NodeDescription_t object to store the corresponding information

            nodeDescriptor = new NodeDescription_t;
            nodeDescriptor->nodeId = (u_int8_t) nodeId - 1;

            nodeDescriptor->coordinates.x = xCoord;
            nodeDescriptor->coordinates.y = yCoord;
            nodeDescriptor->coordinates.z = zCoord;
            if (destNodeId)
            {
                nodeDescriptor->transmitter = true;
                nodeDescriptor->destNodeId = destNodeId - 1;
                nodeDescriptor->destinations.insert(destNodeId - 1);
            }
            else
            {
                nodeDescriptor->transmitter = false;
                nodeDescriptor->destNodeId = 0;
            }
            nodeDescriptor->receiver = receiver;
            nodeDescriptor->codingRouter = codingRouter;
            nodeDescriptor->forwarder = forwarder;
            m_nodesVector.push_back(*nodeDescriptor);
            delete nodeDescriptor;

        }
    }

    //Parsing the channel FER configuration from the file for every channel link

    NS_ASSERT_MSG(scenarioChannelFile, "File (Channel FER file) " << channelFile << " not found: Please fix");

    while (scenarioChannelFile.getline(line, 256))
    {
        lineString = string(line);
        if ((lineString.find('#') == string::npos) || (lineString.find('#') != 0)) //Ignore those lines which begins with the '#' character at its beginning
        {
            for (i = 0; i < (int) lineString.size(); i++)
            {
                if (line [i] != ' ' && line [i] != '\t')
                {
                    ferValue = atoi(line + i);
                    // The FER value must be within the interval [0,10]
                    NS_ASSERT_MSG(ferValue <= 10.0, "All the FER values must be within [0,1]");
                    ferVector.push_back(ferValue);
                }
            }
            m_channelFer.insert(pair <int, vector <u_int8_t> > (lineNumber, ferVector));
            lineNumber++;
            ferVector.clear();
        }
    }

    m_nodesNumber = m_nodesVector.size();

    //DEBUGGING
#ifdef NS3_LOG_ENABLE
    if (g_debug)
    {
        char message[255];
        printf("---Deployment of nodes over the scenario---\n");
        //Show nodes deployment and functionalities
        for (i = 0; i < (int) m_nodesVector.size(); i++)
        {
            sprintf(message, "Node %2d: [%3.1f, %3.1f, %3.1f]  TX:%2d  RX:%1d  RT:%1d  CN:%1d",   						\
					m_nodesVector[i].nodeId, m_nodesVector[i].coordinates.x, m_nodesVector[i].coordinates.y,            \
					m_nodesVector[i].coordinates.z, m_nodesVector[i].destNodeId, m_nodesVector[i].receiver,      		\
					m_nodesVector[i].codingRouter, m_nodesVector[i].forwarder);
            printf("%s\n", message);
        }

        //Show channel FER matrix
        channelFerIter_t iter;
        //Print the links between nodes map (matrix-shaped)
        printf("---Channel FER configuration---\n");
        for (iter = m_channelFer.begin(); iter != m_channelFer.end(); iter++)
        {
            printf("Node %2d  -  ", iter->first);
            for (i = 0; i < (int) (iter->second).size(); i++)
            {
                printf("%2d   ", (iter->second)[i]);
            }
            printf("\n");
        }
    }

#endif   //NS3_LOG_ENABLE
    scenarioConfFile.close ();
    scenarioChannelFile.close ();

    assert (m_nodesNumber == m_nodesVector.size());

    return true;
}


void ConfigureScenario::ParseChannelDescriptionFile ()
{
	NS_LOG_FUNCTION (this);
	char cwdBuf [FILENAME_MAX];
	char line[FILENAME_MAX];
	string value, path;
	string lineString;
	u_int8_t ferValue;
	vector<u_int8_t> ferVector;
	u_int8_t lineNumber = 0;
	fstream file;

	m_configurationFile->GetKeyValue ("STACK", "CHANNEL_CONFIGURATION", value);
	path = std::string(getcwd(cwdBuf, FILENAME_MAX)) + "/src/scenario-creator/scenarios/" + value;

	file.open((const char *) path.c_str(), ios::in);

	assert (file.is_open());

	while (file.getline(line, 256))
	{
		lineString = string(line);
		if ((lineString.find('#') == string::npos) || (lineString.find('#') != 0)) //Ignore those lines which begins with the '#' character at its beginning
		{
			for (u_int16_t i = 0; i < lineString.size(); i++)
			{
				if (line [i] != ' ' && line [i] != '\t')
				{
					ferValue = atoi(line + i);
					// The FER value must be within the interval [0,10]
					NS_ASSERT_MSG(ferValue <= 10.0, "All the FER values must be within [0,1]");
					ferVector.push_back(ferValue);
				}
			}
			m_channelFer.insert(pair <int, vector <u_int8_t> > (lineNumber, ferVector));
			lineNumber++;
			ferVector.clear();
		}
	}

	m_nodesNumber = m_nodesVector.size();

	file.close();
}

void ConfigureScenario::GenerateRandomScenario (u_int16_t nNodes, double maxX, double maxY, u_int16_t dataFlows)
{
    NS_LOG_FUNCTION (nNodes << maxX << maxY);

    UniformVariable randomLocationX (0.0, maxX); //Randomly locate the nodes
    UniformVariable randomLocationY (0.0, maxY); //Randomly locate the nodes
    UniformVariable randomFlow (0, nNodes - 1); //Choose sources/sinks
    u_int16_t i, j, tx, rx;
    NodeDescription_t *nodeDescriptor;
    vector<u_int8_t> temp;

    std::multiset <std::pair <u_int16_t, u_int16_t> > flows;
    std::multiset <std::pair <u_int16_t, u_int16_t> >::iterator flowsIter;

    for (i = 0; i < nNodes; i++) {
        nodeDescriptor = new NodeDescription_t;
        nodeDescriptor->nodeId = i;
        nodeDescriptor->coordinates.x = randomLocationX.GetValue();
        nodeDescriptor->coordinates.y = randomLocationY.GetValue();
        nodeDescriptor->coordinates.z = 0.0; //2-D scenario
        nodeDescriptor->codingRouter = true; //By default, every network element can carry out coding/decoding decisions
        nodeDescriptor->forwarder = true;
        nodeDescriptor->transmitter = false;
        nodeDescriptor->receiver = false;
        NS_LOG_DEBUG ("Node " << i << ": (" << nodeDescriptor->coordinates.x << "," << nodeDescriptor->coordinates.y << "," << nodeDescriptor->coordinates.z << ")");

        m_nodesVector.push_back (*nodeDescriptor);
        delete nodeDescriptor;
    }
    m_nodesNumber = m_nodesVector.size ();

    //Configure the transmitters and receivers, according to the node-ID
    // Important- A flow cannot start and end on the same node
    i = 0;
    while (flows.size() < dataFlows)
    {
        tx = randomFlow.GetInteger (0, nNodes - 1);
        rx = randomFlow.GetInteger (0, nNodes - 1);

        if (tx != rx) {
            flowsIter = flows.insert(pair <u_int16_t, u_int16_t > (tx, rx));
            NS_LOG_DEBUG("Data flow: (" << (int) flowsIter->first << "," << "" << (int) flowsIter->second << ")");
            i++;
            if (!m_nodesVector[tx].transmitter)
                m_nodesVector[tx].transmitter = true;
            if (!m_nodesVector[rx].receiver)
                m_nodesVector[rx].receiver = true;
            m_nodesVector[tx].destinations.insert (rx);
            m_nodesVector[tx].destNodeId = rx;
        }
    }

    //Configure the channel --> Every link will be prone to errors (this is equivalent to set a '5' in each element of the NxN matrix
    for (i = 0; i < m_nodesNumber; i++)
    {
        for (j = 0; j < m_nodesNumber; j++)
        {
            temp.push_back(5);
        }
        m_channelFer.insert(pair<int, vector<u_int8_t> > (i, temp));
        temp.clear();
    }

    //Test
    for (i = 0; i < (u_int16_t) m_nodesVector.size(); i++)
    {
        NS_LOG_DEBUG("Node " << i << ": (" << m_nodesVector[i].coordinates.x << "," << m_nodesVector[i].coordinates.y << "," << m_nodesVector[i].coordinates.z << ") TX: "   \
				<< (int) m_nodesVector[i].transmitter << " RX: " << (int) m_nodesVector[i].receiver);
    }
}

void ConfigureScenario::GenerateLineTopology ()
{
	NS_LOG_FUNCTION_NOARGS();
	bool bidirectionalLine;								/* If this variable is set to 1, the first and the last nodes will
															 be both source and sink nodes; otherwise, a 0 indicates that there
															 will be only a flow   between the first and the last nodes (ONlY LINE TOPOLOGY) */

	float x_coord = 0.0;
	NodeDescription_t *nodeDescriptor;
	vector<u_int8_t> temp;
	string value;

	//Get Bidirectional line from the configuration file
	assert (m_configurationFile->GetKeyValue("STACK", "BIDIRECTIONAL", value) >= 0);
	bidirectionalLine = atoi(value.c_str());

//	int j;

	for (u_int16_t i = 0; i < m_nodesNumber;  i++)
	{
		nodeDescriptor = new NodeDescription_t;
		nodeDescriptor->nodeId = i;
		nodeDescriptor->coordinates.x = x_coord;
		nodeDescriptor->coordinates.y = 0.0;
		nodeDescriptor->coordinates.z = 0.0;

		if (i==0)
		{
			nodeDescriptor->transmitter = true;
			nodeDescriptor->destinations.insert (m_nodesNumber - 1);
			nodeDescriptor->forwarder = false;
			if (bidirectionalLine)
			{
				nodeDescriptor->receiver = true;
			}
		}
		else if (i == m_nodesNumber -1)
		{
			nodeDescriptor->receiver = true;
			nodeDescriptor->forwarder = false;
			if (bidirectionalLine)
			{
				nodeDescriptor->transmitter = true;
				nodeDescriptor->destinations.insert (0);
			}
		}
		else
		{
			nodeDescriptor->transmitter = false;
			nodeDescriptor->forwarder = true;
			nodeDescriptor->receiver = false;
		}

		m_nodesVector.push_back(*nodeDescriptor);

		NS_LOG_DEBUG ("NODE " << (int) i << " X " << nodeDescriptor->coordinates.x << " TX " << (int) nodeDescriptor->transmitter
				<< " (" << (int) nodeDescriptor-> destinations.size() << ") - RX " << (int) nodeDescriptor->receiver);

		delete nodeDescriptor;


		x_coord += m_distance;
	}

	//Configure the channel --> Every link will be prone to errors (this is equivalent to set a '5' in each element of the NxN matrix
//	for (u_int16_t i = 0; i < m_nodesNumber; i++)
//	{
//		for (j = 0; j < m_nodesNumber; j++)
//		{
//			temp.push_back(5);
//		}
//		m_channelFer.insert (pair<int, vector<u_int8_t> > (i, temp));
//		temp.clear();
//	}

	ParseChannelDescriptionFile();
	SetWifiChannel();
}

void ConfigureScenario::SetAttributes ()
{
	NS_LOG_FUNCTION (this);
	string value;

	//Propagation Attributes
	Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(20.0));
	Config::SetDefault("ns3::RangePropagationLossModel::FirstRangeDistance", DoubleValue(20.0));
	Config::SetDefault("ns3::RangePropagationLossModel::SecondRangeDistance", DoubleValue(32.0));

	//Wifi Attributes
	Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("DsssRate2Mbps"));
	Config::SetDefault ("ns3::YansWifiPhy::TxPowerStart", DoubleValue(0.0));
	Config::SetDefault ("ns3::YansWifiPhy::TxPowerEnd", DoubleValue(0.0));
	Config::SetDefault ("ns3::YansWifiPhy::CcaMode1Threshold", DoubleValue(-150.0)); //CS power level

	//ARP attributes (by default, it will perpetually store the entries)
	Config::SetDefault ("ns3::ArpCache::AliveTimeout", TimeValue(Seconds(10000)));
	Config::SetDefault ("ns3::ArpCache::DeadTimeout", TimeValue(Seconds(10000)));

	// Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));    		//Disable RTS/CTS transmission
	Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200")); 		//Disable fragmentation
	Config::SetDefault ("ns3::ConstantRateWifiManager::DataMode", StringValue("DsssRate11Mbps"));    //
	Config::SetDefault ("ns3::ConstantRateWifiManager::ControlMode", StringValue("DsssRate11Mbps")); //
	Config::SetDefault ("ns3::WifiMacQueue::MaxPacketNumber", UintegerValue(100000000)); 					//Maximum number of packets supported by the Wifi MAC buffer
	assert (m_configurationFile->GetKeyValue("WIFI", "TX_NUMBER", value) >= 0);
	Config::SetDefault ("ns3::WifiRemoteStationManager::MaxSlrc", UintegerValue(atoi(value.c_str()))); 						//Maximum number of transmission attempts
	Config::SetDefault ("ns3::WifiNetDevice::Mtu", UintegerValue(1512));   //

	//Network-level attributes
	//Config::SetDefault ("ns3::Ipv4L3Protocol::DefaultTtl", UintegerValue (4));

	//Transport-level attributes
	// TCP Attributes
	if (m_transportProtocol == TCP_PROTOCOL) {
		//Enable/disable NSC (Kernel's TCP handling)
		Config::SetDefault ("ns3::TcpSocket::TcpNoDelay", BooleanValue(true));   				//Does not seem it's working
		//  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1460));
		Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(m_propTracing->GetTraceInfo().packetLength));
		Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(90000000));
		Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(90000000));
	}//NSC
	else if (m_transportProtocol == NSC_TCP_PROTOCOL) {
		m_internetStackHelper.SetTcp("ns3::NscTcpL4Protocol", "Library", StringValue("liblinux2.6.26.so"));
		Config::Set("/NodeList/*/$ns3::Ns3NscStack<linux2.6.26>/net.inet.tcp.mssdflt", UintegerValue(m_propTracing->GetTraceInfo().packetLength));
		Config::Set("/NodeList/*/$ns3::Ns3NscStack<linux2.6.26>/net.inet.tcp.delayed_ack", UintegerValue(0)); //Disable Nagle's algorithm
		Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(90000000));
		Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(90000000));
	}
	else
	{
		Config::SetDefault("ns3::UdpSocket::RcvBufSize", UintegerValue(90000000));
		Config::SetDefault("ns3::UdpSocket::RcvBufSize", UintegerValue(90000000));
	}

	//Network Coding attribute initialization
	assert (m_configurationFile->GetKeyValue("NETWORK_CODING", "ENABLED", value) >= 0);
	if (atoi (value.c_str()))
	{
		assert (m_configurationFile->GetKeyValue("NETWORK_CODING", "PROTOCOL", value) >= 0);

		if (value == "InterFlowNetworkCodingProtocol")
		{
			assert (m_configurationFile->GetKeyValue("NETWORK_CODING", "BUFFER_SIZE", value) >= 0);
			Config::SetDefault ("ns3::InterFlowNetworkCodingBuffer::CodingBufferSize", UintegerValue((u_int32_t) atoi(value.c_str())));
			assert (m_configurationFile->GetKeyValue("NETWORK_CODING", "BUFFER_TIMEOUT", value) >= 0);
			Config::SetDefault ("ns3::InterFlowNetworkCodingBuffer::CodingBufferTimeout",TimeValue(MilliSeconds(atoi(value.c_str()))));
			assert (m_configurationFile->GetKeyValue("NETWORK_CODING", "MAX_CODED_PACKETS", value) >= 0);
			Config::SetDefault ("ns3::InterFlowNetworkCodingBuffer::MaxCodedPackets", UintegerValue((u_int32_t) atoi(value.c_str())));
		}
		else if (value == "IntraFlowNetworkCodingProtocol")
		{
			m_configurationFile->GetKeyValue("NETWORK_CODING", "Q", value);
			assert (atoi(value.c_str()) >= 1 && atoi(value.c_str()) <= 6);
			Config::SetDefault ("ns3::IntraFlowNetworkCodingProtocol::Q", UintegerValue((u_int8_t) atoi(value.c_str())));
			m_configurationFile->GetKeyValue("NETWORK_CODING", "K", value);
			assert (atoi(value.c_str()) > 1 && atoi(value.c_str()) < 256);
			Config::SetDefault ("ns3::IntraFlowNetworkCodingProtocol::K",UintegerValue((u_int16_t) atoi(value.c_str())));
			assert (m_configurationFile->GetKeyValue("NETWORK_CODING", "RECODING", value) >= 0);
			Config::SetDefault ("ns3::IntraFlowNetworkCodingProtocol::Recoding", BooleanValue (bool (atoi(value.c_str()))));
			assert (m_configurationFile->GetKeyValue("NETWORK_CODING", "ITPP", value) >= 0);
			Config::SetDefault ("ns3::IntraFlowNetworkCodingProtocol::Itpp", BooleanValue (bool (atoi(value.c_str()))));
			assert (m_configurationFile->GetKeyValue("NETWORK_CODING", "TIMEOUT", value) >= 0);
			Config::SetDefault ("ns3::IntraFlowNetworkCodingProtocol::BufferTimeout", TimeValue(MilliSeconds(atoi(value.c_str()))));
		}
	}

	//PacketSocket buffer
	Config::SetDefault ("ns3::PacketSocket::RcvBufSize", UintegerValue(90000000));

	//Application layer
	assert (m_configurationFile->GetKeyValue("APPLICATION", "RATE", value) >= 0);
	Config::SetDefault("ns3::OnOffApplication::DataRate", DataRateValue(value));
}

void ConfigureScenario::Init ()
{
    NS_LOG_FUNCTION_NOARGS();
    string value;

    //	GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));
    MobilityHelper mobilityHelper;
    Ptr <ListPositionAllocator> listPositionAllocator = CreateObject<ListPositionAllocator> ();

    //---------------Node creation---------------//
    m_nodeContainer.Create(m_nodesNumber);

    // Create the nodes, update the m_nodesVector information and instance the node's mobility objects
    for (u_int16_t i = 0; i < NodeList().GetNNodes(); i++)
    {
        m_nodesVector[i].node = NodeList().GetNode(i);
        listPositionAllocator->Add(m_nodesVector[i].coordinates);
    }

    //---------------Configure the mobility of the nodes---------------//
    mobilityHelper.SetPositionAllocator(listPositionAllocator);
    mobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobilityHelper.Install (m_nodeContainer);

    //---------------Configure Wifi---------------// --> By the moment, this is the unique physical implementation we support
	SetWifiChannel ();

    //---------------Configure Routing---------------//
    ConfigureRoutingProtocol();

    //Set the upper layer (default configuration). Install the IP stack
    m_internetStackHelper.Install(m_nodeContainer);

    //---------------Configure Network Coding---------------//
    assert (m_configurationFile->GetKeyValue("NETWORK_CODING", "ENABLED", value) >= 0);
    if (atoi (value.c_str()))
    {
    	SetNetworkCodingLayer ();
    }

    //---------------Configure IP level---------------//
    //IP layer configuration--> 2 possibilities: If Network Coding enabled, set an IP address for each NetworkCodingNetDevice; otherwise, assign the addresses to the default WifiNetDevices
    //Note: This concrete scenario will only create a unique network for all the nodes.
    Ipv4AddressHelper ipv4AddressHelper;
    Ipv4InterfaceContainer ipv4InterfaceContainer;

    ipv4AddressHelper.SetBase ("10.0.0.0", "255.255.0.0");
    ipv4InterfaceContainer = ipv4AddressHelper.Assign (m_deviceContainer);

    //---------------Configure the application layer---------------//
    SetUpperLayer ();

    //---------------Configure Tracing---------------//
    Tracing ();

}

void ConfigureScenario::SetWifiChannel ()
{
    NS_LOG_FUNCTION(this);

    string value;

    WifiHelper wifiHelper;
    YansWifiChannelHelper channelHelper;
    YansWifiPhyHelper phyHelper;


    phyHelper = YansWifiPhyHelper::Default ();
    channelHelper.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
//    SetChannel();

    NqosWifiMacHelper nQosWifiMacHelper = nQosWifiMacHelper.Default(); //By default, configure the nodes in AdHoc mode;

    switch (m_simulationChannel) {
        case SIM_RATE_ERROR: //MatrixPropagationLossModel + RateErrorModel (Simplest configuration)
        {
            Ptr<RangePropagationLossModel> prop = CreateObject<RangePropagationLossModel > ();
            channelHelper.AddPropagationLoss(prop);

            Ptr<RateErrorModel> error = CreateObject<RateErrorModel> ();
            error->SetUnit(EU_PKT);
            error->SetRate(m_fer);
            phyHelper.SetErrorModel(error);
            break;
        }
        case SIM_DEFAULT_MODEL: //MatrixPropagationLossModel + YansErrorRateModel/NistErrorRateModel
        {
            string temp;
            //Set the propagation loss model
//        	Ptr<RangePropagationLossModel> prop = CreateObject<RangePropagationLossModel > ();
//        	m_yanswifiChannelHelper.AddPropagationLoss(prop);

            Ptr<LogDistancePropagationLossModel> prop2 = CreateObject <LogDistancePropagationLossModel> ();
            channelHelper.AddPropagationLoss (prop2);

            //Configure the shadowing value
            assert (m_configurationFile->GetKeyValue ("DEFAULT", "FF_VARIANCE", temp)> -80.0);
            Ptr<RandomPropagationLossModel> prop3 = CreateObject <RandomPropagationLossModel> ();
            prop3->SetAttribute ("Variable", RandomVariableValue (NormalVariable(0.0, atof(temp.c_str ()))));
            channelHelper.AddPropagationLoss (prop3);
            phyHelper.SetErrorRateModel ("ns3::YansErrorRateModel");
            break;
        }
        case SIM_HMM_MODEL: //MatrixPropagationLossModel + HiddenMarkovErrorModel
        {
        	string temp;
			string aux;
        	Ptr<RangePropagationLossModel> prop = CreateObject<RangePropagationLossModel > ();
        	channelHelper.AddPropagationLoss(prop);

        	//Create and prepare the loss propagation loss model
        	Ptr<HiddenMarkovPropagationLossModel> hmmModel = CreateObject<HiddenMarkovPropagationLossModel> ();

        	//Get the simulation mode (time or frame-based)
        	assert (m_configurationFile->GetKeyValue ("HMM", "ERROR_UNIT", temp) >= 0);
        	if (temp == "TIME")
        	{
        		hmmModel->SetAttribute ("Mode", EnumValue(HMM_TIME_BASED_SIMULATION));
        		//Enable/disable the dynamic average delay
        		assert (m_configurationFile->GetKeyValue ("HMM", "DYNAMIC_AVERAGE_TIME", temp) >= 0);
        		hmmModel->SetDynamicAverageTime ((bool) atoi(temp.c_str()));
        	}
        	else if (temp == "FRAMES")
        	{
        		hmmModel->SetAttribute ("Mode", EnumValue(HMM_FRAME_BASED_SIMULATION));
        	}
        	else
        	{
        		NS_ABORT_MSG ("Incorrect Hidden Markov model mode. Valid options: TIME or FRAMES. Please fix");
        	}

        	//Check the operation mode
        	assert (m_configurationFile->GetKeyValue ("HMM", "OPERATION", temp) >= 0);

            //Check if the attribute is correctly created
            if (temp == "FILE")   //Set the same matrix transition and emission for the whole set of links between the nodes
            {
            	string transition, emission;
            	assert (m_configurationFile->GetKeyValue ("HMM", "TRANSITION_MATRIX_FILE", transition) >= 0);
            	assert (m_configurationFile->GetKeyValue ("HMM", "EMISSION_MATRIX_FILE", emission) >= 0);
            	hmmModel->InitFromFile (transition, emission);
            }
            else if (temp == "FER")
            {
            	hmmModel->SetAttribute ("FER", DoubleValue (m_fer));
            	hmmModel->InitFromFer (m_channelFer);
            }
            else if (temp == "DISTANCE")   			//Option not finished yet --> Need to improve
            {
            	//Implement a mode linked to the distance between nodes
            	hmmModel->InitFromDistance ();
            }
            else
            {
            	NS_ABORT_MSG ("HMM operation " << temp << " not valid.Please fix");
            }

            //Add both propagation and error models to the Wifi instance helpers
            channelHelper.AddPropagationLoss (hmmModel);
            phyHelper.SetErrorModel (hmmModel->GetErrorModel());

            break;
        }
        case SIM_BEAR_MODEL: //BearPropagationLossModel (which instances a FriisPropagationLossModel object at the same time) + BearErrorModel
        {
            string temp;
            Ptr<RangePropagationLossModel> prop = CreateObject<RangePropagationLossModel > ();
            channelHelper.AddPropagationLoss(prop);

            //Instance the BEAR propagation loss model (the error model is implicitly created)
            Ptr<BearPropagationLossModel> bearModel = CreateObject<BearPropagationLossModel > ();
            bearModel->SetPropagationLoss ("ns3::LogDistancePropagationLossModel");

            //Set a fixed SNR for all the links (Version to be enhanced with a fixed FER for each link)
//            bearModel->SetReceivedSnr (make_pair (true, 10));

            //AR model attribute initialization
            assert (m_configurationFile->GetKeyValue ("BEAR", "COEF_FILE", temp) >= 0);
            bearModel->SetAttribute ("CoefficientsFile", StringValue(temp));
            assert (m_configurationFile->GetKeyValue("BEAR", "FILTER_ORDER", temp) >= 0);
            bearModel->SetAttribute ("ArFilterOrder", IntegerValue(atoi(temp.c_str())));
            assert (m_configurationFile->GetKeyValue("BEAR", "COHERENCE_TIME", temp) >= 0);
            bearModel->SetAttribute ("CoherenceTime", DoubleValue(atof(temp.c_str())));
            assert (m_configurationFile->GetKeyValue("BEAR", "AR_FILTER_ENTRY_NOISE_POWER", temp) >= 0);
            bearModel->SetAttribute ("ArFilterVariance", DoubleValue(atof(temp.c_str())));
            assert (m_configurationFile->GetKeyValue("BEAR", "AR_FILTER_VARIANCE", temp) >= 0);
            bearModel->SetAttribute ("StandardDeviation", DoubleValue(atof(temp.c_str())));
            assert (m_configurationFile->GetKeyValue("BEAR", "FF_VARIANCE", temp) >= 0);
            bearModel->SetAttribute ("FastFadingVariance", DoubleValue(atof(temp.c_str())));

            //Add both propagation and error models to the Wifi instance helpers
            channelHelper.AddPropagationLoss (bearModel);
            phyHelper.SetErrorModel (bearModel->GetErrorModel());
            break;
        }
        case SIM_MANUAL_MODEL: //Nothing to do here
        {
        	u_int16_t i, j;

        	// First, set the RangePropagationLossModel
        	Ptr<RangePropagationLossModel> prop = CreateObject<RangePropagationLossModel > ();
        	channelHelper.AddPropagationLoss(prop);

        	//Second, parse the scenario topology and configure the MatrixPropagationLossErrorModel
        	Ptr<MatrixErrorModel> error = CreateObject<MatrixErrorModel> ();
        	error->SetDefaultFer (0.0);

        	///// MatrixPropagationLossModel Configuration (taken from the channel configuration file)
        	for (i = 0; i < (int) NodeList().GetNNodes (); i++)
        	{
        		for (j = 0; j < (int) NodeList().GetNNodes (); j++)
        		{
        			if (i != j)
        			{
        				if (m_channelFer.find(i) != m_channelFer.end())
        				{
        					//Configure the FER between the nodes. There are three possibilities: 0- The filter leaves the packet to pass through; 1- The filter blocks the packet; 5- The packet go beyond the filter,
        					//but it's up to the next propagation loss model to handle the channel response
        					//printf("Element %d, %d = %d\n", i, j, m_channelFer.find(i)->second[j]);

        					switch (m_channelFer.find(i)->second[j])
        					{
        					case 0: //No FER
        						error->SetFer (NodeList().GetNode(i)->GetId(), NodeList().GetNode(j)->GetId(), 0.0);
        						break;
        					case 1: //All frames will be discarded
        						error->SetFer (NodeList().GetNode(i)->GetId(), NodeList().GetNode(j)->GetId(), 1.0);
        						break;
        					case 5: //Configurable FER (through m_fer variable) --> Need to find a way to instance the desired propagation loss models
        						error->SetFer (NodeList().GetNode(i)->GetId(), NodeList().GetNode(j)->GetId(), m_fer);
        						break;
        					case 6:
        						error->SetFer (NodeList().GetNode(i)->GetId(), NodeList().GetNode(j)->GetId(), m_ferStatic);
        						break;
        					default:
        						NS_LOG_ERROR("Non-handled option");
        						break;
        					}
        				}
        				else
        				{
        					NS_LOG_ERROR("Key " << i << " not found");
        				}
        			}
        		}
        	}

        	phyHelper.SetErrorModel(error);

        	break;
        }
        case SIM_SIMPLE_MODEL:
        {
        	//TO BE IMPLEMENTED
        	break;
        }
    }

    phyHelper.SetChannel(channelHelper.Create());
    wifiHelper.Default();
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager");

    m_deviceContainer = wifiHelper.Install(phyHelper,
    		nQosWifiMacHelper, m_nodeContainer);


    //Connect to the tracing system
    for (u_int8_t i = 0; i < (int) phyHelper.GetChannel()->GetPhyList().size(); i++)
    {
    	phyHelper.GetChannel()->GetPhyList()[i]->SetPhyReceiveCallback(MakeCallback(&ProprietaryTracing::WifiPhyRxTrace, GetProprietaryTracing()));
    }

    //ASCII tracing
    assert (m_configurationFile->GetKeyValue("OUTPUT", "PCAP_TRACING", value) >= 0);
    if (atoi(value.c_str()))
    {
    	string fileName = "PCAP_"  + m_propTracing->GetTraceInfo().transport + '_'	+ m_propTracing->GetTraceInfo().deployment + '_' + m_propTracing->GetTraceInfo().channel + ".pcap";
    	std::replace (fileName.begin(), fileName.end(), '-', '_');

    	phyHelper.EnablePcap("traces/pcap/" + fileName,
    			m_nodeContainer, true);
    }

    //PCAP tracing
    assert(m_configurationFile->GetKeyValue("OUTPUT", "ASCII_TRACING", value) >= 0);
    if (atoi(value.c_str()))
    {
    	string fileName = "ASCII_"  + m_propTracing->GetTraceInfo().transport + '_'	+ m_propTracing->GetTraceInfo().deployment + '_' + m_propTracing->GetTraceInfo().channel + ".txt";
    	std::replace (fileName.begin(), fileName.end(), '-', '_');
    	phyHelper.EnablePcap("traces/ascii/" + fileName,
    			m_nodeContainer, true);
    }

}

void ConfigureScenario::SetNetworkCodingLayer ()
{
	NS_LOG_FUNCTION_NOARGS();

	string typeId;
	u_int16_t i;
	string value;
	bool networkCodingTracing;

	NetworkCodingHelper ncHelper;

	assert (m_configurationFile->GetKeyValue("NETWORK_CODING", "PROTOCOL", typeId) >= 0);
	assert (m_configurationFile->GetKeyValue("OUTPUT", "NETWORK_CODING_LONG_TRACING", value) >= 0);
	networkCodingTracing = atoi (value.c_str());


	if (typeId == "InterFlowNetworkCodingProtocol")
	{
		m_propTracing->GetTraceInfo().networkCoding = "INTER";
		string aux;
		bool embeddedAcks;

		ncHelper.Install(m_nodeContainer, typeId);

		//NC ACK encapsulation scheme
		assert (m_configurationFile->GetKeyValue ("NETWORK_CODING", "EMBEDDED_ACKS", aux) >= 0);
		embeddedAcks = (u_int8_t) atoi (aux.c_str ());
		ncHelper.SetEmbeddedAckScheme (embeddedAcks);

		// End of NC ACK encapsulation scheme setup

		//Connect to callbacks and the Network Monitor
		for (i = 0; i < (int) ncHelper.GetNetworkCodingList().size(); i++)
		{
			Ptr <InterFlowNetworkCodingProtocol> aux = DynamicCast<InterFlowNetworkCodingProtocol > (ncHelper.GetNetworkCodingList()[i]);
			NetworkMonitor::Instance ().AddNetworkCodingLayer (ncHelper.GetNetworkCodingList()[i]);

			//Make sure we are handling the correct node
			assert(aux->GetNode() == m_nodesVector[i].node);

			//Tune the input packet pool buffer parameters according to the node operation
			if (m_nodesVector[i].codingRouter) //The coding router will be the only network element that implements the input packet pool
			{
				aux->SetCodingNode(true);
			}

			//Connect to the Network Coding Trace System
			if (networkCodingTracing)
			{
				aux->SetInterFlowNetworkCodingCallback (MakeCallback (&ProprietaryTracing::InterFlowNetworkCodingLongTrace, m_propTracing));
			}
		}
	}
	else if (typeId == "IntraFlowNetworkCodingProtocol")
	{
		m_propTracing->GetTraceInfo().networkCoding = "INTRA";
		Ptr<IntraFlowNetworkCodingProtocol> more = CreateObject<IntraFlowNetworkCodingProtocol> ();

		ncHelper.Install(m_nodeContainer, typeId);

		for (i = 0; i < (int) ncHelper.GetNetworkCodingList().size(); i++)
		{
			Ptr <IntraFlowNetworkCodingProtocol> aux = DynamicCast<IntraFlowNetworkCodingProtocol > (ncHelper.GetNetworkCodingList()[i]);
			NetworkMonitor::Instance ().AddNetworkCodingLayer (ncHelper.GetNetworkCodingList()[i]);

			//Connect to the Network Coding Trace System
			if (networkCodingTracing)
			{
				aux->SetIntraFlowNetworkCodingCallback (MakeCallback(&ProprietaryTracing::IntraFlowNetworkCodingLongTrace, m_propTracing));
			}
		}

		//Modify the packet length in order to achieved the MTU according to the Intra network Coding flow parameters (i.e. Q, K);
		if (m_transportProtocol == UDP_PROTOCOL)
		{
			m_propTracing->GetTraceInfo().packetLength = m_propTracing->GetTraceInfo().packetLength -
					9 - (u_int8_t) ceil ((double) (atoi (IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(0).initialValue->SerializeToString (MakeUintegerChecker<u_int8_t> ()).c_str()) *
							atoi (IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(1).initialValue->SerializeToString (MakeUintegerChecker<u_int8_t> ()).c_str())/8.0));
		}

		//Ensure that the overall number of packet is a multiple of K
		assert(m_configurationFile->GetKeyValue("SCENARIO", "NUM_PACKETS", value) >= 0);
    	m_propTracing->GetTraceInfo().numPackets = (u_int16_t) ceil ((double) atoi(value.c_str()) /
    			(double) atoi (IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(1).initialValue->SerializeToString (MakeUintegerChecker<u_int8_t> ()).c_str())) *
    			atoi (IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(1).initialValue->SerializeToString (MakeUintegerChecker<u_int8_t> ()).c_str());

				//Check that the attributes has been successfully configured
//				cout << IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(0).name << ": " << IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(0).initialValue->SerializeToString (MakeUintegerChecker<u_int8_t> ()).c_str() << endl;
//				cout << IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(1).name << ": " << IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(1).initialValue->SerializeToString (MakeUintegerChecker<u_int16_t> ()).c_str() << endl;
//				cout << IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(2).name << ": " << IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(2).initialValue->SerializeToString (MakeBooleanChecker ()).c_str() << endl;
//				cout << IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(3).name << ": " << IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(3).initialValue->SerializeToString (MakeBooleanChecker ()).c_str() << endl;
//				cout << IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(4).name << ": " << (int) Time (IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(4).initialValue->SerializeToString(MakeTimeChecker ())).GetMilliSeconds() << endl;

	}
	else if (typeId == "HybridNetworkCodingProtocol")
	{
		//TO BE IMPLEMENTED --> Combine the Inter and intra NC operations (under UDP)
		m_propTracing->GetTraceInfo().networkCoding = "HYBRID";
	}
	else
	{
		NS_ABORT_MSG ("Network Coding Protocol not handled. Please fix: " << typeId);
	}

}

void ConfigureScenario::ConfigureRoutingProtocol()
{
	NS_LOG_FUNCTION(m_routingProtocol);
	string value;
	string routingProtocol;

	//Add the static routing helper
	Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper list;
    AodvHelper aodv;
    OlsrHelper olsr;
    list.Add(staticRouting, 0);

    switch (m_routingProtocol)
    {
        case RT_POPULATE_ROUTING_TABLES:
            routingProtocol = "POPULATE";
            Ipv4GlobalRoutingHelper::PopulateRoutingTables();
            break;
        case RT_STATIC_ROUTING_PROTOCOL:
        	//We have to initialize the static routing handling after waiting the Ipv4 stack is successfully installed at the nodes
        	Simulator::Schedule(MilliSeconds(2.0), &ConfigureScenario::LoadStaticRouting, this, staticRouting);
        	routingProtocol = "STATIC";
        	break;
        case RT_STATIC_GRAPH_ROUTING_PROTOCOL:
        	//We have to initialize the static routing handling after waiting the Ipv4 stack is successfully installed at the nodes
        	Simulator::Schedule(MilliSeconds(2.0), &ConfigureScenario::LoadStaticRoutingFromGraph, this, staticRouting);
        	routingProtocol = "STATIC";
        	break;
        case RT_AODV_PROTOCOL:
        	list.Add(aodv, 10);
        	routingProtocol = "AODV";
        	break;
        case RT_OLSR_PROTOCOL:
        	list.Add(olsr, 10);
        	routingProtocol = "OLSR";
        	break;
    }

    m_internetStackHelper.SetRoutingHelper(list);

    // Trace routing tables
    assert (m_configurationFile->GetKeyValue("OUTPUT", "ROUTING_TABLES", value) >= 0);
    if (atoi(value.c_str()))
    {
        string fileName;
        fileName = "traces/routes/" + m_propTracing->GetTraceInfo().transport + '_'	+ m_propTracing->GetTraceInfo().deployment + '_' + m_propTracing->GetTraceInfo().channel + ".routes";

        Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper > (fileName, std::ios::out);
        //Print according to the class which be in charge of the routing protocol (i.e. AODV, STATIC or OLSR)
        if (routingProtocol == "OLSR")
        {
        	olsr.PrintRoutingTableAllEvery (Seconds(5), routingStream);
        }
        else if (routingProtocol == "AODV")
        {
            aodv.PrintRoutingTableAllEvery (Seconds(5), routingStream);
        }
        else
        {
        	staticRouting.PrintRoutingTableAllEvery (Seconds(5), routingStream);
        }
    }
}

void ConfigureScenario::LoadStaticRouting(Ipv4StaticRoutingHelper staticRouting) {
    NS_LOG_FUNCTION_NOARGS();
    fstream file;
    char cwdBuf [FILENAME_MAX];
    string fileName;
    string temp;

    //Set the path and the name of the file which contains the static routing table; afterwards, open the file
    //Grab the name of the static routing table file
    assert (m_configurationFile->GetKeyValue("STACK", "STATIC_ROUTING_TABLE", temp) >= 0);

    fileName = std::string(getcwd(cwdBuf, FILENAME_MAX)) + "/src/scenario-creator/scenarios/" + temp;
    file.open((const char *) fileName.c_str(), ios::in);

    //We read the file through this way because we always know the number of elements per row (5 in this static routing table)
    NS_ASSERT_MSG(file, "File " << fileName << " not found.");
    {
        char line[256];
        file.getline(line, 256);
        //Pass through the title line
        int nodeId, destination, nexthop, interface, metric;
        while (file >> nodeId >> destination >> nexthop >> interface >> metric) {
            //By default -> Two interfaces per node: 0- Loopback, 1-Output interface, that is to say, WifiNetDevice when NC layer is disabled; otherwise, will be a NetworkCodingNetDevice.
        	//Ipv4Address srcAddress = m_nodeContainer.Get((int) nodeId)->GetObject<Ipv4 > ()->GetAddress(1, 0).GetLocal();
            Ipv4Address dstAddress = m_nodeContainer.Get((int) destination)->GetObject<Ipv4 > ()->GetAddress(1, 0).GetLocal();
            Ipv4Address nextHopAddress = m_nodeContainer.Get((int) nexthop)->GetObject<Ipv4 > ()->GetAddress(1, 0).GetLocal();

            NS_LOG_DEBUG("Node " << (int) nodeId << " IP address " <<
                    m_nodeContainer.Get((int) nodeId)->GetObject<Ipv4 > ()->GetAddress(1, 0).GetLocal()
                    << " Dest adress " << dstAddress << " Nexthop adress " << nextHopAddress);
            Ptr<Ipv4StaticRouting> routingEntry = staticRouting.GetStaticRouting(m_nodeContainer.Get((int) nodeId)->GetObject<Ipv4 > ());

            routingEntry->AddHostRouteTo(dstAddress, nextHopAddress, interface, metric);
        }
    }
    file.close();
}

void ConfigureScenario::LoadStaticRoutingFromGraph (Ipv4StaticRoutingHelper staticRouting)
{
	NS_LOG_FUNCTION_NOARGS();
	fstream file;
	char cwdBuf [FILENAME_MAX];
	char line[256];

	string routing;
	string lineString;
	string fileName;

	u_int8_t temp = 0;
	u_int8_t lineNumber = 1;

	u_int8_t source = 0;
	u_int8_t destination = 0;

	Ptr<Ipv4StaticRouting> routingEntry;

	//Special container needed to instance the static routing table
	map<u_int8_t, vector<u_int8_t> > graphRoutes;

	//Set the path and the name of the file which contains the static routing table; afterwards, open the file
	//Grab the name of the static routing table file
	assert (m_configurationFile->GetKeyValue("STACK", "STATIC_ROUTING_TABLE", routing) >= 0);
	fileName = std::string(getcwd(cwdBuf, FILENAME_MAX)) + "/src/scenario-creator/scenarios/" + routing;
	file.open((const char *) fileName.c_str(), ios::in);

	assert(file);

	//Map the file into a matrix which will hold the different routes between the nodes
	while(file.getline (line, 256))
	{
		vector<u_int8_t> tempVector;

		lineString = string (line);
		if ((lineString.find('#') == string::npos) || (lineString.find('#') != 0))     //Ignore those lines which begins with the '#' character at its beginning
		{
			for(u_int8_t i = 0; i < (int) lineString.size(); i++)
			{
				if ((line [i] != ' ' && line [i] != '\t' && line [i-1] == ' ') || i == 0)   //Grab the first number and the ones which are separated via ' ' or '\t'
				{
					//Special issue --> In a line topology, as we do want to use a unique static routing file, we force the instance to update only with the valid nodes
					temp = atoi (line + i);
					if (temp <= m_nodesNumber)
					{
						tempVector.push_back (temp - 1);
					}
				}
			}

			//TCP/UDP transport protocols --> Number of routes bounded to one
			if ((m_transportProtocol == TCP_PROTOCOL || m_transportProtocol == UDP_PROTOCOL) && graphRoutes.size() < 1)
			{
				graphRoutes.insert (pair<u_int8_t, vector <u_int8_t> > (lineNumber, tempVector));
			}
			//MPTCP protocol --> Up to two different disjoint routes
			else if	(graphRoutes.size() < 2 && m_transportProtocol == MPTCP_PROTOCOL)
			{
				graphRoutes.insert (pair<u_int8_t, vector <u_int8_t> > (lineNumber, tempVector));
			}
			lineNumber++;

			source = tempVector[0];
			destination = tempVector.back();
			tempVector.clear ();
		}
	}

	file.close();

	NS_ABORT_MSG_IF(graphRoutes.size() < 2 && m_transportProtocol == MPTCP_PROTOCOL , "At least two routes for multipath");

	//We are going to fill the static routing table. For that purpose, we need to create an entry for each pair of nodes (remember that there is BIDIRECTIONAL)
	for (map<u_int8_t, vector<u_int8_t> >::iterator iter = graphRoutes.begin(); iter != graphRoutes.end(); iter ++)
	{
		for (u_int8_t j = 0; j < iter->second.size() - 1; j++)
		{
			Ipv4Address srcAddress = m_nodeContainer.Get((int) source)->GetObject<Ipv4 > ()->GetAddress(iter->first, 0).GetLocal();                                     
			Ipv4Address dstAddress = m_nodeContainer.Get((int) destination)->GetObject<Ipv4 > ()->GetAddress(iter->first, 0).GetLocal();
			Ipv4Address nextHopAddress = m_nodeContainer.Get((int) iter->second[j+1])->GetObject<Ipv4 > ()->GetAddress(iter->first, 0).GetLocal();
                                         

			NS_LOG_DEBUG("Node " << (int) iter->second[j] + 1 << " IP address " <<
					m_nodeContainer.Get((int) iter->second[j])->GetObject<Ipv4 > ()->GetAddress(iter->first, 0).GetLocal()
					<< " Dest adress " << dstAddress << " Nexthop adress " << nextHopAddress);

			//Forward route
			routingEntry = staticRouting.GetStaticRouting(m_nodeContainer.Get((int) iter->second[j])->GetObject<Ipv4 > ());
			routingEntry->AddHostRouteTo(dstAddress,   //Destination address
					nextHopAddress,				  //Nexthop address
					iter->first,	//Interface
					0);		//Metric

			//Backward route
			routingEntry = staticRouting.GetStaticRouting(m_nodeContainer.Get((int) iter->second[j+1])->GetObject<Ipv4 > ());
			routingEntry->AddHostRouteTo(srcAddress,   //Destination address
					m_nodeContainer.Get((int) iter->second[j])->GetObject<Ipv4 > ()->GetAddress(iter->first, 0).GetLocal(),				  //Nexthop address
					iter->first,	//Interface
					0);		//Metric

			NS_LOG_DEBUG ("Node " << (int) iter->second[j+1] + 1  << " IP address " <<
					m_nodeContainer.Get((int) iter->second[j+1])->GetObject<Ipv4 > ()->GetAddress(iter->first, 0).GetLocal()
					<< " Dest adress " << srcAddress << " Nexthop adress " << m_nodeContainer.Get((int) iter->second[j])->GetObject<Ipv4 > ()->GetAddress(iter->first, 0).GetLocal());
		}
	}
}

std::string ConfigureScenario::CheckTransportLayer()
{
    NS_LOG_FUNCTION(this);
    if (m_transportProtocol == TCP_PROTOCOL)
        return "ns3::TcpSocketFactory";
    else if (m_transportProtocol == UDP_PROTOCOL)
        return "ns3::UdpSocketFactory";
    else
        return "ns3::TcpSocketFactory";
}

void ConfigureScenario::SetUpperLayer()
{
    NS_LOG_FUNCTION(this);
    string value;
    u_int16_t i;
    u_int16_t portBase = 50000;
    u_int16_t portOffset = 0;
    multiset<u_int16_t>::iterator iter;
    pair<multiset<u_int16_t>::iterator, multiset<u_int16_t>::iterator> destinationsList;

    //Application definition (for this concrete testbed, we are going to use the OnOffApplication environment to inject the traffic into the scenario
    //Transmission nodes --> We have to parse the m_nodesVector looking for the nodes' configuration
    //Reception nodes --> The same method as for the transmitters
    double offset = 0;

    //Read the number of packets
    if (! m_propTracing->GetTraceInfo().numPackets)
    {
    	assert(m_configurationFile->GetKeyValue("SCENARIO", "NUM_PACKETS", value) >= 0);
    	m_propTracing->GetTraceInfo().numPackets = atoi(value.c_str());
    }

    //Before starting the transmission, send dummy packets in order to fill, by means of UdpEcho applications, the ARP cache
    ApplicationContainer clientApps, serverApps;
    for (i = 0; i < (int) m_nodesVector.size(); i++) {
        UdpEchoServerHelper echoServer(9);
        serverApps = echoServer.Install(m_nodesVector[i].node);
        serverApps.Start(Seconds(4.0));
        serverApps.Stop(Seconds(20.0));

        for (int j = 0; j < (int) m_nodesVector.size(); j++)
        {
            if (i != j)
            {
                UdpEchoClientHelper echoClient(m_nodesVector[j].node->GetObject<Ipv4 > ()->GetAddress(1, 0).GetLocal(), 9);
                echoClient.SetAttribute("MaxPackets", UintegerValue(1));
                echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
                echoClient.SetAttribute("PacketSize", UintegerValue(200));

                clientApps = echoClient.Install(m_nodesVector[i].node);
                clientApps.Start(Seconds(1.0 + offset));
                clientApps.Stop(Seconds(20.0));

                offset += 0.01;
            }
        }
    }
    ///// End of cache table filling
    ApplicationContainer sourceAppContainer;
    ApplicationContainer sinkAppContainer;

    offset = 0;
    for (i = 0; i < (int) m_nodesVector.size(); i++) {
        //Configuring the application layer
        if (m_nodesVector[i].transmitter) {
            for (iter = m_nodesVector[i].destinations.begin(); iter != m_nodesVector[i].destinations.end(); iter++, portOffset++) {
                //One pair Application/Sink for each one
                //Application (OnOffApplication)
                OnOffHelper onOff (CheckTransportLayer(), Address(InetSocketAddress((m_nodesVector[*iter].node->GetObject<Ipv4 > ())->GetAddress(1, 0).GetLocal(), portBase + portOffset)));
                onOff.SetAttribute("OnTime", RandomVariableValue(ConstantVariable(1)));
                onOff.SetAttribute("OffTime", RandomVariableValue(ConstantVariable(0)));
                onOff.SetAttribute("PacketSize", UintegerValue(m_propTracing->GetTraceInfo().packetLength));
                onOff.SetAttribute("MaxBytes", UintegerValue(m_propTracing->GetTraceInfo().packetLength * m_propTracing->GetTraceInfo().numPackets));
                sourceAppContainer = onOff.Install (NodeList().GetNode(i));
                sourceAppContainer.Start(Seconds(20.0 + offset));
                sourceAppContainer.Stop(Seconds(990.0));
                NetworkMonitor::Instance().GetSourceApps().Add (sourceAppContainer);
                NS_LOG_DEBUG("OnOff Destination: " << (m_nodesVector [*iter].node->GetObject<Ipv4 > ())->GetAddress(1, 0).GetLocal() << " Port: " << portBase + portOffset);

                //Packet sink
                PacketSinkHelper packetSink (CheckTransportLayer(), Address(InetSocketAddress (Ipv4Address::GetAny(), portBase + portOffset)));
                sinkAppContainer = packetSink.Install(NodeList().GetNode(*iter));
                sinkAppContainer.Start(Seconds(1.0));
                sinkAppContainer.Stop(Seconds(990.0));
                NetworkMonitor::Instance().GetSinkApps().Add (sinkAppContainer);
                NS_LOG_DEBUG ("Sink defined at IP " << (m_nodesVector[*iter].node->GetObject<Ipv4 > ())->GetAddress(1, 0).GetLocal() << " , listening at port " << portBase + portOffset);
            }
        }
    }

    //Connect to TCP trace sources --> It does not work (must fix)
//    Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::OnOffApplication/Tx", MakeCallback (&ProprietaryTracing::ApplicationTxTrace, m_propTracing));
//    Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&ProprietaryTracing::ApplicationRxTrace, m_propTracing));
}


void ConfigureScenario::Tracing ()
{
	NS_LOG_FUNCTION (this);
	string value;

	//Application level tracing
	Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::OnOffApplication/Tx", MakeCallback (&ProprietaryTracing::ApplicationTxTrace, m_propTracing));
	Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&ProprietaryTracing::ApplicationRxTrace, m_propTracing));

	assert (m_configurationFile->GetKeyValue("OUTPUT", "APPLICATION_LEVEL_LONG_TRACING", value) >= 0);
	if (atoi(value.c_str()))
	{
		m_propTracing->EnableApplicationLongTraceFile ();
	}
	assert (m_configurationFile->GetKeyValue("OUTPUT", "APPLICATION_LEVEL_SHORT_TRACING", value) >= 0);
	if (atoi(value.c_str()))
	{
		m_propTracing->EnableApplicationShortTraceFile ();
	}

	//Network Coding level tracing (Depending on the type of NC instances upon the scenario)
	//Only when a network coding scheme is enabled
	if (m_propTracing->GetTraceInfo().networkCoding.size())
	{
		assert (m_configurationFile->GetKeyValue("OUTPUT", "NETWORK_CODING_LONG_TRACING", value) >= 0);
		if (atoi(value.c_str()))
		{
			assert (m_configurationFile->GetKeyValue("NETWORK_CODING", "PROTOCOL", value) >= 0);
			if (value == "InterFlowNetworkCodingProtocol")
			{
				m_propTracing->EnableInterFlowNetworkCodingLongTraceFile();
			}
			else if (value == "IntraFlowNetworkCodingProtocol")
			{
				m_propTracing->EnableIntraFlowNetworkCodingLongTraceFile();
			}
		}
		assert (m_configurationFile->GetKeyValue("OUTPUT", "NETWORK_CODING_SHORT_TRACING", value) >= 0);
		if (atoi(value.c_str()))
		{
			assert (m_configurationFile->GetKeyValue("NETWORK_CODING", "PROTOCOL", value) >= 0);
			if (value == "InterFlowNetworkCodingProtocol")
			{
				m_propTracing->EnableInterFlowNetworkCodingShortTraceFile();
			}
			else if (value == "IntraFlowNetworkCodingProtocol")
			{
				m_propTracing->EnableIntraFlowNetworkCodingShortTraceFile();
			}
		}
	}

	//Phy (Wifi) level tracing
	assert (m_configurationFile->GetKeyValue("OUTPUT", "PHY_WIFI_LONG_TRACING", value) >= 0);
	if (atoi(value.c_str()))
	{
		m_propTracing->EnableWifiPhyLevelTracing ();
	}

	//Flow Monitor
	assert(m_configurationFile->GetKeyValue("OUTPUT", "FLOW_MONITOR", value) >= 0);
	if (atoi(value.c_str()))
	{
		  m_propTracing->EnableFlowMonitor();
	}
}
