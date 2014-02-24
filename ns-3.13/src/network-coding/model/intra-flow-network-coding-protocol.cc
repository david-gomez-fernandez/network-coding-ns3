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
 * Author: Eduardo Rodríguez Maza <eduardo.rodriguez@alumnos.unican.es>
 * 		   David Gómez Fernández <dgomez@tlmat.unican.es>
 *		   Ramón Agüero Calvo <ramon@tlmat.unican.es>
 */

#include "intra-flow-network-coding-protocol.h"

#include "ns3/config.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/llc-snap-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/udp-header.h"

#include "ns3/wifi-net-device.h"
#include "ns3/regular-wifi-mac.h"
#include "ns3/wifi-mac-queue.h"

#include <ctime>
#include <time.h>
#include <math.h>
#include <sys/time.h>

#include <cstdlib>
#include <iostream>
#include <bitset>

#include <vector>
#include <cmath>

#include "ns3/hash-id.h"

using namespace ns3;
using namespace std;
using namespace itpp;

NS_LOG_COMPONENT_DEFINE ("IntraFlowNetworkCodingProtocol");
NS_OBJECT_ENSURE_REGISTERED (IntraFlowNetworkCodingProtocol);

/* see http://www.iana.org/assignments/protocol-numbers */
const uint8_t IntraFlowNetworkCodingProtocol::PROT_NUMBER = 100;

double timeval_diff(struct timeval *a, struct timeval *b) // Calculates time in seconds
{
	return	(double) (a->tv_sec + (double) a->tv_usec / 1000000) -
			(double) (b->tv_sec + (double) b->tv_usec / 1000000);
}

IntraFlowNetworkCodingBufferItem::IntraFlowNetworkCodingBufferItem ()
{
	sourcePort=0;
	destinationPort=0;
}

IntraFlowNetworkCodingStatistics::IntraFlowNetworkCodingStatistics():  txNumber(0), rxNumber(0), downNumber(0), upNumber(0)
{
	timestamp.clear();
	rankTime.clear();
	inverseTime.clear();
}

IntraFlowNetworkCodingBufferItem::IntraFlowNetworkCodingBufferItem (Ptr<Packet> packet, Ipv4Address source, Ipv4Address destination, u_int16_t sourcePort, u_int16_t destinationPort):
packet(packet),
source(source),
destination(destination),
sourcePort(sourcePort),
destinationPort(destinationPort)
{
}

IntraFlowNetworkCodingMapParameters::IntraFlowNetworkCodingMapParameters ()
{
	m_k=0;
	m_rank=0;
	m_fragmentNumber=0;
	m_txCounter = 0;
	m_forwardingNode = false;
}

IntraFlowNetworkCodingMapParameters::~IntraFlowNetworkCodingMapParameters ()
{
	m_txBuffer.clear();
	m_rxBuffer.clear();
}

TypeId IntraFlowNetworkCodingProtocol::GetTypeId (void)
{

	static TypeId tid = TypeId ("ns3::IntraFlowNetworkCodingProtocol")
	.SetParent<NetworkCodingL4Protocol> ()
	.AddConstructor<IntraFlowNetworkCodingProtocol> ()
	.AddAttribute ("Q",
				"Size of the finite field GF(2^Q)",
				UintegerValue (1),
				MakeUintegerAccessor (&IntraFlowNetworkCodingProtocol::m_q),
				MakeUintegerChecker<u_int8_t> ())
	.AddAttribute ("K",
				"Fragment Size",
				UintegerValue (8),
				MakeUintegerAccessor (&IntraFlowNetworkCodingProtocol::m_k),
				MakeUintegerChecker<u_int16_t> ())
	.AddAttribute ("Recoding",
				"Differentiate between a Network coding and a Source coding solution",
				BooleanValue (false),
				MakeBooleanAccessor (&IntraFlowNetworkCodingProtocol::m_recode),
				MakeBooleanChecker ())
	.AddAttribute ("Itpp",
				"Enable/disable the use of the IT++ library to operate with GF(2) ",
				BooleanValue (true),
				MakeBooleanAccessor (&IntraFlowNetworkCodingProtocol::m_itpp),
				MakeBooleanChecker ())
	.AddAttribute ("BufferTimeout",
				"Time during which the protocol will wait until the buffer has at least K packets",
				TimeValue (MilliSeconds(1000)),
				MakeTimeAccessor (&IntraFlowNetworkCodingProtocol::m_bufferTimeout),
				MakeTimeChecker())
				;
	return tid;
}

IntraFlowNetworkCodingProtocol::IntraFlowNetworkCodingProtocol()
{
	NS_LOG_FUNCTION (this);
}

IntraFlowNetworkCodingProtocol::~IntraFlowNetworkCodingProtocol()
{
	NS_LOG_FUNCTION_NOARGS();
	m_mapParameters.clear();
}

