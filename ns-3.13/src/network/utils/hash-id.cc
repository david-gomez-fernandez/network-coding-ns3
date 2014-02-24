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

#include "hash-id.h"


u_int16_t HashID (ns3::Ipv4Address ipSrc, ns3::Ipv4Address ipDst, u_int16_t portSrc, u_int16_t portDst)
{
	unsigned char hashTemp[32];
	unsigned char buffer [12], *ptr = buffer;
	u_int32_t temp;
	u_int16_t result;

	temp = ipSrc.Get ();
	memcpy (ptr + 0, (void *) &temp, 4);
	temp = ipDst.Get ();
	memcpy (ptr + 4, (void *) &temp, 4);
	memcpy (ptr + 8, (void *) &portSrc, 2);
	memcpy (ptr + 10, (void *) &portDst, 2);

	MD5 ((unsigned char *) buffer, 12, hashTemp);
	memcpy ((void *) &result, hashTemp, 2);

	return result;
}

u_int16_t HashID2 (ns3::Ipv4Address ipSrc, ns3::Ipv4Address ipDst, u_int16_t portSrc, u_int16_t portDst)
{
	u_int32_t temp;
	u_int32_t temp2;

	temp = ipSrc.Get ();
	temp2 = ipDst.Get ();

	return (u_int16_t) temp*portSrc+temp2*portDst;
}




