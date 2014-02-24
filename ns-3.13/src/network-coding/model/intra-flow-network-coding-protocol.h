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

#ifndef INTRA_FLOW_NETWORK_CODING_PROTOCOL_H_
#define INTRA_FLOW_NETWORK_CODING_PROTOCOL_H_

#include "ns3/core-module.h"
#include "ns3/ipv4-address.h"
//#include <time.h>
#include <sys/time.h>
//#include <stdlib.h>
#include <stdio.h>

#include <iostream>
#include <list>
#include <vector>

//IT++ finite field operations
#include <itpp/base/gf2mat.h>
#include <itpp/base/vec.h>

//Tweak to avoid the legacy fflas/ffpack bug (recall to modify the "fflas-ffpack/utils/debug.h"
//by adding the anonymous namespace
#define INT64_MAX std::numeric_limits<int64_t>::max()
#define UINT64_MAX std::numeric_limits<uint64_t>::max()
#define INT32_MAX std::numeric_limits<int32_t>::max()
#define UINT32_MAX std::numeric_limits<uint32_t>::max()
#define INT16_MAX std::numeric_limits<int16_t>::max()
#define UINT16_MAX std::numeric_limits<uint16_t>::max()
#define INT8_MAX std::numeric_limits<int8_t>::max()
#define UINT8_MAX std::numeric_limits<uint8_t>::max()

//FFLAS-FFPACK finite field operations
#include "fflas-ffpack/field/modular-balanced.h"
#include "fflas-ffpack/field/modular-int32.h"
#include "fflas-ffpack/ffpack/ffpack.h"
#include "fflas-ffpack/field/field-general.h"
#include "fflas-ffpack/utils/debug.h"
#include "fflas-ffpack/config-blas.h"

#include "network-coding-l4-protocol.h"
#include "intra-flow-network-coding-header.h"

using namespace itpp;
using namespace std;
using namespace FFLAS;
using namespace FFPACK;

typedef FFPACK::Modular<int> Field;
typedef Field::Element Element;

template<class T>
std::ostream& printvect(std::ostream& o, vector<T>& vect) {
    for (size_t i = 0; i < vect.size(); ++i)
        o << vect[i] << " ";
    return o << std::endl;
}

template <class Field>
	typename Field::Element *InsertRow (Field GF, typename Field::Element *C,
	        typename Field::Element *vector, int rowNumber, int rows, int columns)
	{
	    for (u_int16_t i=0; i < columns; i++)
	    {
	        GF.assign (C[rowNumber * columns + i], vector[i]);
	    }
	    return C;
	}

template <class Field>
typename Field::Element *GenerateRandomMatrix (Field GF,
		typename Field::Element *C, int fieldOrder, int rows, int cols)
{
	ns3::UniformVariable ranvar (0, fieldOrder);
	for (u_int32_t i = 0; i < (size_t) rows * cols; i++)
	{
		//        GF.init(C[i], RandInt(0, (int) fieldOrder));
		GF.init (C[i], ranvar.GetInteger ((u_int32_t) 0, (u_int32_t) fieldOrder));
	}
	return C;
}

template <class Field>
typename Field::Element *DelRow (Field GF, typename Field::Element *C,
        int rowNumber, int rows, int columns)
{
    for (u_int16_t i=0; i < columns; i++)
    {
        GF.assign (C[rowNumber * columns + i], 0);
    }
    return C;
}

namespace ns3 {

struct IntraFlowNetworkCodingStatistics
{
	IntraFlowNetworkCodingStatistics ();

	u_int32_t txNumber;
	u_int32_t rxNumber;
	u_int32_t downNumber;			//Number of packets which are received from the upper layer (source nodes)
	u_int32_t upNumber; 			//Number of packets which are delivered to the upper layer (destination nodes)

