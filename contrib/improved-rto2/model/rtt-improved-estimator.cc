/*
 * RttImprovedEstimator — Adaptive RTO Algorithm (implementation)
 *
 * Paper: "Improved RTO Algorithm for TCP Retransmission Timeout"
 *        Xiao Jianliang, Zhang Kun — AMEII 2015
 */

#include "rtt-improved-estimator.h"

#include "ns3/double.h"
#include "ns3/log.h"

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RttImprovedEstimator");
NS_OBJECT_ENSURE_REGISTERED(RttImprovedEstimator);

TypeId
RttImprovedEstimator::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::RttImprovedEstimator")
            .SetParent<RttMeanDeviation>()
            .SetGroupName("Internet")
            .AddConstructor<RttImprovedEstimator>()
            .AddAttribute("Alpha0",
                          "Base gain for SRTT (default 1/8)",
                          DoubleValue(0.125),
                          MakeDoubleAccessor(&RttImprovedEstimator::m_alpha0),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("Beta0",
                          "Base gain for RTTVAR (default 1/4)",
                          DoubleValue(0.25),
                          MakeDoubleAccessor(&RttImprovedEstimator::m_beta0),
                          MakeDoubleChecker<double>(0, 1));
    return tid;
}

RttImprovedEstimator::RttImprovedEstimator()
    : m_prevRtt(Time(0)),
      m_hasPrev(false)
{
    NS_LOG_FUNCTION(this);
}

RttImprovedEstimator::RttImprovedEstimator(const RttImprovedEstimator& r)
    : RttMeanDeviation(r),
      m_alpha0(r.m_alpha0),
      m_beta0(r.m_beta0),
      m_prevRtt(r.m_prevRtt),
      m_hasPrev(r.m_hasPrev)
{
    NS_LOG_FUNCTION(this);
}

RttImprovedEstimator::~RttImprovedEstimator()
{
    NS_LOG_FUNCTION(this);
}

void
RttImprovedEstimator::Measurement(Time t)
{
    NS_LOG_FUNCTION(this << t);

    if (m_nSamples == 0)
    {
        // First sample — same as Jacobson (Eq. 1, 2)
        m_estimatedRtt = t;
        m_estimatedVariation = t / 2;
        m_prevRtt = t;
        m_hasPrev = true;
    }
    else
    {
        double k = 0.0;
        if (m_hasPrev && m_prevRtt.GetSeconds() > 0.0)
        {
            k = std::abs(t.GetSeconds() - m_prevRtt.GetSeconds())
                / m_prevRtt.GetSeconds();
            if (k > 1.0)
                k = 1.0;
        }

        double alpha = m_alpha0 * (1.0 + k);
        double beta  = m_beta0  * (1.0 - k);

        double newSrtt = (1.0 - alpha) * m_estimatedRtt.GetSeconds()
                         + alpha * t.GetSeconds();
        m_estimatedRtt = Time::FromDouble(newSrtt, Time::S);

        double absErr = std::abs(t.GetSeconds() - m_estimatedRtt.GetSeconds());
        double newVar = (1.0 - beta) * m_estimatedVariation.GetSeconds()
                        + beta * absErr;
        m_estimatedVariation = Time::FromDouble(newVar, Time::S);
        if (m_estimatedVariation < Time(0))
            m_estimatedVariation = Time(0);

        m_prevRtt = t;
        m_hasPrev = true;
    }
    m_nSamples++;
}

Ptr<RttEstimator>
RttImprovedEstimator::Copy() const
{
    NS_LOG_FUNCTION(this);
    return CopyObject<RttImprovedEstimator>(this);
}

void
RttImprovedEstimator::Reset()
{
    NS_LOG_FUNCTION(this);
    RttMeanDeviation::Reset();
    m_prevRtt = Time(0);
    m_hasPrev = false;
}

} // namespace ns3
