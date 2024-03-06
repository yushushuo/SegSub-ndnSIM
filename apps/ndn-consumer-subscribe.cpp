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

#include "ndn-consumer-subscribe.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"
#include <time.h>
#include <stdlib.h>

#include "ns3/mobility-module.h"

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerSubscribe");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ConsumerSubscribe);

TypeId
ConsumerSubscribe::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ConsumerSubscribe")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<ConsumerSubscribe>()
        .AddAttribute("VT1", "validity timer1", StringValue("5.0"),
                    MakeDoubleAccessor(&ConsumerSubscribe::m_VT1), MakeDoubleChecker<double>())
        .AddAttribute("WaitTime", "waiting time", StringValue("0.0"),
                    MakeDoubleAccessor(&ConsumerSubscribe::m_WaitTime), MakeDoubleChecker<double>())
        .AddAttribute("LocationX", "LocationX",
                    StringValue("1000"),
                    MakeDoubleAccessor(&ConsumerSubscribe::m_locationX), MakeDoubleChecker<double>())
        .AddAttribute("LocationY", "LocationY",
                    StringValue("1000"),
                    MakeDoubleAccessor(&ConsumerSubscribe::m_locationY), MakeDoubleChecker<double>())
        .AddAttribute("SpeedX", "SpeedX",
                    StringValue("10"),
                    MakeDoubleAccessor(&ConsumerSubscribe::m_speedX), MakeDoubleChecker<double>())
        .AddAttribute("SpeedY", "SpeedY",
                    StringValue("0.0"),
                    MakeDoubleAccessor(&ConsumerSubscribe::m_speedY), MakeDoubleChecker<double>())
        .AddAttribute("StartTimeSub", "StartTimeSub",
                    StringValue("8.0"),
                    MakeDoubleAccessor(&ConsumerSubscribe::m_startTime), MakeDoubleChecker<double>())
//      .AddAttribute("Frequency", "Frequency of interest packets", StringValue("1.0"),
//                    MakeDoubleAccessor(&ConsumerCbr::m_frequency), MakeDoubleChecker<double>())
//
//      .AddAttribute("Randomize",
//                    "Type of send time randomization: none (default), uniform, exponential",
//                    StringValue("none"),
//                    MakeStringAccessor(&ConsumerCbr::SetRandomize, &ConsumerCbr::GetRandomize),
//                    MakeStringChecker())
//
//      .AddAttribute("MaxSeq", "Maximum sequence number to request",
//                    IntegerValue(std::numeric_limits<uint32_t>::max()),
//                    MakeIntegerAccessor(&ConsumerCbr::m_seqMax), MakeIntegerChecker<uint32_t>())

    ;

  return tid;
}

ConsumerSubscribe::ConsumerSubscribe()
  : m_frequency(1.0)
  , m_firstTime(true)
  , m_has_ack(false)
  , m_last_seq(0)
{
  NS_LOG_FUNCTION_NOARGS();
  m_seqMax = std::numeric_limits<uint32_t>::max();
}

ConsumerSubscribe::~ConsumerSubscribe()
{
}

void
ConsumerSubscribe::ScheduleNextPacket()
{
  // double mean = 8.0 * m_payloadSize / m_desiredRate.GetBitRate ();
  //std::cout << "next: " << Simulator::Now().ToDouble(Time::S) << "s\n";

  //现在应该是每隔一秒发一次
  //Simulator::Schedule(Seconds(1.0), &Consumer::SendPacket, this);
  //srand((int)std::time(nullptr));
  //double waiting_time =(double)(rand()%10)/100;
  //std::cout<<waiting_time<<std::endl;
  Simulator::Schedule(Seconds(m_VT1+m_WaitTime/10), &ConsumerSubscribe::SendSubInterest, this);
  //Simulator::Schedule(Seconds(m_VT1+m_WaitTime/10+0.2), &ConsumerSubscribe::ReSendSubInterest, this);
}

