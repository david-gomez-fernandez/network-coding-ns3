/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Universidad de Cantabria
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
 *         Ramón Agüero Calvo <ramon@tlmat.unican.es>
 */

#include "proprietary-tracing.h"
#include "network-monitor.h"

//Needed to parse the packet content (BurstyErrorModel::ParsePacket)
#include "ns3/wifi-mac-header.h"
#include "ns3/llc-snap-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"

#include "ns3/tag.h"

#include "ns3/onoff-application.h"
#include "ns3/packet-sink.h"

#include <algorithm>
#include <numeric>
#include <vector>
#include <iterator>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("ProprietaryTracing");
NS_OBJECT_ENSURE_REGISTERED(ProprietaryTracing);


TracingInformation::TracingInformation()
{
	run = 0;
	totalRuns = 0;
	runOffset = 0;
	fer = 0.0;
	networkCoding="";

	packetLength = 0;
	numPackets = 0;
}

TracingInformation::~TracingInformation()
{

}

ProprietaryTracing::ProprietaryTracing ()
{
    NS_LOG_FUNCTION(this);
    m_totalDataPackets = 0;
    m_totalDataCorrectPackets = 0;
    m_totalDataCorruptedPackets = 0;
    m_txPackets = 0;
    m_rxPackets = 0;
    m_flowMonitorEnabler = false;
}

ProprietaryTracing::~ProprietaryTracing ()
{
    NS_LOG_FUNCTION(this);


}

//To be implemented
void ProprietaryTracing::ApplicationTxTrace (string context, Ptr<const Packet> packet)
{
	m_txPackets ++;

	if (m_applicationLevelLongTraceFile.is_open())
	{

	}

}

void ProprietaryTracing::ApplicationRxTrace (string context, Ptr<const Packet> packet, const Address& address)
{
	m_rxPackets ++;
	if (m_applicationLevelLongTraceFile.is_open())
	{
//		cout << context << endl;
	}
}

void ProprietaryTracing::EnableApplicationLongTraceFile ()
{
	char fileName [FILENAME_MAX];
	char buf[FILENAME_MAX];
	string temp;

	//Depending on whether the Network Coding Layer is enabled or not,
	if (!m_traceInfo.networkCoding.size())
	{
		sprintf (fileName, "APP_LONG_%s_%s_%s_FER_%1.2f_RUN_%03d.tr", m_traceInfo.transport.c_str(),
				m_traceInfo.deployment.c_str(), m_traceInfo.channel.c_str(), m_traceInfo.fer, m_traceInfo.run);
	}
	else
	{
		sprintf (fileName, "APP_LONG_%s_%s_%s_%s_FER_%1.2f_RUN_%03d.tr", m_traceInfo.transport.c_str(), m_traceInfo.networkCoding.c_str(),
						m_traceInfo.deployment.c_str(), m_traceInfo.channel.c_str(), m_traceInfo.fer, m_traceInfo.run);
	}

	//Print the titles line
	temp = string (fileName);
	std::replace (temp.begin(), temp.end(), '-', '_');

	string path = string (getcwd (buf, FILENAME_MAX));
	path += "/traces/" + temp;

	m_applicationLevelLongTraceFile.open(path.c_str(), fstream::out);

	sprintf (buf, "%16s %10s %10s %10s",
			"Time", "Node", "TX", "Length");

	m_applicationLevelLongTraceFile << (string) buf << endl;

}

void ProprietaryTracing::EnableApplicationShortTraceFile()
{
	NS_LOG_FUNCTION_NOARGS ();
		char buf[FILENAME_MAX];

		string fileName;
		string path = string (getcwd (buf, FILENAME_MAX));

		//Depending on whether the Network Coding Layer is enabled or not,
		if (!m_traceInfo.networkCoding.size())
		{
			fileName = "APP_SHORT_"  + m_traceInfo.transport + "_DEFAULT_" + m_traceInfo.deployment + '_' + m_traceInfo.channel + ".tr";
		}
		else
		{
			fileName = "APP_SHORT_"  + m_traceInfo.transport + '_' + m_traceInfo.networkCoding + "_" + m_traceInfo.deployment + '_' + m_traceInfo.channel + ".tr";
		}


		std::replace (fileName.begin(), fileName.end(), '-', '_');
		path += "/traces/" + fileName;

		//	Try to open an existing file; if error, open a new one
		m_applicationLevelShortTraceFile.open (path.c_str (), fstream::in | fstream::out | fstream::ate);

		if (m_applicationLevelShortTraceFile.fail ())
		{
			m_applicationLevelShortTraceFile.close ();
			char headerLine [FILENAME_MAX];
			m_applicationLevelShortTraceFile.open (path.c_str (), fstream::out | fstream::ate);

			sprintf (headerLine, "%6s %8s %10s %8s %8s %8s %8s %14s %14s %14s %14s",
					"No.", "Node", "SRC/SINK", "FER", "Pkt_len", "TX", "RX", "Thput(Mbps)", "Time (sec)", "Latency(ms)", "Jitter(ms^2)");

			m_applicationLevelShortTraceFile << headerLine << endl;
		}

}


