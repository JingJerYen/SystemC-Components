/*******************************************************************************
 * Copyright 2018-2022 MINRES Technologies GmbH
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

#include "configurable_tracer.h"
#include "traceable.h"

using namespace sc_core;
using namespace scc;

#define EN_TRACING_STR "enableTracing"

configurable_tracer::configurable_tracer(std::string const&& name, bool enable_tx, bool enable_vcd, sc_core::sc_object* top)
: tracer(std::move(name), enable_tx ? ENABLE : NONE, enable_vcd ? ENABLE : NONE, top) {}

configurable_tracer::configurable_tracer(std::string const&& name, file_type type, bool enable_vcd, sc_core::sc_object* top)
: tracer(std::move(name), type, enable_vcd ? ENABLE : NONE, top) {}

configurable_tracer::configurable_tracer(std::string const&& name, file_type tx_type, file_type sig_type, sc_core::sc_object* top)
: tracer(std::move(name), tx_type, sig_type, top) {}

configurable_tracer::configurable_tracer(std::string const&& name, file_type type, sc_core::sc_trace_file* tf, sc_core::sc_object* top)
: tracer(std::move(name), type, tf, top) {}

scc::configurable_tracer::~configurable_tracer() {
    for(auto ptr : params)
        delete ptr;
}

// const std::unordered_set<std::string> traceable_kinds = {};
void configurable_tracer::descend(const sc_core::sc_object* obj, bool trace) {
    if(obj == this)
        return;
    const std::string kind = obj->kind();
    if((types_to_trace & trace_types::SIGNALS) == trace_types::SIGNALS && kind == "tlm_signal") {
        if(trace)
            obj->trace(trf);
        return;
    } else if(kind == "sc_vector") {
        if(trace)
            for(auto o : obj->get_child_objects())
                descend(o, trace);
        return;
    } else if(kind == "sc_module") {
        auto trace_enable = get_trace_enabled(obj, default_trace_enable_handle.get_cci_value().get<bool>());
        if(trace_enable)
            obj->trace(trf);
        for(auto o : obj->get_child_objects())
            descend(o, trace_enable);
    } else if(kind == "sc_variable") {
        if(trace && (types_to_trace & trace_types::VARIABLES) == trace_types::VARIABLES)
            obj->trace(trf);
    } else if(kind == "sc_signal" || kind == "sc_clock" || kind == "sc_buffer" || kind == "sc_signal_rv") {
        if(trace && (types_to_trace & trace_types::SIGNALS) == trace_types::SIGNALS)
            try_trace(trf, obj, types_to_trace);
    } else if(kind == "sc_in" || kind == "sc_out" || kind == "sc_inout") {
        if(trace && (types_to_trace & trace_types::PORTS) == trace_types::PORTS)
            try_trace(trf, obj, types_to_trace);
    } else if(const auto* tr = dynamic_cast<const scc::traceable*>(obj)) {
        if(tr->is_trace_enabled())
            obj->trace(trf);
        for(auto o : obj->get_child_objects())
            descend(o, tr->is_trace_enabled());
        //    } else if(trace && traceable_kinds.find(kind)!=traceable_kinds.end()) {
        //        try_trace(trf, obj, types_to_trace);
    }
}

auto scc::configurable_tracer::get_trace_enabled(const sc_core::sc_object* obj, bool fall_back) -> bool {
    auto* attr = obj->get_attribute(EN_TRACING_STR);
    if(attr != nullptr && dynamic_cast<const sc_core::sc_attribute<bool>*>(attr) != nullptr) {
        const auto* a = dynamic_cast<const sc_core::sc_attribute<bool>*>(attr);
        return a->value;
    } else {
        std::string hier_name{obj->name()};
        auto h = cci_broker.get_param_handle(hier_name.append("." EN_TRACING_STR));
        if(h.is_valid())
            return h.get_cci_value().get_bool();
    }
    return fall_back;
}

void configurable_tracer::augment_object_hierarchical(sc_core::sc_object* obj, bool trace_enable) {
    if(dynamic_cast<sc_core::sc_module*>(obj) != nullptr || dynamic_cast<scc::traceable*>(obj) != nullptr) {
        auto* attr = obj->get_attribute(EN_TRACING_STR);
        if(attr == nullptr || dynamic_cast<const sc_core::sc_attribute<bool>*>(attr) == nullptr) { // check if we have no sc_attribute
            std::string hier_name{obj->name()};
            if(hier_name.substr(0, 3) != "$$$") {
                hier_name += "." EN_TRACING_STR;
                auto h = cci_broker.get_param_handle(hier_name);
                if(!h.is_valid()) // we have no cci_param so create one
                    params.push_back(new cci::cci_param<bool>(hier_name, trace_enable, cci_broker, "", cci::CCI_ABSOLUTE_NAME,
                                                              cci_broker.get_originator()));
                else
                    h.set_cci_value(cci::cci_value{default_trace_enable_handle.get_cci_value().get<bool>()});
            }
        } else if(auto battr = dynamic_cast<sc_core::sc_attribute<bool>*>(attr)) {
            battr->value = default_trace_enable_handle.get_cci_value().get<bool>();
        }
        for(auto* o : obj->get_child_objects())
            augment_object_hierarchical(o, trace_enable);
    }
}

void configurable_tracer::end_of_elaboration() {
    add_control();
    tracer::end_of_elaboration();
}
