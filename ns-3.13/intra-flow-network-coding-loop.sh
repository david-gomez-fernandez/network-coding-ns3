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

for fer in 0.0   
do
  for K in 2 4 8 16 32 64 128 255
  do
    for Q in 1 2 3 4 5 6
    do
      #./waf --run "scratch/network-coding-bash-script --Configuration=network-coding-scenario --Fer=$fer --Timeout=$timeout --BufferSize=$buffersize"
      ./waf --run "scratch/test-scenario --Fer=$fer 
      				--ns3::OnOffApplication::DataRate=$dataRate 
      				--ns3::WifiRemoteStationManager::MaxSlrc=$maxSlrc
      				--ns3::IntraFlowNetworkCodingProtocol::K=$K 
      				--ns3::IntraFlowNetworkCodingProtocol::Q=$Q"
   	done
  done
done