void ProprietaryTracing::PrintApplicationStatistics()
{
	char line [FILENAME_MAX];
	double thput;
	double elapsedTime;
	double delay;
	double jitter;

	for (u_int8_t i = 0; i < NetworkMonitor::Instance().GetSourceApps().GetN(); i ++)
	{
		//Get the delay vector, in order to parse the latency and the jitter
		vector<double> delayVector;
		std::copy(NetworkMonitor::Instance().GetSourceApps().Get(i)->GetStats().txTimestamp.begin(),
				NetworkMonitor::Instance().GetSourceApps().Get(i)->GetStats().txTimestamp.end(), std::back_inserter(delayVector));
		std::adjacent_difference(NetworkMonitor::Instance().GetSourceApps().Get(i)->GetStats().txTimestamp.begin(),
				NetworkMonitor::Instance().GetSourceApps().Get(i)->GetStats().txTimestamp.end(),
				delayVector.begin());  //Caution --> The first element is a "spurious" value

		//Throughput (Mbps)
		NetworkMonitor::Instance().GetSourceApps().Get(i)->GetStats().txTimestamp.size() ? thput =
				(m_traceInfo.packetLength * NetworkMonitor::Instance().GetSourceApps().Get(i)->GetStats().txCounter * 8) /
				(NetworkMonitor::Instance().GetSourceApps().Get(i)->GetStats().txTimestamp.back() -
						NetworkMonitor::Instance().GetSourceApps().Get(i)->GetStats().txTimestamp.front()) / 1e6 : thput = 0.0;

		//Elapsed time (seconds)
		NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.size() ? elapsedTime =
				NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.back() -
				NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.front() : elapsedTime = 0.0;

		//Latency (msec)
		delayVector.size() ?
				delay = GetAverage (delayVector, delayVector.begin() + 1, delayVector.end()) * 1000 :
				delay = 0.0;
		//Jitter (msec^2)
		delayVector.size() ?
				jitter = GetVariance (delayVector, delay) * 1e6 :
				jitter = 0.0;

		sprintf (line, "%6d %8d %10d %8.4f %8d %8d %8d %14.4f %14.4f %14.4f %14.4e",
						m_traceInfo.run,
						NetworkMonitor::Instance().GetSourceApps().Get(i)->GetNode()->GetId(),
						0,
						m_traceInfo.fer,
						m_traceInfo.packetLength,
						NetworkMonitor::Instance().GetSourceApps().Get(i)->GetStats().txCounter,
						NetworkMonitor::Instance().GetSourceApps().Get(i)->GetStats().rxCounter,
						thput,
						elapsedTime,
						delay,
						jitter
				);
		m_applicationLevelShortTraceFile << line << endl;
	}

	for (u_int8_t i = 0; i < NetworkMonitor::Instance().GetSinkApps().GetN(); i ++)
	{
		//Get the delay vector, in order to parse the latency and the jitter
		vector<double> delayVector;
		std::copy(NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.begin(),
				NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.end(), std::back_inserter(delayVector));
		std::adjacent_difference(NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.begin(),
				NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.end(),
				delayVector.begin());  //Caution --> The first element is a "spurious" value

		//Throughput (Mbps)
		NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.size() ? thput =
				(m_traceInfo.packetLength * NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxCounter * 8) /
				(NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.back() -
						NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.front()) / 1e6 : thput = 0.0;

		//Elapsed time (seconds)
		NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.size() ? elapsedTime =
						NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.back() -
								NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxTimestamp.front() : elapsedTime = 0.0;

		//Latency (msec)
		delayVector.size() ?
		delay = GetAverage (delayVector, delayVector.begin() + 1, delayVector.end()) * 1000 :
		delay = 0.0;

		//Jitter (msec^2)
		delayVector.size() ?
		jitter = GetVariance (delayVector, delay) * 1e6 :
		jitter = 0.0;


		sprintf (line, "%6d %8d %10d %8.4f %8d %8d %8d %14.4f %14.4f %14.4f %14.4e",
				m_traceInfo.run,
				NetworkMonitor::Instance().GetSinkApps().Get(i)->GetNode()->GetId(),
				1,
				m_traceInfo.fer,
				m_traceInfo.packetLength,
				NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().txCounter,
				NetworkMonitor::Instance().GetSinkApps().Get(i)->GetStats().rxCounter,
				thput,
				elapsedTime,
				delay,
				jitter
		);
		m_applicationLevelShortTraceFile << line << endl;
	}
}