void IntraFlowNetworkCodingProtocol::NotifyNewAggregate()
{
	NS_LOG_FUNCTION_NOARGS();
	if (m_node == 0)
	{
		Ptr<Node> node = this->GetObject<Node> ();

		if (node != 0)
		{
			//Connection with IP layer
			// Downstream: IntraFlowNetworkCodingProtocol::DownTargetCallback --> Ipv4L3Protocol::Send
			Ptr<Ipv4> ipv4 = this->GetObject<Ipv4> ();
			if (ipv4 != 0)
			{
				this->SetNode (node);

				ipv4->Insert (this);
				this->SetDownTarget (MakeCallback (&Ipv4::Send, ipv4));
				//m_moreParameters.ncBuffer.SetNode (node);									//Share the node address with the buffer (it will heavily ease the further handling of the packets)
			}

			//Connect to IpV4L3Protocol::IpForward
			Ptr<Ipv4L3Protocol> ipv4L3 = this->GetObject <Ipv4L3Protocol > ();
			if (ipv4L3)
			{
				ipv4L3->SetIpForwardCallback(MakeCallback(&IntraFlowNetworkCodingProtocol::ParseForwardingReception, this));
			}

			//Connection with UDP layer
			//	Downstream: UdpL4Protocol::Send --> IntraFlowNetworkCodingProtocol::ReceiveFromUpperLayer
			//  Upstream:   IntraFlowNetworkCodingProtocol::ForwardUp --> UdpL4Protocol::Receive
			if (Ptr<UdpL4Protocol> udp = node->GetObject <UdpL4Protocol> ())
			{
				udp->SetDownTarget (MakeCallback (&IntraFlowNetworkCodingProtocol::ReceiveFromUpperLayer, this));
				this->SetUpUdpTarget (MakeCallback (&UdpL4Protocol::Receive, udp));
			}

			//Connect to the WifiNetMacQueue and YansWifiPhy objects
			for (u_int8_t i=0; i<node->GetNDevices(); i++)
			{
				Ptr<WifiNetDevice> wifi = node->GetDevice(i)->GetObject<WifiNetDevice>();
				if (wifi)
				{
					wifi->SetPromiscReceiveCallback (MakeCallback(&NetworkCodingL4Protocol::ReceivePromiscuous, this));
					SetFlushWifiBufferCallback (MakeCallback (&WifiMacQueue::Flush,  DynamicCast<RegularWifiMac>(node->GetDevice(i)->GetObject<WifiNetDevice>()->GetMac())->GetDcaTxopPub()->GetQueue()));
					SetSelectiveFlushWifiBufferCallback (MakeCallback (&WifiMacQueue::SelectiveFlush,  DynamicCast<RegularWifiMac>(node->GetDevice(i)->GetObject<WifiNetDevice>()->GetMac())->GetDcaTxopPub()->GetQueue()));
				}
			}

			ostringstream os;
			os << (int) node->GetId();

			//Connect to the WifiPhy traced callback
			Config::ConnectWithoutContext ("/NodeList/" +  os.str() +"/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin",
					MakeCallback (&IntraFlowNetworkCodingProtocol::WifiBufferEvent, this));
		}
	}
	Object::NotifyNewAggregate ();
}

void IntraFlowNetworkCodingProtocol::ReceiveFromUpperLayer (Ptr<Packet> packet, Ipv4Address source, Ipv4Address destination, uint8_t protocol, Ptr<Ipv4Route> route)
{
	UdpHeader udpHeader;
	u_int16_t flowId;
	Ptr <IntraFlowNetworkCodingMapParameters> mapParameters;

	IntraFlowMapIterator it;

	if (packet-> GetSize() >250)
	{
		packet->PeekHeader (udpHeader);
		flowId= HashID (source, destination, udpHeader.GetSourcePort(), udpHeader.GetDestinationPort());

		it=m_mapParameters.find(flowId);
		if (it==m_mapParameters.end())
		{
			Ptr<IntraFlowNetworkCodingMapParameters> aux = CreateObject<IntraFlowNetworkCodingMapParameters> ();
			aux->m_k = m_k;

			aux->m_fragmentNumber = 0;
			aux->m_txCounter = 0;
			aux->m_forwardingNode = false;
			m_mapParameters.insert(pair<u_int16_t, Ptr<IntraFlowNetworkCodingMapParameters> >(make_pair(flowId, aux)));
		}

		it=m_mapParameters.find(flowId);
		mapParameters=it->second;

		mapParameters->m_txBuffer.push_back(IntraFlowNetworkCodingBufferItem(packet, source, destination, udpHeader.GetSourcePort(), udpHeader.GetDestinationPort()));

		if (mapParameters->m_txBuffer.size() < mapParameters->m_k)
		{
			if (!m_reduceBufferEvent.IsRunning())  // Used for the timer of ReduceBuffer() method which is created when there are less than k packets left
			{
				m_reduceBufferEvent = Simulator::Schedule (m_bufferTimeout, &IntraFlowNetworkCodingProtocol::ReduceBuffer, this, flowId);
			}
		}
		else // In case there are not enough packets to encode
		{
				Encode(flowId);
		}

		//Data packet received from the upper layer
		m_stats.downNumber ++;
	}
	else			// All datagrams < 250 bytes will be immediately delivered downwards
	{
		Ipv4L4Protocol::DownTargetCallback downTarget = GetDownTarget();
		downTarget (packet, source, destination, UdpL4Protocol::PROT_NUMBER, 0);
	}
}


void IntraFlowNetworkCodingProtocol::SendToLowerLayer(Ptr<Ipv4Route> rtentry, Ptr<const Packet> packet, const Ipv4Header &header)
{
	Ptr<Packet> sentPacket= packet->Copy();
	Ptr<Ipv4L3Protocol> ipv4 = m_node->GetObject <Ipv4L3Protocol > ();
	if (ipv4)
	{
		//Ptr<Packet> sentPacket = mapParameters->m_txBuffer[d].packet;
		ipv4->SendRealOutHook(rtentry, sentPacket, header);
	}
}



void IntraFlowNetworkCodingProtocol::DoDispose (void)
{
	NS_LOG_FUNCTION_NOARGS();
}

int IntraFlowNetworkCodingProtocol::GetProtocolNumber (void) const
{
	NS_LOG_FUNCTION_NOARGS();
	return PROT_NUMBER;
}

void IntraFlowNetworkCodingProtocol::Encode (u_int16_t flowId)
{
	NS_LOG_FUNCTION (Simulator::Now().GetSeconds() << this );

	IntraFlowMapIterator it;

	Ptr <IntraFlowNetworkCodingMapParameters> mapParameters;
	Ptr<Packet> codedPacket;
	IntraFlowNetworkCodingHeader ncHeader;
	std::vector<u_int8_t> randomVector;

	it=m_mapParameters.find(flowId);
	mapParameters=it->second;

	if ((mapParameters->m_txBuffer.size() >= mapParameters->m_k) && (mapParameters->m_txCounter <= 5) && (!mapParameters->m_forwardingNode))
	{
		if (m_reduceBufferEvent.IsRunning())
		{
			m_reduceBufferEvent.Cancel();
		}

//		if(mapParameters->m_txBuffer.size()>0)	// This is because sometimes the MORE buffer is empty
		{
			codedPacket = Create <Packet> (mapParameters->m_txBuffer[0].packet->GetSize()); // Packet creation with the buffer packet size

			ncHeader.SetK (mapParameters->m_k);
			ncHeader.SetQ (m_q);
			ncHeader.SetNfrag (mapParameters->m_fragmentNumber);
			ncHeader.SetTx (0);


			// As we are actually making use of an intra-flow coding, every datagram will be addressed to the same destination, hence it is not necessary to check all the source-destination tuples
			ncHeader.SetSourcePort (mapParameters->m_txBuffer[0].sourcePort);
			ncHeader.SetDestinationPort (mapParameters->m_txBuffer[0].destinationPort);

			GenerateRandomVector(mapParameters->m_k, randomVector);

			ncHeader.SetVector(randomVector);
			randomVector.clear (); // Erasure of the random vector
			codedPacket->AddHeader (ncHeader); // Adding the MORE header to the packet

			if (!m_ncCallback.IsNull())
			{
				m_ncCallback(codedPacket, 0, m_node->GetId(),mapParameters->m_txBuffer[0].source,mapParameters->m_txBuffer[0].destination);
			}

			Ipv4L4Protocol::DownTargetCallback downTarget = GetDownTarget();
			downTarget (codedPacket, mapParameters->m_txBuffer[0].source,mapParameters->m_txBuffer[0].destination, IntraFlowNetworkCodingProtocol::PROT_NUMBER, 0); // The node 0 is taken because there are only 2 nodes

			//Increase the transmission counter (Wifi buffer counter)
			mapParameters->m_txCounter++;
		}
	}
}

