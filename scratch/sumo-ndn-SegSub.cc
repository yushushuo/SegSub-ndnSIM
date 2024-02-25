/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/
#include <iostream>
#include <fstream>
#include <sstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ns2-mobility-helper.h"
#include "ns3/netanim-module.h"

#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/ndnSIM-module.h"

using namespace std;
namespace ns3 {

static void
CourseChange (std::ostream *os, std::string foo, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition (); // Get position
  Vector vel = mobility->GetVelocity (); // Get velocity

  // Prints position and velocities
  *os << Simulator::Now () << " POS: x=" << pos.x << ", y=" << pos.y
      << ", z=" << pos.z << "; VEL:" << vel.x << ", y=" << vel.y
      << ", z=" << vel.z << std::endl;
}

void updateMobilityInfo(ndn::AppHelper& CurrentConsumerHelper, NodeContainer nodes, int k)
{
    Ptr<ConstantVelocityMobilityModel> mob = nodes.Get(k)->GetObject<ConstantVelocityMobilityModel>();
    int pos_x = mob->GetPosition ().x;
    int pos_y = mob->GetPosition ().y;
    int vel_x = mob->GetVelocity ().x;
    int vel_y = mob->GetVelocity ().y;
    //std::cout<<pos_x<<std::endl;
    CurrentConsumerHelper.SetAttribute("LocationX", StringValue(std::to_string(pos_x)));
    CurrentConsumerHelper.SetAttribute("LocationY", StringValue(std::to_string(pos_y)));
    CurrentConsumerHelper.SetAttribute("SpeedX", StringValue(std::to_string(vel_x)));
    CurrentConsumerHelper.SetAttribute("SpeedY", StringValue(std::to_string(vel_y)));
}

NS_LOG_COMPONENT_DEFINE("ndn.WifiExample");

//
// DISCLAIMER:  Note that this is an extremely simple example, containing just 2 wifi nodes
// communicating directly over AdHoc channel.
//

// Ptr<ndn::NetDeviceFace>
// MyNetDeviceFaceCallback (Ptr<Node> node, Ptr<ndn::L3Protocol> ndn, Ptr<NetDevice> device)
// {
//   // NS_LOG_DEBUG ("Create custom network device " << node->GetId ());
//   Ptr<ndn::NetDeviceFace> face = CreateObject<ndn::MyNetDeviceFace> (node, device);
//   ndn->AddFace (face);
//   return face;
// }

int
main(int argc, char* argv[])
{
  std::string traceFile;
  std::string logFile;

  int    nodeNum;
  double duration;

  int subscriber_list[30]={0};

  // Config begin
  string scheme = "SegSub";
  int subscriber_num = 3;
  double app1_start_time = 30; 
  double app1_end_time = 80;
  bool is_my_scheme = true; //my/pubsubMob = true
  bool is_native = false; 
  bool is_Mob = false; //pubsubMob = true, 应该没用了
  string fixed_timer = "3.0";
  int producer_id = 22; // 15,3,7,22,1
  int des_ES = 1; // auto 不用手写
  // Config end

  //string trace_file_path="result0930/trace-"+scheme+"-rsu"+to_string(producer_id)+"-v"+to_string(subscriber_num)+".txt";
  string trace_file_path="trace0816.txt";

  app1_start_time = 8; 
  app1_end_time = 80;
  for(int i=0;i<subscriber_num;i++){
    subscriber_list[i]=i+1;
  }

  if(subscriber_num==1){
    app1_start_time = 8; 
    app1_end_time = 80;
    subscriber_list[0]=1;//1个订阅者时：用node 1
  }
  else if(subscriber_num==3){
    app1_start_time = 8; 
    app1_end_time = 80;
    subscriber_list[0]=1;//3个订阅者时：用node 1
    subscriber_list[1]=2;//3个订阅者时：用node 1
    subscriber_list[2]=3;//3个订阅者时：用node 1
  }
  else if(subscriber_num==10){
    app1_start_time = 10; 
    app1_end_time = 80;
    //subscriber_list[2]=13;
  }
  else if(subscriber_num==20){
    app1_start_time = 20; 
    app1_end_time = 90;
    //subscriber_list[15]=22;
  }
  else if(subscriber_num==26){
    app1_start_time = 26; 
    app1_end_time = 98;
  }

  if(producer_id == 15){
    des_ES = 1;
  }
  else if(producer_id==3){
    des_ES = 2;
  }
  else if(producer_id==7){
    des_ES = 2;
  }
  else if(producer_id==22){
    des_ES = 3;
  }
  else if(producer_id==1){
    des_ES = 0;
  }

  // disable fragmentation
  //Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  //Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
  //Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
  //                   StringValue("OfdmRate24Mbps"));
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("100ms"));
  Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue("10p"));
  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate24Mbps"));

  CommandLine cmd;
  cmd.AddValue ("traceFile", "Ns2 movement trace file", traceFile);
  cmd.AddValue ("nodeNum", "Number of nodes", nodeNum);
  cmd.AddValue ("duration", "Duration of Simulation", duration);
  cmd.AddValue ("logFile", "Log file", logFile);
  cmd.Parse(argc, argv);

  if (traceFile.empty () || nodeNum <= 0 || duration <= 0 || logFile.empty ())
    {
      std::cout << "Usage of " << argv[0] << " :\n\n"
      "./waf --run \"ns2-mobility-trace"
      " --traceFile=src/mobility/examples/default.ns_movements"
      " --nodeNum=2 --duration=100.0 --logFile=ns2-mob.log\" \n\n"
      "NOTE: ns2-traces-file could be an absolute or relative path. You could use the file default.ns_movements\n"
      "      included in the same directory of this example file.\n\n"
      "NOTE 2: Number of nodes present in the trace file must match with the command line argument and must\n"
      "        be a positive number. Note that you must know it before to be able to load it.\n\n"
      "NOTE 3: Duration must be a positive number. Note that you must know it before to be able to load it.\n\n";

      return 0;
    }
  
  


  //////////////////////

  WifiHelper wifi;
  // wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
                               StringValue("OfdmRate24Mbps"));

  YansWifiChannelHelper wifiChannel; // = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  //wifiChannel.AddPropagationLoss("ns3::ThreeLogDistancePropagationLossModel");
  //wifiChannel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(250));  

  // YansWifiPhy wifiPhy = YansWifiPhy::Default();
  YansWifiPhyHelper wifiPhyHelper;
  wifiPhyHelper.SetChannel(wifiChannel.Create());
  wifiPhyHelper.Set("TxPowerStart", DoubleValue(5));
  wifiPhyHelper.Set("TxPowerEnd", DoubleValue(5));

  WifiMacHelper wifiMacHelper;
  wifiMacHelper.SetType("ns3::AdhocWifiMac");

  Ns2MobilityHelper mobility = Ns2MobilityHelper (traceFile);
  ofstream os;
  os.open (logFile.c_str ());

  // Create all nodes.
  NodeContainer stas;
  stas.Create (nodeNum);

  

  mobility.Install (); // configure movements for each node, while reading trace file

  // Configure callback for logging
  Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange",
                   MakeBoundCallback (&CourseChange, &os));
  
  int x_add = 0;
  int y_add = 0;
  int len_road = 500;

  //RSU的设置
  NodeContainer RSU_nodes;
  RSU_nodes.Create (25);
  MobilityHelper mobility_RSU;
  Ptr<ListPositionAllocator> positionAlloc_RSU = CreateObject<ListPositionAllocator> ();
  for(int i=0;i<5;i++){
    for(int j=0;j<5;j++){
      positionAlloc_RSU->Add (Vector (x_add+i*len_road, y_add+j*len_road, 0.0));
    }
  }
  mobility_RSU.SetPositionAllocator (positionAlloc_RSU);
  mobility_RSU.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility_RSU.Install(RSU_nodes);

  //ES的设置
  NodeContainer ES_nodes;
  ES_nodes.Create (6);
  MobilityHelper mobility_ES;
  Ptr<ListPositionAllocator> positionAlloc_ES = CreateObject<ListPositionAllocator> ();
  positionAlloc_ES->Add (Vector (x_add+20+len_road, y_add+len_road/2, 0.0));
  positionAlloc_ES->Add (Vector (x_add+20+3*len_road, y_add+len_road/2, 0.0));
  positionAlloc_ES->Add (Vector (x_add+20+len_road, y_add+len_road/2 + len_road*2, 0.0));
  positionAlloc_ES->Add (Vector (x_add+20+3*len_road, y_add+len_road/2 + len_road*2, 0.0));
  positionAlloc_ES->Add (Vector (x_add+20+len_road, y_add+len_road/2 + len_road*4, 0.0));
  positionAlloc_ES->Add (Vector (x_add+20+3*len_road, y_add+len_road/2 + len_road*4, 0.0));
  mobility_ES.SetPositionAllocator (positionAlloc_ES);
  mobility_ES.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility_ES.Install(ES_nodes);

  // p2p link
  PointToPointHelper p2p;
  p2p.Install(RSU_nodes.Get(0), ES_nodes.Get(0));
  p2p.Install(RSU_nodes.Get(1), ES_nodes.Get(0));
  p2p.Install(RSU_nodes.Get(5), ES_nodes.Get(0));
  p2p.Install(RSU_nodes.Get(6), ES_nodes.Get(0));
  p2p.Install(RSU_nodes.Get(10), ES_nodes.Get(0));
  p2p.Install(RSU_nodes.Get(11), ES_nodes.Get(0));
  p2p.Install(RSU_nodes.Get(15), ES_nodes.Get(1));
  p2p.Install(RSU_nodes.Get(16), ES_nodes.Get(1));
  p2p.Install(RSU_nodes.Get(20), ES_nodes.Get(1));
  p2p.Install(RSU_nodes.Get(21), ES_nodes.Get(1));
  p2p.Install(RSU_nodes.Get(2), ES_nodes.Get(2));
  p2p.Install(RSU_nodes.Get(3), ES_nodes.Get(2));
  p2p.Install(RSU_nodes.Get(7), ES_nodes.Get(2));
  p2p.Install(RSU_nodes.Get(8), ES_nodes.Get(2));
  p2p.Install(RSU_nodes.Get(12), ES_nodes.Get(2));
  p2p.Install(RSU_nodes.Get(13), ES_nodes.Get(2));
  p2p.Install(RSU_nodes.Get(17), ES_nodes.Get(3));
  p2p.Install(RSU_nodes.Get(18), ES_nodes.Get(3));
  p2p.Install(RSU_nodes.Get(22), ES_nodes.Get(3));
  p2p.Install(RSU_nodes.Get(23), ES_nodes.Get(3));
  p2p.Install(RSU_nodes.Get(4), ES_nodes.Get(4));
  p2p.Install(RSU_nodes.Get(9), ES_nodes.Get(4));
  p2p.Install(RSU_nodes.Get(14), ES_nodes.Get(4));
  p2p.Install(RSU_nodes.Get(19), ES_nodes.Get(5));
  p2p.Install(RSU_nodes.Get(24), ES_nodes.Get(5));
  p2p.Install(ES_nodes.Get(0), ES_nodes.Get(1));
  p2p.Install(ES_nodes.Get(1), ES_nodes.Get(0));
  p2p.Install(ES_nodes.Get(0), ES_nodes.Get(2));
  p2p.Install(ES_nodes.Get(2), ES_nodes.Get(0));
  p2p.Install(ES_nodes.Get(1), ES_nodes.Get(3));
  p2p.Install(ES_nodes.Get(3), ES_nodes.Get(1));
  p2p.Install(ES_nodes.Get(2), ES_nodes.Get(4));
  p2p.Install(ES_nodes.Get(4), ES_nodes.Get(2));
  p2p.Install(ES_nodes.Get(3), ES_nodes.Get(5));
  p2p.Install(ES_nodes.Get(5), ES_nodes.Get(3));
  p2p.Install(ES_nodes.Get(3), ES_nodes.Get(2));
  p2p.Install(ES_nodes.Get(2), ES_nodes.Get(3));
  p2p.Install(ES_nodes.Get(4), ES_nodes.Get(5));

  NetDeviceContainer wifiNetDevices = wifi.Install(wifiPhyHelper, wifiMacHelper, stas);
  NetDeviceContainer wifiNetDevices2 = wifi.Install(wifiPhyHelper, wifiMacHelper, RSU_nodes);

  //NetDeviceContainer wifiNetDevices2 = wifi.Install(wifiPhyHelper, wifiMacHelper, producer1);
  //NetDeviceContainer wifiNetDevices3 = wifi.Install(wifiPhyHelper, wifiMacHelper, producer2);

  //Fixed nodes------------------------------

  // NodeContainer TL;
  // TL.Create(1);
  // Ptr<Node> n = TL.Get(0);
  // NetDeviceContainer wifiTL = wifi.Install(wifiPhyHelper, wifiMacHelper, TL);
  // ns3::AnimationInterface::SetConstantPosition (n, 300, 100, 0);
  // 3. Install NDN stack
  NS_LOG_INFO("Installing NDN stack");
  ndn::StackHelper ndnHelper;
  // ndnHelper.AddNetDeviceFaceCreateCallback (WifiNetDevice::GetTypeId (), MakeCallback
  // (MyNetDeviceFaceCallback));
  //ndnHelper.setPolicy("nfd::cs::lru");
 // ndnHelper.SetDefaultRoutes(true);
  //ndnHelper.Install(stas);
  //ndnHelper.Install(TL);
  //ndnHelper.InstallAll();

  ndn::StackHelper ndnHelper1;
  ndnHelper1.SetDefaultRoutes(true);
  ndnHelper1.Install(stas);
  ndn::StackHelper ndnHelper2;
  ndnHelper1.Install(RSU_nodes);
  ndnHelper2.Install(ES_nodes);

  // Set BestRoute strategy
  //ndn::StrategyChoiceHelper::Install(stas, "/", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/best-route");
  if(!is_native){
    ndn::StrategyChoiceHelper::Install(RSU_nodes,"/", "/localhost/nfd/strategy/rsu-best-route");
    ndn::StrategyChoiceHelper::Install(ES_nodes,"/", "/localhost/nfd/strategy/es-local-best-route");
    //ndn::StrategyChoiceHelper::Install(ES_nodes.Get(1),"/street10", "/localhost/nfd/strategy/es-destinated-best-route");
    ndn::StrategyChoiceHelper::Install(ES_nodes.Get(des_ES),"/street10", "/localhost/nfd/strategy/es-destinated-best-route");
    ndn::StrategyChoiceHelper::Install(ES_nodes.Get(2),"/street07", "/localhost/nfd/strategy/es-destinated-best-route");
  }
  

  // Installing global routing interface on all nodes
  //ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  //ndnGlobalRoutingHelper.InstallAll();

  //Ptr<Node> producer = RSU_nodes.Get(15);
  Ptr<Node> producer = RSU_nodes.Get(producer_id);
  Ptr<Node> producer2 = RSU_nodes.Get(3);
  std::string prefix_consumer = "/street10/accident";
  std::string prefix_producer = "/street10/accident";
  std::string postfix = "/seq=1/accident03";
  //std::string postfix2 = "/accident03";
  std::string prefix_consumer2 = "/street07/weather";
  std::string prefix_producer2 = "/street07/weather";
  std::string postfix2 = "/seq=1/weather06";

  // 4. Set up applications
  NS_LOG_INFO("Installing Applications");

  //app1:accident
  if(is_Mob){
    app1_start_time = 8; 
    app1_end_time = 30;
  }

  ndn::AppHelper consumerHelper("ns3::ndn::ConsumerSubscribe");
  consumerHelper.SetPrefix(prefix_consumer); //使用此名称生成兴趣或满足此名称的兴趣
  consumerHelper.SetAttribute("VT1", StringValue("3.0")); // my
  consumerHelper.SetAttribute("LifeTime", StringValue("60.0")); // my
  if(!is_my_scheme){
    consumerHelper.SetAttribute("VT1", StringValue(fixed_timer)); // fixed timer / native
    consumerHelper.SetAttribute("LifeTime", StringValue(fixed_timer)); // fixed timer / native
  }
 
  // for(int i=0;i<nodeNum;i++){
  //   consumerHelper.SetAttribute("WaitTime", StringValue(std::to_string(i%20)));
  //   updateMobilityInfo(consumerHelper,stas,i);
  //   ns3::ApplicationContainer appContainer = consumerHelper.Install(stas.Get(i));
  //   appContainer.Start(Seconds(app1_start_time));
  //   appContainer.Stop(Seconds(app1_end_time));
  // }
  for(int i=0;i<subscriber_num;i++){
    consumerHelper.SetAttribute("WaitTime", StringValue(std::to_string(i%20)));
    int subscriber_id = subscriber_list[i];
    updateMobilityInfo(consumerHelper,stas,subscriber_id);
    ns3::ApplicationContainer appContainer = consumerHelper.Install(stas.Get(subscriber_id));
    appContainer.Start(Seconds(app1_start_time));
    appContainer.Stop(Seconds(app1_end_time));
  }

  //app-mob:accident
  ndn::AppHelper consumerHelperMob("ns3::ndn::ConsumerSubscribe");
  consumerHelperMob.SetPrefix(prefix_consumer); //使用此名称生成兴趣或满足此名称的兴趣
  consumerHelperMob.SetAttribute("VT1", StringValue("3.0")); // my
  consumerHelperMob.SetAttribute("LifeTime", StringValue("60.0")); // my
  consumerHelperMob.SetAttribute("StartTimeSub", StringValue(std::to_string(app1_start_time))); // my
  if(!is_my_scheme){
    consumerHelperMob.SetAttribute("VT1", StringValue(fixed_timer)); // fixed timer / native
    consumerHelperMob.SetAttribute("LifeTime", StringValue(fixed_timer)); // fixed timer / native
  }
  if(is_Mob){
    double appMob_start_time = 40; 
    double appMob_end_time = 80;
    for(int i=0;i<subscriber_num;i++){
      consumerHelperMob.SetAttribute("WaitTime", StringValue(std::to_string(i%20)));
      int subscriber_id = subscriber_list[i];
      updateMobilityInfo(consumerHelperMob,stas,subscriber_id);
      ns3::ApplicationContainer appContainerMob = consumerHelperMob.Install(stas.Get(subscriber_id));
      appContainerMob.Start(Seconds(appMob_start_time));
      appContainerMob.Stop(Seconds(appMob_end_time));
    }
  }
  

  //app2:weather
  ndn::AppHelper consumerHelper2("ns3::ndn::ConsumerSubscribe");
  consumerHelper2.SetPrefix(prefix_consumer2); //使用此名称生成兴趣或满足此名称的兴趣
  consumerHelper2.SetAttribute("VT1", StringValue("5.0")); // my
  consumerHelper2.SetAttribute("LifeTime", StringValue("90.0")); // my
  if(!is_my_scheme){
    consumerHelper2.SetAttribute("VT1", StringValue(fixed_timer)); // fixed timer / native
    consumerHelper2.SetAttribute("LifeTime", StringValue(fixed_timer)); // fixed timer / native
  }
  
  for(int i=0;i<nodeNum;i++){
    //consumerHelper.SetAttribute("WaitTime", StringValue(std::to_string(i%20)));
    //updateMobilityInfo(consumerHelper,stas,i);
    //consumerHelper.Install(stas.Get(i)).Start(Seconds(29));
  }

  //ndn::AppHelper producerHelper("ns3::ndn::Producer");
  ndn::AppHelper producerHelper("ns3::ndn::Publisher");
  producerHelper.SetPrefix(prefix_producer);
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.SetAttribute("Postfix", StringValue(postfix));
  producerHelper.SetAttribute("StartTimePub", StringValue(std::to_string(app1_start_time)));
  //producerHelper.SetAttribute("Postfix2", StringValue(postfix2));
  ndn::AppHelper producerHelper_native("ns3::ndn::NativePub");
  producerHelper_native.SetPrefix(prefix_producer);
  producerHelper_native.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper_native.SetAttribute("Postfix", StringValue(postfix));
  producerHelper_native.SetAttribute("StartTimeNative", StringValue(std::to_string(app1_start_time)));

  if(is_native){
    producerHelper_native.Install(producer);
  }
  else{
    producerHelper.Install(producer);
  }


  // Calculate and install FIBs
  //ndn::GlobalRoutingHelper::CalculateRoutes();
  ndn::FibHelper::AddRoute(RSU_nodes.Get(15), "/street10", ES_nodes.Get(1), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(16), "/street10", ES_nodes.Get(1), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(20), "/street10", ES_nodes.Get(1), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(21), "/street10", ES_nodes.Get(1), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(0), "/street10", ES_nodes.Get(0), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(1), "/street10", ES_nodes.Get(0), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(5), "/street10", ES_nodes.Get(0), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(6), "/street10", ES_nodes.Get(0), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(10), "/street10", ES_nodes.Get(0), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(11), "/street10", ES_nodes.Get(0), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(2), "/street10", ES_nodes.Get(2), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(3), "/street10", ES_nodes.Get(2), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(7), "/street10", ES_nodes.Get(2), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(8), "/street10", ES_nodes.Get(2), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(12), "/street10", ES_nodes.Get(2), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(13), "/street10", ES_nodes.Get(2), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(17), "/street10", ES_nodes.Get(3), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(18), "/street10", ES_nodes.Get(3), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(22), "/street10", ES_nodes.Get(3), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(23), "/street10", ES_nodes.Get(3), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(4), "/street10", ES_nodes.Get(4), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(9), "/street10", ES_nodes.Get(4), 1);  
  ndn::FibHelper::AddRoute(RSU_nodes.Get(14), "/street10", ES_nodes.Get(4), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(19), "/street10", ES_nodes.Get(5), 1);  
  ndn::FibHelper::AddRoute(RSU_nodes.Get(24), "/street10", ES_nodes.Get(5), 1); 

  if(producer_id == 15){
    ndn::FibHelper::AddRoute(ES_nodes.Get(1), "/street10", RSU_nodes.Get(15), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(0), "/street10", ES_nodes.Get(1), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(2), "/street10", ES_nodes.Get(0), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(3), "/street10", ES_nodes.Get(1), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(4), "/street10", ES_nodes.Get(2), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(5), "/street10", ES_nodes.Get(3), 1);
  }
  else if(producer_id == 3){
    ndn::FibHelper::AddRoute(ES_nodes.Get(2), "/street10", RSU_nodes.Get(3), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(0), "/street10", ES_nodes.Get(2), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(1), "/street10", ES_nodes.Get(0), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(3), "/street10", ES_nodes.Get(2), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(4), "/street10", ES_nodes.Get(2), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(5), "/street10", ES_nodes.Get(3), 1);
  }
  else if(producer_id == 7){
    ndn::FibHelper::AddRoute(ES_nodes.Get(2), "/street10", RSU_nodes.Get(7), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(0), "/street10", ES_nodes.Get(2), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(1), "/street10", ES_nodes.Get(0), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(3), "/street10", ES_nodes.Get(2), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(4), "/street10", ES_nodes.Get(2), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(5), "/street10", ES_nodes.Get(3), 1);
  }
  else if(producer_id == 22){
    ndn::FibHelper::AddRoute(ES_nodes.Get(3), "/street10", RSU_nodes.Get(22), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(0), "/street10", ES_nodes.Get(2), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(1), "/street10", ES_nodes.Get(3), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(2), "/street10", ES_nodes.Get(3), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(4), "/street10", ES_nodes.Get(2), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(5), "/street10", ES_nodes.Get(3), 1);
  }
  else if(producer_id == 1){
    ndn::FibHelper::AddRoute(ES_nodes.Get(0), "/street10", RSU_nodes.Get(1), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(2), "/street10", ES_nodes.Get(0), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(1), "/street10", ES_nodes.Get(0), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(3), "/street10", ES_nodes.Get(2), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(4), "/street10", ES_nodes.Get(2), 1);
    ndn::FibHelper::AddRoute(ES_nodes.Get(5), "/street10", ES_nodes.Get(3), 1);
  }
  
  ndn::FibHelper::AddRoute(RSU_nodes.Get(15), "/street07", ES_nodes.Get(1), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(16), "/street07", ES_nodes.Get(1), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(20), "/street07", ES_nodes.Get(1), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(21), "/street07", ES_nodes.Get(1), 1); 
  ndn::FibHelper::AddRoute(ES_nodes.Get(1), "/street07", ES_nodes.Get(0), 1);
  ndn::FibHelper::AddRoute(ES_nodes.Get(2), "/street07", RSU_nodes.Get(3), 1);
  ndn::FibHelper::AddRoute(RSU_nodes.Get(0), "/street07", ES_nodes.Get(0), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(1), "/street07", ES_nodes.Get(0), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(5), "/street07", ES_nodes.Get(0), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(6), "/street07", ES_nodes.Get(0), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(10), "/street07", ES_nodes.Get(0), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(11), "/street07", ES_nodes.Get(0), 1); 
  ndn::FibHelper::AddRoute(ES_nodes.Get(0), "/street07", ES_nodes.Get(2), 1);
  ndn::FibHelper::AddRoute(RSU_nodes.Get(2), "/street07", ES_nodes.Get(2), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(3), "/street07", ES_nodes.Get(2), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(7), "/street07", ES_nodes.Get(2), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(8), "/street07", ES_nodes.Get(2), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(12), "/street07", ES_nodes.Get(2), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(13), "/street07", ES_nodes.Get(2), 1); 
  //ndn::FibHelper::AddRoute(ES_nodes.Get(2), "/street07", ES_nodes.Get(0), 1);
  ndn::FibHelper::AddRoute(RSU_nodes.Get(17), "/street07", ES_nodes.Get(3), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(18), "/street07", ES_nodes.Get(3), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(22), "/street07", ES_nodes.Get(3), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(23), "/street07", ES_nodes.Get(3), 1); 
  ndn::FibHelper::AddRoute(ES_nodes.Get(3), "/street07", ES_nodes.Get(2), 1);
  ndn::FibHelper::AddRoute(RSU_nodes.Get(4), "/street07", ES_nodes.Get(4), 1); 
  ndn::FibHelper::AddRoute(RSU_nodes.Get(9), "/street07", ES_nodes.Get(4), 1);  
  ndn::FibHelper::AddRoute(RSU_nodes.Get(14), "/street07", ES_nodes.Get(4), 1); 
  ndn::FibHelper::AddRoute(ES_nodes.Get(4), "/street07", ES_nodes.Get(2), 1);
  ndn::FibHelper::AddRoute(RSU_nodes.Get(19), "/street07", ES_nodes.Get(5), 1);  
  ndn::FibHelper::AddRoute(RSU_nodes.Get(24), "/street07", ES_nodes.Get(5), 1); 
  ndn::FibHelper::AddRoute(ES_nodes.Get(5), "/street07", ES_nodes.Get(4), 1);
  ////////////////

  AnimationInterface anim ("animation.xml");
  Simulator::Stop(Seconds(duration));
  ndn::L3RateTracer::InstallAll(trace_file_path, Seconds(5.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