void ProprietaryTracing::EnableInterFlowNetworkCodingLongTraceFile ()
{
	NS_LOG_FUNCTION_NOARGS();
	/** Build the trace file name. We need the following stuff:
	 * - Transport layer used by the main application (i.e. TCP, UDP)
	 * - Scenario (i.e. X, Butterfly, etc.)
	 * - Channel (i.e. BEAR, HMP, etc.)
	 * - NC Coding Buffer Size (# packets)
	 * - NC Coding Buffer Timeout (in milliseconds)
	 * - NC Max Coded Packets
	 * - NC ACK Buffer Size (# packets)
	 * - NC ACK Buffer Timeout (in milliseconds)
	 * - FER (only useful in FER-based channel models)
	 * - Run
	 */
	char fileName [FILENAME_MAX];
	char buf[FILENAME_MAX];
	string temp;
	sprintf (fileName, "NC_INTER_LONG_%s_%s_%s_BS_%s_BTO_%d_CP_%s_ACKBS_%s_ACKBTO_%d_FER_%1.2f_RUN_%03d.tr", m_traceInfo.transport.c_str(), m_traceInfo.deployment.c_str(), m_traceInfo.channel.c_str(),
			InterFlowNetworkCodingBuffer::GetTypeId().GetAttribute(0).initialValue->SerializeToString(MakeUintegerChecker<u_int32_t> ()).c_str(),
			(int) Time (InterFlowNetworkCodingBuffer::GetTypeId().GetAttribute(1).initialValue->SerializeToString(MakeTimeChecker ())).GetMilliSeconds(),
			InterFlowNetworkCodingBuffer::GetTypeId().GetAttribute(2).initialValue->SerializeToString(MakeUintegerChecker<u_int32_t> ()).c_str(),
			InterFlowNetworkCodingBuffer::GetTypeId().GetAttribute(4).initialValue->SerializeToString(MakeUintegerChecker<u_int32_t> ()).c_str(),
			(int) Time (InterFlowNetworkCodingBuffer::GetTypeId().GetAttribute(5).initialValue->SerializeToString(MakeTimeChecker ())).GetMilliSeconds(),
			m_traceInfo.fer, m_traceInfo.run);

	//Print the titles line
	temp = string (fileName);
	std::replace (temp.begin(), temp.end(), '-', '_');

	string path = string (getcwd (buf, FILENAME_MAX));
	path += "/traces/" + temp;

	m_interFlowNetworkCodingLongFile.open(path.c_str(), fstream::out);

	sprintf (buf, "%16s %10s %10s %16s %16s %10s %10s %10s %16s %16s %12s %12s %12s ",
			"Time", "TX/RX", "Node ID", "Source IP", "Dest. IP", "Src Port", "Dst Port",
			"Length", "TCP SeqNum", "TCP AckNum", "Coded pkts", "Emb. ACKs", "Decoded");

	m_interFlowNetworkCodingLongFile << (string) buf << endl;
}

void ProprietaryTracing::InterFlowNetworkCodingLongTrace (Ptr<Packet> packet, u_int8_t tx, u_int32_t nodeId,
		Ipv4Address source, Ipv4Address destination, u_int8_t codedPackets,
		u_int8_t embeddedAcks, bool decodeSuccess)
{
	NS_LOG_FUNCTION(this);

	//Run this function if and only if the file is open, hence it has to print out the corresponding line
	if (m_interFlowNetworkCodingLongFile.is_open())
	{

		char line [255];
		u_int8_t src [4], dst [4];
		char sourceChar [32], destChar [32];
		source.Serialize(src);
		destination.Serialize(dst);
		sprintf(sourceChar, "%d.%d.%d.%d", src [0], src [1], src [2], src [3]);
		sprintf(destChar, "%d.%d.%d.%d", dst [0], dst [1], dst [2], dst [3]);

		u_int8_t tcpHeaderSize = 0;

		NS_ASSERT((tx >= 0) && (tx <= 5));
		Ptr<Packet> packetCopy = packet->Copy ();

		//Get the TCP header
		InterFlowNetworkCodingHeader interHeader;
		packetCopy->RemoveHeader (interHeader);

		//Received a void encapsulated ACK?
		TcpHeader tcpHeader;

		if (interHeader.GetEmbeddedAcks() && !packetCopy->GetSize() )
		{
			//In this case, we will print out the information relative to the first element found in the buffer
			tcpHeader = interHeader.m_tcpAckVector[0].tcpHeader;
			tcpHeaderSize = 0;
		}
		else
		{
			//Pull the TCP header
			packetCopy->PeekHeader (tcpHeader);
			tcpHeaderSize = tcpHeader.GetSerializedSize ();
		}

		sprintf(line, "%16f %10d %10d %16s %16s %10d %10d %10d %16d %16d %12d %12d %12d",
				Simulator::Now().GetSeconds(), tx, nodeId, sourceChar, destChar,
				tcpHeader.GetSourcePort(), tcpHeader.GetDestinationPort(),
				packet->GetSize() - interHeader.GetSerializedSize() -  tcpHeaderSize,
				tcpHeader.GetSequenceNumber().GetValue(),
				tcpHeader.GetAckNumber().GetValue(),
				codedPackets, embeddedAcks, decodeSuccess);

		m_interFlowNetworkCodingLongFile << line << endl;
	}
}

void ProprietaryTracing::EnableInterFlowNetworkCodingShortTraceFile ()
{
	NS_LOG_FUNCTION_NOARGS ();
	char buf[FILENAME_MAX];

	string fileName;

	string path = string (getcwd (buf, FILENAME_MAX));

	fileName = "NC_INTER_SHORT_"  + m_traceInfo.transport + '_' + m_traceInfo.deployment + '_' + m_traceInfo.channel + ".tr";
	std::replace (fileName.begin(), fileName.end(), '-', '_');
	path += "/traces/" + fileName;

	//	Try to open an existing file; if error, open a new one
	m_interFlowNetworkCodingShortFile.open (path.c_str (), fstream::in | fstream::out | fstream::ate);

	if (m_interFlowNetworkCodingShortFile.fail ())
	{
		m_interFlowNetworkCodingShortFile.close ();
		char headerLine [196];
		m_interFlowNetworkCodingShortFile.open (path.c_str (), fstream::out | fstream::ate);

		sprintf (headerLine, "%6s %8s %8s %8s %10s %10s %8s %10s %10s %14s %14s %14s %14s",
				"No.", "BufSiz", "BufTO", "MaxCP", "AckBufSiz", "AckBufTO", "FER", "C.Rate", "D.rate", "RX_bytes", "Time", "Throughput", "Transmissions");

		m_interFlowNetworkCodingShortFile << headerLine << endl;
	}
}

