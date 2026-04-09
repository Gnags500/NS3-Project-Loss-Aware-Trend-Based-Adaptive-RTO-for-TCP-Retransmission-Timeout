
#ifndef RTT_PROPOSED_H
#define RTT_PROPOSED_H

#include "ns3/rtt-estimator.h"
#include <deque>

namespace ns3 {

class RttProposed : public RttEstimator
{
public:
  static TypeId GetTypeId (void);

  RttProposed ();
  RttProposed (const RttProposed& r);

  void Measurement (Time measure) override;
  Ptr<RttEstimator> Copy () const override;
  void Reset () override;

  void SetLossRate (double rate);
  double GetLossRate () const;
  double GetGamma () const;
  uint32_t GetWindowSize () const;

private:
  double   m_alpha0;      
  double   m_beta0;       
  uint32_t m_windowSize; 
  double   m_gamma;       
  double   m_lossRate;    

  std::deque<Time> m_rttHistory;  
  bool m_firstSample;
};

} // namespace ns3

#endif // RTT_PROPOSED_H
