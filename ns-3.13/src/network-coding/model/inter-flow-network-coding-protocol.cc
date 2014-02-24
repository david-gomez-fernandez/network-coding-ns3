/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 Universidad de Cantabria
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

#include <assert.h>

#include "ns3/object-factory.h"
#include "ns3/type-id.h"
#include "ns3/packet.h"
#include "ns3/wifi-net-device.h"
#include "ns3/hash-id.h"

#include "inter-flow-network-coding-protocol.h"


using namespace ns3;
using namespace std;


NS_LOG_COMPONENT_DEFINE("InterFlowNetworkCodingProtocol");
NS_OBJECT_ENSURE_REGISTERED(InterFlowNetworkCodingProtocol);

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                                   \
		if (m_node) { std::clog << Simulator::Now ().GetSeconds () << " [node " << m_node->GetId () << "] "; }

/* see http://www.iana.org/assignments/protocol-numbers */
const u_int8_t InterFlowNetworkCodingProtocol::PROT_NUMBER = 99;

//In order to avoid the coding of non-desired upper-layer PDUs (i.e. TCP ACKs)
const u_int16_t InterFlowNetworkCodingProtocol::MIN_CODING_LENGTH = 750;

NetworkCodingEndPoint::NetworkCodingEndPoint(Ipv4Address source,
		Ipv4Address destination,
		u_int16_t sourcePort,
		u_int16_t destinationPort) :
		source(source),
		destination(destination),
		sourcePort(sourcePort),
		destinationPort(destinationPort)
{
}

InterFlowNetworkCodingProtocol::InterFlowNetworkCodingProtocol()
{
	NS_LOG_FUNCTION_NOARGS();

	//We need to manually assign the functionalities (WIP - Automate the process for random scenarios)
	m_codingNode = false;
	m_embeddedAcks = false;

	m_ncBuffer = CreateObject <InterFlowNetworkCodingBuffer> ();

	//Network Coding Buffers hook (namely, when this callback invokes the method, the NC layer will extract the first packet from the output buffer
	m_ncBuffer->SetSendDownCallback (MakeCallback (&InterFlowNetworkCodingProtocol::SendDown, this));
	//ACK scheme callback connection
	m_ncBuffer->SetSendDownAckCallback (MakeCallback (&InterFlowNetworkCodingProtocol::SendDownTcpAck, this));
}

InterFlowNetworkCodingProtocol::~InterFlowNetworkCodingProtocol()
{
	NS_LOG_FUNCTION_NOARGS();
	m_endPointTable.clear();
}

TypeId InterFlowNetworkCodingProtocol::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::InterFlowNetworkCodingProtocol")
            .SetParent<Ipv4L4Protocol > ()
            .AddConstructor<InterFlowNetworkCodingProtocol > ()
            ;
    return tid;
}

void InterFlowNetworkCodingProtocol::NotifyNewAggregate()
{
    if (m_node == 0)
    {
        Ptr<Node> node = this->GetObject<Node > ();

        if (node != 0)
        {
            Ptr<Ipv4> ipv4 = this->GetObject<Ipv4 > ();
            if (ipv4 != 0)
            {
                this->SetNode (node);
                ipv4->Insert (this);
                this->SetDownTarget (MakeCallback(&Ipv4::Send, ipv4));
                m_ncBuffer->SetNode (node); //Share the node address with the buffer (it will heavily ease the further handling of the packets)
            }

            //Connect to IpV4L3Protocol::IpForward
            Ptr<Ipv4L3Protocol> ipv4L3 = this->GetObject <Ipv4L3Protocol > ();
            if (ipv4L3)
            {
                ipv4L3->SetIpForwardCallback(MakeCallback(&InterFlowNetworkCodingProtocol::ParseForwardingReception, this));
            }

            //Check Ipv4L4Protocol objects instanced
            //Hook to upper layers--> Bidirectional
            //NOTE: We will catch the packets from the following Ipv4L4Protocol objects, hence we need two different callbacks
            if (Ptr<TcpL4Protocol> tcp = node->GetObject <TcpL4Protocol> ())
            {
                tcp->SetDownTarget(MakeCallback(&InterFlowNetworkCodingProtocol::ReceiveFromUpperLayer, this));
                this->SetUpTcpTarget(MakeCallback(&TcpL4Protocol::Receive, tcp));
            }

//            if (Ptr<UdpL4Protocol> udp = node->GetObject <UdpL4Protocol> ())
//            {
//            	udp->SetDownTarget (MakeCallback (&InterFlowNetworkCodingProtocol::ReceiveFromUpperLayer, this));
//            	this->SetUpUdpTarget (MakeCallback (&UdpL4Protocol::Receive, udp));
//            }

            //Sweep the NetDevices and connect to the promiscuous reception callback
    		// IMPORTANT: Configure the promiscuous nodes
    		//Connect the nodes to a promiscuous reception
    		for (u_int32_t i = 0; i < node->GetNDevices(); i++)
    		{
    			//Aggregate only for WifiNetDevice objects
    			if (node->GetDevice(i)->GetObject<WifiNetDevice> ())
    			{
    				Ptr<WifiNetDevice> wifi = node->GetDevice(i)->GetObject<WifiNetDevice> ();
    				wifi->SetPromiscReceiveCallback (MakeCallback(&NetworkCodingL4Protocol::ReceivePromiscuous, this));
    			}
    		}

        }

//        //Connect to the TcpRxBuffer
//        Config::Connect ("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/NextTxSequence", MakeCallback (&NetworkCodingBuffer::TcpSequenceNumber, m_ncBuffer));
//        Config::Connect ("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/$ns3::TcpNewReno/CongestionWindow", MakeCallback (&NetworkCodingBuffer::TcpSequenceNumber, m_ncBuffer));

    }
    Object::NotifyNewAggregate();
}