void ProprietaryTracing::PrintInterFlowNetworkCodingStatistics ()
{
	NS_LOG_FUNCTION_NOARGS();

	u_int32_t totalCodedTx = 0;
	u_int32_t totalUncodedTx = 0;
	u_int32_t totalDecodeSuccess = 0;
	u_int32_t totalDecodeFailure = 0;
	u_int32_t totalRxBytes = 0;
	u_int32_t totalTransmissions = 0;

	float totalThput = 0.0;
	u_int8_t counter = 0;			//Number of flows counter

	double codingRate = 0.0;
	double decodingRate = 0.0;

	double elapsedTime = 0.0;
	double totalElapsedTime = 0.0;
	char line [FILENAME_MAX];

	for (u_int8_t i = 0; i < NetworkMonitor::Instance().GetNetworkCodingVectorSize(); i ++)
	{
		double thput;
		struct InterFlowNetworkCodingStatistics *temp = NetworkMonitor::Instance().GetNetworkCodingElement(i)->GetNetworkCodingStatistics();

		totalCodedTx += temp->codedPacketTx;
		totalUncodedTx += temp->uncodedPacketTx;
		totalDecodeSuccess += temp->decodeSuccess;
		totalDecodeFailure += temp->decodeFailure;
		totalRxBytes += temp->networkCodingLayerTotalBytes;
		totalTransmissions += temp->transmissionNumber;

		elapsedTime = temp->lastReception - temp->startingTime;
		thput = (double) temp->networkCodingLayerTotalBytes * 8 / (temp->lastReception - temp->startingTime) / 1e6;

		//	if (thput == thput)			//Odd style to check if a double variable has a NaN value
		if (temp->networkCodingLayerTotalBytes)
		{
			totalElapsedTime += elapsedTime;
			totalThput += thput;
			counter ++;
		}
	}

	codingRate = (double) totalCodedTx / (totalCodedTx + totalUncodedTx);
	if (codingRate != codingRate)
		codingRate = 0.0;
	decodingRate = (double) totalDecodeSuccess / (totalDecodeSuccess + totalDecodeFailure);
	if (decodingRate != decodingRate)
		decodingRate = 0.0;

	if (totalThput != totalThput)
		totalThput = 0.0;

	sprintf (line, "%6d %8s %8d %8s %10s %10d %8.2f %10.4f %10.4f %14d %14.4f %14.4f %14d",
			m_traceInfo.run,
			InterFlowNetworkCodingBuffer::GetTypeId().GetAttribute(0).initialValue->SerializeToString(MakeUintegerChecker<u_int32_t> ()).c_str(),
			(int) Time (InterFlowNetworkCodingBuffer::GetTypeId().GetAttribute(1).initialValue->SerializeToString(MakeTimeChecker ())).GetMilliSeconds(),
			InterFlowNetworkCodingBuffer::GetTypeId().GetAttribute(2).initialValue->SerializeToString(MakeUintegerChecker<u_int32_t> ()).c_str(),
			InterFlowNetworkCodingBuffer::GetTypeId().GetAttribute(4).initialValue->SerializeToString(MakeUintegerChecker<u_int32_t> ()).c_str(),
			(int) Time (InterFlowNetworkCodingBuffer::GetTypeId().GetAttribute(5).initialValue->SerializeToString(MakeTimeChecker ())).GetMilliSeconds(),
			m_traceInfo.fer, codingRate, decodingRate,
			totalRxBytes, (double) elapsedTime,
			(double) totalThput / counter,
			totalTransmissions);

	m_interFlowNetworkCodingShortFile << line << endl;
}

void ProprietaryTracing::EnableIntraFlowNetworkCodingLongTraceFile ()
{
	NS_LOG_FUNCTION_NOARGS();
	/** Build the trace file name. We need the following stuff:
	 * - Transport layer used by the main application (i.e. TCP, UDP)
	 * - Scenario (i.e. X, Butterfly, etc.)
	 * - Channel (i.e. BEAR, HMP, etc.)
	 * - NC Coding Buffer Size (# packets)
	 * - NC Coding Buffer Timeout (in milliseconds)
	 * - NC Max Coded Packets
	 * - NC ACK Buffer Size (# packets)
	 * - NC ACK Buffer Timeout (in milliseconds)
	 * - FER (only useful in FER-based channel models)
	 * - Run
	 */
	char fileName [FILENAME_MAX];
	string temp;
	sprintf (fileName, "NC_INTRA_LONG_%s_%s_%s_Q_%s_K_%s_FER_%1.2f_RUN_%03d.tr", m_traceInfo.transport.c_str(), m_traceInfo.deployment.c_str(), m_traceInfo.channel.c_str(),
			IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(0).initialValue->SerializeToString(MakeUintegerChecker<u_int32_t> ()).c_str(),
			IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(1).initialValue->SerializeToString(MakeUintegerChecker<u_int32_t> ()).c_str(),
			m_traceInfo.fer, m_traceInfo.run);

	//Print the titles line
	temp = string (fileName);
	std::replace (temp.begin(), temp.end(), '-', '_');

	char buf[FILENAME_MAX];
	string path = string (getcwd (buf, FILENAME_MAX));
	path += "/traces/" + temp;

	m_intraFlowNetworkCodingLongFile.open(path.c_str(), fstream::out);

	sprintf(buf, "%16s %10s %10s %16s %16s %10s %16s",
			"Time","CODE", "NodeID", "IP_Src","Ip_Dst",
			"Length","Frag_Num" );
	m_intraFlowNetworkCodingLongFile << (string) buf << endl;
}