void
ConsumerSubscribe::StartApplication() 
{
  NS_LOG_FUNCTION_NOARGS();

  // do base stuff
  
  App::StartApplication();
  m_seq = 1;
  //ScheduleNextPacket();//origin
  Simulator::Schedule(Seconds(m_WaitTime/10), &ConsumerSubscribe::SendSubInterest, this);//fixed
  
  //Simulator::Schedule(Seconds(18.2), &ConsumerSubscribe::SendSubInterest, this); //migration
  UpdateMobility();
  
}

void
ConsumerSubscribe::UpdateMobility()
{
  Ptr<ConstantVelocityMobilityModel> mob = GetNode()->GetObject<ConstantVelocityMobilityModel>();
  int pos_x = mob->GetPosition ().x;
  int pos_y = mob->GetPosition ().y;
  int vel_x = mob->GetVelocity ().x;
  int vel_y = mob->GetVelocity ().y;
  //std::cout<<"update mobility info:"<<pos_x<<","<<pos_y<<std::endl;
  m_locationX = pos_x;
  m_locationY = pos_y;
  m_speedX = vel_x;
  m_speedY = vel_y;
  Simulator::Schedule(Seconds(1.0), &ConsumerSubscribe::UpdateMobility, this);

  // 0513
  double current_time = Simulator::Now().ToDouble(Time::S);
  if(current_time>m_startTime+75){
    if(m_last_seq!=12){
      for(int j=m_last_seq+1;j<=12;j++){
        m_resend_seq = j;
        ConsumerSubscribe::ResendSingleInterest();
      }
    }
  }
}

void
ConsumerSubscribe::OnData(shared_ptr<const Data> data)
{
  Name dataName(data->getName());
  NS_LOG_INFO("node(" << GetNode()->GetId() << ") receive Data: " << data->getName());
  name::Component ack("ack");
  name::Component nack("nack");

  if(ack.equals(dataName.at(-2)) && data->getOBU()!=GetNode()->GetId()){
    return;
  }

  
  int current_seq = dataName.at(-1).toSequenceNumber();
  if(current_seq<m_last_seq+1){
    //return; 
  }
  
  Consumer::OnData(data);

  //m_seq ++; //native

  if(ack.equals(dataName.at(-2)) && data->getOBU()==GetNode()->GetId()){
    std::cout<<"Subscriber receives an ACK"<<std::endl;
    double new_timer = data->getACKtimer();
    //std::cout<<new_timer<<std::endl;
    m_VT1 = new_timer;
    m_has_ack = true;
    //std::cout<<m_VT1<<std::endl;
    ScheduleNextPacket();
  }
  //else if(nack.equals(dataName.at(-2))){
  else if(nack.equals(dataName.at(-2)) && data->getOBU()==GetNode()->GetId()){
    std::cout<<"Subscriber receives a NACK"<<std::endl;
    double new_timer = data->getACKtimer();
    //std::cout<<new_timer<<std::endl;
    m_VT1 = new_timer;
    //std::cout<<m_VT1<<std::endl;
    //ScheduleNextPacket();
    std::cout << "Subscriber"<<GetNode()->GetId()<< " Response NACK at " << Simulator::Now().ToDouble(Time::S) << "s\n";
    SendSubInterest();
  }
  else {
    
    if(current_seq == m_last_seq+1){
      m_last_seq ++;
    }
    else if(current_seq>m_last_seq+1){
      for(int j=m_last_seq+1;j<current_seq;j++){
        m_resend_seq = j;
        ConsumerSubscribe::ResendSingleInterest();
        std::cout << "Subscriber"<<GetNode()->GetId()<< " Receive ReData "<< j <<" at "<< Simulator::Now().ToDouble(Time::S) << "s\n";
      }
      m_last_seq = current_seq;
    }
  }
  
}

