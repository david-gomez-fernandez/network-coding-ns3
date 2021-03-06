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
 * 		   Mario Puente <mario.puente@alumnos.unican.es>
 *		   Ramón Agüero Calvo <ramon@tlmat.unican.es>
 */

#include <assert.h>
#include "inter-flow-network-coding-buffer.h"
#include "ns3/hash-id.h"
//#include <openssl/md5.h> //Open SSL library --> Needed for hashing

using namespace std;

NS_LOG_COMPONENT_DEFINE ("InterFlowNetworkCodingBuffer");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (InterFlowNetworkCodingBuffer);

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                                   \
  if (m_node) { std::clog << Simulator::Now ().GetSeconds () << " [node " << m_node->GetId () << "] "; }

NetworkCodingItem::NetworkCodingItem ()
{
	protocol = 0;
	hash = 0;
}

NetworkCodingItem::NetworkCodingItem (Ptr<Packet> packet, Ipv4Address source, Ipv4Address destination,
		u_int8_t protocol, u_int16_t hash, Time tstamp):
		packet (packet),
		source (source),
		destination (destination),
		protocol (protocol),
		hash (hash),
		tstamp (tstamp)
{
}

TcpAckItem::TcpAckItem ()
{
}

TcpAckItem::TcpAckItem (Ptr<Packet> packet, TcpHeader header, Ipv4Address source, Ipv4Address destination,
		Time tstamp):
		packet (packet),
		header(header),
		source (source),
		destination (destination),
		tstamp (tstamp)
{
}

void NetworkCodingItem::Print (std::ostream &os) const
{
	os << " NetworkCodingItem: Packet size " << packet->GetSize() << " Source " << source << " Destination " << destination <<
			 " Protocol " << (int) protocol << " Hash " << std::hex << hash << std::dec << " Tstamp " << tstamp.GetMilliSeconds();
}

void TcpAckItem::Print (std::ostream &os) const
{
	os << " TcpAckInfo: Packet size " << packet->GetSize() << " Destination " << destination << " Tstamp " << tstamp.GetMilliSeconds();
}

std::ostream & operator << (std::ostream &os, const struct NetworkCodingItem &item)
{
  item.Print (os);
  return os;
}

TypeId
InterFlowNetworkCodingBuffer::GetTypeId (void) {
	static TypeId tid = TypeId ("ns3::InterFlowNetworkCodingBuffer")
	.SetParent<Object> ()
	.AddConstructor<InterFlowNetworkCodingBuffer> ()
	.AddAttribute ("CodingBufferSize",
				"Number of packets that an input packet pool is able to store",
				UintegerValue (2),
				MakeUintegerAccessor (&InterFlowNetworkCodingBuffer::SetBufferSize,
									  &InterFlowNetworkCodingBuffer::GetBufferSize),
				MakeUintegerChecker<u_int32_t> ())
	.AddAttribute ("CodingBufferTimeout",
				"Time interval during which an input packet pool will store a packet (in milliseconds) --> Will trigger an event",
				TimeValue (MilliSeconds(40)),
				MakeTimeAccessor (&InterFlowNetworkCodingBuffer::SetBufferTimeout),
				MakeTimeChecker())
	.AddAttribute ("MaxCodedPackets",
				"Maximum number of packets that be coded together with",
				UintegerValue (2),
				MakeUintegerAccessor (&InterFlowNetworkCodingBuffer::SetMaxCodedPackets,
									  &InterFlowNetworkCodingBuffer::GetMaxCodedPackets),
				MakeUintegerChecker <u_int8_t> ())
	.AddAttribute ("DecodingBufferStoreTime",
				"Time during which the decoding buffer will store a native packet (in seconds)",
				TimeValue (Seconds (1)),
				MakeTimeAccessor (&InterFlowNetworkCodingBuffer::m_decodingBufferStoreTime),
				MakeTimeChecker())
	.AddAttribute ("AckBufferSize",
			"Number of packet that the ACK buffer is able to store",
			UintegerValue (0),
			MakeUintegerAccessor (&InterFlowNetworkCodingBuffer::SetAckBufferSize,
								  &InterFlowNetworkCodingBuffer::GetAckBufferSize),
			MakeUintegerChecker<u_int32_t> ())
	.AddAttribute ("AckBufferTimeout",
			"Time interval during which the ACK buffer shall store an ACK",
			TimeValue (MilliSeconds(0)),
			MakeTimeAccessor (&InterFlowNetworkCodingBuffer::SetAckBufferStoreTime,
							  &InterFlowNetworkCodingBuffer::GetAckBufferStoreTime),
			MakeTimeChecker())
	;
  return tid;
}