void IntraFlowNetworkCodingProtocol::Recode (u_int16_t flowId)
{
	IntraFlowMapIterator it;
	Ptr <IntraFlowNetworkCodingMapParameters> mapParameters;
	Ptr<Packet> codedPacket;
	IntraFlowNetworkCodingHeader ncHeader;
	std::vector<u_int8_t> randomVector;
	std::vector<u_int8_t> recodedVector;
	bool exit = false;

	int gf=pow(2,m_q);
	Field GF(gf);

	it=m_mapParameters.find (flowId);
	mapParameters=it->second;

	std::vector <u_int8_t> zeros (mapParameters->m_k, 0);
	NS_LOG_FUNCTION (Simulator::Now().GetSeconds() << this );
	if(mapParameters->m_txCounter <= 1 && mapParameters->m_rank >= 2)
	{
		if (m_reduceBufferEvent.IsRunning())
		{
			m_reduceBufferEvent.Cancel();
		}

		if(mapParameters->m_txBuffer.size()>0)	// This is because sometimes the MORE buffer is empty
		{
			codedPacket = Create <Packet> (mapParameters->m_txBuffer[0].packet->GetSize()); // Packet creation with the buffer packet size
			//moreHeader.SetProtocolNumber (17); // The number of protocol is established

			ncHeader.SetK (mapParameters->m_k);
			ncHeader.SetNfrag (mapParameters->m_fragmentNumber);
			ncHeader.SetTx (0);
			ncHeader.SetQ(m_q);

			// As we are actually making use of an intra-flow coding, every datagram will be addressed to the same destination, hence it is not necessary to check all the source-destination tuples
			ncHeader.SetSourcePort (mapParameters->m_txBuffer[0].sourcePort);
			ncHeader.SetDestinationPort (mapParameters->m_txBuffer[0].destinationPort);

			while(!exit)
			{
				GenerateRandomVector(mapParameters->m_k, randomVector);
				if(m_q==1 && m_itpp==1)
				{
					itpp::bvec recodedVectorItpp;
					itpp::bvec randomVectorItpp;

					for(u_int8_t i = 0; i < randomVector.size(); i++)
					{
						int valuen= randomVector [i];
						randomVectorItpp.ins (i,valuen);
					}

					recodedVectorItpp = mapParameters->m_vectorMatrix.transpose() * randomVectorItpp; // We use the transpose because itpp only has the operator to do "matrix*vector" and not "vector*matrix"
					for(u_int8_t i = 0; i < randomVector.size(); i++)
					{
						int value = (int)recodedVectorItpp [i];
						recodedVector.push_back(value);
					}
				}
				else
				{
					Field::Element *randomVectorGf=(Field::Element *) calloc(m_k, sizeof (Field::Element));
					Field::Element *recodedVectorGf=(Field::Element *) calloc(m_k, sizeof (Field::Element));

					for(u_int8_t i = 0; i < randomVector.size(); i++)
					{
						int valuen= randomVector [i];
						GF.init (randomVectorGf[i], valuen);
					}
					FFLAS::fgemm (GF, FflasNoTrans, FflasNoTrans, 1, mapParameters->m_k, mapParameters->m_k, 1,
							randomVectorGf, mapParameters->m_k, mapParameters->m_vectorMatrixGf,
							mapParameters->m_k, 0, recodedVectorGf, mapParameters->m_k);

					for(u_int8_t i = 0; i < randomVector.size(); i++)
					{
						int value = (int) recodedVectorGf [i];
						recodedVector.push_back (value);
					}
					free (randomVectorGf);
					free (recodedVectorGf);
				}
				if (recodedVector == zeros)
				{
					randomVector.clear ();
					recodedVector.clear();
				}
				else
				{
					exit = true;
				}
			}

			ncHeader.SetVector(recodedVector);
			randomVector.clear (); // Erasure of the random vector
			recodedVector.clear ();

			if (!m_ncCallback.IsNull())
			{
				m_ncCallback(codedPacket, 7, m_node->GetId(),mapParameters->m_txBuffer[0].source,mapParameters->m_txBuffer[0].destination);
			}

			codedPacket->AddHeader (ncHeader); // Adding the MORE header to the packet

			Ipv4L4Protocol::DownTargetCallback downTarget = GetDownTarget();
			downTarget (codedPacket, mapParameters->m_txBuffer[0].source,mapParameters->m_txBuffer[0].destination, IntraFlowNetworkCodingProtocol::PROT_NUMBER, 0); // The node 0 is taken because there are only 2 nodes

			//Increase the transmission counter (Wifi buffer counter)
			mapParameters->m_txCounter++;
		}
	}
}

