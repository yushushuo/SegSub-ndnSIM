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

#include "rsu-best-route-strategy.hpp"
#include "algorithm.hpp"
#include "common/logger.hpp"
#include "ns3/nstime.h"
#include <string>
#include <bits/stdc++.h>
#include <algorithm>

namespace nfd {
namespace fw {

NFD_LOG_INIT(RSUBestRouteStrategy);
NFD_REGISTER_STRATEGY(RSUBestRouteStrategy);

const time::milliseconds RSUBestRouteStrategy::RETX_SUPPRESSION_INITIAL(10);
const time::milliseconds RSUBestRouteStrategy::RETX_SUPPRESSION_MAX(250);

// my grid2 
const int RSUBestRouteStrategy::rsu_x[25]={0,0,0,0,0,
                    500,500,500,500,500,
                    1000,1000,1000,1000,1000,
                    1500,1500,1500,1500,1500,
                    2000,2000,2000,2000,2000};
const int RSUBestRouteStrategy::rsu_y[25]={0,500,1000,1500,2000,
                    0,500,1000,1500,2000,
                    0,500,1000,1500,2000,
                    0,500,1000,1500,2000,
                    0,500,1000,1500,2000};


RSUBestRouteStrategy::RSUBestRouteStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , ProcessNackTraits(this)
  , m_retxSuppression(RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    NDN_THROW(std::invalid_argument("RSUBestRouteStrategy does not accept parameters"));
  }
  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    NDN_THROW(std::invalid_argument(
      "RSUBestRouteStrategy does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
}

const Name&
RSUBestRouteStrategy::getStrategyName()
{
  static const auto strategyName = Name("/localhost/nfd/strategy/rsu-best-route").appendVersion(5);
  return strategyName;
}

void
RSUBestRouteStrategy::afterReceiveInterest(const Interest& interest, const FaceEndpoint& ingress,
                                        const shared_ptr<pit::Entry>& pitEntry)
{
  RetxSuppressionResult suppression = m_retxSuppression.decidePerPitEntry(*pitEntry);
  if (suppression == RetxSuppressionResult::SUPPRESS) {
    //std::cout<<"RetxSuppressionResult::SUPPRESS"<<std::endl;
    NFD_LOG_DEBUG(interest << " from=" << ingress << " suppressed");
    return;
  }

  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  auto it = nexthops.end();

  if (suppression == RetxSuppressionResult::NEW || suppression == RetxSuppressionResult::FORWARD) {
    // forward to nexthop with lowest cost except downstream
    //std::cout<<"RetxSuppressionResult::NEW"<<std::endl;
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
      int packet_loss_ratio = rand()%10;
      int dynamic_limit = 7; 

      shared_ptr<Interest> new_interest = make_shared<Interest>(interest);
      if(interest.getLastHopType()==1){
        std::cout<<"RSU receive interest from "<<interest.getOBU()<<std::endl;
        double timer1 = calculateVT1(interest);
        double tolerant_time=1;

        if(packet_loss_ratio>dynamic_limit){
          timer1 = 2.0; 
        }

        time::milliseconds validity_timer1((long int)(timer1+tolerant_time)*1000);
        new_interest->setInterestLifetime(validity_timer1);
        new_interest->setLastHopType(2);
        std::cout<<"RSU reset VT1:";
        std::cout<<timer1<<std::endl;
        //if(suppression == RetxSuppressionResult::NEW){
          std::cout<<"RSU receive send ack to "<<interest.getOBU()<<std::endl;
          this->prepareSendACK(*new_interest,ingress.face, timer1);//
        //}

        if(packet_loss_ratio>dynamic_limit){
          return; 
        }
      }
      else{
        time::milliseconds producer_timer(50*1000);
        new_interest->setInterestLifetime(producer_timer);
      }
      
      pitEntry->UpdateMaxTimerOrinsertInRecord(ingress.face, *new_interest);//
      this->sendInterest(*new_interest, outFace, pitEntry);//
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
    //std::cout<<"eligible upstream that is used earliest"<<std::endl;
    this->sendInterest(interest, outFace, pitEntry);
    NFD_LOG_DEBUG(interest << " from=" << ingress << " retransmit-retry-to=" << outFace.getId());
  }
}

void
RSUBestRouteStrategy::afterReceiveNack(const lp::Nack& nack, const FaceEndpoint& ingress,
                                    const shared_ptr<pit::Entry>& pitEntry)
{
  this->processNack(nack, ingress.face, pitEntry);
}

void
RSUBestRouteStrategy::prepareSendACK(const Interest& interest, Face& egress, double timer1)
{
  auto data = make_shared<Data>();
  Name dataName(interest.getName());
  //
  uint32_t seq = interest.getName().at(-1).toSequenceNumber();
  std::string ack_postfix = "/ack/seq=";
  std::string seq_str = to_string(seq);
  ack_postfix.insert(9,seq_str);
  dataName.append(ndn::Name(ack_postfix));
  //std::cout<<dataName<<std::endl;
  
  //std::cout<<"ack to vehicle "<<interest.getOBU()<<std::endl;

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

  data->setACKtimer(timer1);
  data->setOBU(interest.getOBU());

  // to create real wire encoding
  data->wireEncode();

  this->sendACK(*data,egress);
  std::cout<<"RSU sends an ACK:";
  std::cout<<dataName<<std::endl;

}

double
RSUBestRouteStrategy::calculateVT1(const Interest& interest)
{
  int pos_x = interest.getLocationX();
  int pos_y = interest.getLocationY();
  int vel_x = interest.getSpeedX();
  int vel_y = interest.getSpeedY();
  int rsu_id = 0;
  double min_distance = 20000;
  for(int i=0;i<25;i++){
    double distance = sqrt(pow((pos_x-rsu_x[i]),2)+pow((pos_y-rsu_y[i]),2));
    if(distance<min_distance){
      rsu_id = i;
      min_distance = distance;
    }
  }
  double remain_time;
  double half_radius = 250;
  //240^2+50^2 ~ 250^2
  if(vel_x!=0 && (abs(vel_x)>abs(vel_y))){
    double edge_x;
    if(vel_x*(pos_x-rsu_x[rsu_id])>0){//
      edge_x = half_radius - abs(pos_x-rsu_x[rsu_id]);
    }
    else{//
      edge_x = half_radius + abs(pos_x-rsu_x[rsu_id]);
    }
    remain_time = edge_x/(double)vel_x;
    std::cout<<"vel-x = "<<vel_x<<",edge-x = "<<edge_x<<",remain_time = "<<remain_time<<" -> ";
  }
  else if(vel_y!=0){
    double edge_y;
    if(vel_y*(pos_y-rsu_y[rsu_id])>0){//
      edge_y = half_radius - abs(pos_y-rsu_y[rsu_id]);
    }
    else{//
      edge_y = half_radius + abs(pos_y-rsu_y[rsu_id]);
    }
    remain_time = edge_y/(double)vel_y;
    std::cout<<"vel-y = "<<vel_y<<",edge-y = "<<edge_y<<",remain_time = "<<remain_time<<" -> ";
  }
  else{
    remain_time = 10000;
  }
  remain_time = abs(remain_time);

  //0920
  double rem_max = 50;
  if(remain_time>rem_max){
    remain_time=rem_max;
  }
  
  double k=0.5;
  double k2=0.2;
  double freqLoss = 0.2;
  double Tmin = 2.0;
  double Tmax = 25.0;
  double SPS = k*remain_time+k2/freqLoss;

  double w = 0.06;
  double SPS_new = Tmin+w*remain_time/freqLoss;

  double alpha = 0.5;  //adjusting
  double beta = 0.13;  //adjusting
  double SPS0920 = alpha*remain_time/rem_max+beta*(1-freqLoss);

  if(remain_time<5){
    SPS_new = Tmin;
    return Tmin;
  }

  double validity_timer1 = std::max(Tmin,SPS0920*Tmax);
  std::cout<<rsu_id<<" ";

  return validity_timer1;
}

} // namespace fw
} // namespace nfd