InterFlowNetworkCodingBuffer::InterFlowNetworkCodingBuffer ()
{
	NS_LOG_FUNCTION (this);


	//Connect to the TcpRxBuffer
	Config::Connect ("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/NextTxSequence", MakeCallback (&InterFlowNetworkCodingBuffer::TcpSequenceNumber, this));
}

InterFlowNetworkCodingBuffer::~InterFlowNetworkCodingBuffer ()
{
	NS_LOG_FUNCTION (this);

	//Input packet pool
	if (! m_input.size())
		m_input.clear();

	//Input packet pool timeouts
	if (! m_inputTimeouts.size())
	{
		for (InputPacketPoolTimeoutIterator i = m_inputTimeouts.begin(); i != m_inputTimeouts.end(); i++)
		{
			if (i->second.IsRunning())
				i->second.Cancel();
		}
		m_inputTimeouts.clear();
	}

	//Output buffer
	if (! m_output.size())
		m_output.clear();
	if (! m_outputTimeouts.size())
	{
		for (OutputBufferTimeoutIterator i = m_outputTimeouts.begin(); i != m_outputTimeouts.end(); i++)
		{
			if (i->IsRunning())
				i->Cancel();
		}
		m_outputTimeouts.clear();
	}

	//Decoding buffer
	if (! m_decodingBuffer.size())
		m_decodingBuffer.clear();

	//Ack buffer
	if (! m_ackBuffer.size())
		m_ackBuffer.clear();

}

Ptr<Node> InterFlowNetworkCodingBuffer::GetNode ()
{
	return m_node;
}

void InterFlowNetworkCodingBuffer::SetNode (Ptr<Node> node)
{
	m_node = node;
}

u_int32_t InterFlowNetworkCodingBuffer::GetBufferSize () const
{
	return m_maxBufferSize;
}

void InterFlowNetworkCodingBuffer::SetBufferSize (u_int32_t maxBuf)
{
	m_maxBufferSize = maxBuf;
}

Time InterFlowNetworkCodingBuffer::GetBufferTimeout () const
{
	return m_ncBufferTimeout;
}

void InterFlowNetworkCodingBuffer::SetBufferTimeout (Time timeout)
{
	m_ncBufferTimeout = timeout;
}

u_int8_t InterFlowNetworkCodingBuffer::GetMaxCodedPackets () const
{
	return m_maxCodedPackets;
}

void InterFlowNetworkCodingBuffer::SetMaxCodedPackets (u_int8_t maxCodedPackets)
{
	m_maxCodedPackets = maxCodedPackets;
}

Time InterFlowNetworkCodingBuffer::GetDecodingBufferStoreTime () const
{
	return m_decodingBufferStoreTime;
}

void InterFlowNetworkCodingBuffer::SetDecodingBufferStoreTime (Time storeTime)
{
	m_decodingBufferStoreTime = storeTime;
}

u_int8_t InterFlowNetworkCodingBuffer::GetAckBufferSize () const
{
	return m_ackBufferSize;
}

void InterFlowNetworkCodingBuffer::SetAckBufferSize (u_int8_t maxBuf)
{
	m_ackBufferSize = maxBuf;
}

Time InterFlowNetworkCodingBuffer::GetAckBufferStoreTime () const
{
	return m_ackBufferStoreTime;
}

void InterFlowNetworkCodingBuffer::SetAckBufferStoreTime (Time storeTime)
{
	m_ackBufferStoreTime = storeTime;
}

void InterFlowNetworkCodingBuffer::SetSendDownCallback (SendDownCallback callback)
{
	m_sendDownCallback = callback;
}

void InterFlowNetworkCodingBuffer::SetSendDownAckCallback (SendDownCallback callback)
{
	m_sendDownAckCallback = callback;
}

