/*******************************************************************************
 * Copyright 2021 MINRES Technologies GmbH
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

#include "axi_target.h"

#include <scc/mt19937_rng.h>
#include <scc/report.h>
#include <tlm/scc/tlm_gp_shared.h>

using namespace axi;

inline unsigned get_cci_randomized_value(cci::cci_param<int> const& p) {
    if(p.get_value() < 0)
        return scc::MT19937::uniform(0, -p.get_value());
    return p.get_value();
}

axi_target_base::axi_target_base(const sc_core::sc_module_name& nm, axi::pe::axi_target_pe& pe)
: sc_module(nm)
, pe(pe) {
#if SYSTEMC_VERSION < 20250221
    SC_HAS_PROCESS(axi_target_base);
#endif
    SC_THREAD(trans_queue);
}

unsigned axi_target_base::access(tlm::tlm_generic_payload& trans) {
    peq.notify(&trans);
    return std::numeric_limits<unsigned>::max();
}

void axi_target_base::trans_queue() {
    auto delay = sc_core::SC_ZERO_TIME;
    while(true) {
        tlm::scc::tlm_gp_shared_ptr trans = peq.get();
        isck->b_transport(*trans, delay);
        pe.operation_resp(*trans,
                          trans->is_write() ? get_cci_randomized_value(pe.wr_resp_delay) : get_cci_randomized_value(pe.rd_resp_delay));
    }
}
