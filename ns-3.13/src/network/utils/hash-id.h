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

#ifndef HASH_ID_H_
#define HASH_ID_H_

#include <openssl/md5.h> //Open SSL library --> Needed for hashing
#include <ns3/ipv4-address.h>
#include <string.h>

/**
 * Hashing. The function will receive the EndPoints and will return the MD5 32-bit hash function, using the Open SSL library
 * \param ipSrc   Source IP address
 * \param ipDst   Destination IP address
 * \param portSrc Source port (TCP/UDP)
 * \param portDst Destination port (TCP/UDP)
 * \returns 	  The hash digest value
 */
u_int16_t HashID (ns3::Ipv4Address ipSrc, ns3::Ipv4Address ipDst, u_int16_t portSrc, u_int16_t portDst);


#endif /* HASH_ID_H_ */