void InterFlowNetworkCodingProtocol::SetNode(Ptr<Node> node)
{
	m_node = node;
}

Ptr<Node> InterFlowNetworkCodingProtocol::GetNode()
{
    return m_node;
}

Ptr<Packet> InterFlowNetworkCodingProtocol::Encode(Ptr<Packet> pkt1, Ptr<Packet> pkt2)
{
    u_int8_t *buffer1, *buffer2, *outputBuffer;
    u_int32_t i, max;
    Ptr<Packet> codedPkt;

    //Zero padding with the shortest packet
    if (pkt1->GetSize() > pkt2->GetSize())
        max = pkt1->GetSize();
    else
        max = pkt2->GetSize();

    //Get the payload of both packets
    buffer1 = (u_int8_t *) malloc (max);
    buffer2 = (u_int8_t *) malloc (max);
    outputBuffer = (u_int8_t *) malloc (max);
    memset (buffer1, 0, max);
    memset (buffer2, 0, max);
    memset (outputBuffer, 0, max);
    pkt1 -> CopyData (buffer1, (signed) pkt1->GetSize());
    pkt2 -> CopyData (buffer2, (signed) pkt2->GetSize());
    DBG_DUMP ("ENC1", buffer1, (signed) max);
    DBG_DUMP ("ENC2", buffer2, (signed) max);

    //XOR operation
    for (i = 0; i < max; i++)
    {
        outputBuffer [i] = buffer1[i] ^ buffer2 [i];
    }

    DBG_DUMP("OUT", outputBuffer, (signed) max);
    codedPkt = Create<Packet > (outputBuffer, max);

    free (buffer1);
    free (buffer2);
    free (outputBuffer);

    return codedPkt;
}

Ptr<Packet> InterFlowNetworkCodingProtocol::Decode(Ptr<Packet> codedPkt, Ptr<Packet> nativePkt)
{
    u_int8_t *buffer1, *buffer2, *outputBuffer;
    u_int32_t i, max;
    Ptr<Packet> outputPkt;

    // Zero padding with the shortest packet
    // NOTE: Theoretically, a decodable native packet will never be longer than a coded packet
    if (codedPkt->GetSize() > nativePkt->GetSize())
        max = codedPkt->GetSize();
    else
        max = nativePkt->GetSize();

    //Get the payload of both packets
    buffer1 = (u_int8_t *) malloc(max);
    buffer2 = (u_int8_t *) malloc(max);
    outputBuffer = (u_int8_t *) malloc(max);
    memset(buffer1, 0, max);
    memset(buffer2, 0, max);
    memset(outputBuffer, 0, max);
    codedPkt->CopyData(buffer1, (signed) codedPkt->GetSize());
    nativePkt->CopyData(buffer2, (signed) nativePkt->GetSize());
    DBG_DUMP("DEC1", buffer1, (signed) max);
    DBG_DUMP("DEC2", buffer2, (signed) max);

    //XOR operation
    for (i = 0; i < max; i++)
    {
        outputBuffer [i] = buffer1 [i] ^ buffer2 [i];
    }

    DBG_DUMP("OUT", outputBuffer, (signed) max);
    outputPkt = Create<Packet> (outputBuffer, max);

    free (buffer1);
    free (buffer2);
    free (outputBuffer);

    return outputPkt;
}

void InterFlowNetworkCodingProtocol::ReceiveFromUpperLayer (Ptr<Packet> packet, Ipv4Address source, Ipv4Address destination, uint8_t protocol, Ptr<Ipv4Route> route)
{
	NS_LOG_FUNCTION(Simulator::Now().GetSeconds() << packet->GetSize() << (int) protocol);

	NS_LOG_INFO("Received a native packet from the upper layer " << packet);

	//Update the route list
	if (m_routes.find(route) == m_routes.end())
		m_routes.insert(route);

	//Parse the received packet and act accordingly
	switch (protocol)
	{
	case 6:  //TcpL4Protocol::PROT_NUMBER
	{
		//Check if the packet contain a session establishment
		TcpHeader tcpHeader;
		packet->PeekHeader(tcpHeader);

		//Catch the TCP flows through the SYN or SYN + ACK packets to update the EndPoint hash table
		if (tcpHeader.GetFlags() & TcpHeader::SYN)
		{
			NS_LOG_INFO((int) m_node->GetId() << " --> SYN: " << source << " > " << destination << " Flags: 0x"
					<< std::hex << (int) tcpHeader.GetFlags() << std::dec << " Packet size " << (int) packet->GetSize());

			UpdateEndPointTable (NetworkCodingEndPoint (source, destination, tcpHeader.GetSourcePort(), tcpHeader.GetDestinationPort()));
		}

		InterFlowNetworkCodingHeader ncHeader;
		u_int16_t hash = HashID (source, destination, tcpHeader.GetSourcePort(), tcpHeader.GetDestinationPort());

		packet->PeekHeader (tcpHeader);
		ncHeader.SetProtocolNumber (TcpL4Protocol::PROT_NUMBER); //This solution only encapsulates (by the moment) TCP data packets
		ncHeader.SetPacketType (0);
		ncHeader.SetCodedPackets (1);

		ncHeader.m_packetVector.push_back (InterFlowNetworkCodingHeader::Item(destination, tcpHeader.GetSequenceNumber(), hash, packet->GetSize()));

		if (packet->GetSize() > MIN_CODING_LENGTH)
		{
			m_ncBuffer->UpdateDecodingBuffer(packet, source, destination, TcpL4Protocol::PROT_NUMBER);
		}

		packet->AddHeader (ncHeader);
		m_downTarget (packet, source, destination, InterFlowNetworkCodingProtocol::PROT_NUMBER, route);

		//Trace the results
		if (!m_interFlowNetworkCodingCallback.IsNull ())
		{
			m_interFlowNetworkCodingCallback (packet, 2, m_node->GetId(), source, destination, ncHeader.GetCodedPackets(),
					ncHeader.GetEmbeddedAcks(), false);
		}

		//Remove the input packet pool (only at coding nodes)
		NS_LOG_INFO ("Sent a native packet " << " Header " << ncHeader);

		//Update statistics
		if (m_codingNode)
		{
			m_ncStatistics.uncodedPacketTx ++;
		}

		m_ncStatistics.transmissionNumber ++;

		break;
	}
	case 17:    // UdpL4Protocol::PROT_NUMBER (MARIO)
	{
//		cout << Simulator::Now().GetSeconds() << " : Packet length " <<  packet->GetSize() << endl;
		m_downTarget (packet, source, destination, UdpL4Protocol::PROT_NUMBER, route);

		break;
	}
	default:

		NS_LOG_INFO ("Received protocol not handled: " << std::hex << protocol << std::dec);
		break;

	}
}