void IntraFlowNetworkCodingProtocol::Decode(Ipv4Header const &header, Ptr<Ipv4Interface> incomingInterface, u_int16_t flowId)
{
	NS_LOG_FUNCTION_NOARGS();

	//Specific variable definition
	Field::Element *vectorMatrix_inverse=(Field::Element *) calloc(m_k * m_k, sizeof (Field::Element));
	Field::Element *vectorMatrixGf_copy=(Field::Element *) calloc(m_k * m_k, sizeof (Field::Element));
	Field::Element *zeroMatrix=(Field::Element *) calloc(m_k * m_k, sizeof (Field::Element));
	Field::Element *eye=(Field::Element *) calloc(m_k * m_k,  sizeof (Field::Element));
	int nullity;
	int gf=pow(2, m_q);
	Field GF(gf);

	struct timeval startTime, endTime;
	IntraFlowNetworkCodingBufferItem item;

	u_int16_t sourcePort=0;
	u_int16_t destinationPort=0;

	IntraFlowMapIterator it;
	Ptr <IntraFlowNetworkCodingMapParameters> mapParameters;

	it=m_mapParameters.find(flowId);
	mapParameters=it->second;

	if(m_q==1 && m_itpp==true)
	{
		gettimeofday(&startTime, NULL);
		GF2mat inverse = mapParameters->m_vectorMatrix.inverse();
		gettimeofday(&endTime, NULL);
	}
	else
	{

		FFLAS::fzero(GF, mapParameters->m_k, mapParameters->m_k, zeroMatrix, mapParameters->m_k);
		FFLAS::fadd(GF, mapParameters->m_k, mapParameters->m_k, mapParameters->m_vectorMatrixGf, mapParameters->m_k, zeroMatrix, mapParameters->m_k, vectorMatrixGf_copy, mapParameters->m_k);
		gettimeofday(&startTime, NULL);
		FFPACK::Invert(GF,mapParameters->m_k, vectorMatrixGf_copy, mapParameters->m_k, vectorMatrix_inverse, mapParameters->m_k, nullity);
		gettimeofday(&endTime, NULL);
		//FFLAS::fgemm(GF, FflasNoTrans, FflasNoTrans, mapParameters->m_k, mapParameters->m_k, mapParameters->m_k, 1, mapParameters->m_vectorMatrixGf, mapParameters->m_k, vectorMatrix_inverse, mapParameters->m_k, 0, eye, mapParameters->m_k);

		free (eye);
		free(vectorMatrixGf_copy);
		free(vectorMatrix_inverse);
		free(zeroMatrix);
	}
	m_stats.inverseTime.push_back(1000*timeval_diff(&endTime, &startTime)); 	// Inverse times in ms
	m_stats.timestamp.push_back(Simulator::Now().GetSeconds()); 				// The end time is the last one


	for (u_int8_t r=0; r < mapParameters->m_rxBuffer.size(); r++)					// Sending the packet to the upper layers
	{
		Ptr<Packet> copy = mapParameters->m_rxBuffer[r].packet->Copy(); 			// Copy of the packet with the MORE header

		IntraFlowNetworkCodingHeader ncHeader;

		mapParameters->m_rxBuffer[r].packet->RemoveHeader(ncHeader); 				// Remove the MORE header to send it to the upper layers

		sourcePort= mapParameters->m_rxBuffer[r].sourcePort;
		destinationPort= mapParameters->m_rxBuffer[r].destinationPort;

		UdpHeader udpHeader;
		mapParameters->m_rxBuffer[r].packet->RemoveHeader(udpHeader); 				// Remove the UDP header
		udpHeader.SetSourcePort(ncHeader.GetSourcePort());
		udpHeader.SetDestinationPort(ncHeader.GetDestinationPort());

		mapParameters->m_rxBuffer[r].packet->AddHeader(udpHeader);
		item =  mapParameters->m_rxBuffer[r];
		if (!m_ncCallback.IsNull())
		{
			m_ncCallback(copy, 4, m_node->GetId(), header.GetSource(), header.GetDestination());
		}
		m_upUdpTarget (item.packet,header,incomingInterface);

		//Increase the number of received packets
		m_stats.upNumber ++;
	}

	// Increase the ACK count
	(mapParameters->m_fragmentNumber)=(mapParameters->m_fragmentNumber)+1;

	SendAck (header.GetSource(), header.GetDestination(), sourcePort, destinationPort, true);
	for( int d = 0; d < mapParameters->m_k; d++)
	{
		mapParameters->m_rxBuffer.pop_back();
	}
}

void IntraFlowNetworkCodingProtocol::ReduceBuffer (u_int16_t flowId)
{
	IntraFlowMapIterator it;

	it=m_mapParameters.find(flowId);

	if (it != m_mapParameters.end())
	{
		it->second->m_k = it->second->m_txBuffer.size();
		Encode(flowId);
	}
}

void IntraFlowNetworkCodingProtocol::GenerateRandomVector (u_int16_t K, std::vector<u_int8_t>& randomVector)
{
	NS_LOG_FUNCTION_NOARGS();
	UniformVariable random (0, pow(2,m_q));

	// Since it would not perform any piece of valid information, all the null vectors will be automatically discarded,
	std::vector <u_int8_t> zeros ((K), 0);
	bool exit = false;							//Exit condition

	while (! exit) // Condition to avoid sending null vectors, it will be noticed for low k values
	{
		for (u_int16_t i=0 ; i < (K) ; i++ )
		{
			int value = random.GetValue();
			randomVector.push_back(value);
		}

		if (randomVector == zeros)
		{
			randomVector.clear();
		}
		else
		{
			exit = true;
		}
	}
}

enum Ipv4L4Protocol::RxStatus IntraFlowNetworkCodingProtocol::Receive (Ptr<Packet> packet, Ipv4Header const &header, Ptr<Ipv4Interface> incomingInterface)
{

	NS_LOG_FUNCTION (this);
	struct timeval startTime, endTime;
	IntraFlowNetworkCodingHeader ncHeader; 			   // Once we have the header of the arriving packet, it is necessary to insert the random vector in the matrix
	u_int16_t flowId;

	IntraFlowMapIterator it;
	Ptr <IntraFlowNetworkCodingMapParameters> mapParameters;

	Ptr<Packet> copy = packet->Copy(); // Copy of the arriving packet
	copy->RemoveHeader(ncHeader);    // Taking the MORE header of the packet

	flowId= HashID (header.GetSource(), header.GetDestination(), ncHeader.GetSourcePort(), ncHeader.GetDestinationPort());

	it = m_mapParameters.find (flowId);

