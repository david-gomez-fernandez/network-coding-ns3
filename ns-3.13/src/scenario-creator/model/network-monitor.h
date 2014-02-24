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

#ifndef NETWORK_MONITOR_H_
#define NETWORK_MONITOR_H_

#include <map>
#include <iostream>

#include "ns3/core-module.h"
#include "ns3/application-container.h"
#include "trace-stats.h"

#include "ns3/network-coding-l4-protocol.h"

namespace ns3 {

	class NetworkMonitor: public Object
	{
		friend class ConfigureScenario;
		friend class ProprietaryTracing;
	public:
		static TypeId GetTypeId ( void );
		static NetworkMonitor& Instance ( );
		/*
		 * Get the number of elements stored in the NC vector
		 * \returns The NetworkCodingL4Protocol vector size
		 */
		size_t GetNetworkCodingVectorSize ();
		/**
		 * Get the i-th element stored in the Network Coding vector
		 * \param i Index of the desired element
		 */
		Ptr<NetworkCodingL4Protocol> GetNetworkCodingElement (u_int32_t i);
		/**
		 * Add a NetworkCodingL4Protocol object to the monitor's vector
		 * \param element A pointer to the object
		 */
		void AddNetworkCodingLayer (Ptr <NetworkCodingL4Protocol> element);

		/*
		 * \return The application container that holds the source apps
		 */
		inline ApplicationContainer& GetSourceApps () {return m_sourceApps;}

		/*
		 * \return The application container that holds the source apps
		 */
		inline ApplicationContainer& GetSinkApps () {return m_sinkApps;}

		/**
		 *
		 */
		void Init ( void  );
		/**
		 * Manually remove the information stored in this class (i.e. vectors, etc.)
		 */
		void Reset ();

	private:
		NetworkMonitor ( );
		~NetworkMonitor ( );
		inline friend std::ostream& operator << (std::ostream& out, const NetworkMonitor& rm);

		//Application Layer monitoring
		ApplicationContainer m_sourceApps;
		ApplicationContainer m_sinkApps;

		//Network Coding Layer monitoring
		std::vector<Ptr <NetworkCodingL4Protocol> > m_networkCodingVector;  //Pointers to the base class: recall to dynamic cast!


	};

	std::ostream&
	operator << (std::ostream& out, const NetworkMonitor& monitor)
	{
		//    for (auto& item_ : rm.m_nodesMap )
		//    {
		//        out << "Entry for address: "; item_.first.Print(out); out << std::endl;
		//        out << "\tNode in position ["
		//                << item_.second->m_mobilityModel->GetPosition().x << ", "
		//                << item_.second->m_mobilityModel->GetPosition().y << "]"
		//                << std::endl;
		//    }
		//
		//    for (auto& item_ : rm.m_routEntsMap )
		//    {
		//        out << "Rout ent. [" << item_.first << "] with addresses :"<< std::endl;
		//        for (auto& item2_ : *(item_.second))
		//        {
		//            out << "\t"; item2_.Print(out); out << std::endl;
		//        }
		//    }

		return out;
	}
}  //namespace ns3

#endif /* NETWORK_MONITOR_H_ */