bool InterFlowNetworkCodingBuffer::UpdateDecodingBuffer (Ptr<Packet> packet, Ipv4Address source, Ipv4Address destination, uint8_t protocol)
{
	NS_LOG_FUNCTION (m_node->GetObject<Ipv4>()->GetAddress(1,0).GetLocal() << ")" << " Elements " << m_input.size());
	Cleanup();
	TcpHeader header;
	u_int32_t hash;

	packet->PeekHeader (header);
	hash = HashID (source, destination, header.GetSourcePort(), header.GetDestinationPort());

	DecodingBufferIterator i = m_decodingBuffer.find (make_pair (hash, header.GetSequenceNumber().GetValue()));

	if (i == m_decodingBuffer.end())
	{
		//Save a copy of the original packet
		//	Ptr<Packet> packetCopy = packet->Copy ();
		m_decodingBuffer.insert (pair <decodingKey, struct NetworkCodingItem> (make_pair(hash, header.GetSequenceNumber().GetValue()), NetworkCodingItem (packet, source, destination, protocol,
				hash, Simulator::Now())));
		return true;
	}
	else
	{
		return false;
	}
}

bool InterFlowNetworkCodingBuffer:: UpdateInputPacketPool (Ptr <Packet> packet, InterFlowNetworkCodingHeader ncHeader, Ipv4Address source, Ipv4Address destination, u_int8_t protocol)
{
	NS_LOG_FUNCTION ("Node" << m_node->GetId() << " " << m_ncBufferTimeout.GetSeconds() <<  " "  << m_maxBufferSize);
	Cleanup();

	assert (ncHeader.GetCodedPackets() == 1); 			//Native packet received

	u_int32_t key;
	TcpHeader header;
	packet->PeekHeader (header);
	key = HashID (source, destination, header.GetSourcePort(), header.GetDestinationPort());

	//Lookup into the flow id-defined buffer
	InputPacketPoolIterator iter = m_input.find (key);

	//If not a coding node, just leave

	//Check the maximum buffer size (in case it is zero and we cannot insert any new packet, going through the Network Coding layer)
	if (m_maxBufferSize > 0)
	{
		//No flow id buffer: Create the element and update the timer
		if (iter == m_input.end())
		{
			NS_LOG_INFO (" ====> Empty buffer " << std::hex << key << std::dec << ". Inserting packet " << header);
			InputBuffer buffer;
			buffer.push_back (NetworkCodingItem (packet, source, destination, protocol,
					key, Simulator::Now()));
			m_input.insert (pair<u_int32_t, InputBuffer> (key, buffer));
			UpdateInputPacketPoolTimeouts (key, INSERT_ELEMENT);
		}
		else		//The buffer already exists, check the size (in order not to overflow it)
		{
			if (iter->second.size() == m_maxBufferSize)		//Buffer full --> Send the oldest packet downwards
			{
				std::vector <struct NetworkCodingItem> out;
				NS_LOG_INFO (" =====> Buffer full ("  << std::hex << key << std::dec << "). Sent down the oldest native packet "
						<< " Elements " << iter->second.size());

				out.push_back(*(iter->second.begin()));
				m_output.push_front (out);

				//Send down the element located in the current flow-id input buffer
				iter->second.pop_front();
				//Insert the new element at the end of the buffer
				iter->second.push_back (NetworkCodingItem (packet, source, destination, protocol, key, Simulator::Now()));


				//As we have changed the first element at the input packet queue, we need to update the buffer timeout
				UpdateInputPacketPoolTimeouts (key, INSERT_ELEMENT);

				//Send callback --> By default, hook to SimpleNetworkCoding::SendDown
				if (! m_sendDownCallback.IsNull())
					m_sendDownCallback ();
			}
			else		//Buffer not full. Just store the packet into it (No timeout update needed)
			{
				iter->second.push_back (NetworkCodingItem (packet, source, destination, protocol, key, Simulator::Now()));
				NS_LOG_INFO (" ====> Inserting packet (" << std::hex << key << std::dec
						 << ")" << " Elements " << iter->second.size() << " Packet " << header);
			}
		}

		return true;
	}
	else			//Directly send the packet, since the buffer size is null
	{
		std::vector <struct NetworkCodingItem> output;
		output.push_back (NetworkCodingItem (packet, source, destination, protocol,
				key, Simulator::Now()));
		m_output.push_front (output);

		//	Connect and send
		if (! m_sendDownCallback.IsNull())
		{
			m_sendDownCallback ();
		}

		return false;
	}
}

