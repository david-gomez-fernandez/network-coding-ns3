
# Network coding implementation on ns-3 (ns-3.13) #

In this repository we present the following features, whose operation can be split into the following points:

- **Network Coding**. The cornerstone of the repo. We have added a brand new level between the transport and IP layers and implements two different Network Coding protocols to improve the poor performance exhibited by TCP over wireless mesh networks: the first one aims at mixing the information belonging to different TCP data flows (*InterFlowNetworkCodingProtocol*, whose prior work could point to [1]); on the other hand, the second protocol opts for UDP as the transport level solution, providing a reliable transmission by means of the random linear coding of packets belonging to the same flow (*IntraFlowNetworkCodingProtocol*, whose one of its main exponents is [2]). Last, but not least, it is worth highlighting that we had to rely on different cross-layer mechanisms to perform the full protocol operation (namely, they are able to have a direct interplay with some Wifi's sublayers, i.e. *WifiMacQueue*).
- **Scenario creator**. In order to ease the construction of any scenario (since our project only focuses on wireless topologies, the unique physical layer with which we are dealing with is based on the IEEE 802.11b physical recommendation), we have developed a new module that allows the "offline" scenarios off-line configuration (without the need of a new compilation). Take a look at the documentation inside the module to have a better knowledge about how to generate scenarios through this tool.
- **IEEE 802.11b channel models** (namely, propagation and error ones). Based on the results achieved over a empirical campaign carried out over a real indoor scenario (i.e. a typical office environment), we have tailored two different channel models that aims to replicate the memory behavior showcased by a real wireless channel, where errors do not occur isolatedly, but they happen in bursts. In a nutshell, *HMP* defines a hidden Markov process to represent to dynamic behavior of a wireless channel; on the other hand, *BEAR* introduces an *auto-reggresive* filter so as to estimate the received signal strength . For further details, please refer to our [GitHub's]() repository that focuses on these ones. Last, but not least, the reader can find more information in our publications regarding these models (i.e. [3], [4], [5]).
- **Additional stuff** belonging to already-existing sources (i.e. MatrixErrorModel, ProprietaryTracing, etc.).

## External libraries ##

We have needed to use a number of functionalities that belongs to external libraries:

1. [Open SSL](https://www.openssl.org/). The open source toolkit for SSL/TLS. Namely, we have chosen the popular *MD5* function to carry out the **hash** operations within the Network Coding module. It can be easily found through i.e. the Synaptic package repository.
2. [IT++](http://itpp.sourceforge.net/4.3.1/) (*libitpp*). C++ mathematical library. Namely, we will use its Galois Field GF(2) operations, since its performance is significatively higher that the one achieved by the previous one over the base field GF(2). The main drawback of this library is that it do not support all the matrix operations we need for extended *GFs*. As well as the previous one, there is no problem to find this library.
3. [FFLAS/FFPACK](http://linalg.org/fflas-ffpack-html/index.html). Another C++ mathematical library that allows us to work with extended Galois Fields GF(2^q). Undoubtely, this is the most problematic library to install. First, it has various dependencies that must be installed before the final package (i.e. Givaro, lapack, etc.). I strongly recommend to Google it before installing. Besides, there is a bug located in this library which will trigger a system crash. The solution we have followed is to slightly tweak one of the header (namely, */fflas-ffpack/utils/debug.h*), introducing an anonymous namespace. You can find the solution [here](https://groups.google.com/forum/#!topic/ffpack-devel/GT2lNc0x-n4).

This implies the need to link them at the corresponding "wscript" files (take a look at the *network-coding-module*).

## Installation ##

In order not to overload the repository with too much unnecessary code, we have **ONLY** included all the files that strictly belong to the ns-3.13 folder. Hence, it should be enough to compile it and enjoy :-).

One of our main goals was not to tamper the legacy coded provided by the simulator. However, there have been various situations in which we had to introduce "pieces of code" to allow a correct operation of our solutions. Namely, we have tweaked two different modules (i.e. wifi and internet). In addition, it is worth highlighting that all the "new code" introduced is bounded within labels in order to have a quick location within the source files, as shown below. We mention this because one could try to merge our code with his/hers, hence it could lead to problems.

```
////David/Ramón

NEW PROPRIETARY CODE

////End David/Ramón

```

## Working tests @ 04/07/2014 ##

We have tested the whole solution under the following g++ compilers, including the optimized compilation:
- g++ 4.4.7
- g++ 4.6.3 -> This last test has been recently assessed over a fresh Linux OS install.

Although there are newer compiler versions, we have experimented a number of issues with them, opting for using older ones. If the problem persist, try [this](http://stackoverflow.com/questions/7832892/how-to-change-the-default-gcc-compiler-in-ubuntu) to force a explicit version of the compilers

## Current status  @ 04/07/2014 ##

The current version of the project still lacks in various aspects, which will be tackled in forthcoming updates:
1. The *InterFlowNetworkCodingProtocol* entity is still under construction. Although we have already provided many valid results that have been included into various publications, we are currently merging it with the new solution, based on a single common abstract base class (*NetworkCodingL4Protocol*). For that purpose, there are still a number of tweaks that must be addressed before considering that this protocol is fully ready. For a further description of the protocol, the reader might refer to [6] and [7].
2. By the way, we are still trying to *squeeze* the performance of the *IntraFlowNetworkCodingProtocol*, optimizing its performance with brand new features. However, the main drawback of this part of the work is that we have not any publication accepted, thus we cannot link any reference :-( that help the reader to outline the broad range of possibilities brought about by this sort of Network Coding.
3. For the sake of an easier application, we are preparing a patch that 

In addition, we foresee to perform the following operations:
- Complete the documentation (i.e. individual README files that deepen into the details for each of the brand new modules).
- Port the code to a newer *ns-3* version (the latest one at the moment of writing this file is *ns-3.19*).

## References ##
[1] [XORs in the air. Practical Wireless Network Coding ](https://www.dropbox.com/s/rvt0irhoyl8x6e8/XORs%20in%20the%20air.%20Practical%20wireless%20network%20coding.pdf) (IEEE Transactions on Networking 2008)
[2] [MORE. A Network Coding Approach to Opportunistic Routing](https://www.dropbox.com/s/wlcl32x2tf84m8n/MORE.%20A%20Network%20Coding%20Approach%20to%20Opportunistic%20Routing.pdf) (MIT Technical Report)
[3] [Accurate Simulation of IEEE 802.11 Indoor Links: A "Bursty" Channel Based on Real Measurements](https://www.dropbox.com/s/gx5dyqsxl1sanid/Accurate%20Simulation%20of%20802.11%20Indoor%20Links.%20A%20Bursty%20Channel%20Based%20on%20Real%20Measurements.pdf) (EURASIP Journal on Wireless Communications and Networking 2010)
[4] [On the Modeling of a Realistic Wireless Channel by means of a Hidden Markov Process](https://www.dropbox.com/s/zulssvld21i1fqq/ChannelModel.pdf) (IEEE WiMob 2013)
[5] [Replication of the Bursty Behavior of Indoor WLAN Channels](https://www.dropbox.com/s/vdfpgxqydhqcmf7/WNS3%202013%20%28ISBN%29.pdf) (WNS3 2013)
[6] [Impact of Network Coding on TCP Performance in Wireless Mesh Networks](https://www.dropbox.com/s/4y7cjxm884k2y0o/PIMRC%20Proceedings.pdf) (IEEE PIMRC 2012)
[7] [TCP Acknowledgement Encapsulation in Coded Multi-hop Wireless Networks](https://www.dropbox.com/s/bvl4c57mo1hshg4/NetCod2013.pdf)(IEEE VTC 2014)

## Caveats ##

1. If anybody does want to use our tracing system, please ensure you have created a "traces/" folder at "/ns-3.13/". Otherwise, the trace file creation will be simply ignored.
2. You can find (unformatted), a different README files for each folder within the "scenario-creator" module. Please take a look before creating new scenarios (the creation is quite straightforward, though).
3. It is worth highlighting that this project IS NOT A PROFFESSIONAL NOR A COMMERCIAL contribution. This is an academic work. Therefore, there might be prone to be an "underoptimal" implementation.

## Contact ##

For any question, criticism, doubt or curiosity, do not hesitate and contact us; we would be glad to provide an answer *ASAP*.

* * *
David Gómez Fernández (<dgomez@tlmat.unican.es>)
Eduardo Rodríguez Maza (<eduardo.rodriguezm@alumnos.unican.es>)
Ramón Agüero Calvo (<ramon@tlmat.unican.es>)
**[Universidad de Cantabria](www.unican.es "Universidad de Cantabria")**
2014
* * *