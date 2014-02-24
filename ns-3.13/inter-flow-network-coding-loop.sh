#! /bin/bash

#Attributes to be "tweaked" in a loop
# Wifi Channel FER:				--Fer=0.0
# Application data rate:  		--ns3::OnOffApplication::DataRate=11Mbps
# Number of IEEE 802.11 retx: 	--ns3::WifiRemoteStationManager::MaxSlrc=4
# Intra-flow NC K Value:		--ns3::IntraFlowNetworkCodingProtocol::K=64
# Intra-flow NC Q Value:		--ns3::IntraFlowNetworkCodingProtocol::Q=6
runOffset=0
maxSlrc=4
dataRate=11Mbps
maxCodedPackets=2

for fer in 0.0 0.1 0.2 0.3 0.4 0.5 0.6  
do
  for buffersize in 0 2 4 6 8 10 20 40
  do
    for bufferTimeout in 10 20 40 60 80 100 200 400 800 1600
    do
      #./waf --run "scratch/network-coding-bash-script --Configuration=network-coding-scenario --Fer=$fer --Timeout=$timeout --BufferSize=$buffersize"
      ./waf --run "scratch/test-scenario --Fer=$fer 
      				--ns3::OnOffApplication::DataRate=$dataRate 
      				--ns3::WifiRemoteStationManager::MaxSlrc=$maxSlrc
      				--ns3::InterFlowNetworkCodingBuffer::CodingBufferSize=$buffersize 
      				--ns3::InterFlowNetworkCodingBuffer::CodingBufferTimeout=$bufferTimeout
              --ns3::InterFlowNetworkCodingBuffer::MaxCodedPackets=$maxCodedPackets"
   	done
  done
done