void InterFlowNetworkCodingProtocol::SendDown ()
{
	NS_LOG_FUNCTION (this);
    u_int8_t i;
    Ptr<Packet> outputPacket;
    Ptr <Ipv4Route> route;
    Ipv4Address source, destination;
    std::vector <struct NetworkCodingItem> outgoingPacketVector;
    //	u_int8_t protocol;

    outgoingPacketVector = m_ncBuffer->OutputBufferExtraction();

    assert (outgoingPacketVector.size());

    //By default, the first packet into the buffer defines the source and destination addresses
    source = outgoingPacketVector[0].source;
    destination = outgoingPacketVector[0].destination;

    //Create the Network Coding header
    InterFlowNetworkCodingHeader header;
    header.SetProtocolNumber (outgoingPacketVector[0].protocol); //This first version will only take care of TCP packets
    header.SetPacketType (0);   //Data packet
    header.SetCodedPackets (outgoingPacketVector.size());

    //We need to convert from NetworkCodingBuffer::NetworkCodingItem to InterFlowNetworkCodingHeader::Item
    //Recall that if the we are sending a native packet, we need no more additional information
    for (i = 0; i < outgoingPacketVector.size (); i++)
    {
        //We need to extract the sequence number and the flags from the IP header
        TcpHeader tcpHeader;
        outgoingPacketVector[i].packet->PeekHeader (tcpHeader);

        header.m_packetVector.push_back(InterFlowNetworkCodingHeader::Item (outgoingPacketVector[i].destination, tcpHeader.GetSequenceNumber (),
                outgoingPacketVector[i].hash, outgoingPacketVector[i].packet->GetSize ()));
    }

    //	Code the packet (if necessary; if not, just send the packet)
    if (outgoingPacketVector.size() > 1) {
        std::vector <TcpHeader> tcpVector;
        //Get the maximum packet size (in order to code them together (At this point, we are including the TCP headers)
        u_int32_t maximum = 0;
        for (i = 0; i < outgoingPacketVector.size (); i++)
        {
            if (outgoingPacketVector[i].packet->GetSize () > maximum)
                maximum = outgoingPacketVector[i].packet->GetSize ();
        }

        outputPacket = Create <Packet > (maximum); //We must include the TCP header
        for (i = 0; i < outgoingPacketVector.size (); i++)

        {
            TcpHeader temp;
            outgoingPacketVector[i].packet->PeekHeader (temp);
            tcpVector.push_back(temp);

            outputPacket = Encode (outputPacket, outgoingPacketVector[i].packet);
        }

        NS_LOG_INFO("\t-> Send a coded packet downwards");

        for (i = 0; i < tcpVector.size(); i++)
        {
            NS_LOG_INFO("\t" << tcpVector [i]);
        }
        tcpVector.clear();
    }
    else //No coding opportunity --> Send the native packet
    {
        TcpHeader temp;
        outputPacket = outgoingPacketVector[0].packet->Copy();
        outputPacket->PeekHeader(temp);
        NS_LOG_INFO("\t-> Send a native packet downwards" << endl << "\t" << temp);
    }

    //Get the route (if available)
    if (!m_routes.empty()) {
    	for (RoutesIterator iter = m_routes.begin(); iter != m_routes.end(); iter++)
    	{
    		route = *iter;
    		if ((route->GetDestination() == destination) && (route->GetSource() == source))
    		{
    			NS_LOG_LOGIC("Route found " << route->GetSource() << "  " << route->GetDestination());
    			break;
    		}
    	}
    }
    else
        route = 0;

    //After the creation of the packet (coded or not), we will search whether we can encapsulate an ACK within the NC header. If so, we will include as many packets as possible
    // (i.e. in order to deal with this challenge, we have to compare the size of the resulting packet with the NC layer MSS). NOTE: Only for coding nodes
    if (m_codingNode)
    {
    	m_ncBuffer->EncapsulateTcpAckSegments (header, outputPacket->GetSize ());
    }

    outputPacket->AddHeader (header);

    //Trace the transmission
    if (!m_interFlowNetworkCodingCallback.IsNull ())
    {
    	m_interFlowNetworkCodingCallback (outputPacket, 3, m_node->GetId (), source, destination, header.GetCodedPackets(),
    			header.GetEmbeddedAcks(), false);
    }

    //Update statistics
    if (m_codingNode)
    {
    	if (header.GetCodedPackets () == 1)
    		m_ncStatistics.uncodedPacketTx ++;
    	else
    		m_ncStatistics.codedPacketTx ++;
    }
    m_ncStatistics.transmissionNumber ++;

    NS_LOG_DEBUG (Simulator::Now().GetSeconds() << " - " << (int) m_node->GetId() << " --> SendDownDataPacket " << outputPacket->GetSize() << " " << header );
    m_downTarget (outputPacket, source, destination, PROT_NUMBER, route);
}

