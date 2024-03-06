/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2021,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "es-local-best-route-strategy.hpp"
#include "algorithm.hpp"
#include "common/logger.hpp"
#include "common/global.hpp"
#include "ns3/nstime.h"
#include <string>

namespace nfd {
namespace fw {

NFD_LOG_INIT(ESLocalBestRouteStrategy);
NFD_REGISTER_STRATEGY(ESLocalBestRouteStrategy);

const time::milliseconds ESLocalBestRouteStrategy::RETX_SUPPRESSION_INITIAL(10);
const time::milliseconds ESLocalBestRouteStrategy::RETX_SUPPRESSION_MAX(250);

// //my grid
const int ESLocalBestRouteStrategy::rsu_x[25]={0,0,0,0,0,
                    500,500,500,500,500,
                    1000,1000,1000,1000,1000,
                    1500,1500,1500,1500,1500,
                    2000,2000,2000,2000,2000};
const int ESLocalBestRouteStrategy::rsu_y[25]={0,500,1000,1500,2000,
                    0,500,1000,1500,2000,
                    0,500,1000,1500,2000,
                    0,500,1000,1500,2000,
                    0,500,1000,1500,2000};

 const int ESLocalBestRouteStrategy::es_x[6]={520,1520,520,1520,520,1520};
 const int ESLocalBestRouteStrategy::es_y[6]={250,250,1250,1250,2250,2250};

ESLocalBestRouteStrategy::ESLocalBestRouteStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , ProcessNackTraits(this)
  , m_retxSuppression(RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    NDN_THROW(std::invalid_argument("ESLocalBestRouteStrategy does not accept parameters"));
  }
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    NDN_THROW(std::invalid_argument(
      "ESLocalBestRouteStrategy does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
}

const Name&
ESLocalBestRouteStrategy::getStrategyName()
{
  static const auto strategyName = Name("/localhost/nfd/strategy/es-local-best-route").appendVersion(5);
  return strategyName;
}

void
ESLocalBestRouteStrategy::afterReceiveInterest(const Interest& interest, const FaceEndpoint& ingress,
                                        const shared_ptr<pit::Entry>& pitEntry)
{
  RetxSuppressionResult suppression = m_retxSuppression.decidePerPitEntry(*pitEntry);
  if (suppression == RetxSuppressionResult::SUPPRESS) {
    NFD_LOG_DEBUG(interest << " from=" << ingress << " suppressed");
    return;
  }

  if (suppression == RetxSuppressionResult::FORWARD) {
    //shared_ptr<Interest> temp_interest = make_shared<Interest>(interest);
    //pitEntry->OnlyinsertInRecord(ingress.face, interest);
    pitEntry->UpdateMaxTimerOrinsertInRecord(ingress.face, interest);
    NFD_LOG_DEBUG(interest << " from=" << ingress << " forward");
    return;
  }

  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  auto it = nexthops.end();

  if (suppression == RetxSuppressionResult::NEW) {
    // forward to nexthop with lowest cost except downstream
    it = std::find_if(nexthops.begin(), nexthops.end(), [&] (const auto& nexthop) {
      return isNextHopEligible(ingress.face, interest, nexthop, pitEntry);
    });

    if (it == nexthops.end()) { 
      NFD_LOG_DEBUG(interest << " from=" << ingress << " noNextHop");

      lp::NackHeader nackHeader;
      nackHeader.setReason(lp::NackReason::NO_ROUTE);
      this->sendNack(nackHeader, ingress.face, pitEntry);
      this->rejectPendingInterest(pitEntry);
      return;
    }

    Face& outFace = it->getFace();
    NFD_LOG_DEBUG(interest << " from=" << ingress << " newPitEntry-to=" << outFace.getId());

    if(interest.getInterestType()==2){
      shared_ptr<Interest> new_interest = make_shared<Interest>(interest);
      if(interest.getLastHopType()==2){
        double timer2 = calculateVT2(interest);
        double tolerant_time=1;
        time::milliseconds validity_timer2((long int)(timer2)*1000);
        new_interest->setInterestLifetime(validity_timer2);
        std::cout<<"ES-local reset VT2:";
        std::cout<<timer2<<std::endl;
        new_interest->setLastHopType(3);
        m_timeoutEvent = getScheduler().schedule(validity_timer2*1000000, [=] { prepareSendRetACK(interest,ingress.face); });
      }
      
      pitEntry->UpdateMaxTimerOrinsertInRecord(ingress.face, *new_interest);
      this->sendInterest(*new_interest, outFace, pitEntry);
    }
    else{
      this->sendInterest(interest, outFace, pitEntry);
    }
    
    return;
  }

  // find an unused upstream with lowest cost except downstream
  it = std::find_if(nexthops.begin(), nexthops.end(),
                    [&, now = time::steady_clock::now()] (const auto& nexthop) {
                      return isNextHopEligible(ingress.face, interest, nexthop, pitEntry, true, now);
                    });

  if (it != nexthops.end()) {
    Face& outFace = it->getFace();
    this->sendInterest(interest, outFace, pitEntry);
    NFD_LOG_DEBUG(interest << " from=" << ingress << " retransmit-unused-to=" << outFace.getId());
    return;
  }

  // find an eligible upstream that is used earliest
  it = findEligibleNextHopWithEarliestOutRecord(ingress.face, interest, nexthops, pitEntry);
  if (it == nexthops.end()) {
    NFD_LOG_DEBUG(interest << " from=" << ingress << " retransmitNoNextHop");
  }
  else {
    Face& outFace = it->getFace();
    this->sendInterest(interest, outFace, pitEntry);
    NFD_LOG_DEBUG(interest << " from=" << ingress << " retransmit-retry-to=" << outFace.getId());
  }
}

void
ESLocalBestRouteStrategy::afterReceiveNack(const lp::Nack& nack, const FaceEndpoint& ingress,
                                    const shared_ptr<pit::Entry>& pitEntry)
{
  this->processNack(nack, ingress.face, pitEntry);
}

void
ESLocalBestRouteStrategy::prepareSendRetACK(const Interest& interest, Face& egress)
{
  auto data = make_shared<Data>();
  Name dataName(interest.getName());
  uint32_t seq = interest.getName().at(-1).toSequenceNumber();
  std::string nack_postfix = "/nack/seq=";
  std::string seq_str = to_string(seq);
  nack_postfix.insert(10,seq_str);
  dataName.append(ndn::Name(nack_postfix));

  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(5000_ms));

  data->setContent(make_shared< ::ndn::Buffer>(1024));

  ndn::SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  data->setSignatureInfo(signatureInfo);

  ::ndn::EncodingEstimator estimator;
  ::ndn::EncodingBuffer encoder(estimator.appendVarNumber(0), 0);
  encoder.appendVarNumber(0);
  data->setSignatureValue(encoder.getBuffer());

  data->setACKtimer(7.0);
  data->setOBU(interest.getOBU());

  // to create real wire encoding
  data->wireEncode();

  this->sendACK(*data,egress);
  std::cout<<"ES-local send Ret-ACK---------------------------";
  std::cout<<dataName<<std::endl;

}

void
ESLocalBestRouteStrategy::prepareSendPre(const Interest& interest, int xPre, int yPre)
{
  auto data = make_shared<Data>();
  Name dataName(interest.getName());
  uint32_t seq = interest.getName().at(-1).toSequenceNumber();
  std::string nack_postfix = "/pre-sub/seq=";
  std::string seq_str = to_string(seq);
  nack_postfix.insert(10,seq_str);
  dataName.append(ndn::Name(nack_postfix));

  //dataName.append(ndn::Name("/ACK"));
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(5000_ms));