void ProprietaryTracing::EnableIntraFlowNetworkCodingShortTraceFile ()
{
	NS_LOG_FUNCTION_NOARGS ();
	char buf[FILENAME_MAX];

	string fileName;

	string path = string (getcwd (buf, FILENAME_MAX));

	fileName = "NC_INTRA_SHORT_"  + m_traceInfo.transport + '_' + m_traceInfo.deployment + '_' + m_traceInfo.channel + ".tr";
	std::replace (fileName.begin(), fileName.end(), '-', '_');
	path += "/traces/" + fileName;

	//	Try to open an existing file; if error, open a new one
	m_intraFlowNetworkCodingShortFile.open (path.c_str (), fstream::in | fstream::out | fstream::ate);

	if (m_intraFlowNetworkCodingShortFile.fail ())
	{
		m_intraFlowNetworkCodingShortFile.close ();
		char headerLine [FILENAME_MAX];
		m_intraFlowNetworkCodingShortFile.open (path.c_str (), fstream::out | fstream::ate);

		sprintf (headerLine, "%6s %5s %8s %10s %5s %5s %14s %8s %8s %8s %8s %16s %16s %16s %16s %16s %16s",
				"No.", "Node", "FER", "Pkt_len", "Q", "K", "Thput(Mbps)", "TX_app", "RX_app","TX_nc","Rx_nc","Avg_delay(ms)","Var_delay(ms^2)","Avg_Rank(ms)","Var_Rank(ms^2)","Avg_Inv(ms)","Var_Inv(ms^2)");

		m_intraFlowNetworkCodingShortFile << headerLine << endl;
	}
}

void ProprietaryTracing::IntraFlowNetworkCodingLongTrace (Ptr<Packet> packet, u_int8_t tx, u_int32_t nodeId, Ipv4Address source, Ipv4Address destination)
{
	NS_LOG_FUNCTION(this);
	char line [255];
	u_int8_t src [4], dst [4];
	char sourceChar [32], destChar [32];
	source.Serialize(src);
	destination.Serialize(dst);
	sprintf(sourceChar, "%d.%d.%d.%d", src [0], src [1], src [2], src [3]);
	sprintf(destChar, "%d.%d.%d.%d", dst [0], dst [1], dst [2], dst [3]);

	NS_ASSERT((tx >= 0) && (tx <= 9));
	Ptr<Packet> packetCopy = packet->Copy ();

	IntraFlowNetworkCodingHeader header;
	packetCopy->RemoveHeader(header);

	if( tx==3 || tx==5)	// Used to distinguish between the delivery and reception of the ack's
	{
		sprintf(line, "%16f %10d %10d %16s %16s %10d %16d",
				Simulator::Now().GetSeconds(), tx, nodeId, sourceChar, destChar,
				packetCopy->GetSize(),header.GetNfrag() );
	}
	else
	{
		sprintf(line, "%16f %10d %10d %16s %16s %10d %16d",
				Simulator::Now().GetSeconds(), tx, nodeId, sourceChar, destChar,
				packetCopy->GetSize()-8,header.GetNfrag() );

	}

		m_intraFlowNetworkCodingLongFile << line << endl;
}