void InterFlowNetworkCodingProtocol::SendDownTcpAck ()  		//Send Down a raw TCP acknowledgement (no payload)
{
	NS_LOG_FUNCTION (this);
	Ptr <Ipv4Route> route;
	Ipv4Address source, destination;
	InterFlowNetworkCodingHeader ncHeader;
	Ptr<Packet> outputPacket = Create <Packet> ();

	ncHeader.SetProtocolNumber (6); 		//This is a legacy TCP operation, hence there will not be another type of protocol
	ncHeader.SetPacketType (0);  		    //Data packet
	ncHeader.SetCodedPackets (0);			//Raw ACK transmission
	ncHeader.SetEmbeddedAcks (0);

	m_ncBuffer->EncapsulateTcpAckSegments (ncHeader, outputPacket->GetSize ());

	destination = ncHeader.m_tcpAckVector[0].destination;

	//Get the route (if available)
	if (!m_routes.empty())
	{
		for (RoutesIterator iter = m_routes.begin(); iter != m_routes.end(); iter++)
		{
			route = *iter;
			if ((route->GetDestination() == destination) && (route->GetSource() == source))
			{
				NS_LOG_LOGIC("Route found " << route->GetSource() << "  " << route->GetDestination());
				break;
			}
		}
	}
	else
	{
		route = 0;
	}

	//Search into the EndPoint table the source IP address which sent the packet
	for (EndPointIterator j = m_endPointTable.begin(); j != m_endPointTable.end(); j++)
	{
		if (j->second.source == destination)	// Flow found --> Forward up
		{
			source =  j->second.destination;
		}
	}

	outputPacket->AddHeader (ncHeader);

	//Trace the results
	if (!m_interFlowNetworkCodingCallback.IsNull ())
	{
		m_interFlowNetworkCodingCallback (outputPacket, 4, m_node->GetId(), source, destination, ncHeader.GetCodedPackets(),
				ncHeader.GetEmbeddedAcks(), false);
	}

	NS_LOG_DEBUG (Simulator::Now().GetSeconds() << " - " << (int) m_node->GetId() << " -> SendDownRawAck " << outputPacket->GetSize()
			<< " " << source << " >> " << destination <<  " " << ncHeader );
	m_downTarget(outputPacket, source, destination, PROT_NUMBER, route);
}


void InterFlowNetworkCodingProtocol::ForwardUp (Ptr<Packet> packet, const Ipv4Header &header, Ptr<Ipv4Interface> incomingInterface)
{
    NS_LOG_FUNCTION(this);

    Ptr<Packet> copy = packet->Copy ();
    TcpHeader temp;
    copy->RemoveHeader (temp);

//    cout << Simulator::Now().GetSeconds() << " " << (int) m_node->GetId() <<" Forward Up: " << packet-> GetSize() << " " << temp << endl;

    if (incomingInterface) //After a native reception, we can inmediately forward up the packet to the upper layer
    {
        if (!m_upNscTcpTarget.IsNull())
            m_upNscTcpTarget (packet, header, incomingInterface);
        if (!m_upTcpTarget.IsNull())
            m_upTcpTarget (packet, header, incomingInterface);
    }
    else
    {
        Ptr<Ipv4L3Protocol> ipv4L3Protocol = m_node->GetObject<Ipv4L3Protocol > ();
        assert (ipv4L3Protocol);

        u_int32_t interfaceIndex = ipv4L3Protocol->GetInterfaceForAddress(header.GetDestination());

        if (!m_upNscTcpTarget.IsNull())
            m_upNscTcpTarget(packet, header, ipv4L3Protocol->GetInterface(interfaceIndex));
        if (!m_upTcpTarget.IsNull())
            m_upTcpTarget(packet, header, ipv4L3Protocol->GetInterface(interfaceIndex));
    }

    //Update reception statistics (Reception --> We have to remove the TCP header length, thus it was not pulled out from the packet
    m_ncStatistics.networkCodingLayerTotalBytes += packet->GetSize() - temp.GetSerializedSize ();
    if (m_ncStatistics.transmissionStarted == false)
    {
    	m_ncStatistics.transmissionStarted = true;
    	m_ncStatistics.startingTime = Simulator::Now().GetSeconds();
    }

    m_ncStatistics.lastReception = Simulator::Now().GetSeconds();

}

enum Ipv4L4Protocol::RxStatus InterFlowNetworkCodingProtocol::Receive(Ptr<Packet> packet, const Ipv4Header &header, Ptr<Ipv4Interface> incomingInterface)
{
    NS_LOG_FUNCTION(this << Simulator::Now().GetSeconds() << packet->GetSize() << (int) header.GetProtocol());
    Ptr<Packet> packetCopy = packet->Copy();


    InterFlowNetworkCodingHeader ncHeader;
    packet->RemoveHeader(ncHeader);

//    cout << Simulator::Now().GetSeconds() << " (" << (int) m_node->GetId () << ") : NC Receive " << packet->GetSize() << " " << ncHeader << endl;