void InterFlowNetworkCodingBuffer::UpdateInputPacketPoolTimeouts (u_int16_t hash, BufferTimeoutState_t state)
{
	NS_LOG_FUNCTION (m_node->GetObject<Ipv4>()->GetAddress(1,0).GetLocal());
	Cleanup ();
	switch (state)
	{
	case INSERT_ELEMENT:   //New packet arrival
	{
		InputPacketPoolTimeoutIterator i = m_inputTimeouts.find (hash);
		if (i == m_inputTimeouts.end())    //No timer in the searched buffer; therefore, we just insert the new timeout
		{
			m_inputTimeouts.insert (pair <u_int16_t, EventId> (hash, Simulator::Schedule (m_ncBufferTimeout, &InterFlowNetworkCodingBuffer::HandleInputPacketPoolTimeout, this, hash)));
			NS_LOG_INFO (Simulator::Now().GetMilliSeconds() << ": Timeout created (Input packet pool) --> " << std::hex << hash << std::dec << " in " << m_ncBufferTimeout.GetMilliSeconds()
					<< " milliseconds");
		}
		else //If after a new insertion, the buffer is full, we need to remove the timeout and replace it by the new one
		{
			InputPacketPoolIterator element = m_input.find (hash);
//			if (element->second.size() == m_maxBufferSize)
			{
				//Extract the oldest element, update the timer and insert the new packet
				if (i->second.IsRunning())
					i->second.Cancel();
				NS_LOG_INFO (Simulator::Now().GetMilliSeconds() << ": Timeout replaced (Input packet pool) --> " << std::hex << hash << std::dec << " in " <<
										(Simulator::Now() - element->second.begin()->tstamp).GetMilliSeconds()	<< " milliseconds");

				assert ((Simulator::Now() - element->second.begin()->tstamp) < m_ncBufferTimeout);
				i->second = Simulator::Schedule ((m_ncBufferTimeout - (Simulator::Now() - element->second.begin()->tstamp)), &InterFlowNetworkCodingBuffer::HandleInputPacketPoolTimeout, this, hash);
			}
		}
		break;
	}
	case EJECT_ELEMENT:   //When a packet goes down the stack, we need to handle this event and cancel its corresponding timer
	{
		InputPacketPoolIterator element = m_input.find (hash);
		InputPacketPoolTimeoutIterator i = m_inputTimeouts.find (hash);

		//It is impossible not to find an element
		assert (i != m_inputTimeouts.end());
		if (i->second.IsRunning())
			i->second.Cancel();

		//If there are any other packets remaining in the flow-id buffer, update the new timeout
		if (element == m_input.end()) 	//No packets stored --> Remove the hash entry
		{
			m_inputTimeouts.erase (hash);
		}
		else	//Update with the "new" first element
		{
			NS_LOG_INFO (Simulator::Now().GetMilliSeconds() << ": Timeout replaced (Input packet pool) --> " << std::hex << hash << std::dec << " in " <<
					(m_ncBufferTimeout - (Simulator::Now() - element->second.begin()->tstamp)).GetMilliSeconds()	<< " milliseconds");

			assert ((Simulator::Now() - element->second.begin()->tstamp) <= m_ncBufferTimeout);
			i->second = Simulator::Schedule ((m_ncBufferTimeout - (Simulator::Now() - element->second.begin()->tstamp)), &InterFlowNetworkCodingBuffer::HandleInputPacketPoolTimeout, this, hash);
		}
		break;
	}
	default:
		break;
	}
}

NetworkCodingItem InterFlowNetworkCodingBuffer::InputPacketPoolExtraction (u_int16_t hash)
{
	NS_LOG_FUNCTION (this);
	NetworkCodingItem item = (*(m_input.find(hash)->second.begin()));

	if (m_input.find(hash)->second.size () == 1)
	{
		m_input.find (hash)->second.clear();
		m_input.erase (hash);
	}
	else
	{
		m_input.find (hash)->second.pop_front();
	}

	UpdateInputPacketPoolTimeouts (hash, EJECT_ELEMENT);

	return item;
}