	//If there is no flow registered with this flow ID, create a new entry.
	if (it == m_mapParameters.end())
	{
		Ptr<IntraFlowNetworkCodingMapParameters> aux = CreateObject<IntraFlowNetworkCodingMapParameters> ();

		//mapParameters=it->second;
		aux->m_k = 0;
		aux->m_rank = 0;
		aux->m_fragmentNumber = 0;

		ResetMatrices (flowId);

		m_mapParameters.insert(pair<u_int16_t, Ptr<IntraFlowNetworkCodingMapParameters> >(make_pair(flowId, aux)));
	}

	it = m_mapParameters.find(flowId);
	mapParameters = it->second;

	// Reception of a Data packet
	if ( ncHeader.GetTx() == 0)
	{
		//Variable definition
		Field::Element *headerVectorGf=(Field::Element *) calloc(m_k, sizeof (Field::Element));
		Field::Element *vectorMatrixGf_copy=(Field::Element *) calloc(m_k*m_k, sizeof (Field::Element));
		Field::Element *zeroMatrix=(Field::Element *) calloc(m_k*m_k, sizeof (Field::Element));
		int gf=pow(2,m_q);
		Field GF(gf);
		double secs;
		u_int8_t actualRank;
		itpp::bvec headerVector;
		headerVector.zeros();

		if(ncHeader.GetNfrag() >= mapParameters->m_fragmentNumber)
		{
			m_stats.rxNumber ++;
			std::vector <u_int8_t> randomVector;
			randomVector = ncHeader.GetVector(); // Get the vector in the header read in "deserialized"

			if(m_q==1 && m_itpp==true)				//IT++ library
			{
				for(u_int8_t i = 0; i < randomVector.size(); i++)
				{
					int valuen= randomVector[i];
					headerVector.ins (i,valuen);
				}

				if (mapParameters->m_vectorMatrix.row_rank() == mapParameters->m_k) // In case there are several fragments
				{
					mapParameters->m_k=ncHeader.GetK();
					mapParameters->m_vectorMatrix=GF2mat(mapParameters->m_k, mapParameters->m_k); 	// Overwriting with a new matrix of size mapParameters->m_k*mapParameters->m_k
					mapParameters->m_rank = 0; // Zeroing the matrix rank
				}

				mapParameters->m_vectorMatrix.set_row(mapParameters->m_rank, headerVector); // Once we have the packet header, the random vector is inserted in the matrix

				gettimeofday(&startTime, NULL);
				actualRank=mapParameters->m_vectorMatrix.row_rank();
				gettimeofday(&endTime, NULL);
			}
			else									//FFLAS-FFPACK library
			{
				FFLAS::fzero(GF, mapParameters->m_k, mapParameters->m_k, zeroMatrix, mapParameters->m_k);

				for(u_int8_t i = 0; i < randomVector.size(); i++)
				{
					int valuen= randomVector[i];
					GF.init(headerVectorGf[i], valuen);
				}

				FFLAS::fadd(GF, mapParameters->m_k, mapParameters->m_k, mapParameters->m_vectorMatrixGf,
						mapParameters->m_k, zeroMatrix, mapParameters->m_k, vectorMatrixGf_copy,
						mapParameters->m_k);
				if (FFPACK::Rank(GF, mapParameters->m_k, mapParameters->m_k, vectorMatrixGf_copy, mapParameters->m_k) == mapParameters->m_k) // In case there are several fragments
				{
					mapParameters->m_k=ncHeader.GetK();
					mapParameters->m_vectorMatrixGf=(Field::Element *) calloc (mapParameters->m_k * mapParameters->m_k, sizeof (Field::Element));
					mapParameters->m_rank = 0; // Zeroing the matrix rank
				}

				InsertRow(GF, mapParameters->m_vectorMatrixGf, headerVectorGf, mapParameters->m_rank, 1, mapParameters->m_k);
				free (headerVectorGf);

				FFLAS::fadd(GF, mapParameters->m_k, mapParameters->m_k, mapParameters->m_vectorMatrixGf,
						mapParameters->m_k, zeroMatrix, mapParameters->m_k, vectorMatrixGf_copy, mapParameters->m_k);
				gettimeofday (&startTime, NULL);
				actualRank=FFPACK::Rank(GF, mapParameters->m_k, mapParameters->m_k, vectorMatrixGf_copy, mapParameters->m_k);
				gettimeofday (&endTime, NULL);
			}

			if (!m_ncCallback.IsNull())
			{
				m_ncCallback(packet->Copy(), 2, m_node->GetId(), header.GetSource(), header.GetDestination());
			}
			secs = timeval_diff(&endTime, &startTime);
			m_stats.rankTime.push_back(secs*1000); // Time to calculate the rank

			if (actualRank > mapParameters->m_rank)  // Check the linear independence of the vector and the matrix by using the rank
			{
				mapParameters->m_rank++; // If it is linear independent the row is incremented to fill the next one
				mapParameters->m_rxBuffer.push_back (IntraFlowNetworkCodingBufferItem(packet, header.GetSource(),header.GetDestination(), ncHeader.GetSourcePort(), ncHeader.GetDestinationPort() ));
				if (actualRank == mapParameters->m_k)
				{
					Decode (header, incomingInterface, flowId); // The matrix is full and the inverse is made by the function Decode
				}
			}
		}
		else			//If the RX node receives a packet whose nFrag is lower than the one this entity is expecting,
						//it will immediately send a forced ACK, aiming to warn the TX node that it has not updated correctly
						//its fragment number
		{
			if (!m_ncCallback.IsNull())
			{
				m_ncCallback(packet->Copy(), 9, m_node->GetId(), header.GetSource(), header.GetDestination());
			}

			//Previously commented
			SendAck (header.GetSource(), header.GetDestination(), ncHeader.GetSourcePort(), ncHeader.GetDestinationPort(), false);
		}

		free(vectorMatrixGf_copy);
		free(zeroMatrix);
	}
	//Reception of an ACK
	else // If Tx=1,2, the packet is an ACK sent by the Rx
	{
		if (!m_ncCallback.IsNull())
		{
			m_ncCallback (packet, 3, m_node->GetId(), header.GetSource(), header.GetDestination());
		}

		if (ncHeader.GetTx() == 1  || ncHeader.GetTx() == 2)		//Upon the reception of a normal ACK, we will remove the corresponding fragment from the TX buffer
		{
			//Since the ACK comes backwards, we must invert the endpoints in order to get to correct hash
			flowId= HashID (header.GetDestination(), header.GetSource(), ncHeader.GetDestinationPort(), ncHeader.GetSourcePort());

			if(ncHeader.GetNfrag() > mapParameters->m_fragmentNumber)
			{
				ChangeFragment (ncHeader.GetNfrag(), flowId, false);
			}
		}
	}
	return Ipv4L4Protocol::RX_OK;
}

