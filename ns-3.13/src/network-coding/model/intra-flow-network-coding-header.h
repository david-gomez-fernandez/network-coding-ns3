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


#ifndef INTRA_FLOW_NETWORK_CODING_HEADER_H_
#define INTRA_FLOW_NETWORK_CODING_HEADER_H_

#include "ns3/header.h"

#include "ns3/object.h"
#include "ns3/random-variable.h"
#include "ns3/packet.h"
#include "ns3/sequence-number.h"
#include "ns3/ipv4-address.h"
#include "ns3/tcp-header.h"

#include <stdio.h>
#include <iostream>


#include <openssl/md5.h> //Open SSL library --> Needed for hashing

namespace ns3 {

class IntraFlowNetworkCodingHeader:  public Header
{
public:
	/**
	 * Attribute handler
	 */
	static TypeId GetTypeId (void);
	virtual TypeId GetInstanceTypeId (void) const;
	/**
	 * Default constructor
	 */
	IntraFlowNetworkCodingHeader ();
	/**
	 * Default destructor
	 */
	virtual ~IntraFlowNetworkCodingHeader ();

	//Getters/setters
	virtual u_int16_t GetK() const;
	void SetK(u_int16_t k);
	virtual u_int16_t GetQ() const;
	void SetQ(u_int16_t q);
	virtual u_int32_t GetNfrag() const;
	void SetNfrag(u_int32_t nfrag);
	virtual u_int8_t GetTx() const;
	void SetTx(u_int8_t tx);
	void SetSourcePort (u_int16_t port);
	u_int16_t GetSourcePort () const;
	void SetDestinationPort (u_int16_t port);
	u_int16_t GetDestinationPort () const;

	virtual const std::vector<u_int8_t>& GetVector() const;
	void SetVector(const std::vector<u_int8_t>& vector);

	//Inherited methods from base class "Header" (pure virtual)
	virtual uint32_t GetSerializedSize (void) const;
	virtual void Serialize (Buffer::Iterator start) const;
	virtual u_int32_t Deserialize (Buffer::Iterator start);
	virtual void Print (std::ostream &os) const;

private:
	u_int16_t m_k;              // Fragment size --> Number of packets encoded altogether
	u_int8_t m_q; 				// GF(2^q) --> The current work version will only work with q=1 (NOTE: This parameter will not be included within the MORE header)
	u_int32_t m_nfrag;          //Fragment number
	u_int8_t m_tx;              //	Type of transmission: 0 means a data TX; 1 for backwards ACK

	u_int16_t m_sourcePort;
	u_int16_t m_destinationPort;

	std::vector <u_int8_t> m_vector;       // Coefficients vector
};


} //end namespace ns3
#endif /* INTRA_FLOW_NETWORK_CODING_HEADER_H_ */
