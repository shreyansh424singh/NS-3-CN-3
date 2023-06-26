/*
                                                          (10.10.3.2)
                                                        Node4
                                                        / 
                                                       / 10.10.3.0/24
                                                      /                                                       
      Node1 -------------- Node2 -------------- Node3 
            10.10.1.0/24          10.10.2.0/24        \
                                                       \ 10.10.4.0/24
                                                        \
                                                        Node5
                                                          (10.10.4.2)

*/
 
#include <fstream>
#include <string>
#include <string>
#include <cassert>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/double.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/tcp-l4-protocol.h"
#include <bits/stdc++.h>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("jcomp");

//MyApp Application
class MyApp : public Application{

        public:
                MyApp ();
                virtual ~MyApp();
                void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

        private:
                virtual void StartApplication (void);
                virtual void StopApplication (void);
                void ScheduleTx (void);
                void SendPacket (void);

                Ptr<Socket>     m_socket;
                Address         m_peer;
                uint32_t        m_packetSize;
                uint32_t        m_nPackets;
                DataRate        m_dataRate;
                EventId         m_sendEvent;
                bool            m_running;
                uint32_t        m_packetsSent;
};

//Constructor and Destructor
MyApp::MyApp ()
        : m_socket (0),
        m_peer (),
        m_packetSize (0),
        m_nPackets (0),
        m_dataRate (0),
        m_sendEvent (),
        m_running (false),
        m_packetsSent (0)
{
}

MyApp::~MyApp(){
        m_socket = 0;
}

void MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate){
        m_socket = socket;
        m_peer = address;
        m_packetSize = packetSize;
        m_nPackets = nPackets;
        m_dataRate = dataRate;
}

//Initializing member variables
void MyApp::StartApplication (void){
        m_running = true;
        m_packetsSent = 0;
        m_socket->Bind ();
        m_socket->Connect (m_peer);
        SendPacket ();
}

//Stop creating simulation events
void MyApp::StopApplication (void){
        m_running = false;
        if (m_sendEvent.IsRunning ()){
                Simulator::Cancel (m_sendEvent);
        }
        if (m_socket){
                m_socket->Close ();
        }
}

//Recall that StartApplication called SendPacket to start the chain of events that describes the Application behavior
void MyApp::SendPacket (void){
        Ptr<Packet> packet = Create<Packet> (m_packetSize);
        m_socket->Send (packet);
        if (++m_packetsSent < m_nPackets){
                ScheduleTx ();
        }
}

//Call ScheduleTx to schedule another transmit event (a SendPacket) until the Application decides it has sent enough.
void MyApp::ScheduleTx (void){
        if (m_running){
                double time = Simulator::Now().GetSeconds();
                // change the data rate to 500Kbps for UDP socket after 30 seconds.
                if(time>=30.0 && m_socket->GetSocketType() == 2){
                        m_dataRate = DataRate("500Kbps");
                }
                Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate())));
                m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
        }
}

static void CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd){
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newCwnd-oldCwnd << " " << newCwnd << std::endl;
}

int main (int argc, char *argv[]){
    uint32_t packetSize=1040;
    uint32_t nPackets=100000;

    CommandLine cmd;
    cmd.Parse(argc,argv);
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",StringValue("ns3::TcpVegas"));

    NodeContainer nodes;
    nodes.Create (5);
    NodeContainer nodes12 = NodeContainer (nodes.Get (0), nodes.Get (1)); //h2r1
    NodeContainer nodes23 = NodeContainer (nodes.Get (1), nodes.Get (2)); //r1r2
    NodeContainer nodes34 = NodeContainer (nodes.Get (2), nodes.Get (3)); //r2h3
    NodeContainer nodes35 = NodeContainer (nodes.Get (2), nodes.Get (4)); //r2r3

    InternetStackHelper internet;
    internet.Install (nodes);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue("500Kbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
    NetDeviceContainer device12 = pointToPoint.Install (nodes12);
    NetDeviceContainer device23 = pointToPoint.Install (nodes23);
    NetDeviceContainer device34 = pointToPoint.Install (nodes34);
    NetDeviceContainer device35 = pointToPoint.Install (nodes35);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.10.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interface12 = ipv4.Assign (device12);
    ipv4.SetBase ("10.10.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interface23 = ipv4.Assign (device23);
    ipv4.SetBase ("10.10.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interface34 = ipv4.Assign (device34);
    ipv4.SetBase ("10.10.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interface35 = ipv4.Assign (device35);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    //Set up TCP server connection
    uint16_t port = 8000;
    ApplicationContainer sinkApp;
    Address sinkLocalAddress(InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);

    sinkApp.Add(sinkHelper.Install(nodes34.Get(1)));
    sinkApp.Start (Seconds (0.5));
    sinkApp.Stop (Seconds (100.5));

    ApplicationContainer sinkApp1;
    Address sinkLocalAddress1(InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper1 ("ns3::UdpSocketFactory", sinkLocalAddress1);

    sinkApp1.Add(sinkHelper1.Install(nodes35.Get(1)));
    sinkApp1.Start (Seconds (19.5));
    sinkApp1.Stop (Seconds (100.5));

    //Create the socket and connect the Trace source
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
    Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (nodes.Get (1), UdpSocketFactory::GetTypeId ());

    //Create object of MyApp
    Ptr<MyApp> clientApp1 = CreateObject<MyApp>();
    Ptr<MyApp> clientApp2 = CreateObject<MyApp>();

    //Make connections
    Ipv4Address serverAddress1 = interface34.GetAddress (1);
    Address remoteAddress1 (InetSocketAddress (serverAddress1, port));

    Ipv4Address serverAddress2 = interface35.GetAddress (1);
    Address remoteAddress2 (InetSocketAddress (serverAddress2, port));

    //Bind socket with application and connect to server
    clientApp1->Setup(ns3TcpSocket, remoteAddress1, packetSize, nPackets, DataRate("250Kbps"));
    clientApp2->Setup(ns3UdpSocket, remoteAddress2, packetSize, nPackets, DataRate("250Kbps"));

    //Install application to Client
    nodes.Get (0)->AddApplication(clientApp1);
    nodes.Get (1)->AddApplication(clientApp2);

    //Set start and stop time
    clientApp1->SetStartTime(Seconds(1.0));
    clientApp1->SetStopTime(Seconds(100.0));
    clientApp2->SetStartTime(Seconds(20.0));
    clientApp2->SetStopTime(Seconds(100.0));

    // pcap enable
    pointToPoint.EnablePcapAll ("task3");

    string wnd_file = "Task.cwnd";
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream (wnd_file);
    ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream));

    Simulator::Stop (Seconds(100));
    Simulator::Run ();
    Simulator::Destroy ();
}