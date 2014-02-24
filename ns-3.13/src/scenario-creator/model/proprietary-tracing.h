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
 *         Ramón Agüero Calvo <ramon@tlmat.unican.es>
 */

#ifndef PROPRIETARY_TRACING_H_
#define PROPRIETARY_TRACING_H_

#include "ns3/object.h"
#include "ns3/random-variable.h"
#include "ns3/traced-value.h"
#include "ns3/traced-callback.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/address.h"

#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/network-coding-module.h"

#include "ns3/flow-monitor-module.h"


#include "trace-stats.h"

#include <math.h>

namespace ns3
{

struct TracingInformation
{
	TracingInformation();
	~TracingInformation();
	//Simulation setup
	u_int32_t run;
	u_int32_t runOffset;
	u_int32_t totalRuns;
	u_int16_t packetLength;
	u_int16_t numPackets;

	double fer;
	std::string scenario;
	std::string networkCoding;
	std::string transport;
	std::string channel;
	std::string deployment;
};

class ProprietaryTracing: public Object
{
public:
	/**
	 * Default constructor
	 */
	ProprietaryTracing();
	/**
	 * Default destructor
	 */
	~ProprietaryTracing();

	//Statistics
	inline u_int32_t GetCorrectPackets () {return m_totalDataCorrectPackets;}
	inline u_int32_t GetCorruptedPackets () {return m_totalDataCorruptedPackets;}
	inline u_int32_t GetTotalPackets () {return m_totalDataPackets;}
	inline u_int32_t GetTxPackets () {return m_txPackets;}
	inline u_int32_t GetRxPackets () {return m_rxPackets;}

	/*
	 * \brief Conversion from Mac48Address format to
	 * \param mac Mac address in Mac48Address format
	 * \return The MAC address in string format
	 */
	std::string ConvertMacToString (Mac48Address mac);

	/*
	 * \brief Application level transmission tracing (Connected to trace callback)
	 * \param context
	 * \param packet
	 */
	void ApplicationTxTrace (string context, Ptr<const Packet> packet);

	/*
	 * \brief Application level reception tracing (Connected to trace callback)
	 * \param context
	 * \param packet
	 */
	void ApplicationRxTrace (string context, Ptr<const Packet> packet, const Address& address);

	/*
	 * Enable and open the trace file corresponding to the Application layer (long format)
	 */
	void EnableApplicationLongTraceFile ();

	/*
	 * Enable and open the trace file corresponding to the Application layer (short format)
	 */
	void EnableApplicationShortTraceFile ();

	/*
	 * Print the final statistics gathered at the application layer
	 */
	void PrintApplicationStatistics ();

	/*
	 * Enable and open the long tracing system belonging to the Network Coding layer,
	 * which will use the network monitor to perform the statistic studio of the simulation
	 */
	void EnableInterFlowNetworkCodingLongTraceFile ();

	/*
	 * Enable and open the long tracing system belonging to the Network Coding layer,
	 * which will use the network monitor to perform the statistic studio of the simulation
	 */
	void EnableInterFlowNetworkCodingShortTraceFile ();

	/*
	 * Receive a packet from the InterFlowNetworkCodingProtocol layer
	 * \param packet The packet itself
	 * \param tx 0 for a reception, 1 for a transmission at the TX nodes; 2 for a transmission (coded or not) at the coding router
	 * \param nodeId ID of the node which sent the signal to this member function
	 * \param source Packet's IP source address
	 * \param destination Packet's IP destination address
	 * \param codedPackets The total number of coded packets
	 * \param embeddedPackets
	 * \param decodeSuccess True if the reception of a coded packet brang about a decoding success; false otherwise
	 */
	void InterFlowNetworkCodingLongTrace (Ptr<Packet> packet, u_int8_t tx, u_int32_t nodeId, Ipv4Address source,
			Ipv4Address destination, u_int8_t codedPackets, u_int8_t embeddedAcks, bool decodeSuccess = false);

	/*
	 * After the simulation, print the main statistics belonging to the inter flow network coding layer
	 */
	void PrintInterFlowNetworkCodingStatistics ();

	/*
	 * Enable and open the long tracing system belonging to the Network Coding layer,
	 * which will use the network monitor to perform the statistic studio of the simulation
	 */
	void EnableIntraFlowNetworkCodingLongTraceFile ();

