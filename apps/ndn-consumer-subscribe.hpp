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

#ifndef NDN_CONSUMER_SUBSCRIBE_H
#define NDN_CONSUMER_SUBSCRIBE_H

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ndn-consumer.hpp"

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn-apps
 * @brief Ndn application for sending out Interest packets at a "constant" rate (Poisson process)
 */
class ConsumerSubscribe : public Consumer {
public:
  static TypeId
  GetTypeId();

  /**
   * \brief Default constructor
   * Sets up randomizer function and packet sequence number
   */
  ConsumerSubscribe();
  virtual ~ConsumerSubscribe();

  virtual void
  OnData(shared_ptr<const Data> data);


protected:
  /**
   * \brief Constructs the Interest packet and sends it using a callback to the underlying NDN
   * protocol
   */
  virtual void
  ScheduleNextPacket();

  virtual void
  StartApplication();

  void
  SendSubInterest();

  void
  ResendSingleInterest();

  void
  UpdateMobility();

  void
  ReSendSubInterest();

protected:
  double m_frequency; // Frequency of interest packets (in hertz)
  bool m_firstTime;
  bool m_has_ack;
  double m_VT1; // VT1
  double m_WaitTime;

  double m_locationX;
  double m_locationY;
  double m_speedX;
  double m_speedY;

  double m_startTime;

  int m_last_seq;
  uint32_t m_resend_seq; 

  Ptr<RandomVariableStream> m_random;
  std::string m_randomType;
};

} // namespace ndn
} // namespace ns3

#endif