void IntraFlowNetworkCodingProtocol::ParseForwardingReception (Ptr<Ipv4Route> rtentry, Ptr<const Packet> packet, const Ipv4Header &header)
{
	NS_LOG_FUNCTION(this);

//	cout << Simulator::Now().GetSeconds() << " <-- ParseForwardingReception::IN -- " << (int) packet->GetSize() << endl;

	IntraFlowNetworkCodingHeader ncHeader;
	Ptr<Packet> copy= packet->Copy();

	u_int16_t flowId;
	itpp::bvec headerVector;
	u_int8_t actualRank;

	Ptr <IntraFlowNetworkCodingMapParameters> mapParameters;
	IntraFlowMapIterator it;

	std::vector <u_int8_t> vectr;

	copy->RemoveHeader (ncHeader);
	headerVector.zeros ();
	flowId = HashID (header.GetSource(), header.GetDestination(), ncHeader.GetSourcePort(), ncHeader.GetDestinationPort());

	// Map creation of a new flow ID
	it = m_mapParameters.find (flowId);
	if (it == m_mapParameters.end ())
	{
//		cout << "New fragment " << ncHeader <<  endl;
		Ptr<IntraFlowNetworkCodingMapParameters> aux = CreateObject<IntraFlowNetworkCodingMapParameters> ();
		aux->m_k = ncHeader.GetK(); 		//The intermediate node will known the fragment size from the value extracted from the NC header
		aux->m_rank = 0;
		aux->m_fragmentNumber = 0;
		aux->m_txCounter = 0;
		aux->m_forwardingNode = true;
		m_mapParameters.insert (pair<u_int16_t, Ptr<IntraFlowNetworkCodingMapParameters> >(make_pair(flowId, aux)));

		//Initialize the matrices
		if (m_recode)
		{
			ResetMatrices (flowId);
		}
	}

	it=m_mapParameters.find (flowId);
	mapParameters = it->second;

//	cout << Simulator::Now().GetSeconds() << " - " << (int) it->second->m_k << " - " << ncHeader.GetK() << endl;

	if(ncHeader.GetTx() == 0)	// Data packet
	{
		if(m_recode)
		{
			if(ncHeader.GetNfrag() >= mapParameters->m_fragmentNumber)
			{
				vectr = ncHeader.GetVector(); // Get the vector in the header read in "deserialized"

				if(m_q==1 && m_itpp==true)		//IT++ library
				{
					//Compose the random vector
					for(u_int8_t i=0; i < vectr.size(); i++)
					{
						int valuen= vectr[i];
						headerVector.ins(i,valuen);
					}

					if (mapParameters->m_vectorMatrix.row_rank() == mapParameters->m_k) // In case there are several fragments
					{
						mapParameters->m_k=ncHeader.GetK();
						mapParameters->m_vectorMatrix = GF2mat(mapParameters->m_k, mapParameters->m_k); 	// Overwriting with a new matrix of size mapParameters->m_k*mapParameters->m_k
						mapParameters->m_rank = 0; // Zeroing the matrix rank
					}

					mapParameters->m_vectorMatrix.set_row (mapParameters->m_rank, headerVector); // Once we have the packet header, the random vector is inserted in the matrix

					actualRank=mapParameters->m_vectorMatrix.row_rank();
				}
				else							//FFLAS-FFPACK library
				{
					//Variable definition
					int gf=pow(2,m_q);
					Field GF(gf);
					Field::Element *headerVectorGf=(Field::Element *) calloc (m_k, sizeof (Field::Element));
					Field::Element *vectorMatrixGf_copy=(Field::Element *) calloc (m_k*m_k, sizeof (Field::Element));
					Field::Element *zeroMatrix=(Field::Element *) calloc (m_k*m_k, sizeof (Field::Element));
					FFLAS::fzero(GF, mapParameters->m_k, mapParameters->m_k, zeroMatrix, mapParameters->m_k);

					//Compose the random vector
					for(u_int8_t d=0; d < vectr.size(); d++)
					{
						int valuen= vectr[d];
						GF.init(headerVectorGf[d], valuen);
					}

					FFLAS::fadd(GF, mapParameters->m_k, mapParameters->m_k, mapParameters->m_vectorMatrixGf, mapParameters->m_k, zeroMatrix, mapParameters->m_k, vectorMatrixGf_copy, mapParameters->m_k);
					if (FFPACK::Rank(GF, mapParameters->m_k, mapParameters->m_k, vectorMatrixGf_copy, mapParameters->m_k) == mapParameters->m_k) // In case there are several fragments
					{
						mapParameters->m_k = ncHeader.GetK();
						mapParameters->m_vectorMatrixGf = (Field::Element *) calloc(mapParameters->m_k * mapParameters->m_k, sizeof (Field::Element));
						mapParameters->m_rank = 0; // Zeroing the matrix rank
					}

					InsertRow(GF, mapParameters->m_vectorMatrixGf, headerVectorGf, mapParameters->m_rank, 1, mapParameters->m_k);
					free(headerVectorGf);

					FFLAS::fadd (GF, mapParameters->m_k, mapParameters->m_k, mapParameters->m_vectorMatrixGf, mapParameters->m_k, zeroMatrix, mapParameters->m_k, vectorMatrixGf_copy, mapParameters->m_k);
					actualRank=FFPACK::Rank(GF, mapParameters->m_k, mapParameters->m_k, vectorMatrixGf_copy, mapParameters->m_k);

					free (vectorMatrixGf_copy);
					free (zeroMatrix);
				}
				if (actualRank > mapParameters->m_rank && actualRank < mapParameters->m_k)  // Check the linear independence of the vector and the matrix using the rank
				{
					mapParameters->m_rank++; // If it is linear independent the row is incremented to fill the next one
					mapParameters->m_txBuffer.push_back (IntraFlowNetworkCodingBufferItem(copy, header.GetSource(),header.GetDestination(), ncHeader.GetSourcePort(), ncHeader.GetDestinationPort() ));

					if (!m_ncCallback.IsNull())
					{
						m_ncCallback(copy, 6, m_node->GetId(),mapParameters->m_txBuffer[0].source,mapParameters->m_txBuffer[0].destination);
					}
					if (actualRank >= 2 && mapParameters->m_txCounter==0)
					//if (actualRank >= (int) (m_k / 2.0) && mapParameters->m_txCounter==0)
					{
						Recode (flowId);
					}
				}
			}
			else
			{
				SendAck (header.GetSource(), header.GetDestination(), ncHeader.GetSourcePort(), ncHeader.GetDestinationPort(), false);
				SelectiveFlushWifiBuffer(flowId);
			}

		}
		else // RLSC scheme --> Just forward
		{
			// Refresh the fragment number and deliver the packets to the lower layers
			if  (ncHeader.GetNfrag()>=mapParameters->m_fragmentNumber)
			{
				Ptr<Ipv4L3Protocol> ipv4 = m_node->GetObject <Ipv4L3Protocol > ();
				if (ipv4)
				{
					Ptr<Packet> forwardedPacket= packet->Copy();
					ipv4->SendRealOutHook(rtentry, forwardedPacket, header);
					mapParameters->m_txCounter ++;
				}
			}
			else
			{
				SelectiveFlushWifiBuffer (flowId);		// Flushing the correct flow of data of the buffer
			}
		}
	}
	else // ACK
	{
 		flowId = HashID (header.GetDestination(), header.GetSource(), ncHeader.GetDestinationPort(), ncHeader.GetSourcePort());
 		it = m_mapParameters.find(flowId);

		if(ncHeader.GetNfrag() > it->second->m_fragmentNumber)
		{
			ChangeFragment (ncHeader.GetNfrag(), flowId, true);
		}
		//Forward the packet
		Ptr<Ipv4L3Protocol> ipv4 = m_node->GetObject <Ipv4L3Protocol > ();
		if (ipv4)
		{
			Ptr<Packet> forwardedPacket = packet->Copy();
			ipv4->SendRealOutHook (rtentry, forwardedPacket, header);
		}
	}
//	cout << Simulator::Now().GetSeconds() << " --> ParseForwardingReception::OUT " << (int) packet->GetSize() << endl;
}