void ProprietaryTracing::PrintIntraFlowNetworkCodingStatistics ()
{
	u_int8_t q = atoi(IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(0).initialValue->SerializeToString (MakeUintegerChecker<u_int8_t> ()).c_str());
	string itpp = IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(3).initialValue->SerializeToString (MakeBooleanChecker ());

	for (u_int8_t i = 0; i < NetworkMonitor::Instance().GetNetworkCodingVectorSize(); i ++)
	{
		Ptr<IntraFlowNetworkCodingProtocol> protocol = DynamicCast<IntraFlowNetworkCodingProtocol> (NetworkMonitor::Instance().GetNetworkCodingElement(i));
		IntraFlowNetworkCodingStatistics temp (protocol->GetStats());
		char line [FILENAME_MAX];
		double avgDelay;
		double varDelay;
		double avgRank;
		double varRank;
		double avgInverse;
		double varInverse;

		//Get the delay vector
		vector<double> delayVector;
		std::copy(temp.timestamp.begin(), temp.timestamp.end(), std::back_inserter(delayVector));
		std::adjacent_difference(temp.timestamp.begin(), temp.timestamp.end(),delayVector.begin());  //Caution --> The first element is a "spurious" value

		//Those nodes which do no receive any data information will have its throughput equal to zero (obviously)
		double thput;
		temp.timestamp.size() ? thput = (m_traceInfo.packetLength * temp.upNumber * 8)/ (temp.timestamp.back() - temp.timestamp.front()) / 1e6 : thput = 0.0;

		//Get the average values (in milliseconds)
		temp.timestamp.size() > 2 ? avgDelay = GetAverage (delayVector, delayVector.begin() + 1, delayVector.end()) * 1000 : avgDelay = 0.0;
		temp.timestamp.size() > 2 ? varDelay = GetVariance (delayVector, avgDelay) * 1e6 : varDelay = 0.0;
		temp.rankTime.size() ? avgRank = GetAverage (temp.rankTime, temp.rankTime.begin(), temp.rankTime.end()) : avgRank = 0.0;
		temp.rankTime.size() ? varRank = GetVariance (temp.rankTime, avgRank) : varRank = 0.0;
		temp.inverseTime.size() ? avgInverse = GetAverage (temp.inverseTime, temp.inverseTime.begin(), temp.inverseTime.end()) : avgInverse = 0.0;
		temp.inverseTime.size() ? varInverse = GetVariance (temp.inverseTime, avgInverse) : varInverse = 0.0;

		//Special case: Print a "0" in those cases where we are using the IT++ library
		sprintf (line, "%6d %5d %8.2f %10d %5d %5d %14.6f %8d %8d %8d %8d %16.4f %16.2e %16.4f %16.2e %16.4f %16.2e",
				m_traceInfo.run, i, m_traceInfo.fer, m_traceInfo.packetLength,
				(itpp=="true" && q==1) ? 0 : q,   //Q
				atoi(IntraFlowNetworkCodingProtocol::GetTypeId().GetAttribute(1).initialValue->SerializeToString (MakeUintegerChecker<u_int8_t> ()).c_str()),   //K
				thput, temp.downNumber, temp.upNumber, temp.txNumber, temp.rxNumber, avgDelay, varDelay, avgRank, varRank, avgInverse, varInverse);

		m_intraFlowNetworkCodingShortFile << line << endl;
	}
}

void ProprietaryTracing::EnableWifiPhyLevelTracing ()
{
	/** Build the trace file name. We need the following stuff:
	 * - Transport layer used by the main application (i.e. TCP, UDP)
	 * - Scenario (i.e. X, Butterfly, etc.)
	 * - Channel (i.e. BEAR, HMP, etc.)
	 * - FER (only useful in FER-based channel models)
	 * - Run
	 */
	char fileName [FILENAME_MAX];
	string temp;
	sprintf (fileName, "PHY_WIFI_%s_%s_%s_FER_%.2f_RUN_%03d.tr", m_traceInfo.transport.c_str(), m_traceInfo.deployment.c_str(), m_traceInfo.channel.c_str(),
			m_traceInfo.fer, m_traceInfo.run);

	//Print the titles line
	temp = string (fileName);
	std::replace (temp.begin(), temp.end(), '-', '_');

	char buf[FILENAME_MAX];
	string path = string (getcwd (buf, FILENAME_MAX));
	path += "/traces/" + temp;

	m_phyWifiLevelTracing.open(path.c_str(), fstream::out);

	sprintf(buf, "%10s %8s %5s %18s %18s %6s %6s %16s %16s %14s %8s %8s %12s %12s %6s %8s %13s",
			"Time", "Node_ID", "CRC", "MAC_SRC", "MAC_DST", "RETX", "SN", "IP_SRC", "IP_DST", "PROT", "SRC_PORT", "DST_PORT", "TCP_SN", "TCP_Ack", "Flags", "Length", "SNR/State");

	m_phyWifiLevelTracing << (string) buf << endl;
}

