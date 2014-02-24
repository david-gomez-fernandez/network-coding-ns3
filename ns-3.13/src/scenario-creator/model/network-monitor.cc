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
 * Author: Luis Francisco Diez Fernandez <ldiez@tlmat.unican.es>
 * 		   David Gómez Fernández <dgomez@tlmat.unican.es>
 *		   Ramón Agüero Calvo <ramon@tlmat.unican.es>
 */


#include "ns3/node-list.h"
#include "ns3/network-monitor.h"

#include <iostream>

NS_LOG_COMPONENT_DEFINE ("NetworkMonitor");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (NetworkMonitor);

TypeId
NetworkMonitor::GetTypeId ( void )
{
	NS_LOG_FUNCTION_NOARGS ();
    static TypeId tid = TypeId ( "ns3::NetworkMonitor" )
    ;
    return tid;
};

NetworkMonitor& NetworkMonitor::Instance ( )
{
	NS_LOG_FUNCTION_NOARGS ();
    static NetworkMonitor instance;
    return instance;
}

NetworkMonitor::NetworkMonitor ( )
{
	NS_LOG_FUNCTION (this);
}

NetworkMonitor::~NetworkMonitor ( )
{
	NS_LOG_FUNCTION (this);

}

void NetworkMonitor::Init ( void )
{
    NS_LOG_FUNCTION (this);

    // for all nodes
    for ( uint8_t nodesIter_ = 0; nodesIter_ < NodeList::GetNNodes (); nodesIter_++)
    {
    }
}

size_t NetworkMonitor::GetNetworkCodingVectorSize ()
{
	NS_LOG_FUNCTION (this);
	return m_networkCodingVector.size();
}

void NetworkMonitor::AddNetworkCodingLayer (Ptr<NetworkCodingL4Protocol> element)
{
	NS_LOG_FUNCTION (this);
	m_networkCodingVector.push_back (element);
}

Ptr<NetworkCodingL4Protocol> NetworkMonitor::GetNetworkCodingElement (u_int32_t i)
{
	NS_LOG_FUNCTION (this);
	NS_ASSERT (i < m_networkCodingVector.size());
	return m_networkCodingVector[i];
}

void NetworkMonitor::Reset ()
{
	NS_LOG_FUNCTION (this);

	if (m_networkCodingVector.size())
	{
		m_networkCodingVector.clear();
	}

	if (m_sourceApps.GetN())
	{
		m_sourceApps.Reset();
	}

	if (m_sinkApps.GetN())
	{
		m_sinkApps.Reset();
	}

}


}	//Namespace ns3