	/*
	 * Enable and open the long tracing system belonging to the Network Coding layer,
	 * which will use the network monitor to perform the statistic studio of the simulation
	 */
	void EnableIntraFlowNetworkCodingShortTraceFile ();

	/*
	 * Receive a packet from the InterFlowNetworkCodingProtocol layer
	 * \param packet The packet itself
	 * \param tx 0 for a reception, 1 for a transmission at the TX nodes; 2 for a transmission (coded or not) at the coding router
	 * \param nodeId ID of the node which sent the signal to this member function
	 * \param source Packet's IP source address
	 * \param destination Packet's IP destination address
	 */
	void IntraFlowNetworkCodingLongTrace (Ptr<Packet> packet, u_int8_t tx, u_int32_t nodoId, Ipv4Address source, Ipv4Address destination);

	/*
	 * After the simulation, print the main statistics belonging to the inter flow network coding layer
	 */
	void PrintIntraFlowNetworkCodingStatistics ();

	/*
	 * Enable and open the long tracing system belonging to the Network Coding layer,
	 * which will use the network monitor to perform the statistic studio of the simulation
	 */
	void EnableWifiPhyLevelTracing ();

	/*
	 * Wifi Phy-level tracing (hooked at YansWifiPhy::EndReceive created callback)
	 * \param packet Pointer to the packet
	 * \param error An 1 means that the frame was successfully received; 0 otherwise
	 * \param snr Depending on the configured channel model, this value could lead to different parameters (i.e. SNR, HMP-state, etc.)
	 * \param nodeId ID of the node that uses the callback
	 */
	void WifiPhyRxTrace (Ptr<Packet> packet, bool error, double snr, int nodeId);

	/*
	 * Enable the usage of the legacy ns-3 FlowMonitor simulation behavior analysis
	 */
	 void EnableFlowMonitor ();


	/*
	 * Print the statistics gathered by the Flow Monitor
	 */
	void PrintFlowMonitorStats ();

	/**
	 * Print out the main statistic to the standard output (by default: terminal)
	 */
	void PrintToPrompt ();

	/*
	 * Take care of printing all the statistics upon the ending of a simulation
	 */
	void PrintStatistics ();

	/*
	 *  Makes the trace information container public (connection to the ConfigureScenario class)
	 *  \returns A reference to the struct that holds the information about the tracing system
	 */
	inline struct TracingInformation& GetTraceInfo () {return m_traceInfo;}

	/**
	 * Given a vector, calculate its average
	 * \param vector
	 * \returns The average value
	 */
	template <typename T1>
	double GetAverage(std::vector<T1> delayVector);
	template <typename T1, typename _InputIterator>
	double GetAverage(std::vector<T1> delayVector, _InputIterator __first, _InputIterator __last);
	/*
	 *  Calculate the variance of a given samples vector
	 *  \param delayVector The vector that contains the delay between consecutive receptions
	 *  \param average The average value, previously calculated
	 *  \returns The overall variance
	 */
	template <typename T1>
	double GetVariance(std::vector<T1> delayVector, double average);


private:
	//File name
	TracingInformation m_traceInfo;

	//Application layer statistics
	u_int32_t m_txPackets;
	u_int32_t m_rxPackets;

	//Lower layer statistics
	u_int32_t m_totalDataPackets;
	u_int32_t m_totalDataCorrectPackets;
	u_int32_t m_totalDataCorruptedPackets;

	//File handlers
	//Application level tracing
	fstream m_applicationLevelLongTraceFile;
	fstream m_applicationLevelShortTraceFile;

	//Network Coding level tracing
	fstream m_interFlowNetworkCodingLongFile;
	fstream m_interFlowNetworkCodingShortFile;
	fstream m_intraFlowNetworkCodingLongFile;
	fstream m_intraFlowNetworkCodingShortFile;

	//Wifi Phy Level tracing
	fstream m_phyWifiLevelTracing;

	//FlowMonitor handler
	bool m_flowMonitorEnabler;
	FlowMonitorHelper m_flowMonitorHelper;
	Ptr<FlowMonitor> m_flowMonitor;
};

}  //End namespace ns3

#endif /* PROPRIETARY_TRACING_H_ */
