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

#include "ndn-publisher.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/double.h"

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include <memory>

NS_LOG_COMPONENT_DEFINE("ndn.Publisher");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(Publisher);

TypeId
Publisher::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::Publisher")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<Publisher>()
      .AddAttribute("Prefix", "Prefix, for which publisher has the data", StringValue("/"),
                    MakeNameAccessor(&Publisher::m_prefix), MakeNameChecker())
      .AddAttribute(
         "Postfix",
         "Postfix that is added to the output data (e.g., for adding publisher-uniqueness)",
         StringValue("/"), MakeNameAccessor(&Publisher::m_postfix), MakeNameChecker())
      .AddAttribute(
         "Postfix2",
         "Postfix2 that is added to the output data (e.g., for adding publisher-uniqueness)",
         StringValue("/"), MakeNameAccessor(&Publisher::m_postfix2), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&Publisher::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(1.0)), MakeTimeAccessor(&Publisher::m_freshness),
                    MakeTimeChecker())
      .AddAttribute("StartTimePub", "StartTimePub",
                    StringValue("8"),
                    MakeDoubleAccessor(&Publisher::m_startTime), MakeDoubleChecker<double>())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&Publisher::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&Publisher::m_keyLocator), MakeNameChecker());
  return tid;
}

Publisher::Publisher()
      :m_seq(1)
{
  NS_LOG_FUNCTION_NOARGS();
}

// inherited from Application base class.
void
Publisher::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);
  Publisher::SchedulePublish();
}

void
Publisher::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void
Publisher::OnInterest(shared_ptr<const Interest> interest)
{
  App::OnInterest(interest); // tracing inside

  NS_LOG_FUNCTION(this << interest);

  if (!m_active)
    return;
  
  if (interest->getInterestType()==2)
    return;

  //if (interest->getInterestType()==1)
  //  return;

  Name dataName(interest->getName());
  // dataName.append(m_postfix);
  // dataName.appendVersion();

  auto data = make_shared<Data>();
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

  data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));

  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }

  data->setSignatureInfo(signatureInfo);

  ::ndn::EncodingEstimator estimator;
  ::ndn::EncodingBuffer encoder(estimator.appendVarNumber(m_signature), 0);
  encoder.appendVarNumber(m_signature);
  data->setSignatureValue(encoder.getBuffer());
  data->setOBU(50);//重传的interest

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") responding with Data: " << data->getName());

  // to create real wire encoding
  data->wireEncode();

  m_transmittedDatas(data, this, m_face);
  m_appLink->onReceiveData(*data);
}

void
Publisher::SchedulePublish()
{
  Name accident("/street10/accident");
  Name weather("/street07/weather");
  //bool is_migration = false;
  double start_time = m_startTime;
  if(accident.equals(m_prefix)){
    
      Simulator::Schedule(Seconds(15.0+start_time), &Publisher::PublishNextData, this);
      Simulator::Schedule(Seconds(16.0+start_time), &Publisher::PublishNextData, this);
      Simulator::Schedule(Seconds(17.0+start_time), &Publisher::PublishNextData, this);
      Simulator::Schedule(Seconds(18.0+start_time), &Publisher::PublishNextData, this); //

      Simulator::Schedule(Seconds(22.0+start_time), &Publisher::PublishNextData, this); //
      Simulator::Schedule(Seconds(24.0+start_time), &Publisher::PublishNextData, this);
      Simulator::Schedule(Seconds(26.0+start_time), &Publisher::PublishNextData, this);
      Simulator::Schedule(Seconds(28.0+start_time), &Publisher::PublishNextData, this); 
      
      Simulator::Schedule(Seconds(40.0+start_time), &Publisher::PublishNextData, this); //
      Simulator::Schedule(Seconds(42.0+start_time), &Publisher::PublishNextData, this); //
      Simulator::Schedule(Seconds(44.0+start_time), &Publisher::PublishNextData, this); //
      Simulator::Schedule(Seconds(46.0+start_time), &Publisher::PublishNextData, this); //
  }
  else if(weather.equals(m_prefix)){
    Simulator::Schedule(Seconds(42.0), &Publisher::PublishNextData, this);
    Simulator::Schedule(Seconds(46.0), &Publisher::PublishNextData, this);
    Simulator::Schedule(Seconds(48.0), &Publisher::PublishNextData, this);
    Simulator::Schedule(Seconds(50.0), &Publisher::PublishNextData, this);
    Simulator::Schedule(Seconds(53.0), &Publisher::PublishNextData, this);
  }
  
}

void
Publisher::PublishNextData()
{
  Name dataName(m_prefix);
  dataName.append(m_postfix);
  shared_ptr<Name> nameWithSequence = make_shared<Name>(dataName);
  nameWithSequence->appendSequenceNumber(m_seq);
  m_seq += 1;
  //dataName.append(m_postfix2);

  auto data = make_shared<Data>();
  data->setName(*nameWithSequence);
  //data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));
  data->setFreshnessPeriod(::ndn::time::milliseconds(5000));

  data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));

  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }

  data->setSignatureInfo(signatureInfo);

  ::ndn::EncodingEstimator estimator;
  ::ndn::EncodingBuffer encoder(estimator.appendVarNumber(m_signature), 0);
  encoder.appendVarNumber(m_signature);
  data->setSignatureValue(encoder.getBuffer());

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") responding with Data: " << data->getName());

  // to create real wire encoding
  data->wireEncode();

  m_transmittedDatas(data, this, m_face);
  m_appLink->onReceiveData(*data);
  
  std::cout<<"Publisher sends a Data:";
  std::cout<<dataName<<std::endl;
}

} // namespace ndn
} // namespace ns3