void ProprietaryTracing::WifiPhyRxTrace (Ptr<Packet> packet, bool error, double snr, int nodeId)
{
	NS_LOG_FUNCTION(this);

	//Output message
	char output [FILENAME_MAX];
	bool somethingToPrint = false;

	//IP address to string converters
	u_int8_t source [4], destination [4];
	char sourceChar [32], destChar [32];

	//Protocol headers
	WifiMacHeader wifiHeader;
	LlcSnapHeader llcHeader;
	Ipv4Header ipHeader;
	TcpHeader tcpHeader;
	UdpHeader udpHeader;
	InterFlowNetworkCodingHeader interHeader;
	IntraFlowNetworkCodingHeader intraHeader;

	//Update overall statistics (for short-tracing issues)
	if (packet->GetSize() > 500)
	{
		if (error == 1)
		{
			m_totalDataCorrectPackets++;
			m_totalDataPackets++;
		}
		else
		{
			m_totalDataCorruptedPackets++;
			m_totalDataPackets++;
		}
	}


	if (m_phyWifiLevelTracing.is_open())
	{
		//Parse packet and print the most highlighting data
		Ptr<Packet> pktCopy = packet->Copy ();

		pktCopy->RemoveHeader (wifiHeader);

		if (wifiHeader.IsData ())
		{
			pktCopy->RemoveHeader(llcHeader);
			switch (llcHeader.GetType())
			{
			case 0x0806: //ARP (Nothing to trace)

			break;
			case 0x0800: //IP packet
				pktCopy->RemoveHeader(ipHeader);

				ipHeader.GetSource().Serialize (source); //Get a string from Ipv4Address
				ipHeader.GetDestination().Serialize (destination);
				sprintf(sourceChar, "%d.%d.%d.%d", source[0], source[1], source[2], source[3]);
				sprintf(destChar, "%d.%d.%d.%d", destination[0], destination[1], destination[2], destination[3]);

				switch (ipHeader.GetProtocol())
				{
				case 1:	//ICMP

					break;
				case 6: //TCP
					somethingToPrint = true;
					pktCopy->RemoveHeader (tcpHeader);

					sprintf (output, "%10f %8d %5d %18s %18s %6d %6d %16s %16s %14s %8d %8d %12d %12d %6X %8d %13.3f",
							Simulator::Now().GetSeconds(), nodeId, error, ConvertMacToString (wifiHeader.GetAddr2()).c_str(), ConvertMacToString(wifiHeader.GetAddr1()).c_str(),
							wifiHeader.IsRetry(), wifiHeader.GetSequenceNumber (),
							sourceChar, destChar, "TCP", tcpHeader.GetSourcePort(), tcpHeader.GetDestinationPort(),
							tcpHeader.GetSequenceNumber ().GetValue(), tcpHeader.GetAckNumber().GetValue(),
							tcpHeader.GetFlags (), pktCopy->GetSize (), snr);

					break;
				case 17: //UDP
					somethingToPrint = true;
					pktCopy->RemoveHeader (udpHeader);

					sprintf(output, "%10f %8d %5d %18s %18s %6d %6d %16s %16s %14s %8d %8d %12d %12d %6d %8d %13.3f",
					                    Simulator::Now().GetSeconds(), nodeId, error, ConvertMacToString(wifiHeader.GetAddr2()).c_str(), ConvertMacToString(wifiHeader.GetAddr1()).c_str(),
					                    wifiHeader.IsRetry(), wifiHeader.GetSequenceNumber(),
					                    sourceChar, destChar, "UDP", udpHeader.GetSourcePort(), udpHeader.GetDestinationPort(),
					                    0, 0, 0, pktCopy->GetSize(), snr);

					break;
				case 99:  //Inter-Flow Network Coding
					somethingToPrint = true;
					pktCopy->RemoveHeader (interHeader);

					//It might contain either TCP segments or UDP datagrams
					switch (interHeader.GetProtocolNumber())
					{
					case 6:  //TCP
						pktCopy->RemoveHeader (tcpHeader);
						sprintf (output, "%10f %8d %5d %18s %18s %6d %6d %16s %16s %14s %8d %8d %12d %12d %6X %8d %13.3f",
								Simulator::Now().GetSeconds(), nodeId, error, ConvertMacToString(wifiHeader.GetAddr2()).c_str(), ConvertMacToString(wifiHeader.GetAddr1()).c_str(),
								wifiHeader.IsRetry(), wifiHeader.GetSequenceNumber(),
								sourceChar, destChar, "Inter-NC", tcpHeader.GetSourcePort(), tcpHeader.GetDestinationPort(),
								tcpHeader.GetSequenceNumber().GetValue(), tcpHeader.GetAckNumber().GetValue(),
								tcpHeader.GetFlags(), pktCopy->GetSize(), snr);

						break;
					case 17:  //UDP
						pktCopy->RemoveHeader (udpHeader);
						sprintf(output, "%10f %8d %5d %18s %18s %6d %6d %16s %16s %14s %8d %8d %12d %12d %6d %8d %13.3f",
								Simulator::Now().GetSeconds(), nodeId, error, ConvertMacToString(wifiHeader.GetAddr2()).c_str(), ConvertMacToString(wifiHeader.GetAddr1()).c_str(),
								wifiHeader.IsRetry(), wifiHeader.GetSequenceNumber(),
								sourceChar, destChar, "Inter-NC", udpHeader.GetSourcePort(), udpHeader.GetDestinationPort(),
								0, 0, 0, pktCopy->GetSize(), snr);


						break;

					default:
						NS_ABORT_MSG ("Protocol not handled by the NC entity. Please fix");
						break;
					}
					break;

				case 100: //Intra-Flow Network Codingcase 100:
					somethingToPrint = true;
					pktCopy->RemoveHeader (intraHeader);
					sprintf(output, "%10f %8d %5d %18s %18s %6d %6d %16s %16s %14s %8d %8d %12d %12d %6d %8d %13.3f",
							Simulator::Now().GetSeconds(), nodeId, error, ConvertMacToString(wifiHeader.GetAddr2()).c_str(), ConvertMacToString(wifiHeader.GetAddr1()).c_str(),
							wifiHeader.IsRetry(), wifiHeader.GetSequenceNumber(),
							sourceChar, destChar, "Intra-NC", intraHeader.GetSourcePort(), intraHeader.GetDestinationPort(),
							0, 0, 0, pktCopy->GetSize(), snr);
					break;

				default:
					NS_LOG_ERROR("Protocol not implemented yet (IP) --> " << ipHeader.GetProtocol());
					break;
				}
				break;
				default:
					NS_LOG_ERROR("Protocol not implemented yet (LLC) --> " << std::hex << llcHeader.GetType() << std::dec);
					break;
			}
		}

		if (somethingToPrint)
			m_phyWifiLevelTracing << (string) output << endl;
	}
}

void ProprietaryTracing::PrintToPrompt ()
{
	char output [FILENAME_MAX];
	sprintf(output, "Run %d - %d/%d (FER = %f) %d/%d (Application Loss Rate= %f)",
			m_traceInfo.run,
			m_totalDataCorrectPackets,
			m_totalDataPackets,
			(double)  ((double) m_totalDataPackets - (double) m_totalDataCorrectPackets) / (double) m_totalDataPackets,
			m_txPackets,
			m_rxPackets,
					(double)  ((double) m_txPackets	- (double) m_rxPackets)	/ (double) m_txPackets);

	cout << output << endl;
}

