

#ifndef RTT_IMPROVED_PAPER_H
#define RTT_IMPROVED_PAPER_H

#include "ns3/rtt-estimator.h"

namespace ns3 {

class RttImprovedPaper : public RttEstimator
{
public:
  static TypeId GetTypeId (void);

  RttImprovedPaper ();
  RttImprovedPaper (const RttImprovedPaper& r);

  void Measurement (Time measure) override;
  Ptr<RttEstimator> Copy () const override;
  void Reset () override;

private:
  double m_alpha0;     
  double m_beta0;      
  bool   m_adaptive;   
  Time   m_prevRtt;    
  bool   m_firstSample;
};

} // namespace ns3

#endif // RTT_IMPROVED_PAPER_H