    //NOTE: This sort of reception will be which forwards up the packet (not the promiscuous one)
    if (ncHeader.GetCodedPackets() == 1) //The received packet is native, hence it is necessarily delivered to the receiver node (it cannot be any other possibility)
    {
    	Ipv4Header temp = header;
    	temp.SetProtocol (ncHeader.GetProtocolNumber ());

        ForwardUp(packet, temp, incomingInterface);
        NS_LOG_INFO ("(" << m_node->GetId() << ") " << Simulator::Now().GetSeconds() << " <-- Native reception " << packet->GetSize() << " " << ncHeader.GetSerializedSize());
        //Trace the results
        if (!m_interFlowNetworkCodingCallback.IsNull())
        {
        	m_interFlowNetworkCodingCallback(packetCopy, 0, m_node->GetId(), header.GetSource(), header.GetDestination(), ncHeader.GetCodedPackets(),
            		ncHeader.GetEmbeddedAcks(), true);
        }
    }
    return Ipv4L4Protocol::RX_OK;
}

bool InterFlowNetworkCodingProtocol::ReceivePromiscuous (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, const Address &from, const Address &to, NetDevice::PacketType packetType)
{
	NS_LOG_FUNCTION_NOARGS();
	Ptr<Packet> tracedPacket;
	Ptr<Packet> packetCopy = packet->Copy();
	Ipv4Header ipHeader;
	Ptr<Ipv4> ipv4;
	u_int8_t i;

	//Discard the packets destinated to the concrete node --> they will be handled in InterFlowNetworkCodingProtocol::Receive
	//	if (packetType == NetDevice::PACKET_HOST)
	//		return false;

	switch (protocol)
	{
	case 0x800: //IP packet
		packetCopy->RemoveHeader (ipHeader);
		ipv4 = m_node->GetObject<Ipv4 > ();

		switch (ipHeader.GetProtocol ())
		{
		case 6: //TCPL4Protocol::PROT_NUMBER

			break;
		case 17: //UdpL4Protocol::PROT_NUMBER

			break;
		case 99: // InterFlowNetworkCodingProtocol::PROT_NUMBER
			// We will store a packet at the decoding buffer if and only if the IP destination address is different from the node which has overheard the packet
			// (it is worth highlighting that the packet might contain packets addressed to different destinations).
			// In order to avoid infinite echo transmissions, we will not process either the packets if the IP source address coincides with the overhearing node's one

			tracedPacket = packetCopy->Copy();

			InterFlowNetworkCodingHeader networkCodingHeader;
			packetCopy->RemoveHeader (networkCodingHeader);

			if (networkCodingHeader.GetProtocolNumber() == TcpL4Protocol::PROT_NUMBER)
			{
				//Overhearing a native packet, headed to any node --> Store in the decoding buffer
				if (networkCodingHeader.GetCodedPackets() == 1)
				{
					// 1 - Capture the flow-id hash (if connection-related SYN/FIN primitives)
					TcpHeader tcpHeader;
					if (networkCodingHeader.GetCodedPackets())
					{
						packetCopy->PeekHeader(tcpHeader);

						//Catch the TCP flows through the SYN or SYN + ACK packets to update the EndPoint hash table
						if (tcpHeader.GetFlags() & TcpHeader::SYN)
						{
							NS_LOG_INFO ((int) m_node->GetId() << " <-- SYN: " << ipHeader.GetSource() << " > " << ipHeader.GetDestination()
									<< " Flags: 0x" << std::hex << (int) tcpHeader.GetFlags() << std::dec << " Packet size " << (int) packet->GetSize());

							UpdateEndPointTable (NetworkCodingEndPoint (ipHeader.GetSource(), ipHeader.GetDestination(), tcpHeader.GetSourcePort(), tcpHeader.GetDestinationPort()));
						}
					}

					// 2 - Update the decoding buffer
//					if (m_receiverNode && (packetCopy->GetSize() > MIN_CODING_LENGTH))
					if (packetCopy->GetSize() > MIN_CODING_LENGTH)
					{
						m_ncBuffer->UpdateDecodingBuffer(packetCopy, ipHeader.GetSource(), ipHeader.GetDestination(), 6);
					}

					// 3 - Trace the native reception (trace only if not the intended receiver)
					if (ipHeader.GetDestination() != m_node->GetObject<Ipv4 > ()->GetAddress(1, 0).GetLocal())
					{
						if (!m_interFlowNetworkCodingCallback.IsNull())
						{
							m_interFlowNetworkCodingCallback(tracedPacket, 1, m_node->GetId(), ipHeader.GetSource(), ipHeader.GetDestination(), networkCodingHeader.GetCodedPackets(),
									networkCodingHeader.GetEmbeddedAcks(), true);
						}
					}
				}
				else 	//Coded packet --> Try to decode (if the nodes is any of the destinations)
				{
					NS_LOG_INFO ("\t<- Received a coded packet ");

					for ( i = 0; i < networkCodingHeader.m_packetVector.size(); i++)
					{
						if (AmIDestination(networkCodingHeader.m_packetVector[i].destination))
						{
							Ipv4Header ipHeader;
							ipHeader.SetProtocol (networkCodingHeader.GetProtocolNumber());

							Ptr <Packet> decoded = DecodeAttempt (packetCopy, networkCodingHeader, ipHeader);
							NS_LOG_INFO("\t" << networkCodingHeader);

							if (decoded) //Decode success (increment success counter)
							{
								TcpHeader tcpDecodedHeader;
								decoded->PeekHeader (tcpDecodedHeader);

								NS_LOG_INFO ("\t<-- Decode successfully " << tcpDecodedHeader);

//								cout << "Decode success " << networkCodingHeader << endl;

								ForwardUp (decoded, ipHeader, 0);

								//Trace the results (decoding success)
								if (!m_interFlowNetworkCodingCallback.IsNull())
								{
									m_interFlowNetworkCodingCallback(tracedPacket, 0, m_node->GetId(), ipHeader.GetSource(), ipHeader.GetDestination(), networkCodingHeader.GetCodedPackets(),
											networkCodingHeader.GetEmbeddedAcks(), true);
								}

							}
							else //Decode failure (increment corresponding counter) + Future explicit Native retransmission
							{
								//Trace the results (decoding failure)
								if (!m_interFlowNetworkCodingCallback.IsNull())
								{
									m_interFlowNetworkCodingCallback(tracedPacket, 0, m_node->GetId(), ipHeader.GetSource(), ipHeader.GetDestination(), networkCodingHeader.GetCodedPackets(),
											networkCodingHeader.GetEmbeddedAcks(), false);
								}

								//Update statistics (decoding failure)
								if (m_ncStatistics.transmissionStarted == false)
								{
									m_ncStatistics.transmissionStarted = true;
									m_ncStatistics.startingTime = Simulator::Now().GetSeconds();
								}
								m_ncStatistics.lastReception = Simulator::Now().GetSeconds();
							}
						}
					}
				}

				//ACK encapsulation
				//Search for an embedding opportunity
				//Conditions:
				// - To be a void-payload packet
				// - Not to be a primitive (i.e. SYN or FIN)
				//NOTE: Native packet reception --> The packet will never be forwarded up to the upper layer; it will only be stored in the decoding buffer and update the tracing statistics

				//Parse for an ACK encapsulation --> We need the complete EndPoint information
				if (networkCodingHeader.GetEmbeddedAcks())
				{

					for (i = 0; i < networkCodingHeader.m_tcpAckVector.size(); i++)
					{
						if (AmIDestination (networkCodingHeader.m_tcpAckVector[i].destination))
						{

							ipHeader.SetDestination (networkCodingHeader.m_tcpAckVector[i].destination);
							ipHeader.SetSource (Ipv4Address ("0.0.0.0"));
							ipHeader.SetProtocol (6);   //  TCP

//							NS_LOG_UNCOND ("(" << m_node->GetId() << ") : Promisc Reception " << endl << "   Packet length " << packetCopy->GetSize()
//									<< " EndPointMap size " << (int) m_endPointTable.size() << endl << networkCodingHeader << " " << (int) i << endl << " "
//									<< networkCodingHeader.m_tcpAckVector[i].tcpHeader << endl << "IP header " << ipHeader);

							for (EndPointIterator j = m_endPointTable.begin(); j != m_endPointTable.end(); j++)
							{
								if (j->second.source == networkCodingHeader.m_tcpAckVector[i].destination)	// Flow found --> Forward up
								{
									Ptr<Packet> newPacket = Create <Packet> ();
									networkCodingHeader.m_tcpAckVector[i].tcpHeader.SetFlags (0x10);

									newPacket->AddHeader (networkCodingHeader.m_tcpAckVector[i].tcpHeader);
									ipHeader.SetSource (j->second.destination);

//									NS_LOG_UNCOND (Simulator::Now().GetSeconds() << ": (" << (int) m_node->GetId() << ") *-*-*-*  ACK retrieved " << ipHeader.GetSource() << " >> " << ipHeader.GetDestination() <<
//											" -- " << networkCodingHeader.m_tcpAckVector[i].tcpHeader << " " << Simulator::Now().GetSeconds());

									if (!m_interFlowNetworkCodingCallback.IsNull())
									{
										Ptr<Packet> tracedPacket = Create <Packet> ();
										InterFlowNetworkCodingHeader temp;
										temp = networkCodingHeader;
										temp.m_packetVector.clear ();
										temp.m_tcpAckVector.clear ();
										temp.m_tcpAckVector.push_back (networkCodingHeader.m_tcpAckVector[i]);

										tracedPacket->AddHeader (temp);

										m_interFlowNetworkCodingCallback(tracedPacket, 5, m_node->GetId(), ipHeader.GetSource(), ipHeader.GetDestination(), temp.m_packetVector.size(),
												temp.m_tcpAckVector.size(), true);
									}

									ForwardUp (newPacket, ipHeader, 0);
								}
							}
						}
					}
				}
			}
			break;
		}
		break;
		default:
			NS_LOG_LOGIC("Unknown protocol");
			break;
	}

	return true;
}