void IntraFlowNetworkCodingProtocol::ChangeFragment (u_int16_t nFrag, u_int16_t flowId, bool forwardingNode)
{
	NS_LOG_FUNCTION (this);

//	cout << "--- Change fragment::IN - " << std::hex << flowId << std::dec << endl;

	IntraFlowMapIterator it;
	Ptr <IntraFlowNetworkCodingMapParameters> mapParameters;

	it=m_mapParameters.find(flowId);
	mapParameters=it->second;

	if (forwardingNode)
	{
		mapParameters->m_txBuffer.clear();

		mapParameters->m_fragmentNumber=nFrag;			// Taking the number of fragment
		mapParameters->m_rank= 0;						// Zeroing the rank of the matrix

		//Start over the matrices (only with a RLNC scheme)
		if (m_recode)
		{
			ResetMatrices (flowId);
		}
		mapParameters->m_txCounter=0;

		SelectiveFlushWifiBuffer(flowId);

//		cout << "ERASE " << endl;
	}
	//Source nodes operation
	else
	{
		if (mapParameters->m_txBuffer.size() && nFrag > mapParameters->m_fragmentNumber)			// Erasure of the buffer
		{

			//Buffer erasure of the already-decoded fragment
			for( u_int16_t i=0; i < mapParameters->m_k; i++)
			{
				mapParameters->m_txBuffer.erase (mapParameters->m_txBuffer.begin ());
			}
		}

		if ( mapParameters->m_txBuffer.size() >= mapParameters->m_k)
		{
			mapParameters->m_fragmentNumber = nFrag; 	// It is necessary to refresh the number of fragment
		}
		else if (mapParameters->m_txBuffer.size() < mapParameters->m_k)
		{
			if(mapParameters->m_txBuffer.size() != 0)
			{
				mapParameters->m_fragmentNumber = nFrag; 	// It is necessary to refresh the number of fragment
				//mapParameters->m_fragmentNumber++;
				m_reduceBufferEvent = Simulator::Schedule (m_bufferTimeout, &IntraFlowNetworkCodingProtocol::ReduceBuffer, this, flowId); // It is necessary to include a timer to send the rest of the packets in case there are less than mapParameters->m_k. There is a specific function for that purpose.
			}
			else
			{
				//NS_LOG_UNCOND("THERE ARE NO MORE PACKETS");
			}
		}

		//Restore the legacy value of K (if had changed before by a ReduceBuffer call)
		mapParameters->m_k = atoi (IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(1).initialValue->SerializeToString (MakeUintegerChecker<u_int16_t> ()).c_str());
		mapParameters->m_txCounter = 0;
		SelectiveFlushWifiBuffer(flowId);
		Encode (flowId);
	}
//	SelectiveFlushWifiBuffer(flowId);

//	cout << "--- Change fragment::OUT - " << std::hex << flowId << std::dec << endl;
}

void IntraFlowNetworkCodingProtocol::SendAck (Ipv4Address source, Ipv4Address destination, u_int16_t sourcePort, u_int16_t destinationPort, bool normalAck)
{

	NS_LOG_FUNCTION_NOARGS ();
	Ptr<Packet> packet; // Packet creation
	packet = Create <Packet> (0);
	IntraFlowNetworkCodingHeader ncHeader;
	IntraFlowNetworkCodingBufferItem item;
	u_int16_t flowId;

	IntraFlowMapIterator it;
	Ptr <IntraFlowNetworkCodingMapParameters> mapParameters;

	//Different value according to the type of message
	// 1- Normal ACK
	// 2- Backward request (this message is triggered when the TX fragment number overlaps the receiver's one.
	flowId = HashID (source, destination, sourcePort, destinationPort);

	it=m_mapParameters.find(flowId);
	mapParameters=it->second;

	ncHeader.SetK (0);
	// K=0 because there is no vector in the MORE header so it only reads the useful fields
	ncHeader.SetNfrag (mapParameters->m_fragmentNumber);
	ncHeader.SetTx (normalAck ? 1 : 2);
	//Invert the port in order to get the correct HASH (other side)
	ncHeader.SetSourcePort (destinationPort);
	ncHeader.SetDestinationPort (sourcePort);
	packet->AddHeader (ncHeader); // Add the header to the packet
	if (!m_ncCallback.IsNull())
	{
		m_ncCallback(packet, 5, m_node->GetId(), destination, source);
	}

	Ipv4L4Protocol::DownTargetCallback downTarget = GetDownTarget();
	downTarget (packet, destination, source, IntraFlowNetworkCodingProtocol::PROT_NUMBER, 0); // Change the source and established the destination
}