  data->setContent(make_shared< ::ndn::Buffer>(1024));

  ndn::SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  data->setSignatureInfo(signatureInfo);

  ::ndn::EncodingEstimator estimator;
  ::ndn::EncodingBuffer encoder(estimator.appendVarNumber(0), 0);
  encoder.appendVarNumber(0);
  data->setSignatureValue(encoder.getBuffer());

  data->setACKtimer(7.0);
  data->setOBU(interest.getOBU());

  // to create real wire encoding
  data->wireEncode();

  //this->sendACK(*data,egress);

}


double
ESLocalBestRouteStrategy::calculateVT2(const Interest& interest)
{
  int pos_x = interest.getLocationX();
  int pos_y = interest.getLocationY();
  int vel_x = interest.getSpeedX();
  int vel_y = interest.getSpeedY();

  //
  for(int i=0;i<27;i++){
    for(int j=1;j<5;j++){
      vehicle_Xinfo[i][5-j]=vehicle_Xinfo[i][4-j];
      vehicle_Yinfo[i][5-j]=vehicle_Xinfo[i][4-j];
    }
  }
  vehicle_Xinfo[interest.getOBU()][0]=pos_x;
  vehicle_Yinfo[interest.getOBU()][0]=pos_y;

  float xResult=0.0;
  float LValue[4];
  int q,m;
  float temp1,temp2;
 
  int rsu_id = 0;
  double min_distance = 20000;
  for(int i=0;i<25;i++){
    double distance = sqrt(pow((pos_x-rsu_x[i]),2)+pow((pos_y-rsu_y[i]),2));
    if(distance<min_distance){
      rsu_id = i;
      min_distance = distance;
    }
  }
  int ES_id;
  if(rsu_id==0||rsu_id==1||rsu_id==5||rsu_id==6||rsu_id==10||rsu_id==11){
    ES_id=0;
  }
  else if(rsu_id==15||rsu_id==16||rsu_id==20||rsu_id==21){
    ES_id=1;
  }
  else if(rsu_id==2||rsu_id==3||rsu_id==7||rsu_id==8||rsu_id==12||rsu_id==13){
    ES_id=2;
  }
  else if(rsu_id==17||rsu_id==18||rsu_id==22||rsu_id==23){
    ES_id=3;
  }
  else if(rsu_id==4||rsu_id==9||rsu_id==14){
    ES_id=4;
  }
  else if(rsu_id==24||rsu_id==25){
    ES_id=5;
  }

  double remain_time;
  double yidianwu_radius = 750;//modify
  double radius = 460;//modify
  //240^2+50^2 ~ 250^2
  if(vel_x!=0){
    double edge_x;
    if(vel_x*(pos_x-es_x[ES_id])>0){//正在远离
      edge_x = yidianwu_radius - abs(pos_x-es_x[ES_id]);
    }
    else{//正在接近
      edge_x = yidianwu_radius + abs(pos_x-es_x[ES_id]);
    }
    remain_time = edge_x/(double)vel_x;
  }
  else if(vel_y!=0){
    double edge_y;
    if(vel_y*(pos_y-es_y[ES_id])>0){//正在远离
      edge_y = radius - abs(pos_y-es_y[ES_id]);
    }
    else{//正在接近
      edge_y = radius + abs(pos_y-es_y[ES_id]);
    }
    remain_time = edge_y/(double)vel_y;
  }
  else{
    remain_time = 10000;
  }

  double k=0.7;
  double freqLoss = 0.2;
  double Tmin = 50.0; 
  double Tmax = 100.0;
  double rem_max = 100;
  double alpha = 0.4;  //adjusting
  double beta = 0.5;  //adjusting
  double SPS = alpha*remain_time/rem_max+beta*(1-freqLoss);
  double validity_timer1 = std::max(Tmin,std::min(SPS*remain_time,Tmax));

  return validity_timer1;
}

} // namespace fw
} // namespace nfd