	std::vector<double> timestamp;
	std::vector<double> rankTime;
	std::vector<double> inverseTime;
};


struct IntraFlowNetworkCodingBufferItem {
	IntraFlowNetworkCodingBufferItem ();
	IntraFlowNetworkCodingBufferItem (
			 Ptr<Packet> packet,
			 Ipv4Address source,
			 Ipv4Address destination,
			 u_int16_t   sourcePort,
			 u_int16_t   destinationPort);
	Ptr<Packet> packet;
	Ipv4Address source;
	Ipv4Address destination;
	u_int16_t   sourcePort;
	u_int16_t   destinationPort;
};

class IntraFlowNetworkCodingMapParameters;

/*
 * Class that defines a brand new Network coding protocol which combines the information belonging to the same flow.
 * Besides, it can be configured according to two different responses:
 *  - RLSC (Random Linear Source Coding) -> The coding tasks will be only carried out at source node; hence,
 *    intermediate nodes just forward the packets they receive.
 *  - RLNC (Random Linear Network Coding) -> In this case, intermediate routers will have the possibility of "re-coding"
 *    those packets that it has previously stored with the one it receives at an instant t_i
 */
class IntraFlowNetworkCodingProtocol: public NetworkCodingL4Protocol {
public:

	// Callbacks
	/**
	 * arg1: Packet
	 * arg2: 0 ENCODING, 1 SENDNOCODED, 2 RECEIVE, 3 RECEIVEACK, 4 DECODE, 5 SENDACK, 6 RECEIVENOCODED, 9 RECEIVEOUTOFFRAGMENT
	 * arg3: Node ID
	 * arg4: Source IP Address
	 * arg5: Destination IP Address
	 * arg6: IntraFlowNetworkCodingHeader*/

	typedef Callback<void, Ptr<Packet>, u_int8_t, u_int32_t, Ipv4Address, Ipv4Address> IntraFlowNetworkCodingCallback;

	/**
	 * Through this callback we will invoke the forced erasure of the intrinsic WiFi buffer of each node (allocated at the object RegularWifiMac)
	 */
	typedef Callback<void> FlushWifiBufferCallback;

	/**
	 * Through this callback we will invoke the forced erasure of a packet of the intrinsic Wifi buffer of each node (allocated at the object RegularWifiMac)
	 */
	typedef Callback<void, u_int16_t > SelectiveFlushWifiBufferCallback;

	static const uint8_t PROT_NUMBER;
	/**
	 * Attribute handler
	 */
	static TypeId GetTypeId (void);
	/**
	 * Default constructor
	 */
	IntraFlowNetworkCodingProtocol ();

	/**
	 * Default destructor
	 */
	virtual ~IntraFlowNetworkCodingProtocol ();

	/**
	 * \Returns the protocol number of this protocol.
	 */
	virtual int GetProtocolNumber (void) const;

	/**
	 * \param packet packet to send
	 * \param source source address of packet
	 * \param destination address of packet
	 * \param protocol number of packet
	 * \param route route entry
	 *
	 * Higher-level layers call this method to send a packet
	 * down the stack to the IP level
	 */
	virtual void ReceiveFromUpperLayer (Ptr<Packet> packet, Ipv4Address source, Ipv4Address destination, uint8_t protocol, Ptr<Ipv4Route> route);

	/**
	 * As its name clearly shows, this function will be in charge of sending the information to the lower layer
	 * (by default, the IP level, namely, Ipv4L4Protocol::Send)
	 * \param
	 */
	void SendToLowerLayer (Ptr<Ipv4Route> rtentry, Ptr<const Packet> packet, const Ipv4Header &header);

	/**
	 *  The Receive function takes the arriving packets and sends them
	 *  towards the Decode function which takes responsibility of sending
	 *  them to the transport layer (UDP). It also receives the ACK's when
	 *  the Rx send them.
	 * \param p packet to forward up
	 * \param header IPv4 Header information
	 * \param incomingInterface the Ipv4Interface on which the packet arrived
	 * Called from lower-level layers to send the packet up
	 * in the stack.
	 */
	virtual enum Ipv4L4Protocol::RxStatus Receive (Ptr<Packet> packet, Ipv4Header const &header, Ptr<Ipv4Interface> incomingInterface);  	//Old version, inherited form base class Ipv4L4Protocol

	/**
	 * In order to enable the Network Coding functionalities, we have to tamper the legacy packet flow,
	 * allowing the NC layer to handle the information at intermediate routers. In this function, we will
	 * decide what to do with the corresponding packet, according to the scheme configured: RLSC (in this
	 * scheme, relay nodes just forward the packets) or RLNC (recombine the information belonging to them)
	 * \param rtEntry IP route (obtained from any Ipv4RoutingProtocol)
	 * \param packet The affected packet
	 * \param header Packet's IP header, extracted at the lower layer
	 */
	void ParseForwardingReception (Ptr<Ipv4Route> rtentry, Ptr<const Packet> packet, const Ipv4Header &header);

