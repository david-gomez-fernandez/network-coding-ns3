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

#ifndef NETWORK_CODING_HELPER_H_
#define NETWORK_CODING_HELPER_H_

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-net-device.h"
#include "ns3/inter-flow-network-coding-protocol.h"

#include <string>
#include <vector>

namespace ns3 {

class NetworkCodingHelper
{
public:
	//Default constructor
	NetworkCodingHelper ();
	//Default destructor
	~NetworkCodingHelper ();

	/**
	 * Create and aggregate the Network Coding stack into the nodes stored at the specified NodeContainer. Besides, hook a promiscuous reception which connect the method WifiNetDevice::ForwardUp
	 * with our promiscuous handler, SimpleNetworkCoding::ReceivePromiscuous
	 * \param c Node Container to which we are going to install the stack
	 * \param tid Network Coding protocol typeid string
	 */
	void Install (NodeContainer c, std::string typeId);

	/**
	 * \return The vector that contains all the NetworkCodingL4Protocol stacks installed at the nodes (one per each one)
	 */
	inline std::vector <Ptr <NetworkCodingL4Protocol> > GetNetworkCodingList () {return m_networkCodingList;}

	/**
	 * \brief Create (by means of the ObjectFactory construction mechanisms) an object belonging to the Typeif
	 * \param node Pointer to the node in which we want to aggregate the object
	 * \param typeId String that defines the Network Coding Protocol we want to install at the nodes
	 * \returns A "neutral" point to the object. Is correspond to the calling method to "dynamically" cast this pointer to the desired class
	 */
	Ptr<NetworkCodingL4Protocol> CreateAndAggregateObjectFromTypeId (Ptr<Node> node, const std::string typeId);

	/*
	 * Enable/disable the TCP Ack encapsulation scheme
	 */
	void SetEmbeddedAckScheme (bool flag);

	/**
	 * Set the coefficients by means of an ObjectFactory
	 * \param type the type of ns3::WifiRemoteStationManager to create.
	 * \param n0 the name of the attribute to set
	 * \param v0 the value of the attribute to set
	 * \param n1 the name of the attribute to set
	 * \param v1 the value of the attribute to set
	 * \param n2 the name of the attribute to set
	 * \param v2 the value of the attribute to set
	 * \param n3 the name of the attribute to set
	 * \param v3 the value of the attribute to set
	 * \param n4 the name of the attribute to set
	 * \param v4 the value of the attribute to set
	 * \param n5 the name of the attribute to set
	 * \param v5 the value of the attribute to set
	 * \param n6 the name of the attribute to set
	 * \param v6 the value of the attribute to set
	 * \param n7 the name of the attribute to set
	 * \param v7 the value of the attribute to set
	 *
	 * All the attributes specified in this method should exist
	 * in the requested station manager.
	 */
	 void SetNetworkCodingLevel (std::string type,
	                                std::string n0 = "", const AttributeValue &v0 = EmptyAttributeValue (),
	                                std::string n1 = "", const AttributeValue &v1 = EmptyAttributeValue (),
	                                std::string n2 = "", const AttributeValue &v2 = EmptyAttributeValue (),
	                                std::string n3 = "", const AttributeValue &v3 = EmptyAttributeValue (),
	                                std::string n4 = "", const AttributeValue &v4 = EmptyAttributeValue (),
	                                std::string n5 = "", const AttributeValue &v5 = EmptyAttributeValue (),
	                                std::string n6 = "", const AttributeValue &v6 = EmptyAttributeValue (),
	                                std::string n7 = "", const AttributeValue &v7 = EmptyAttributeValue ());

private:
	 ObjectFactory m_networkCoding;
	 std::vector <Ptr <NetworkCodingL4Protocol> > m_networkCodingList;

};

}  //End namespace ns3
#endif /* NETWORK_CODING_HELPER_H_ */
