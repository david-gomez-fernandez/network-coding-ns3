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

#ifndef SCRATCH_LOGGING_H_
#define SCRATCH_LOGGING_H_

#include "ns3/core-module.h"

namespace ns3 {

void EnableLogging () {

//	LOGGING   --> Uncomment desired files to show log messages
//	-- Physical layer
//	LogComponentEnable ("WifiPhy", LOG_ALL);
//	LogComponentEnable ("YansWifiPhy", LOG_FUNCTION);
//	LogComponentEnable ("YansWifiChannel", LOG_FUNCTION);
//	LogComponentEnable ("YansWifiHelper", LOG_ALL);
//	LogComponentEnable ("DcaTxop", LOG_ALL);
//	LogComponentEnable ("PropagationLossModel", LOG_ALL);
//	LogComponentEnable ("WifiPhyStateHelper", LOG_LEVEL);
//	LogComponentEnable ("HiddenMarkovPropagationLossModel", LOG_ALL);
//	LogComponentEnable ("HiddenMarkovErrorModel", LOG_ALL);
//	LogComponentEnable ("HiddenMarkovModelEntry", LOG_ALL);
//	LogComponentEnable ("YansErrorRateModel", LOG_ALL);
//	LogComponentEnable ("DcfManager", LOG_DEBUG);

//	-- Link Level
//	LogComponentEnable ("MacLow", LOG_ALL);
//	LogComponentEnable ("MacRxMiddle", LOG_ALL);
//	LogComponentEnable ("WifiMacQueue", LOG_FUNCTION);
//	LogComponentEnable ("WifiMacQueue", LOG_ALL);
//	LogComponentEnable ("DcaTxop", LOG_FUNCTION);
//	LogComponentEnable ("DcfManager", LOG_FUNCTION);
//	LogComponentEnable ("RegularWifiMac",LOG_FUNCTION);
//	LogComponentEnable ("RegularWifiMac",LOG_ALL);
//	LogComponentEnable ("StaWifiMac",LOG_FUNCTION);
//	LogComponentEnable ("AdhocWifiMac",LOG_ALL);
//	LogComponentEnable ("WifiNetDevice", LOG_ALL);

//	--ARP
//	LogComponentEnable ("ArpL3Protocol", LOG_ALL);
//        LogComponentEnable ("ArpCache", LOG_ALL);

//	-- Network layer
//	LogComponentEnable ("Ipv4L3Protocol", LOG_ALL);
//	LogComponentEnable ("Ipv4Interface", LOG_ALL);
//	LogComponentEnable ("InternetStackHelper", LOG_ALL);

//	--Network Coding layer
//	LogComponentEnable ("NetworkCodingL4Protocol", LOG_DEBUG);
//	LogComponentEnable ("InterFlowNetworkCodingHeader", LOG_DEBUG);
//	LogComponentEnable ("InterFlowNetworkCodingBuffer", LOG_DEBUG);
//	LogComponentEnable ("InterFlowNetworkCodingProtocol", LOG_DEBUG);
//	LogComponentEnable ("IntraFlowNetworkCodingHeader", LOG_ALL);
//	LogComponentEnable ("IntraFlowNetworkCodingProtocol", LOG_FUNCTION);


//	-- Transport layer
//	LogComponentEnable ("TcpL4Protocol", LOG_ALL);
//	LogComponentEnable ("NscTcpL4Protocol", LOG_FUNCTION);
//	LogComponentEnable ("UdpL4Protocol", LOG_ALL);
//	LogComponentEnable ("UdpSocket", LOG_ALL);
//	LogComponentEnable ("UdpSocketImpl", LOG_ALL);
//  LogComponentEnable ("TcpNewReno", LOG_INFO);
//  LogComponentEnable ("TcpReno", LOG_ALL);
//  LogComponentEnable ("TcpTahoe", LOG_ALL);
//  LogComponentEnable ("TcpSocketBase", LOG_ALL);
        
//	-- Propagation Loss Models
//	LogComponentEnable("PropagationLossModel", LOG_ALL);

//	-- Error Models
//	LogComponentEnable("ErrorModel", LOG_DEBUG);
//	LogComponentEnable("HiddenMarkovErrorModel", LOG_INFO);
//	LogComponentEnable("ArModel", LOG_LOGIC);
//	LogComponentEnable("ArModel", LOG_ALL);

//	-- Test unit
//	LogComponentEnable("Experiment", LOG_DEBUG);

//	--	Other levels
//	LogComponentEnable ("Socket", LOG_ALL);
//	LogComponentEnable ("PacketSocket", LOG_ALL);
//	LogComponentEnable ("TcpSocketBase", LOG_ALL);
//	LogComponentEnable ("TcpReno", LOG_ALL);
//	LogComponentEnable ("TcpNewReno", LOG_ALL);


//	-- Routing
//	LogComponentEnable ("Ipv4StaticRouting", LOG_ALL);

//  -- Application
//	LogComponentEnable ("OnOffApplication", LOG_ALL);
//	LogComponentEnable ("PacketSink", LOG_INFO);
//  LogComponentEnable ("UdpEchoClientApplication", LOG_ALL);
//  LogComponentEnable ("UdpEchoServerApplication", LOG_ALL);

//	--Abstract information
//	LogComponentEnable ("Node", LOG_ALL);

//	-- Configure Scenario
//	LogComponentEnable ("ConfigureScenario", LOG_DEBUG);
//	LogComponentEnable ("ProprietaryTracing", LOG_DEBUG);
//	LogComponentEnable ("ConfigurationFile", LOG_ALL);
//	LogComponentEnable ("NetworkMonitor", LOG_ALL);

}

} //namespace ns3
#endif /* SCRATCH_LOGGING_H_ */