void ProprietaryTracing::PrintStatistics ()
{
	NS_LOG_FUNCTION (this);

	if (m_applicationLevelLongTraceFile.is_open())
	{
		m_applicationLevelLongTraceFile.close();
	}

	if (m_applicationLevelShortTraceFile.is_open())
	{
		PrintApplicationStatistics();
		m_applicationLevelShortTraceFile.close();
	}

	if (m_interFlowNetworkCodingLongFile.is_open())
	{
		m_interFlowNetworkCodingLongFile.close();
	}

	if (m_interFlowNetworkCodingShortFile.is_open())
	{
		PrintInterFlowNetworkCodingStatistics();
		m_interFlowNetworkCodingShortFile.close();
	}

	if (m_intraFlowNetworkCodingLongFile.is_open())
	{
		m_intraFlowNetworkCodingLongFile.close();
	}

	if (m_intraFlowNetworkCodingShortFile.is_open())
	{
		PrintIntraFlowNetworkCodingStatistics();
		m_intraFlowNetworkCodingShortFile.close();
	}

	if (m_phyWifiLevelTracing.is_open())
	{
		m_phyWifiLevelTracing.close();
	}

	PrintToPrompt();

	if (m_flowMonitorEnabler)
	{
		PrintFlowMonitorStats ();
	}

}

void ProprietaryTracing::EnableFlowMonitor ()
{
	m_flowMonitorEnabler = true;

	// Install FlowMonitor on all nodes
	m_flowMonitor = m_flowMonitorHelper.InstallAll();
}

void ProprietaryTracing::PrintFlowMonitorStats ()
{
	string fileName;
	char message [FILENAME_MAX];

	//IP address to string converters
	u_int8_t source [4], destination [4];
	char sourceChar [32], destinationChar [32];

	cout << "print flow monitor stats" << endl;

	// Print per flow statistics
	m_flowMonitor->CheckForLostPackets ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (m_flowMonitorHelper.GetClassifier ());
	std::map<FlowId, FlowMonitor::FlowStats> stats = m_flowMonitor->GetFlowStats ();

	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); iter ++)
	{
			Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);

			//Avoid printing the ARP messages
			if ((t.sourcePort != 9) && (t.destinationPort !=9))
			{
				t.sourceAddress.Serialize(source); //Get a string from Ipv4Address
				t.destinationAddress.Serialize(destination);
				sprintf(sourceChar, "%d.%d.%d.%d", source[0], source[1], source[2], source[3]);
				sprintf(destinationChar, "%d.%d.%d.%d", destination[0], destination[1], destination[2], destination[3]);


				sprintf (message, "Flow %3d - Protocol %4d: %16s (%5d) -> %16s (%5d) - TX=%6d RX=%6d (%6d lost) --> Thput=%10.2f Kbps - FWD packets=%6d",
						iter->first, t.protocol, sourceChar, t.sourcePort, destinationChar, t.destinationPort,
						iter->second.txPackets, iter->second.rxPackets, iter->second.lostPackets,
						(double) (iter->second.rxBytes * 8)/(iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstRxPacket.GetSeconds()) / 1024,
						iter->second.timesForwarded);

				cout << message << endl;
			}
	}

	//Comment/discomment to enable/disable the XML output
	fileName = "traces/flowmonitor/FlowMonitor_"  + m_traceInfo.transport + '_' + m_traceInfo.deployment + '_' + m_traceInfo.channel + ".flowmon";
	//NetworkMonitor::Instance().m_flowMonitor->SerializeToXmlFile(fileName, true, true);
}


std::string ProprietaryTracing::ConvertMacToString (Mac48Address mac)
{
    //	NS_LOG_FUNCTION(mac);
    u_int8_t temp[6];
    char result[24];
    mac.CopyTo(temp);

    sprintf(result, "%02X:%02X:%02X:%02X:%02X:%02X", temp[0], temp[1], temp[2], temp[3], temp[4], temp[5]);

    return std::string(result);
}

template <typename T1>
double ProprietaryTracing::GetAverage (typename std::vector<T1> delayVector)
{
	return  std::accumulate (delayVector.begin(), delayVector.end(), 0.0) / delayVector.size();
}

template <typename T1, typename _InputIterator>
double ProprietaryTracing::GetAverage(std::vector<T1> delayVector, _InputIterator __first, _InputIterator __last)
{
	typedef typename iterator_traits<_InputIterator>::value_type _ValueType;

	// concept requirements
	__glibcxx_function_requires(_InputIteratorConcept<_InputIterator>)
	__glibcxx_function_requires(_OutputIteratorConcept<_OutputIterator,
			_ValueType>)
	__glibcxx_requires_valid_range(__first, __last);

	return  std::accumulate (__first, __last, 0.0) / delayVector.size();
}

template <typename T1>
double ProprietaryTracing::GetVariance(std::vector<T1> delayVector, double average)
{
	int sum = 0;
	for (u_int16_t i = 0; i < delayVector.size(); i++)
	{
		sum += (delayVector[i] - average) * (delayVector[i] - average);
	}
	return sum / ((double) delayVector.size() - 1);
}

