/*******************************************************************************
 * Copyright 2019-2024 MINRES Technologies GmbH
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

#include "tlm/scc/scv/tlm_extension_recording_registry.h"
#include <array>
#include <tilelink/tl_tlm.h>

namespace tilelink {
namespace {
const std::array<std::string, 3> cmd_str{"R", "W", "I"};
}

template <> const char* to_char<opcode_e>(opcode_e v) {
    switch(v) {
    case opcode_e::Get:
        return "Get";
    case opcode_e::AccessAckData:
        return "AccessAckData";
    case opcode_e::PutFullData:
        return "PutFullData";
    case opcode_e::PutPartialData:
        return "PutPartialData";
    case opcode_e::AccessAck:
        return "AccessAck";
    case opcode_e::ArithmeticData:
        return "ArithmeticData";
    case opcode_e::LogicalData:
        return "LogicalData";
    case opcode_e::Intent:
        return "Intent";
    case opcode_e::HintAck:
        return "HintAck";
    case opcode_e::AcquireBlock:
        return "AcquireBlock";
    case opcode_e::AcquirePerm:
        return "AcquirePerm";
    case opcode_e::Grant:
        return "Grant";
    case opcode_e::GrantData:
        return "GrantData";
    case opcode_e::GrantAck:
        return "GrantAck";
    case opcode_e::ProbeBlock:
        return "ProbeBlock";
    case opcode_e::ProbePerm:
        return "ProbePerm";
    case opcode_e::ProbeAck:
        return "ProbeAck";
    case opcode_e::ProbeAckData:
        return "ProbeAckData";
    case opcode_e::Release:
        return "Release";
    case opcode_e::ReleaseData:
        return "ReleaseData";
    case opcode_e::ReleaseAck:
        return "ReleaseAck";
    default:
        return "UNKNOWN";
    }
}

template <> const char* to_char<param_e>(param_e v) {
    switch(v) {
    case param_e::CAP_2T:
        return "Cap:2T (0)";
    case param_e::CAP_2B:
        return "Cap:2B (1)";
    case param_e::CAP_2N:
        return "Cap:2N (2)";
    case param_e::GROW_N2B:
        return "Grow:N2B (0)";
    case param_e::GROW_N2T:
        return "Grow:N2T (1)";
    case param_e::GROW_B2T:
        return "Grow:B2T (2)";
    case param_e::PRUNE_T2B:
        return "Prune:T2B (0)";
    case param_e::PRUNE_T2N:
        return "Prune:T2N (1)";
    case param_e::PRUNE_B2N:
        return "Prune:B2N (2)";
    case param_e::REP_T2T:
        return "Report:T2T (0)";
    case param_e::REP_B2B:
        return "Report:B2B (1)";
    case param_e::REP_N2N:
        return "Report:N2N (2)";
    default:
        return "UNKNOWN";
    }
}

std::ostream& operator<<(std::ostream& os, const tlm::tlm_generic_payload& t) {
    os << "CMD:" << cmd_str[t.get_command()] << ", "
       << "ADDR:0x" << std::hex << t.get_address() << ", TXLEN:0x" << t.get_data_length();
    if(auto e = t.get_extension<tilelink::tilelink_extension>()) {
        os << ", "
           << "OPC:0x" << std::hex << static_cast<unsigned>(e->get_opcode()) << "PARAM:" << e->get_param();
    }
    os << " [ptr:" << &t << "]";
    return os;
}

using namespace tlm::scc::scv;

class tlc_ext_recording : public tlm_extensions_recording_if<tl_protocol_types> {

    void recordBeginTx(SCVNS scv_tr_handle& handle, tl_protocol_types::tlm_payload_type& trans) override {
        auto ext = trans.get_extension<tilelink_extension>();
        if(ext) {
            handle.record_attribute("trans.tl.opcode", std::string(to_char(ext->get_opcode())));
            handle.record_attribute("trans.tl.param", std::string(to_char(ext->get_param())));
            handle.record_attribute("trans.tl.source", ext->get_source());
            handle.record_attribute("trans.tl.sink", ext->get_sink());
            handle.record_attribute("trans.tl.corrupt", ext->is_corrupt());
            handle.record_attribute("trans.tl.denied", ext->is_denied());
        }
    }

    void recordEndTx(SCVNS scv_tr_handle& handle, tl_protocol_types::tlm_payload_type& trans) override {}
};

namespace scv {
using namespace tlm::scc::scv;
#if defined(__GNUG__)
__attribute__((constructor))
#endif
bool register_extensions() {
    tilelink::tilelink_extension ext; // NOLINT
    tlm::scc::scv::tlm_extension_recording_registry<tilelink::tl_protocol_types>::inst().register_ext_rec(
        ext.ID, new tlc_ext_recording()); // NOLINT
    return true;                          // NOLINT
}
bool registered = register_extensions();
} // namespace scv
} // namespace tilelink
