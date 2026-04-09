/*
 * RttImprovedEstimator — Adaptive RTO Algorithm
 *
 * Paper: "Improved RTO Algorithm for TCP Retransmission Timeout"
 *        Xiao Jianliang, Zhang Kun — AMEII 2015
 *
 * Inherits from RttMeanDeviation (Jacobson/Karels) and overrides
 * Measurement() so that α and β adapt to the rate of RTT change.
 */

#ifndef RTT_IMPROVED_ESTIMATOR_H
#define RTT_IMPROVED_ESTIMATOR_H

#include "ns3/nstime.h"
#include "ns3/rtt-estimator.h"

namespace ns3
{

/**
 * @ingroup tcp
 * @brief Improved RTT estimator with adaptive α and β.
 *
 * k    = |RTT_{n+1} − RTT_n| / RTT_n   (capped at 1)
 * α    = α₀ × (1 + k)
 * β    = β₀ × (1 − k)
 * SRTT, RTTVAR updated with the adaptive α, β using the same
 * EWMA form as the Jacobson algorithm.
 */
class RttImprovedEstimator : public RttMeanDeviation
{
  public:
    static TypeId GetTypeId();

    RttImprovedEstimator();
    RttImprovedEstimator(const RttImprovedEstimator& r);
    ~RttImprovedEstimator() override;

    void Measurement(Time t) override;
    Ptr<RttEstimator> Copy() const override;
    void Reset() override;

  private:
    double m_alpha0;    
    double m_beta0;     
    Time   m_prevRtt;   
    bool   m_hasPrev;   
};

} // namespace ns3

#endif /* RTT_IMPROVED_ESTIMATOR_H */
