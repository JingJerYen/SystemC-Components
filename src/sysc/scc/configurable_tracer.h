/*******************************************************************************
 * Copyright 2017, 2018 MINRES Technologies GmbH
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

#ifndef _SCC_CONFIGURABLE_TRACER_H_
#define _SCC_CONFIGURABLE_TRACER_H_

#include "tracer.h"
/** \ingroup scc-sysc
 *  @{
 */
/**@{*/
//! @brief SCC SystemC utilities
namespace scc {
/**
 * @class configurable_tracer
 * @brief configurable tracer for automatic port and signal tracing
 *
 * This class traverses the SystemC object hierarchy and registers all signals and ports found with the tracing
 * infrastructure. Using a sc_core::sc_attribute or a CCI param named "enableTracing" this can be switch on or off
 * on a per module basis
 */
class configurable_tracer : public tracer {
public:
    /**
     * constructs a tracer object
     *
     * @param name basename of the trace file(s)
     * @param enable_tx enables transaction tracing
     * @param enable_vcd enable VCD (signal based) tracing
     * @param top the topleve to use to trace variables
     */
    configurable_tracer(std::string const&& name, bool enable_tx = true, bool enable_vcd = true, sc_core::sc_object* top = nullptr);
    /**
     * constructs a tracer object
     *
     * @param name basename of the trace file(s)
     * @param enable_tx enables transaction tracing
     * @param enable_vcd enable VCD (signal based) tracing
     * @param top the topleve to use to trace variables
     */
    configurable_tracer(std::string const& name, bool enable_tx = true, bool enable_vcd = true, sc_core::sc_object* top = nullptr)
    : configurable_tracer(std::string(name), enable_tx, enable_vcd, top) {}
    /**
     * constructs a tracer object
     *
     * @param name basename of the trace file(s)
     * @param type type of trace file for transactions
     * @param enable enable VCD (signal based) tracing
     * @param top the topleve to use to trace variables
     */
    configurable_tracer(std::string const&& name, file_type type, bool enable_vcd = true, sc_core::sc_object* top = nullptr);
    /**
     * constructs a tracer object
     *
     * @param name basename of the trace file(s)
     * @param type type of trace file for transactions
     * @param enable_vcd enable VCD (signal based) tracing
     * @param top the topleve to use to trace variables
     */
    configurable_tracer(std::string const& name, file_type type, bool enable_vcd = true, sc_core::sc_object* top = nullptr)
    : configurable_tracer(std::string(name), type, enable_vcd, top) {}
    /**
     * constructs a tracer object
     *
     * @param name basename of the trace file(s)
     * @param tx_type type of trace file for transactions
     * @param sig_type type of trace file for signals
     * @param top the topleve to use to trace variables
     */
    configurable_tracer(std::string const&& name, file_type tx_type, file_type sig_type, sc_core::sc_object* top = nullptr);
    configurable_tracer(std::string const& name, file_type tx_type, file_type sig_type, sc_core::sc_object* top = nullptr)
    : configurable_tracer(std::string(name), tx_type, sig_type, top) {}
    /**
     * constructs a tracer object
     *
     * @param name basename of the trace file(s)
     * @param type type of trace file for transactions
     * @param tf the trace file to use for signal and POD tracing
     * @param top the topleve to use to trace variables
     */
    configurable_tracer(std::string const&& name, file_type type, sc_core::sc_trace_file* tf = nullptr, sc_core::sc_object* top = nullptr);
    /**
     * constructs a tracer object
     *
     * @param name basename of the trace file(s)
     * @param type type of trace file for transactions
     * @param tf the trace file to use for signal and POD tracing
     * @param top the topleve to use to trace variables
     */
    configurable_tracer(std::string const& name, file_type type, sc_core::sc_trace_file* tf = nullptr, sc_core::sc_object* top = nullptr)
    : configurable_tracer(std::string(name), type, tf, top) {}
    /**
     * destructor
     */
    ~configurable_tracer();
    /**
     * adds default trace control attribute of name 'enableTracing' to each sc_module in a design hierarchy
     */
    void add_control() { add_control(default_trace_enable_handle.get_cci_value().get<bool>()); }
    /**
     * adds default trace control attribute of name 'enableTracing' to each sc_module in a design hierarchy
     *
     * @param trace_default the default value of the attribute to be added
     */
    void add_control(bool trace_default) {
        if(control_added)
            return;
        for(auto* o : sc_core::sc_get_top_level_objects())
            augment_object_hierarchical(o, trace_default);
        control_added = true;
    }

protected:
    //! depth-first walk thru the design hierarchy and trace signals resp. call trace() function
    void descend(const sc_core::sc_object*, bool trace_all = false) override;
    //! check for existence of 'enableTracing' attribute and return value of default otherwise
    bool get_trace_enabled(const sc_core::sc_object*, bool = false);
    //! add the 'enableTracing' attribute to sc_module
    void augment_object_hierarchical(sc_core::sc_object*, bool);

    void end_of_elaboration() override;
    //! array of created cci parameter
    std::vector<cci::cci_param_untyped*> params;
    bool control_added{false};
};

} /* namespace scc */
/** @} */ // end of scc-sysc
#endif    /* _SCC_CONFIGURABLE_TRACER_H_ */
