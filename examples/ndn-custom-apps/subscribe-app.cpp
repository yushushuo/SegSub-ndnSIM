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

// subscribe-app.cpp

#include "subscribe-app.hpp"

#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"

#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/ndnSIM/helper/ndn-fib-helper.hpp"

#include "ns3/random-variable-stream.h"

NS_LOG_COMPONENT_DEFINE("SubscribeApp");

namespace ns3 {

    NS_OBJECT_ENSURE_REGISTERED(SubscribeApp);

// register NS-3 type
    TypeId
    SubscribeApp::GetTypeId()
    {
        static TypeId tid = TypeId("SubscribeApp").SetParent<ndn::App>().AddConstructor<SubscribeApp>();
        return tid;
    }

// Processing upon start of the application
    void
    SubscribeApp::StartApplication()
    {
        // initialize ndn::App
        ndn::App::StartApplication();

        // Add entry to FIB for `/prefix/sub`
        ndn::FibHelper::AddRoute(GetNode(), "/prefix/sub", m_face, 0);

        // Schedule send of first interest
        Simulator::Schedule(Seconds(1.0), &SubscribeApp::SendInterest, this);
        Simulator::Schedule(Seconds(2.0), &SubscribeApp::SendInterest, this);
        Simulator::Schedule(Seconds(3.0), &SubscribeApp::SendInterest, this);
    }

// Processing when application is stopped
    void
    SubscribeApp::StopApplication()
    {
        // cleanup ndn::App
        ndn::App::StopApplication();
    }

    void
    SubscribeApp::SendInterest()
    {
        /////////////////////////////////////
        // Sending one Interest packet out //
        /////////////////////////////////////

        // Create and configure ndn::Interest
        auto interest = std::make_shared<ndn::Interest>("/prefix/sub");
        Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
        interest->setNonce(rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
        interest->setInterestLifetime(ndn::time::seconds(1));
        interest->setInterestType(2);

        NS_LOG_DEBUG("Sending Interest packet for " << *interest);

        // Call trace (for logging purposes)
        m_transmittedInterests(interest, this, m_face);

        m_appLink->onReceiveInterest(*interest);
    }

// Callback that will be called when Interest arrives
    void
    SubscribeApp::OnInterest(std::shared_ptr<const ndn::Interest> interest)
    {
        ndn::App::OnInterest(interest);

        NS_LOG_DEBUG("Received Interest packet for " << interest->getName());

        // Note that Interests send out by the app will not be sent back to the app !

        auto data = std::make_shared<ndn::Data>(interest->getName());
        data->setFreshnessPeriod(ndn::time::milliseconds(1000));
        data->setContent(std::make_shared< ::ndn::Buffer>(1024));
        ndn::StackHelper::getKeyChain().sign(*data);

        NS_LOG_DEBUG("Sending Data packet for " << data->getName());

        // Call trace (for logging purposes)
        m_transmittedDatas(data, this, m_face);

        m_appLink->onReceiveData(*data);
    }

// Callback that will be called when Data arrives
    void
    SubscribeApp::OnData(std::shared_ptr<const ndn::Data> data)
    {
        NS_LOG_DEBUG("Receiving Data packet for " << data->getName());

        std::cout << "DATA received for name " << data->getName() << std::endl;
    }

} // namespace ns3
