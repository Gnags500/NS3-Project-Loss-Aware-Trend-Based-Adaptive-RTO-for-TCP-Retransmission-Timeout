

#include "rtt-improved-paper.h"
#include "ns3/double.h"
#include "ns3/boolean.h"
#include "ns3/log.h"

#include <cmath>
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RttImprovedPaper");
NS_OBJECT_ENSURE_REGISTERED (RttImprovedPaper);

TypeId
RttImprovedPaper::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RttImprovedPaper")
    .SetParent<RttEstimator> ()
    .SetGroupName ("Internet")
    .AddConstructor<RttImprovedPaper> ()
    .AddAttribute ("Alpha0",
                   "Baseline alpha (1/8 = 0.125 in Jacobson)",
                   DoubleValue (0.125),
                   MakeDoubleAccessor (&RttImprovedPaper::m_alpha0),
                   MakeDoubleChecker<double> (0.0, 1.0))
    .AddAttribute ("Beta0",
                   "Baseline beta (1/4 = 0.25 in Jacobson)",
                   DoubleValue (0.25),
                   MakeDoubleAccessor (&RttImprovedPaper::m_beta0),
                   MakeDoubleChecker<double> (0.0, 1.0))
    .AddAttribute ("Adaptive",
                   "If true, use adaptive alpha/beta (Eq. 6-11); "
                   "if false, use fixed alpha0/beta0 (traditional Jacobson Eq. 3-5)",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RttImprovedPaper::m_adaptive),
                   MakeBooleanChecker ())
  ;
  return tid;
}

RttImprovedPaper::RttImprovedPaper ()
  : m_alpha0 (0.125),
    m_beta0 (0.25),
    m_adaptive (true),
    m_prevRtt (Seconds (0)),
    m_firstSample (true)
{
  NS_LOG_FUNCTION (this);
}

RttImprovedPaper::RttImprovedPaper (const RttImprovedPaper& r)
  : RttEstimator (r),
    m_alpha0 (r.m_alpha0),
    m_beta0 (r.m_beta0),
    m_adaptive (r.m_adaptive),
    m_prevRtt (r.m_prevRtt),
    m_firstSample (r.m_firstSample)
{
  NS_LOG_FUNCTION (this);
}

void
RttImprovedPaper::Measurement (Time measure)
{
  NS_LOG_FUNCTION (this << measure);

  double rtt = measure.GetSeconds ();

  if (m_firstSample)
    {
      m_estimatedRtt = measure;
      m_estimatedVariation = Time (measure / 2);
      m_firstSample = false;
      m_prevRtt = measure;
      NS_LOG_DEBUG ("First sample: SRTT=" << m_estimatedRtt.GetMilliSeconds ()
                    << "ms RTTVAR=" << m_estimatedVariation.GetMilliSeconds () << "ms");
      return;
    }

  double alpha, beta;

  if (m_adaptive && m_prevRtt > Seconds (0))
    {
      double prevSec = m_prevRtt.GetSeconds ();
      double k = std::abs (rtt - prevSec) / prevSec;
      k = std::min (k, 1.0);
      alpha = m_alpha0 * (1.0 + k);
      beta  = m_beta0  * (1.0 - k);
    }
  else
    {
      alpha = m_alpha0;
      beta  = m_beta0;
    }

  double srttSec = (1.0 - alpha) * m_estimatedRtt.GetSeconds () + alpha * rtt;
  m_estimatedRtt = Seconds (srttSec);

  double rttvarSec = (1.0 - beta) * m_estimatedVariation.GetSeconds ()
                     + beta * std::abs (srttSec - rtt);
  m_estimatedVariation = Seconds (rttvarSec);

  m_prevRtt = measure;

  NS_LOG_DEBUG ("SRTT=" << m_estimatedRtt.GetMilliSeconds ()
                << "ms RTTVAR=" << m_estimatedVariation.GetMilliSeconds ()
                << "ms alpha=" << alpha << " beta=" << beta);
}

Ptr<RttEstimator>
RttImprovedPaper::Copy () const
{
  NS_LOG_FUNCTION (this);
  return CopyObject<RttImprovedPaper> (this);
}

void
RttImprovedPaper::Reset ()
{
  NS_LOG_FUNCTION (this);
  RttEstimator::Reset ();
  m_prevRtt = Seconds (0);
  m_firstSample = true;
}

} // namespace ns3