void
ConsumerSubscribe::ResendSingleInterest()
{
    uint32_t seq = std::numeric_limits<uint32_t>::max(); // invalid
    shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
    //seq = m_seq;  //native
    seq = 1; // 
    nameWithSequence->appendSequenceNumber(seq);
    Name n1("accident03");
    nameWithSequence->append(n1);
    nameWithSequence->appendSequenceNumber(m_resend_seq);
    //

    // shared_ptr<Interest> interest = make_shared<Interest> ();
    shared_ptr<Interest> interest = make_shared<Interest>();
    interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
    interest->setName(*nameWithSequence);
    interest->setCanBePrefix(true);
    interest->setMustBeFresh(true);//tryingggggggggggggggg
    time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    interest->setInterestLifetime(interestLifeTime);
    interest->setInterestType(1);
    interest->setLastHopType(0);
    interest->setOBU(GetNode()->GetId());

    interest->setLocationX(m_locationX);
    interest->setLocationY(m_locationY);
    interest->setSpeedX(m_speedX);
    interest->setSpeedY(m_speedY);
    //std::cout<<"m_locationX:";
    //std::cout<<m_locationX<<std::endl;

    //NS_LOG_INFO("> Interest for " << seq);

    //WillSendOutInterest(seq);
    std::cout << "Subscriber"<<GetNode()->GetId()<< " sends Single Interest["<<interest->getName()<<"] at " << Simulator::Now().ToDouble(Time::S) << "s\n";

    m_transmittedInterests(interest, this, m_face);
    m_appLink->onReceiveInterest(*interest);

    //ScheduleNextPacket(); //native
    //Simulator::Schedule(Seconds(0.5), &ConsumerSubscribe::ReSendSubInterest, this); //my
}

void
ConsumerSubscribe::SendSubInterest()
{
//    auto interest = std::make_shared<ndn::Interest>("/prefix/sub");
//    // Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
//    interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
//    interest->setCanBePrefix(false);
//    interest->setInterestLifetime(ndn::time::seconds(1));
//    interest->setInterestType(2);
//
//    m_transmittedInterests(interest, this, m_face);
//    m_appLink->onReceiveInterest(*interest);
//
//    ScheduleNextPacket();

    //

    //
    //  double current_time = Simulator::Now().ToDouble(Time::S);
    //  double start_time = 10;
    //  //double start_time = m_startTime;
    //  if(current_time>start_time+25&&current_time<start_time+41){
    //   double interval = start_time+41-current_time;
    //   Simulator::Schedule(Seconds(interval), &ConsumerSubscribe::SendSubInterest, this); 
    //   return;
    //  }
    //

    uint32_t seq = std::numeric_limits<uint32_t>::max(); // invalid
    shared_ptr<Name> nameWithSequence = make_shared<Name>(m_interestName);
    //seq = m_seq;  //native
    seq = 1; // my
    nameWithSequence->appendSequenceNumber(seq);
    //

    // shared_ptr<Interest> interest = make_shared<Interest> ();
    shared_ptr<Interest> interest = make_shared<Interest>();
    interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
    interest->setName(*nameWithSequence);
    interest->setCanBePrefix(true);
    interest->setMustBeFresh(true);//
    time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    interest->setInterestLifetime(interestLifeTime);
    interest->setInterestType(2);
    //interest->setInterestType(3); //native
    interest->setLastHopType(0);
    interest->setOBU(GetNode()->GetId());

    interest->setLocationX(m_locationX);
    interest->setLocationY(m_locationY);
    interest->setSpeedX(m_speedX);
    interest->setSpeedY(m_speedY);
    //std::cout<<"m_locationX:";
    //std::cout<<m_locationX<<std::endl;

    //NS_LOG_INFO("> Interest for " << seq);

    //WillSendOutInterest(seq);
    std::cout << "Subscriber"<<GetNode()->GetId()<< " sends Interest["<<interest->getName()<<"] at " << Simulator::Now().ToDouble(Time::S) << "s\n";

    m_transmittedInterests(interest, this, m_face);
    m_appLink->onReceiveInterest(*interest);

    //ScheduleNextPacket(); //native
    Simulator::Schedule(Seconds(0.5), &ConsumerSubscribe::ReSendSubInterest, this); //my
}

void
ConsumerSubscribe::ReSendSubInterest()
{
  if(!m_has_ack){
    std::cout << "Subscriber"<<GetNode()->GetId()<< " Resends Interest at " << Simulator::Now().ToDouble(Time::S) << "s\n";
    //SendSubInterest();
    Simulator::Schedule(Seconds(5.0), &ConsumerSubscribe::SendSubInterest, this); //my
  }
  m_has_ack = false;
}

} // namespace ndn
} // namespace ns3