bool IntraFlowNetworkCodingProtocol::ReceivePromiscuous (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, const Address &from, const Address &to, NetDevice::PacketType packetType)
{
	NS_LOG_FUNCTION_NOARGS();

	//Check whether the packet is headed to us
	//First, we do need to get the destination IP address from the IP header
	Ptr<Packet> copy = packet->Copy();
	Ipv4Header ipHeader;

	switch (protocol)
	{
	case 0x800:
		copy->RemoveHeader (ipHeader);
		//Second -> Check whether this object belongs to the destination address
		if (AmIDestination (ipHeader.GetDestination()) && packetType != NetDevice::PACKET_HOST)
		{
			//Third, process only those packets that carry IntraFlowNetworkCodingProtocol
			switch (ipHeader.GetProtocol())
			{
			case IntraFlowNetworkCodingProtocol::PROT_NUMBER:
			{
				IntraFlowNetworkCodingHeader ncHeader;
				Ptr<Packet> copy2=copy->Copy();
				copy->RemoveHeader(ncHeader);

//				//In case we are receiving a IntraFlowNetworkCodingProtocol packet, we need to map from the NetDevice (provided by this function)
//				//to an Ipv4Interface, in order to forward up the packet
//				cout << Simulator::Now().GetSeconds() << " :Packet forwarded up " << (int) m_node->GetId() << " " <<
//						m_node->GetObject<Ipv4L3Protocol>()->GetInterface (m_node->GetObject<Ipv4L3Protocol>()->GetInterfaceForDevice(device))->GetAddress(0) << endl;
//				m_upUdpTarget (copy, ipHeader, m_node->GetObject<Ipv4L3Protocol>()->GetInterface (m_node->GetObject<Ipv4L3Protocol>()->GetInterfaceForDevice(device)));

				//Send to the legacy receive code
				//if(&& moreHeader.GetTx()==0)
				{
					Receive (copy2, ipHeader, m_node->GetObject<Ipv4L3Protocol>()->GetInterface (m_node->GetObject<Ipv4L3Protocol>()->GetInterfaceForDevice(device)));
				}

				break;
			}
			default:
				break;
			}
		}
		break;
	default:
		break;
	}
	return true;
}

bool IntraFlowNetworkCodingProtocol::AmIDestination(const Ipv4Address &destination)
{
	NS_LOG_FUNCTION(this);
	return (destination == m_node->GetObject<Ipv4 > ()->GetAddress(1, 0).GetLocal());
}

void IntraFlowNetworkCodingProtocol::FlushWifiBuffer ()
{
	m_flushCallback ();
}

void IntraFlowNetworkCodingProtocol::SelectiveFlushWifiBuffer (u_int16_t flowId)
{
	m_selectiveFlushCallback (flowId);
	Encode (flowId);
}

void IntraFlowNetworkCodingProtocol::WifiBufferEvent (Ptr<const Packet> packet)
{
	WifiMacHeader macHeader;
	LlcSnapHeader llcHeader;
	Ipv4Header ipHeader;
	IntraFlowNetworkCodingHeader ncHeader;
	UdpHeader udpHeader;

	u_int16_t flowId;

	Ptr<Packet> copy = packet->Copy();

	copy->RemoveHeader(macHeader);

	//Identify flows in order to keep track of the WifiMacQueue size
	if (macHeader.IsData() && !macHeader.GetAddr1().IsBroadcast() && !macHeader.IsRetry())
	{
		copy->RemoveHeader (llcHeader);
		switch (llcHeader.GetType())
		{
		case 0x0806:            //ARP
			break;
		case 0x0800:            //IP packet
			copy->RemoveHeader(ipHeader);
			switch (ipHeader.GetProtocol())
			{
			case 6:             //TCP
			case 17:            //UDP
			case 99:    //Network Coding --> Force the shortest frames to be correct
				break;
			case 100:
			{
				copy->RemoveHeader(ncHeader);
				if (ncHeader.GetTx() == 0)        //Data packets
				{
					//Look up if the output packet is already stored into any of the buffers
					flowId = HashID (ipHeader.GetSource(), ipHeader.GetDestination(), ncHeader.GetSourcePort(), ncHeader.GetDestinationPort());
					IntraFlowMapIterator iter = m_mapParameters.find(flowId);

					if (iter != m_mapParameters.end())
					{
						iter->second->m_txCounter --;

//						if (! iter->second->m_txCounter)
						{
							if(iter->second->m_forwardingNode)
							{
								Recode(flowId);
							}
							else
							{
								Encode(flowId);
							}
						}
						//Increase the statistics counter (tracing purposes)
						m_stats.txNumber ++;
					}
				}

				break;
			}
			default:
				break;
			}
			break;
			default:
				break;
		}
	}
}

void IntraFlowNetworkCodingProtocol::ResetMatrices (u_int16_t flowId)
{
	IntraFlowMapIterator iter = m_mapParameters.find (flowId);

	if (iter != m_mapParameters.end())
	{
		if(m_q==1 && m_itpp==true)						// Overwriting with the new matrix
		{
			iter->second->m_vectorMatrix = GF2mat (iter->second->m_k, iter->second->m_k);
		}
		else
		{
//			if (iter->second->m_vectorMatrixGf)
//			{
//				free (iter->second->m_vectorMatrixGf);
//			}
			iter->second->m_vectorMatrixGf = (Field::Element *) calloc(iter->second->m_k * iter->second->m_k, sizeof (Field::Element));
		}

		iter->second->m_rank = 0;
	}

}