void InterFlowNetworkCodingProtocol::ParseForwardingReception (Ptr<Ipv4Route> rtentry, Ptr<const Packet> p, const Ipv4Header &header)
{
    NS_LOG_FUNCTION(this);

    InterFlowNetworkCodingHeader networkCodingHeader;
    TcpHeader tcpHeader;
    Ptr<Packet> packetCopy = p -> Copy();

    packetCopy -> RemoveHeader(networkCodingHeader);
    //Assert we are working on the TCP protocol
    if (networkCodingHeader.GetProtocolNumber() == TcpL4Protocol::PROT_NUMBER)
    {
    	if (networkCodingHeader.GetCodedPackets())
    	{
    		packetCopy->PeekHeader(tcpHeader);
    	}

        //In the flow goes across this condition, the coding nodes will search for a coding opportunity
    	if (networkCodingHeader.GetCodedPackets() == 1 && (packetCopy->GetSize() > MIN_CODING_LENGTH))
    		//Native packet --> Search for a coding opportunity (it has to fulfill the coding requirements; in this case, the packet has to be longer than the coding threshold
    	{
    		u_int16_t hash = HashID (header.GetSource(), header.GetDestination(), tcpHeader.GetSourcePort(), tcpHeader.GetDestinationPort());

    		// If the overheard packet fulfills the coding requirements (i.e. minimum packet length), it will be used to update the input packet pool
    		if (m_ncBuffer->UpdateInputPacketPool(packetCopy, networkCodingHeader, header.GetSource(), header.GetDestination(), 6))
    		{
    			m_ncBuffer->SearchForCodingOpportunity(hash);
    		}
    	}

    	//Here we are taking care of the ACK forwarding scheme. In order to be able to encapsulate an ACK, the promiscuous reception must fulfill the following constraints:
    	//  - ACK encapsulation scheme enabled
    	//  - The node is a CODING node
    	//  - No TCP payload (that is to say, at the NC layer, the packet size should be equal to 20 bytes, the TCP header length)
    	//  - We will not embed the connection primitives (TCP segments with the SYN/FIN flags switched on)

    	else if (m_embeddedAcks && m_codingNode && !(packetCopy->GetSize() - tcpHeader.GetSerializedSize())
    			&& m_ncBuffer->GetAckBufferSize() && (m_ncBuffer->GetAckBufferStoreTime().GetSeconds() > 0)
        		&& !(tcpHeader.GetFlags() & (TcpHeader::SYN | TcpHeader::FIN)) )
        {
//        	if (m_ncBuffer->SearchAckEncapsulation ())   //ACK encapsulation found --> We will store the corresponding ACK into the buffer till the next native/coded packet is about to be delivered
//        	{
//        		NS_LOG_UNCOND ("Encapsulable ACK " << packetCopy->GetSize());
        		m_ncBuffer->UpdateAckBuffer (packetCopy, header.GetSource(), header.GetDestination());

//        	}
//        	else    //Cannot find an ACK encapsulation --> Direct ACK forwarding
//        	{
//        		//Forward the packet
//        		Ptr<Ipv4L3Protocol> ipv4 = m_node->GetObject <Ipv4L3Protocol > ();
//        		if (ipv4)
//        		{
//        			TcpHeader tcpHeader;
//        			packetCopy->PeekHeader(tcpHeader);
//
//        			Ptr<Packet> packet = p->Copy();
//        			ipv4->SendRealOutHook(rtentry, packet, header);
//        		}
//        	}
        }

        else 		//Coded packets --> Just forward (by the moment) --> in the near future, we can smartly re-encode previously coded packets, thus increasing the overall performance
        {

            //Forward the packet
            Ptr<Ipv4L3Protocol> ipv4 = m_node->GetObject <Ipv4L3Protocol > ();
            if (ipv4)
            {
                Ptr<Packet> packet = p->Copy();
                ipv4->SendRealOutHook(rtentry, packet, header);
            }
        }
    }
}

