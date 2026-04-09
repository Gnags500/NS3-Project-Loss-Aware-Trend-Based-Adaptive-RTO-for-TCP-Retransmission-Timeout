
#include "rtt-proposed.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/log.h"

#include <cmath>
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RttProposed");
NS_OBJECT_ENSURE_REGISTERED (RttProposed);

TypeId
RttProposed::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RttProposed")
    .SetParent<RttEstimator> ()
    .SetGroupName ("Internet")
    .AddConstructor<RttProposed> ()
    .AddAttribute ("Alpha0",
                   "Baseline alpha (1/8 = 0.125 in Jacobson)",
                   DoubleValue (0.125),
                   MakeDoubleAccessor (&RttProposed::m_alpha0),
                   MakeDoubleChecker<double> (0.0, 1.0))
    .AddAttribute ("Beta0",
                   "Baseline beta (1/4 = 0.25 in Jacobson)",
                   DoubleValue (0.25),
                   MakeDoubleAccessor (&RttProposed::m_beta0),
                   MakeDoubleChecker<double> (0.0, 1.0))
    .AddAttribute ("WindowSize",
                   "Number of recent RTT pairs for trend computation (W)",
                   UintegerValue (1),
                   MakeUintegerAccessor (&RttProposed::m_windowSize),
                   MakeUintegerChecker<uint32_t> (1, 100))
    .AddAttribute ("Gamma",
                   "Loss sensitivity factor (gamma)",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&RttProposed::m_gamma),
                   MakeDoubleChecker<double> (0.0, 10.0))
  ;
  return tid;
}

RttProposed::RttProposed ()
  : m_alpha0 (0.125),
    m_beta0 (0.25),
    m_windowSize (1),
    m_gamma (1.0),
    m_lossRate (0.0),
    m_firstSample (true)
{
  NS_LOG_FUNCTION (this);
}

RttProposed::RttProposed (const RttProposed& r)
  : RttEstimator (r),
    m_alpha0 (r.m_alpha0),
    m_beta0 (r.m_beta0),
    m_windowSize (r.m_windowSize),
    m_gamma (r.m_gamma),
    m_lossRate (r.m_lossRate),
    m_rttHistory (r.m_rttHistory),
    m_firstSample (r.m_firstSample)
{
  NS_LOG_FUNCTION (this);
}

void
RttProposed::Measurement (Time measure)
{
  NS_LOG_FUNCTION (this << measure);

  double rtt = measure.GetSeconds ();

  if (m_firstSample)
    {
      // Eq. (1): SRTT₁ = RTT₁
      m_estimatedRtt = measure;
      // Eq. (2): RTTVAR₁ = RTT₁ / 2
      m_estimatedVariation = Time (measure / 2);
      m_firstSample = false;
      m_rttHistory.push_back (measure);
      NS_LOG_DEBUG ("First sample: SRTT=" << m_estimatedRtt.GetMilliSeconds ()
                    << "ms RTTVAR=" << m_estimatedVariation.GetMilliSeconds () << "ms");
      return;
    }

  m_rttHistory.push_back (measure);
  while (m_rttHistory.size () > m_windowSize + 1)
    m_rttHistory.pop_front ();

  uint32_t pairs = static_cast<uint32_t> (m_rttHistory.size ()) - 1;
  pairs = std::min (pairs, m_windowSize);

  double ktrend = 0.0;
  for (uint32_t i = 0; i < pairs; ++i)
    {
      size_t idxCurr = m_rttHistory.size () - 1 - i;
      size_t idxPrev = m_rttHistory.size () - 2 - i;
      double prevSec = m_rttHistory[idxPrev].GetSeconds ();
      double currSec = m_rttHistory[idxCurr].GetSeconds ();
      if (prevSec > 0)
        ktrend += std::abs (currSec - prevSec) / prevSec;
    }
  if (pairs > 0)
    ktrend /= pairs;

  ktrend = std::max (-1.0, std::min (1.0, ktrend));

  double alpha = m_alpha0 * (1.0 + ktrend);
  double beta  = m_beta0  * (1.0 - ktrend);

  
  double srttSec = (1.0 - alpha) * m_estimatedRtt.GetSeconds () + alpha * rtt;
  m_estimatedRtt = Seconds (srttSec);
  double rttvarSec = (1.0 - beta) * m_estimatedVariation.GetSeconds ()
                     + beta * std::abs (srttSec - rtt);
  m_estimatedVariation = Seconds (rttvarSec);

  NS_LOG_DEBUG ("ktrend=" << ktrend << " alpha=" << alpha << " beta=" << beta
                << " SRTT=" << m_estimatedRtt.GetMilliSeconds ()
                << "ms RTTVAR=" << m_estimatedVariation.GetMilliSeconds () << "ms");
}

void
RttProposed::SetLossRate (double rate)
{
  m_lossRate = rate;
}

double
RttProposed::GetLossRate () const
{
  return m_lossRate;
}

double
RttProposed::GetGamma () const
{
  return m_gamma;
}

uint32_t
RttProposed::GetWindowSize () const
{
  return m_windowSize;
}

Ptr<RttEstimator>
RttProposed::Copy () const
{
  NS_LOG_FUNCTION (this);
  return CopyObject<RttProposed> (this);
}

void
RttProposed::Reset ()
{
  NS_LOG_FUNCTION (this);
  RttEstimator::Reset ();
  m_rttHistory.clear ();
  m_firstSample = true;
  m_lossRate = 0.0;
}

} // namespace ns3
