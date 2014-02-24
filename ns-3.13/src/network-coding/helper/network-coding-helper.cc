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


#include <sys/types.h>
#include <assert.h>

#include "network-coding-helper.h"

using namespace ns3;
using namespace std;


NetworkCodingHelper::NetworkCodingHelper()
{

}

NetworkCodingHelper::~NetworkCodingHelper()
{

}

void
NetworkCodingHelper::SetNetworkCodingLevel (std::string type,
		std::string n0, const AttributeValue &v0,
		std::string n1, const AttributeValue &v1,
		std::string n2, const AttributeValue &v2,
		std::string n3, const AttributeValue &v3,
		std::string n4, const AttributeValue &v4,
		std::string n5, const AttributeValue &v5,
		std::string n6, const AttributeValue &v6,
		std::string n7, const AttributeValue &v7)
{
	m_networkCoding = ObjectFactory ();
	m_networkCoding.SetTypeId (type);
	m_networkCoding.Set (n0, v0);
	m_networkCoding.Set (n1, v1);
	m_networkCoding.Set (n2, v2);
	m_networkCoding.Set (n3, v3);
	m_networkCoding.Set (n4, v4);
	m_networkCoding.Set (n5, v5);
	m_networkCoding.Set (n6, v6);
	m_networkCoding.Set (n7, v7);
}

void NetworkCodingHelper::Install (NodeContainer c, std::string typeId)
{

	for (NodeContainer::Iterator iter = c.Begin(); iter != c.End(); iter ++)
	{
		Ptr<Node> node = *iter;
		Ptr <NetworkCodingL4Protocol> networkCoding = CreateAndAggregateObjectFromTypeId (node, "ns3::" + typeId);
		networkCoding->SetNode(node);
		m_networkCodingList.push_back (networkCoding);

//		// IMPORTANT: Configure the promiscuous nodes
//		//Connect the nodes to a promiscuous reception
//		for (u_int32_t i = 0; i < node->GetNDevices(); i++)
//		{
//			//Aggregate only for WifiNetDevice objects
//			if (node->GetDevice(i)->GetObject<WifiNetDevice> ())
//			{
//				Ptr<WifiNetDevice> wifi = node->GetDevice(i)->GetObject<WifiNetDevice> ();
//				wifi->SetPromiscReceiveCallback(MakeCallback(&NetworkCodingL4Protocol::ReceivePromiscuous, networkCoding));
//			}
//		}
	}
}


Ptr<NetworkCodingL4Protocol> NetworkCodingHelper::CreateAndAggregateObjectFromTypeId (Ptr<Node> node, const std::string typeId)
{
	ObjectFactory factory;
	factory.SetTypeId (typeId);
	Ptr<NetworkCodingL4Protocol> protocol = factory.Create <NetworkCodingL4Protocol> ();
	node->AggregateObject (protocol);

	return protocol;
}

void NetworkCodingHelper::SetEmbeddedAckScheme (bool flag)
{
	for (u_int16_t i = 0; i < m_networkCodingList.size (); i++)
	{
		//Check if we are working on SimpleNetworkCoding scheme
		Ptr<InterFlowNetworkCodingProtocol> temp = DynamicCast<InterFlowNetworkCodingProtocol> (m_networkCodingList[i]);
		if (temp)
			temp->SetEmbeddedAcks (flag);
	}
}