//Ptr<Packet> InterFlowNetworkCodingProtocol::DecodeAttempt(Ptr<Packet> packet, InterFlowNetworkCodingHeader header, Ipv4Header &ipHeader)
//{
//    u_int8_t nativePacketsFound = 0;
//    u_int16_t searchedHash = 0;
//    std::vector <Ptr <Packet> > nativePool;
//
//    //Parse the NC header in order to retrieve the information relative to the native packets which are coded together with the received packet
//    for (u_int8_t i = 0; i < header.m_packetVector.size(); i++)
//    {
//        Ptr<Packet> temp = m_ncBuffer->SearchIntoDecodingBuffer(header.m_packetVector[i].hash, header.m_packetVector[i].seqNum.GetValue());
//        if (temp)
//        {
//            nativePool.push_back(temp);
//            nativePacketsFound++;
//        }
//        else
//        {
//            searchedHash = header.m_packetVector[i].hash;
//        }
//    }
//
//    //Decode success (we have stored N - 1 native packets from the received batch of N coded packets
//    if (nativePacketsFound == header.m_packetVector.size() - 1)
//    {
//        Ptr <Packet> output = packet;
//
//        for (u_int8_t i = 0; i < nativePool.size(); i++)
//        {
//            TcpHeader tcpHeader;
//            nativePool[i]->PeekHeader(tcpHeader);
//            output = Decode(output, nativePool[i]);
//        }
//
//        //Search the source IP address into the EndPoints hash table
//        EndPointIterator iter = m_endPointTable.find(searchedHash);
//        if (iter != m_endPointTable.end())
//        {
//            ipHeader.SetSource(iter->second.source);
//            ipHeader.SetDestination(iter->second.destination);
//            ipHeader.SetProtocol(header.GetProtocolNumber());
//
//            //Update statistics (decode success)
//            m_ncStatistics.decodeSuccess ++;
//            return output;
//        }
//        else
//        {
//        	m_ncStatistics.decodeFailure ++;
//            return 0;
//        }
//    }
//    else
//    {
//        ipHeader.SetSource(Ipv4Address("0.0.0.0"));
//        ipHeader.SetDestination(Ipv4Address("0.0.0.0"));
//        NS_LOG_INFO("Decoding error " << header);
//
//        //Update statistics
//        m_ncStatistics.decodeFailure ++;
//
//        return 0;
//    }
//}

