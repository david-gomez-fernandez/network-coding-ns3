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

#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/log.h"

#include "intra-flow-network-coding-header.h"

#include <stdio.h>

#include <cstdlib>
#include <iostream>       // std::cout
#include <bitset>         // std::bitset

#include <ctime>
#include <math.h>
#include <vector>
#include <cmath>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("IntraFlowNetworkCodingHeader");
NS_OBJECT_ENSURE_REGISTERED (IntraFlowNetworkCodingHeader);

IntraFlowNetworkCodingHeader::IntraFlowNetworkCodingHeader()
{
	NS_LOG_FUNCTION(this);
	m_k = 0;
	m_q = 1;     //By the moment, we will only work with GF (2^1)
    m_nfrag = 0;
    m_tx = 0;
    m_sourcePort = 0;
    m_destinationPort = 0;
}

IntraFlowNetworkCodingHeader::~IntraFlowNetworkCodingHeader()
{
	NS_LOG_FUNCTION(this);
}

TypeId
IntraFlowNetworkCodingHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::IntraFlowNetworkCodingHeader")
    .SetParent<Header> ()
    .AddConstructor<IntraFlowNetworkCodingHeader> ();
  return tid;
}

TypeId
IntraFlowNetworkCodingHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t IntraFlowNetworkCodingHeader::GetSerializedSize (void) const
{
	NS_LOG_FUNCTION (this);

		//New header
		return (9 + (u_int8_t) ceil ((double) (m_k * m_q) / 8.0));
}

 u_int16_t IntraFlowNetworkCodingHeader::GetK() const
{
	return m_k;
}

void IntraFlowNetworkCodingHeader::SetK(u_int16_t k)
{
	m_k = k;
}

u_int16_t IntraFlowNetworkCodingHeader::GetQ() const
{
	return m_q;
}

void IntraFlowNetworkCodingHeader::SetQ(u_int16_t q)
{
	m_q = q;
}

u_int32_t IntraFlowNetworkCodingHeader::GetNfrag() const
{
	return m_nfrag;
}

void IntraFlowNetworkCodingHeader::SetNfrag(u_int32_t nfrag)
{
	m_nfrag = nfrag;
}

u_int8_t IntraFlowNetworkCodingHeader::GetTx() const
{
	return m_tx;
}


void IntraFlowNetworkCodingHeader::SetTx(u_int8_t tx)
{
	m_tx = tx;
}

void IntraFlowNetworkCodingHeader::SetSourcePort (u_int16_t port)
{
	m_sourcePort = port;
}

u_int16_t IntraFlowNetworkCodingHeader::GetSourcePort () const
{
	return m_sourcePort;
}

void IntraFlowNetworkCodingHeader::SetDestinationPort (u_int16_t port)
{
	m_destinationPort = port;
}

u_int16_t IntraFlowNetworkCodingHeader::GetDestinationPort () const
{
	return m_destinationPort;
}


const std::vector<u_int8_t>& IntraFlowNetworkCodingHeader::GetVector() const
{
	return m_vector;
}

void IntraFlowNetworkCodingHeader::SetVector(const std::vector<u_int8_t>& vector)
{
	m_vector = vector;
}

void IntraFlowNetworkCodingHeader::Serialize (Buffer::Iterator start) const
{
	NS_LOG_FUNCTION (this);
	Buffer::Iterator i =start;

	//Fixed-size header
	i.WriteU8 (m_k);
	i.WriteU8 (m_q);
	i.WriteU16 (m_nfrag);
	i.WriteU8 (m_tx);
	i.WriteU16 (m_sourcePort);
	i.WriteU16 (m_destinationPort);

    std::bitset <8160> binaryVector;
    for (u_int32_t i=0; i<m_k; i++)
    {
        std::bitset<8160> temp (m_vector[i]);
        temp <<= i*m_q;
        binaryVector |= temp;
    }
	std::bitset <8160> maskHeader (255);
	std::vector <u_int8_t> headerVector;
	for (u_int32_t i=0; i < ceil((float) (m_k*m_q)/8); i++)
	{
		headerVector.push_back((binaryVector & maskHeader).to_ulong());
		binaryVector >>= 8;
	}
	if (m_k)
	{
		for (u_int8_t count = 0; count < headerVector.size(); count ++)
		{
			i.WriteU8 (headerVector [count]);
		}
	}
}

u_int32_t IntraFlowNetworkCodingHeader::Deserialize (Buffer::Iterator start)
{
	NS_LOG_FUNCTION (this);
	Buffer::Iterator i = start;

	m_k = i.ReadU8 ();
	m_q= i.ReadU8 ();
	m_nfrag = i.ReadU16 ();
	m_tx = i.ReadU8 ();
	m_sourcePort = i.ReadU16();
	m_destinationPort = i.ReadU16();
	std::vector <u_int8_t> headerVector;

	if (m_k)
	{
		vector<u_int8_t> codedVector;
		for( int j=0; j<ceil((float) (m_k*m_q)/8); j++)
		{
			u_int8_t temp = i.ReadU8();
			headerVector.push_back(temp);
		}
	}

	std::bitset <8160> deserializedVector;
	 std::bitset <8160> mask (pow(2,m_q)-1);
	for (u_int32_t i=0; i<headerVector.size(); i++)
	{
		std::bitset<8160> temp (headerVector[i]);
		temp <<= i*8;
		deserializedVector |= temp;
	}
	for (u_int8_t i=0; i< m_k; i++)
	{
		m_vector.push_back ((deserializedVector & mask).to_ulong());
		deserializedVector >>= m_q;
	}

	return GetSerializedSize ();
}

 void IntraFlowNetworkCodingHeader::Print (std::ostream &os) const
{
	os << " Tx= " << (int) m_tx << " k= " << (int) m_k << " q= " << (int)m_q << " Source Port " << m_sourcePort << " Destination Port " << m_destinationPort << " Nº Fragmento= " << (int) m_nfrag;
	os << " Vector ";

	for (u_int8_t i = 0; i < m_vector.size(); i++)
	{
		os << (int) m_vector[i] << " ";
	}
	os << std::endl;
}
