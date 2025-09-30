/*******************************************************************************
 * Copyright 2022 MINRES Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#ifndef _SCC_SC_CLOCK_EXT_H_
#define _SCC_SC_CLOCK_EXT_H_

#include <cci_configuration>
#include <sysc/communication/sc_clock.h>

/** \ingroup scc-sysc
 *  @{
 */
/**@{*/
//! @brief SCC SystemC utilities
namespace scc {
/**
 * \brief A clock source with construction time configurable start delay.
 *
 * The sc_clock_ext class extends the sc_core::sc_clock class to provide additional functionality, such as configuration via CCI.
 * It allows users to specify the period, duty cycle, and start time of the generated clock .
 *
 */
struct sc_clock_ext : public sc_core::sc_clock {
    /**
     * \brief The period of the generated clock.
     *
     * This parameter allows users to specify the period of the generated clock.
     */
    cci::cci_param<sc_core::sc_time> period;
    /**
     * \brief The duty cycle of the generated clock.
     *
     * This parameter allows users to specify the duty cycle of the generated clock.
     */
    cci::cci_param<double> duty_cycle;
    /**
     * \brief The start time of the generated clock.
     *
     * This parameter allows users to specify the start time of the generated clock.
     */
    cci::cci_param<sc_core::sc_time> initial_delay;

    sc_clock_ext(const char* name_, const sc_core::sc_time& period_, double duty_cycle_ = 0.5,
                 const sc_core::sc_time& start_time_ = sc_core::SC_ZERO_TIME, bool posedge_first_ = true)
    : sc_core::sc_clock(name_, period_, duty_cycle_, start_time_, posedge_first_)
    , period(get_cci_name(name_, "period"), period_, "The period of the generated clock")
    , duty_cycle(get_cci_name(name_, "duty_cycle"), duty_cycle_, "The duty cycle of the generated clock")
    , initial_delay(get_cci_name(name_, "start_time"), start_time_, "The start time of the generated clock") {
        // period.register_post_write_callback(&sc_clock_ext::period_write_callback,this);
    }

    virtual ~sc_clock_ext() = default;

protected:
    void end_of_elaboration() override {
        init(period.get_value(), duty_cycle.get_value(), initial_delay.get_value(), m_posedge_first);
        if(initial_delay.get_value() != m_start_time) {
            if(m_posedge_first) {
                m_next_posedge_event.cancel();
                m_next_posedge_event.notify(initial_delay.get_value());
            } else {
                m_next_negedge_event.cancel();
                m_next_negedge_event.notify(initial_delay.get_value());
            }
        }
    }
    void period_write_callback(const cci::cci_param_write_event<int>& ev) {}
    static inline std::string get_cci_name(const char* base, const char* name) { return std::string(base) + "_" + name; }
};
} // namespace scc
/** @} */ // end of scc-sysc
#endif    //