void InterFlowNetworkCodingBuffer::HandleInputPacketPoolTimeout (u_int16_t hash)
{
	NS_LOG_FUNCTION (m_node->GetObject<Ipv4>()->GetAddress(1,0).GetLocal() << ")" << " Elements " << m_input.size());

	NS_LOG_INFO ("Timeout triggered (input packet pool) --> " << std::hex << hash << std::dec);
	//Compose the outgoing packet vector (the NetworkCodingL4Protocol will be in charge of making the headers and sending the packet)
	std::vector <struct NetworkCodingItem> out;

	InputPacketPoolIterator i = m_input.find (hash);

	//Prepare the native packet to send, thus deleting it from its corresponding InputPacketPool
	if (i != m_input.end())
	{
		out.push_back(*(i->second.begin()));
		m_output.push_front (out);

		UpdateOutbutBufferTimeout ();
		//If the flow-id input packet buffer has only 1 element, delete the whole buffer (and the timeout); otherwise, remove the element and update the timer
//		InputPacketPoolTimeoutIterator time = m_inputTimeouts.find (hash);
		assert (m_inputTimeouts.find (hash) != m_inputTimeouts.end());
	}

	//Update the input packet pool (Delete the outgoing packet and update the timer)
	if (i->second.size() == 1)		//Only one packet at the queue --> Delete the whole flow-id buffer
	{
		i->second.clear();
		m_input.erase (hash);
	}
	else							//More than one packet --> Pop the oldest element
	{
		i->second.pop_front();
	}

	UpdateInputPacketPoolTimeouts (hash, EJECT_ELEMENT);

	//Connect and send
	if (! m_sendDownCallback.IsNull())
	{
		//m_sendDownCallback (out);
		m_sendDownCallback ();
	}
}

void InterFlowNetworkCodingBuffer::UpdateOutbutBufferTimeout () //To be completed
{
    NS_LOG_FUNCTION (this);


}

std::vector<NetworkCodingItem> InterFlowNetworkCodingBuffer::OutputBufferExtraction ()
{
        std::vector<NetworkCodingItem> output = *(m_output.begin());
        m_output.pop_front();

        if (m_outputTimeouts.size()) {
            EventId timer = *(m_outputTimeouts.begin());
            if (timer.IsRunning())
                timer.Cancel();
            m_outputTimeouts.pop_front();
        }

        return output;
}

bool InterFlowNetworkCodingBuffer::SearchForCodingOpportunity(u_int16_t hash)
{
	NS_LOG_FUNCTION (this);
	Cleanup();
	//If the input packet pool has more than one packet stored (in two different queues), will trigger a coding opportunity
	u_int8_t codedPacketCounter = 1;
	Time timeout; 			//We need to get the oldest packet timestamp so that we can set the optimal timeout

	InputBuffer base = m_input.find(hash)->second ;
	std::vector <struct NetworkCodingItem> temp;

	//The first step consist in checking whether there is a coding opportunity into the output buffer
	if (m_input.size() == 1)
	{
		NS_LOG_DEBUG ("Cannot find a coding opportunity");
		return false;
	}
        
	//First, we need to search into the output buffer for a coding opportunity
	temp.push_back (InputPacketPoolExtraction (hash));
	timeout = temp[0].tstamp;

	//Second, if the first step does not yield any result, look into the input packet pool in order to find an available packet to be coded with
	for (InputPacketPoolIterator iter = m_input.begin(); iter != m_input.end(); iter++)
	{
		if ((hash != iter->first) && (codedPacketCounter <= m_maxCodedPackets))
		{
			NetworkCodingItem item = InputPacketPoolExtraction (iter->first);
			temp.push_back (item);

			if (item.tstamp < timeout)
				timeout = item.tstamp;
			codedPacketCounter ++;

			if (codedPacketCounter == m_maxCodedPackets)   //We have reached the maximum number of "codeable" packets; therefore we notify the NetworkCodingL4Protocol
			{
				m_output.push_front (temp);
				if (! m_sendDownCallback.IsNull())
					m_sendDownCallback ();
				return true;
			}
		}
	}

	//If after these search we can still encode more packets together, we hold the provisional vector of packets to be coded together with during a time before
	//it is sent to the Network Coding delivery member (Pending work)
	m_output.push_back (temp);

	if (! m_sendDownCallback.IsNull())
		m_sendDownCallback ();

	return true;
}

Ptr<Packet> InterFlowNetworkCodingBuffer::SearchIntoDecodingBuffer (u_int16_t hash, u_int32_t seqNum)
{
	NS_LOG_FUNCTION (std::hex << hash << std::dec << seqNum);
	Cleanup();

	DecodingBufferIterator iter = m_decodingBuffer.find (make_pair <u_int16_t, u_int32_t> (hash, seqNum));

	if (iter != m_decodingBuffer.end())
	{
		return iter->second.packet;
	}
	else
	{
		return 0;
	}
}

bool InterFlowNetworkCodingBuffer::SearchAckEncapsulation ()
{
	NS_LOG_FUNCTION_NOARGS ();

	//First, search into both the output and the input buffer
	if (m_output.size() ||  m_input.size())
	{
		return true;
	}
	return false;
}