	/**
	 * As we need to overhear all the packets incoming from the node's surrounding area, we must configure promiscuous nodes. For that purpose, we need to hook a method through the method
	 * WifiNetDevice::SetPromiscReceiveCallback, which will invoke this method (since we have linked them). Therefore, a "promiscuous" packet will be bypassed directly from the Link Layer
	 * the Network Coding level, thus passing through the network layer. Therefore, we must discard the IP header in order to get execution exceptions.
	 * NOTE: It is worth highlighting that the callback method will deliver every received frame (including if the node is the expected receiver), hence he have to be careful of parsing
	 * duplicated packets
	 * \param device The NetDevice object from which we will receive the packet
	 * \param packet The overheard packet
	 * \param protocol LLC Header's protocol type
	 * \param from Source address (class Address)
	 * \param to Destination address (class Address)
	 * \param packetType NetDevice::PacketType enumerate
	 * \returns True if success; false otherwise
	 */
	bool ReceivePromiscuous (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, const Address &from, const Address &to, NetDevice::PacketType packetType);

	/**
	 * After the successfull decoding of a complete fragment, the receiver entity will immediately send an ACK to the sender, thus notifying it to pass towards the next one.
	 * NOTE: There could be a desynchronization problem between both entities ACKs. In such a case, the receiver will asynchronously send its current Fragment number to fix the issue.
	 * \param source IP header source address
	 * \param destination IP destination address
	 * \param normalAck True if the message corresponds to a normal ACK; false if the message is sent to sync between the endpoints
	 */
	void SendAck (Ipv4Address source, Ipv4Address destination, u_int16_t sourcePort, u_int16_t destinationPort, bool normalAck);

	/**
	 * Manually remove the Wifi Buffer (namely, after the reception of an IntraFlowNetworkCodingProtocol ACK)
	 */
	void FlushWifiBuffer ();

	/**
	 * Manually remove one flow of data in the Wifi Buffer (namely, after the reception of an IntraFlowNetworkCodingProtocol ACK)
	 */
	void SelectiveFlushWifiBuffer (u_int16_t);

	/**
	 * This method allows a caller to set the current down target callback set for this L4 protocol
	 * \param cb current Callback for the L4 protocol
	 */
	inline void SetUpUdpTarget (UpTargetCallback cb) {m_upUdpTarget = cb;}

	/*
	 * Generate a random coefficient vector to encode the data packets with.
	 * It is worth highlighting that this function will retu
	 * \param K Size of the vector
	 * \param randomVector Reference of the vector in which the coefficients will be generated (each element will be a u_int8_t variable)
	 */
	void GenerateRandomVector (u_int16_t K, std::vector<u_int8_t>& randomVector);

	/**
	 *  Choose a random vector (of size K) and combine it with the K first stored packets within the buffer
	 */
	void Encode (u_int16_t flowId);

	/*
	 * \param header IP header of the packet that triggers the decoding process
	 * \param incomingInterface Interface from which the packet has been received
	 * \param flowId Hash ID
	 *
	 */
	void Decode (Ipv4Header const &header, Ptr<Ipv4Interface> incomingInterface, u_int16_t flowId);

	/*
	 * When the RLNC scheme is enabled, intermediate relay nodes will be able to recombine the information
	 * belonging to the packets
	 * \param flowId Hash of the corresponding flow
	 */
	void Recode (u_int16_t flowId);

	/**
	 *
	 */
	void ForwardPacket (u_int16_t flowId, int d);

	/**
	 * In case the buffer has not stored at least K packets to combine them,
	 */
	void ReduceBuffer (u_int16_t flowId);

	/*
	 * \param nFrag New fragment number (received from the IntraFlowNetworkCodingHeader)
	 * \flowId Hash of the corresponding flow
	 * \forwardingNode Flag to determine whether the node is acting as a forwarding entity or not
	 */
	void ChangeFragment(u_int16_t nFrag, u_int16_t flowId, bool forwardingNode);