Ptr<Packet> InterFlowNetworkCodingProtocol::DecodeAttempt(Ptr<Packet> packet, InterFlowNetworkCodingHeader header, Ipv4Header &ipHeader)
{
    u_int8_t nativePacketsFound = 0;
    u_int16_t searchedHash = 0;
    std::vector <Ptr <Packet> > nativePool;
    std::vector <pair <u_int16_t, u_int32_t>  > nativeFoundKeyVector;

    //Parse the NC header in order to retrieve the information relative to the native packets which are coded together with the received packet
    for (u_int8_t i = 0; i < header.m_packetVector.size(); i++)
    {
        Ptr<Packet> temp = m_ncBuffer->SearchIntoDecodingBuffer(header.m_packetVector[i].hash, header.m_packetVector[i].seqNum.GetValue());
        if (temp)
        {
            nativePool.push_back(temp);
            nativeFoundKeyVector.push_back (make_pair (header.m_packetVector[i].hash, header.m_packetVector[i].seqNum.GetValue()));
            nativePacketsFound++;
        }
        else
        {
            searchedHash = header.m_packetVector[i].hash;
        }
    }

    //Decode success (we have stored N - 1 native packets from the received batch of N coded packets
    if (nativePacketsFound == header.m_packetVector.size() - 1)
    {
        Ptr <Packet> output = packet;

        for (u_int8_t i = 0; i < nativePool.size(); i++)
        {
            TcpHeader tcpHeader;
            nativePool[i]->PeekHeader(tcpHeader);
            output = Decode(output, nativePool[i]);
        }

        //Search the source IP address into the EndPoints hash table
        EndPointIterator iter = m_endPointTable.find (searchedHash);
        if (iter != m_endPointTable.end())
        {
            ipHeader.SetSource(iter->second.source);
            ipHeader.SetDestination(iter->second.destination);
            ipHeader.SetProtocol(header.GetProtocolNumber());

            //Update statistics (decode success)
            m_ncStatistics.decodeSuccess ++;

//            Remove the succesful packet from the Decoding buffer
//            for (u_int8_t i = 0; i < nativeFoundKeyVector.size(); i ++)
//            {
//            	m_ncBuffer.CleanPacketAtDecodingBuffer (nativeFoundKeyVector[i].first, nativeFoundKeyVector[i].second);
//            }

            nativePool.clear();
            nativeFoundKeyVector.clear();

            return output;
        }
        else
        {
        	m_ncStatistics.decodeFailure ++;

        	nativePool.clear();
        	nativeFoundKeyVector.clear();
            return 0;
        }
    }
    else
    {
        ipHeader.SetSource(Ipv4Address("0.0.0.0"));
        ipHeader.SetDestination(Ipv4Address("0.0.0.0"));
        NS_LOG_INFO("Decoding error " << header);

        //Update statistics
        m_ncStatistics.decodeFailure ++;

        nativePool.clear();
        nativeFoundKeyVector.clear();
        return 0;
    }
}






void InterFlowNetworkCodingProtocol::UpdateEndPointTable(struct NetworkCodingEndPoint entry)
{
    NS_LOG_FUNCTION(this);
    u_int16_t hash = HashID (entry.source, entry.destination, entry.sourcePort, entry.destinationPort);

    if (m_endPointTable.find (hash) == m_endPointTable.end())
    {

//    	//Search and attach the socket corresponding to the flow
//    	for (u_int16_t i = 0; i < m_tcp->GetSocketSize(); i++)
//    	{
//    		entry.socket = m_tcp->GetSocket(i);
//
//    		//Check if the socket has a valid IP address tuple
//    		if (! m_tcp->GetSocket(i)->GetEndpoint()->GetLocalAddress().IsEqual(Ipv4Address("0.0.0.0")) &&
//    				! m_tcp->GetSocket(i)->GetEndpoint()->GetPeerAddress().IsEqual(Ipv4Address("0.0.0.0")))
//    		{
//
//    			NS_LOG_UNCOND ("Node " << NodeList().GetNodeIndex(m_node) << " HASH " << std::hex << hash << std::dec <<
//    					" Socket " << (int)  i << endl <<
//    					m_tcp->GetSocket(i)->GetEndpoint()->GetLocalAddress() << " " << (int) m_tcp->GetSocket(i)->GetEndpoint()->GetLocalPort() << " " <<
//    					m_tcp->GetSocket(i)->GetEndpoint()->GetPeerAddress() << " " << (int) m_tcp->GetSocket(i)->GetEndpoint()->GetPeerPort());
//    		}
//    	}

    	m_endPointTable.insert(pair<u_int16_t, struct NetworkCodingEndPoint > (hash, entry));
    }

    NS_LOG_DEBUG ("(" << (int) m_node->GetId() << ") EndPointTable updated: " << std::hex << hash << std::dec << " "
    		<< entry.source << " (" << entry.sourcePort << ") >> " << entry.destination << " (" << entry.destinationPort <<
    		")");
}

void InterFlowNetworkCodingProtocol::SetNetworkCodingBufferParameters(Time bufferTimeout, u_int32_t bufferSize, u_int8_t maxCodedPackets)
{
	NS_LOG_FUNCTION_NOARGS ();
    m_ncBuffer->SetBufferTimeout(bufferTimeout);
    m_ncBuffer->SetBufferSize(bufferSize);
    m_ncBuffer->SetMaxCodedPackets(maxCodedPackets);
}

void InterFlowNetworkCodingProtocol::SetEncapsulationAckBufferParameters (Time bufferTimeout, u_int32_t bufferSize)
{
	NS_LOG_FUNCTION_NOARGS ();
	m_ncBuffer->SetAckBufferStoreTime(bufferTimeout);
	m_ncBuffer->SetAckBufferSize (bufferSize);
}

int InterFlowNetworkCodingProtocol::GetProtocolNumber() const
{
    return PROT_NUMBER;
}

bool InterFlowNetworkCodingProtocol::AmIDestination(const Ipv4Address &destination)
{
    NS_LOG_FUNCTION(this);
    return (destination == m_node->GetObject<Ipv4 > ()->GetAddress(1, 0).GetLocal());
}

void InterFlowNetworkCodingProtocol::DoDispose()
{
    NS_LOG_FUNCTION_NOARGS();

    m_endPointTable.clear();
    m_routes.clear();

    m_node = 0;
    GetDownTarget().Nullify();
    Ipv4L4Protocol::DoDispose();
}
