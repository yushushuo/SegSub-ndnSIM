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

#include "es-destinated-best-route-strategy.hpp"
#include "algorithm.hpp"
#include "common/logger.hpp"
#include "common/global.hpp"
#include "ns3/nstime.h"
#include <string>

namespace nfd {
namespace fw {

NFD_LOG_INIT(ESDestinatedBestRouteStrategy);
NFD_REGISTER_STRATEGY(ESDestinatedBestRouteStrategy);

const time::milliseconds ESDestinatedBestRouteStrategy::RETX_SUPPRESSION_INITIAL(10);
const time::milliseconds ESDestinatedBestRouteStrategy::RETX_SUPPRESSION_MAX(250);

ESDestinatedBestRouteStrategy::ESDestinatedBestRouteStrategy(Forwarder& forwarder, const Name& name)
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
ESDestinatedBestRouteStrategy::getStrategyName()
{
  static const auto strategyName = Name("/localhost/nfd/strategy/es-destinated-best-route").appendVersion(5);
  return strategyName;
}

void
ESDestinatedBestRouteStrategy::afterReceiveInterest(const Interest& interest, const FaceEndpoint& ingress,
                                        const shared_ptr<pit::Entry>& pitEntry)
{
  RetxSuppressionResult suppression = m_retxSuppression.decidePerPitEntry(*pitEntry);
  if (suppression == RetxSuppressionResult::SUPPRESS) {
    NFD_LOG_DEBUG(interest << " from=" << ingress << " suppressed");
    return;
  }

  if (suppression == RetxSuppressionResult::FORWARD) {
    //shared_ptr<Interest> temp_interest = make_shared<Interest>(interest);
    //temp_interest->setInterestLifetime(0_ms);
    //pitEntry->insertOrUpdateInRecord(ingress.face, *temp_interest);
    NFD_LOG_DEBUG(interest << " from=" << ingress << " forward");
    return;
  }

  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();
  auto it = nexthops.end();

  if (suppression == RetxSuppressionResult::NEW) {
    m_data_map[interest.getName()] = 0;

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

    if(interest.getLastHopType()==3){
      double freqLoss = 0.3;
      double Tmin = 2.0;
      double Tmax = 25.0;
      double alpha = 0.03;  //adjusting
      double beta = 0.4;  //adjusting
      double SPS = alpha*m_data_map[interest.getName()]+beta*(1-freqLoss);

      double validity_timer3 = std::max(Tmin,SPS*Tmax);
        double tolerant_time=1;
        new_interest->setInterestLifetime((long int)(validity_timer3)*1000);
      //this->sendInterest(interest, outFace, pitEntry);
      std::cout<<"ES-destinated reset VT3"<<std::endl;
      pitEntry->UpdateMaxTimerOrinsertInRecord(ingress.face, *new_interest);
      this->sendInterest(*new_interest, outFace, pitEntry);

      m_timeoutEvent = getScheduler().schedule(interest.getInterestLifetime()*1000000, [=] { prepareSendRetACK(interest,ingress.face); });
      
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
ESDestinatedBestRouteStrategy::afterReceiveNack(const lp::Nack& nack, const FaceEndpoint& ingress,
                                    const shared_ptr<pit::Entry>& pitEntry)
{
  this->processNack(nack, ingress.face, pitEntry);
}

void
ESDestinatedBestRouteStrategy::afterReceiveData(const Data& data, const FaceEndpoint& ingress,
                                    const shared_ptr<pit::Entry>& pitEntry)
{
  NFD_LOG_DEBUG("afterReceiveData pitEntry=" << pitEntry->getName()
                << " in=" << ingress << " data=" << data.getName());
  
  Name dataName(data.getName());
  for(auto& x: m_data_map){
    if(x.first.isPrefixOf(dataName)){
      m_data_map[x.first] += 1;
    }
  }
  std::cout<<"afterReceiveData, data Map:"<<std::endl;
  for(auto& x: m_data_map){
    std::cout<<x.first<<"  :  "<<x.second<<std::endl;
  }

  //this->beforeSatisfyInterest(data, ingress, pitEntry);
  //this->sendDataToAll(data, pitEntry, ingress.face);
}

void
ESDestinatedBestRouteStrategy::prepareSendRetACK(const Interest& interest, Face& egress)
{
  auto data = make_shared<Data>();
  Name dataName(interest.getName());
  uint32_t seq = interest.getName().at(-1).toSequenceNumber();
  std::string nack_postfix = "/nack/seq=";
  std::string seq_str = to_string(seq);
  nack_postfix.insert(10,seq_str);
  dataName.append(ndn::Name(nack_postfix));
  //std::cout<<dataName<<std::endl;

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

  // to create real wire encoding
  data->wireEncode();

  this->sendACK(*data,egress);
  std::cout<<"ES-destinated send Ret-ACK";
  std::cout<<dataName<<std::endl;

}


} // namespace fw
} // namespace nfd