bool InterFlowNetworkCodingBuffer::EncapsulateTcpAckSegments (InterFlowNetworkCodingHeader &ncHeader, u_int16_t packetLength)
{
	NS_LOG_FUNCTION (this);
	Cleanup();
	u_int16_t outputPacketSize = 0;
	bool moreAcks = true;
	u_int8_t embeddedAcks = 0;

	if (! m_ackBuffer.size())   //No ACK segments stored
	{
		return false;
	}
	else
	{
		outputPacketSize = packetLength + ncHeader.GetSerializedSize ();
		while ((outputPacketSize < 1476) && (moreAcks))
		{
			TcpAckItem item = AckBufferExtraction();
			UpdateAckBufferTimeout ();

			ncHeader.m_tcpAckVector.push_back(InterFlowNetworkCodingHeader::AckInfo (item.destination, item.header));
			embeddedAcks ++;

			if (! m_ackBuffer.size())
				moreAcks = false;
		}
		ncHeader.SetEmbeddedAcks (embeddedAcks);
	}
	return false;
}

bool InterFlowNetworkCodingBuffer::UpdateAckBuffer (Ptr <Packet> packet, Ipv4Address source, Ipv4Address destination)
{
	NS_LOG_FUNCTION (this);

	//Insert the received packet into the buffer, excepting the case in which the aforementioned buffer is full; in that
	//situation, we will send downwards the oldest element
	if (! m_ackBuffer.size())   //Empty buffer --> Insert the first element and update the buffer timeout
	{
		TcpHeader tcpHeader;
		packet->PeekHeader (tcpHeader);

		m_ackBuffer.push_back (TcpAckItem(packet, tcpHeader, source, destination, Simulator::Now()));
		UpdateAckBufferTimeout ();
	}
	else if (m_ackBuffer.size() < m_ackBufferSize)  //Just insert the element (no need to update the timeout, since we will no modify the oldest element
	{
		TcpHeader tcpHeader;
		packet->PeekHeader (tcpHeader);

		m_ackBuffer.push_back (TcpAckItem(packet, tcpHeader, source, destination, Simulator::Now()));
	}
	else									   		//Eject the first element (the oldest) and insert the new reception
	{
		if (! m_sendDownAckCallback.IsNull())
		{
				m_sendDownAckCallback ();
		}
		UpdateAckBufferTimeout ();
	}

//	NS_LOG_UNCOND ("(" << (int) m_node->GetId()  << ") " << Simulator::Now().GetSeconds() << " - InterFlowNetworkCodingBuffer::UpdateAckBuffer " << m_ackBuffer.size());
	return true;
}

TcpAckItem InterFlowNetworkCodingBuffer::AckBufferExtraction ()
{
	NS_LOG_FUNCTION (this);

	TcpAckItem	 item = *(m_ackBuffer.begin());
	m_ackBuffer.pop_front();

	return item;
}

void InterFlowNetworkCodingBuffer::UpdateAckBufferTimeout ()
{
	NS_LOG_FUNCTION (this);

	//Cancel the previous timers
	if (m_ackBufferTimeout.IsRunning())
		m_ackBufferTimeout.Cancel();

	if (m_ackBuffer.size())
	{
		TcpAckItem item = *m_ackBuffer.begin();
		m_ackBufferTimeout = Simulator::Schedule  ((m_ackBufferStoreTime - (Simulator::Now() - item.tstamp)), &InterFlowNetworkCodingBuffer::HandleAckBufferTimeout, this);
	}
}

void InterFlowNetworkCodingBuffer::HandleAckBufferTimeout ()
{
	NS_LOG_FUNCTION (this);

	assert (m_ackBuffer.size());

	if (! m_sendDownAckCallback.IsNull())
		m_sendDownAckCallback ();
}


void
InterFlowNetworkCodingBuffer::Cleanup (void)
{
	NS_LOG_FUNCTION_NOARGS();
	if (m_decodingBuffer.empty ())
	{
		return;
	}

	Time now = Simulator::Now ();
	for (DecodingBufferIterator i = m_decodingBuffer.begin(); i != m_decodingBuffer.end();i++)
	{
		if (i->second.tstamp + m_decodingBufferStoreTime < now)
			m_decodingBuffer.erase (i);
	}
}


void InterFlowNetworkCodingBuffer::TcpSequenceNumber (string context, SequenceNumber32 seqNum)
{
	cout << context << " " << seqNum << endl;

}


}   //End namespace ns3