	//Callback hooks
	/*
	 *
	 */
	inline void SetIntraFlowNetworkCodingCallback (IntraFlowNetworkCodingCallback cb) {m_ncCallback = cb;}

	/*
	 * Connection to WifiMacQueue::Flush (by default)
	 */
	inline void SetFlushWifiBufferCallback (FlushWifiBufferCallback cb) {m_flushCallback = cb;}

	/*
	 * Connection to WifiMacQueue::SelectiveFlush (by default)
	 */
	inline void SetSelectiveFlushWifiBufferCallback (SelectiveFlushWifiBufferCallback cb) {m_selectiveFlushCallback = cb;}

	/*
	 * Function connected to the YansWifiPhy transmission operation. We will use this solution to dynamically inject traffic to the
	 * lower layer according to its output rate.
	 * \param packet The packet that will be parsed
	 */
	void WifiBufferEvent (Ptr<const Packet> packet);

	/*
	 * \returns The container that holds the gathered statistics
	 */
	inline IntraFlowNetworkCodingStatistics GetStats () {return m_stats;}

protected:
	/**
	 * Cleanup the object
	 */
	virtual void DoDispose (void);
	/*
	 * This function will notify other components connected to the node that a new stack member is now connected
	 * This will be used to notify Layer 3 protocol of layer 4 protocol stack to connect them together.
	 */
	virtual void NotifyNewAggregate ();
	/*
	 * Check whether the IP Address correspond to the one assigned to myself
	 * \param destination The IP header destination address
	 * \returns True if that is the case; false otherwise
	 */
	bool AmIDestination (const Ipv4Address &destination);
	/**
	 * After a successful reception process, forwarding and sink nodes must reset their reception matrices,
	 * in order to be ready to receive a potential new fragment
	 */
	void ResetMatrices (u_int16_t flowId);
private:
	//Attributes
	u_int8_t m_q;									// GF(2^q)
	u_int16_t m_k;									// Fragment size
	bool m_recode;									//True = RLNC; False = RLSC
	bool m_itpp;
	Time m_bufferTimeout;							//Time during which the protocol will wait until the buffer has at least K packets

	//Info map container
	std::map <u_int16_t, Ptr <IntraFlowNetworkCodingMapParameters> > m_mapParameters;
	typedef std::map <u_int16_t, Ptr <IntraFlowNetworkCodingMapParameters> >::iterator IntraFlowMapIterator;

	EventId m_reduceBufferEvent;

	//Different-purpose callbacks
	IntraFlowNetworkCodingCallback m_ncCallback;		// Callback used to trace the main results achieved
	FlushWifiBufferCallback m_flushCallback;
	SelectiveFlushWifiBufferCallback m_selectiveFlushCallback;

	//Connection to the upper (UDP) layer
	UpTargetCallback m_upUdpTarget;

	//Stats container
	IntraFlowNetworkCodingStatistics m_stats;
};


/**
 * Class that stores individual information for each of the flow present on the scenario
 */
class IntraFlowNetworkCodingMapParameters :public Object
{

	friend class IntraFlowNetworkCodingProtocol;
public:
	/**
	 * Default constructor
	 */
	IntraFlowNetworkCodingMapParameters();
	/*
	 * Default destructor
	 */
	virtual ~IntraFlowNetworkCodingMapParameters();

private:
	bool m_forwardingNode;					//Differentiate between and endpoint and a forwarding node (this flag is enable upon the first call of a "ParseForwardingReception" function

	u_int16_t m_k;
	u_int8_t m_rank;
	u_int32_t m_fragmentNumber;

	int m_txCounter;						//IMPORTANT: parameter used to dynamically inject traffic to the lower layer

	//Reception matrices (only one per time, depending on the value of q)
	itpp::GF2mat m_vectorMatrix;
	Field::Element *m_vectorMatrixGf;

	//Transmission and reception buffers
	std::vector <IntraFlowNetworkCodingBufferItem> m_txBuffer;			//"Infinite" buffer -> Source nodes (source coding)
	std::vector <IntraFlowNetworkCodingBufferItem> m_rxBuffer;			//Buffer of size "K" -> Forwarding (only with RLNC) and sink nodes
};

}  	// End namespace


#endif /* INTRA_FLOW_NETWORK_CODING_PROTOCOL_H_ */
