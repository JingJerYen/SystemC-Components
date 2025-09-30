/*******************************************************************************
 * Copyright 2021-2022 MINRES Technologies GmbH
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

#ifndef _BUS_AXI_PIN_ACE_LITE_TARGET_H_
#define _BUS_AXI_PIN_ACE_LITE_TARGET_H_

#include <axi/axi_tlm.h>
#include <axi/fsm/base.h>
#include <axi/fsm/protocol_fsm.h>
#include <axi/signal_if.h>
#include <systemc>
#include <tlm/scc/tlm_mm.h>
#include <util/ities.h>

//! TLM2.0 components modeling AXI
namespace axi {
//! pin level adapters
namespace pin {

using namespace axi::fsm;

template <typename CFG>
struct ace_lite_target : public sc_core::sc_module,
                         public aw_ace_lite<CFG, typename CFG::slave_types>,
                         public wdata_ace_lite<CFG, typename CFG::slave_types>,
                         public b_ace_lite<CFG, typename CFG::slave_types>,
                         public ar_ace_lite<CFG, typename CFG::slave_types>,
                         public rresp_ace_lite<CFG, typename CFG::slave_types>,
                         protected axi::fsm::base,
                         public axi::axi_bw_transport_if<axi::axi_protocol_types> {
    SC_HAS_PROCESS(ace_lite_target);

    using payload_type = axi::axi_protocol_types::tlm_payload_type;
    using phase_type = axi::axi_protocol_types::tlm_phase_type;

    sc_core::sc_in<bool> clk_i{"clk_i"};

    axi::axi_initiator_socket<CFG::BUSWIDTH> isckt{"isckt"};

    ace_lite_target(sc_core::sc_module_name const& nm)
    : sc_core::sc_module(nm)
    // ace_lite has no ack, therefore coherent= false
    , base(CFG::BUSWIDTH, false) {
        instance_name = name();
        isckt.bind(*this);
        SC_METHOD(clk_delay);
        sensitive << clk_i.pos();
        dont_initialize();
        SC_THREAD(ar_t);
        SC_THREAD(rresp_t);
        SC_THREAD(aw_t);
        SC_THREAD(wdata_t);
        SC_THREAD(bresp_t);
    }

private:
    tlm::tlm_sync_enum nb_transport_bw(payload_type& trans, phase_type& phase, sc_core::sc_time& t) override;

    void invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) override;

    void end_of_elaboration() override { clk_if = dynamic_cast<sc_core::sc_clock*>(clk_i.get_interface()); }

    axi::fsm::fsm_handle* create_fsm_handle() override { return new fsm_handle(); }

    void setup_callbacks(axi::fsm::fsm_handle*) override;

    void clk_delay() {
#ifdef DELTA_SYNC
        if(sc_core::sc_delta_count_at_current_time() < 5) {
            clk_self.notify(sc_core::SC_ZERO_TIME);
            next_trigger(clk_self);
        } else
            clk_delayed.notify(sc_core::SC_ZERO_TIME /*clk_if ? clk_if->period() - 1_ps : 1_ps*/);
#else
        clk_delayed.notify(axi::CLK_DELAY);
#endif
    }
    void ar_t();
    void rresp_t();
    void aw_t();
    void wdata_t();
    void bresp_t();
    static typename CFG::data_t get_read_data_for_beat(fsm::fsm_handle* fsm_hndl);
    struct aw_data {
        unsigned id;
        uint64_t addr;
        unsigned prot;
        unsigned size;
        unsigned cache;
        unsigned burst;
        unsigned qos;
        unsigned region;
        unsigned len;
        unsigned domain;
        unsigned snoop;
        unsigned bar;
        unsigned unique;
        unsigned stashnid;
        unsigned stashlpid;
        bool lock;
        uint64_t user;
    };

    std::deque<axi::fsm::fsm_handle*> snp_resp_queue;

    sc_core::sc_clock* clk_if{nullptr};
    sc_core::sc_event clk_delayed, clk_self, ar_end_req_evt, wdata_end_req_evt;
    std::array<fsm_handle*, 3> active_req_beat{nullptr, nullptr, nullptr};
    std::array<fsm_handle*, 3> active_req{nullptr, nullptr, nullptr};
    std::array<fsm_handle*, 3> active_resp_beat{nullptr, nullptr, nullptr};
    scc::peq<aw_data> aw_que;
    scc::peq<std::tuple<uint8_t, fsm_handle*>> rresp_vl;
    scc::peq<std::tuple<uint8_t, fsm_handle*>> wresp_vl;
};
} // namespace pin
} // namespace axi

template <typename CFG>
inline tlm::tlm_sync_enum axi::pin::ace_lite_target<CFG>::nb_transport_bw(payload_type& trans, phase_type& phase, sc_core::sc_time& t) {
    auto ret = tlm::TLM_ACCEPTED;
    SCCTRACE(SCMOD) << "nb_transport_bw with " << phase << " with delay= " << t << " of trans " << trans;
    if(phase == END_PARTIAL_REQ || phase == tlm::END_REQ) { // read/write
        schedule(phase == tlm::END_REQ ? EndReqE : EndPartReqE, &trans, t, false);
    } else if(phase == axi::BEGIN_PARTIAL_RESP || phase == tlm::BEGIN_RESP) { // read/write response
        schedule(phase == tlm::BEGIN_RESP ? BegRespE : BegPartRespE, &trans, t, false);
    } else
        SCCFATAL(SCMOD) << "Illegal phase received: " << phase;
    return ret;
}

template <typename CFG>
inline void axi::pin::ace_lite_target<CFG>::invalidate_direct_mem_ptr(sc_dt::uint64 start_range, sc_dt::uint64 end_range) {}

template <typename CFG> typename CFG::data_t axi::pin::ace_lite_target<CFG>::get_read_data_for_beat(fsm_handle* fsm_hndl) {
    auto beat_count = fsm_hndl->beat_count;
    auto size = axi::get_burst_size(*fsm_hndl->trans);
    auto byte_offset = beat_count * size;
    auto offset = (fsm_hndl->trans->get_address() + byte_offset) & (CFG::BUSWIDTH / 8 - 1);
    typename CFG::data_t data{0};
    if(offset && (size + offset) > (CFG::BUSWIDTH / 8)) { // un-aligned multi-beat access
        if(beat_count == 0) {
            auto dptr = fsm_hndl->trans->get_data_ptr();
            for(size_t i = offset; i < size; ++i, ++dptr) {
                auto bit_offs = i * 8;
                data(bit_offs + 7, bit_offs) = *dptr;
            }
        } else {
            auto beat_start_idx = byte_offset - offset;
            auto data_len = fsm_hndl->trans->get_data_length();
            auto dptr = fsm_hndl->trans->get_data_ptr() + beat_start_idx;
            for(size_t i = 0; i < size && (beat_start_idx + i) < data_len; ++i, ++dptr) {
                auto bit_offs = i * 8;
                data(bit_offs + 7, bit_offs) = *dptr;
            }
        }
    } else { // aligned or single beat access
        auto dptr = fsm_hndl->trans->get_data_ptr() + byte_offset;
        for(size_t i = 0; i < size; ++i, ++dptr) {
            auto bit_offs = (offset + i) * 8;
            data(bit_offs + 7, bit_offs) = *dptr;
        }
    }
    return data;
}

template <typename CFG> inline void axi::pin::ace_lite_target<CFG>::setup_callbacks(fsm_handle* fsm_hndl) {
    fsm_hndl->fsm->cb[RequestPhaseBeg] = [this, fsm_hndl]() -> void { fsm_hndl->beat_count = 0; };
    fsm_hndl->fsm->cb[BegPartReqE] = [this, fsm_hndl]() -> void {
        sc_assert(fsm_hndl->trans->get_command() == tlm::TLM_WRITE_COMMAND);
        tlm::tlm_phase phase = axi::BEGIN_PARTIAL_REQ;
        sc_core::sc_time t(sc_core::SC_ZERO_TIME);
        auto ret = isckt->nb_transport_fw(*fsm_hndl->trans, phase, t);
        if(ret == tlm::TLM_UPDATED) {
            schedule(EndPartReqE, fsm_hndl->trans, t, true);
        }
    };
    fsm_hndl->fsm->cb[EndPartReqE] = [this, fsm_hndl]() -> void {
        wdata_end_req_evt.notify();
        active_req_beat[tlm::TLM_WRITE_COMMAND] = nullptr;
        fsm_hndl->beat_count++;
    };
    fsm_hndl->fsm->cb[BegReqE] = [this, fsm_hndl]() -> void {
        tlm::tlm_phase phase = tlm::BEGIN_REQ;
        sc_core::sc_time t(sc_core::SC_ZERO_TIME);
        auto ret = isckt->nb_transport_fw(*fsm_hndl->trans, phase, t);
        if(ret == tlm::TLM_UPDATED) {
            schedule(EndReqE, fsm_hndl->trans, t, true);
        }
    };
    fsm_hndl->fsm->cb[EndReqE] = [this, fsm_hndl]() -> void {
        switch(fsm_hndl->trans->get_command()) {
        case tlm::TLM_READ_COMMAND:
            ar_end_req_evt.notify();
            active_req_beat[tlm::TLM_READ_COMMAND] = nullptr;
            break;
        case tlm::TLM_WRITE_COMMAND:
            wdata_end_req_evt.notify();
            active_req_beat[tlm::TLM_WRITE_COMMAND] = nullptr;
            fsm_hndl->beat_count++;
            break;
        default:
            break;
        }
    };
    fsm_hndl->fsm->cb[BegPartRespE] = [this, fsm_hndl]() -> void {
        assert(fsm_hndl->trans->is_read());
        active_resp_beat[tlm::TLM_READ_COMMAND] = fsm_hndl;
        rresp_vl.notify({1, fsm_hndl});
    };
    fsm_hndl->fsm->cb[EndPartRespE] = [this, fsm_hndl]() -> void {
        // scheduling the response
        assert(fsm_hndl->trans->is_read());
        tlm::tlm_phase phase = axi::END_PARTIAL_RESP;
        sc_core::sc_time t(clk_if ? ::scc::time_to_next_posedge(clk_if) - 1_ps : sc_core::SC_ZERO_TIME);
        auto ret = isckt->nb_transport_fw(*fsm_hndl->trans, phase, t);
        active_resp_beat[tlm::TLM_READ_COMMAND] = nullptr;
        fsm_hndl->beat_count++;
    };
    fsm_hndl->fsm->cb[BegRespE] = [this, fsm_hndl]() -> void {
        SCCTRACE(SCMOD) << "processing event BegRespE for trans " << *fsm_hndl->trans;
        auto size = axi::get_burst_size(*fsm_hndl->trans);
        active_resp_beat[fsm_hndl->trans->get_command()] = fsm_hndl;
        switch(fsm_hndl->trans->get_command()) {
        case tlm::TLM_READ_COMMAND:
            rresp_vl.notify({3, fsm_hndl});
            break;
        case tlm::TLM_WRITE_COMMAND:
            wresp_vl.notify({3, fsm_hndl});
            break;
        default:
            break;
        }
    };
    fsm_hndl->fsm->cb[EndRespE] = [this, fsm_hndl]() -> void {
        // scheduling the response
        tlm::tlm_phase phase = tlm::END_RESP;
        sc_core::sc_time t(clk_if ? ::scc::time_to_next_posedge(clk_if) - 1_ps : sc_core::SC_ZERO_TIME);
        auto ret = isckt->nb_transport_fw(*fsm_hndl->trans, phase, t);
        SCCTRACE(SCMOD) << "EndResp of setup_cb with coherent = " << coherent;
        fsm_hndl->finish.notify();
        active_resp_beat[fsm_hndl->trans->get_command()] = nullptr;
    };
}

template <typename CFG> inline void axi::pin::ace_lite_target<CFG>::ar_t() {
    this->ar_ready.write(false);
    wait(sc_core::SC_ZERO_TIME);
    auto arid = 0U;
    auto arlen = 0U;
    auto arsize = util::ilog2(CFG::BUSWIDTH / 8);
    auto data_len = (1 << arsize) * (arlen + 1);
    while(true) {
        wait(clk_delayed);
        while(!this->ar_valid.read()) {
            wait(this->ar_valid.posedge_event());
            wait(CLK_DELAY); // verilator might create spurious events
        }
        SCCTRACE(SCMOD) << "ARVALID detected for 0x" << std::hex << this->ar_addr.read();
        arid = this->ar_id->read().to_uint();
        arlen = this->ar_len->read().to_uint();
        arsize = this->ar_size->read().to_uint();
        data_len = (1 << arsize) * (arlen + 1);
        auto gp = tlm::scc::tlm_mm<>::get().allocate<axi::ace_extension>(data_len);
        gp->set_address(this->ar_addr.read());
        gp->set_command(tlm::TLM_READ_COMMAND);
        gp->set_streaming_width(data_len);
        axi::ace_extension* ext;
        gp->get_extension(ext);
        ext->set_id(arid);
        ext->set_length(arlen);
        if(this->ar_lock->read())
            ext->set_exclusive(true);
        ext->set_size(arsize);
        ext->set_burst(axi::into<axi::burst_e>(this->ar_burst->read()));
        ext->set_cache(this->ar_cache->read());
        ext->set_prot(this->ar_prot->read());
        ext->set_qos(this->ar_qos->read());
        ext->set_region(this->ar_region->read());
        ext->set_domain(axi::into<axi::domain_e>(this->ar_domain->read())); // ace extension
        ext->set_snoop(axi::into<axi::snoop_e>(this->ar_snoop->read()));
        ext->set_barrier(axi::into<axi::bar_e>(this->ar_bar->read()));

        active_req_beat[tlm::TLM_READ_COMMAND] = find_or_create(gp);
        react(axi::fsm::protocol_time_point_e::BegReqE, active_req_beat[tlm::TLM_READ_COMMAND]);
        wait(ar_end_req_evt);
        this->ar_ready.write(true);
        wait(clk_i.posedge_event());
        this->ar_ready.write(false);
    }
}

template <typename CFG> inline void axi::pin::ace_lite_target<CFG>::rresp_t() {
    this->r_valid.write(false);
    wait(sc_core::SC_ZERO_TIME);
    fsm_handle* fsm_hndl;
    uint8_t val;
    while(true) {
        // rresp_vl notified in BEGIN_PARTIAL_REQ ( val=1 ??)or in BEG_RESP(val=3??)
        std::tie(val, fsm_hndl) = rresp_vl.get();
        SCCTRACE(SCMOD) << __FUNCTION__ << " val = " << (uint16_t)val << " beat count = " << fsm_hndl->beat_count;
        SCCTRACE(SCMOD) << __FUNCTION__ << " got read response beat of trans " << *fsm_hndl->trans;
        auto ext = fsm_hndl->trans->get_extension<axi::ace_extension>();
        this->r_data.write(get_read_data_for_beat(fsm_hndl));
        this->r_resp.write(ext->get_cresp());
        this->r_valid.write(val & 0x1);
        this->r_id->write(ext->get_id());
        this->r_last->write(val & 0x2);
        do {
            wait(this->r_ready.posedge_event() | clk_delayed);
            if(this->r_ready.read()) {
                auto evt = (val & 0x2) ? axi::fsm::protocol_time_point_e::EndRespE : axi::fsm::protocol_time_point_e::EndPartRespE;
                react(evt, active_resp_beat[tlm::TLM_READ_COMMAND]);
            }
        } while(!this->r_ready.read());
        SCCTRACE(SCMOD) << "finished read response beat of trans [" << fsm_hndl->trans << "]";
        wait(clk_i.posedge_event());
        this->r_valid.write(false);
        this->r_last->write(false);
    }
}

template <typename CFG> inline void axi::pin::ace_lite_target<CFG>::aw_t() {
    this->aw_ready.write(false);
    wait(sc_core::SC_ZERO_TIME);
    const auto awsize = util::ilog2(CFG::BUSWIDTH / 8);
    while(true) {
        wait(clk_delayed);
        while(!this->aw_valid.read()) {
            wait(this->aw_valid.posedge_event());
            wait(CLK_DELAY); // verilator might create spurious events
        }
        SCCTRACE(SCMOD) << "AWVALID detected for 0x" << std::hex << this->aw_addr.read();
        // clang-format off
            aw_data awd = { this->aw_id->read().to_uint(),
                    this->aw_addr.read().to_uint64(),
                    this->aw_prot.read().to_uint(),
                    this->aw_size->read().to_uint(),
                    this->aw_cache->read().to_uint(),
                    this->aw_burst->read().to_uint(),
                    this->aw_qos->read().to_uint(),
                    this->aw_region->read().to_uint(),
                    this->aw_len->read().to_uint(),
                    this->aw_domain->read().to_uint(),
                    this->aw_snoop->read().to_uint(),
                    this->aw_bar->read().to_uint(),
                    this->aw_unique->read(),
                    this->aw_stashniden->read() ? 0U : this->aw_stashnid->read().to_uint(),
                    this->aw_stashlpiden->read()? 0U : this->aw_stashlpid->read().to_uint(),
                    this->aw_lock->read() ? true : false,
                    0};
        // clang-format on
        aw_que.notify(awd);
        this->aw_ready.write(true);
        wait(clk_i.posedge_event());
        this->aw_ready.write(false);
    }
}

template <typename CFG> inline void axi::pin::ace_lite_target<CFG>::wdata_t() {
    this->w_ready.write(false);
    wait(sc_core::SC_ZERO_TIME);
    while(true) {
        wait(this->w_valid.read() ? clk_delayed : this->w_valid.posedge_event());
        if(this->w_valid.read()) {
            if(!active_req[tlm::TLM_WRITE_COMMAND]) {
                if(!aw_que.has_next())
                    wait(aw_que.event());
                auto awd = aw_que.get();
                auto data_len = (1 << awd.size) * (awd.len + 1);
                auto gp = tlm::scc::tlm_mm<>::get().allocate<axi::ace_extension>(data_len, true);
                gp->set_address(awd.addr);
                gp->set_command(tlm::TLM_WRITE_COMMAND);
                axi::ace_extension* ext;
                gp->get_extension(ext);
                ext->set_id(awd.id);
                ext->set_length(awd.len);
                ext->set_size(awd.size);
                ext->set_burst(axi::into<axi::burst_e>(awd.burst));
                ext->set_prot(awd.prot);
                ext->set_qos(awd.qos);
                ext->set_cache(awd.cache);
                ext->set_region(awd.region);
                ext->set_snoop(axi::into<axi::snoop_e>(awd.snoop));
                ext->set_barrier(axi::into<axi::bar_e>(awd.bar));
                // ace_lite does not have aw_unique   ext->set_unique(awd.unique);
                ext->set_stash_nid(awd.stashnid);
                ext->set_stash_lpid(awd.stashlpid);
                ext->set_exclusive(awd.lock);
                if(CFG::USERWIDTH)
                    ext->set_user(axi::common::id_type::CTRL, awd.user);
                active_req_beat[tlm::TLM_WRITE_COMMAND] = find_or_create(gp);
                active_req[tlm::TLM_WRITE_COMMAND] = active_req_beat[tlm::TLM_WRITE_COMMAND];
                active_req_beat[tlm::TLM_WRITE_COMMAND]->aux.i32.i0 = 0;
            }
            auto* fsm_hndl = active_req[tlm::TLM_WRITE_COMMAND];
            SCCTRACE(SCMOD) << "WDATA detected for 0x" << std::hex << this->ar_addr.read();
            auto& gp = fsm_hndl->trans;
            auto data = this->w_data.read();
            auto strb = this->w_strb.read();
            auto last = this->w_last->read();
            auto beat_count = fsm_hndl->beat_count;
            auto size = axi::get_burst_size(*fsm_hndl->trans);
            auto byte_offset = beat_count * size;
            auto offset = (fsm_hndl->trans->get_address() + byte_offset) & (CFG::BUSWIDTH / 8 - 1);
            if(offset && (size + offset) > (CFG::BUSWIDTH / 8)) { // un-aligned multi-beat access
                if(beat_count == 0) {
                    auto dptr = fsm_hndl->trans->get_data_ptr();
                    auto beptr = fsm_hndl->trans->get_byte_enable_ptr();
                    for(size_t i = offset; i < size; ++i, ++dptr, ++beptr) {
                        auto bit_offs = i * 8;
                        *dptr = data(bit_offs + 7, bit_offs).to_uint();
                        *beptr = strb[i] ? 0xff : 0;
                        fsm_hndl->aux.i32.i0 += strb[i] ? 1 : 0;
                    }
                } else {
                    auto beat_start_idx = byte_offset - offset;
                    auto data_len = fsm_hndl->trans->get_data_length();
                    auto dptr = fsm_hndl->trans->get_data_ptr() + beat_start_idx;
                    auto beptr = fsm_hndl->trans->get_byte_enable_ptr() + beat_start_idx;
                    for(size_t i = 0; i < size && (beat_start_idx + i) < data_len; ++i, ++dptr, ++beptr) {
                        auto bit_offs = i * 8;
                        *dptr = data(bit_offs + 7, bit_offs).to_uint();
                        *beptr = strb[i] ? 0xff : 0;
                        fsm_hndl->aux.i32.i0 += strb[i] ? 1 : 0;
                    }
                }
            } else { // aligned or single beat access
                auto dptr = fsm_hndl->trans->get_data_ptr() + byte_offset;
                auto beptr = fsm_hndl->trans->get_byte_enable_ptr() + byte_offset;
                for(size_t i = 0; i < size; ++i, ++dptr, ++beptr) {
                    auto bit_offs = (offset + i) * 8;
                    *dptr = data(bit_offs + 7, bit_offs).to_uint();
                    *beptr = strb[offset + i] ? 0xff : 0;
                    fsm_hndl->aux.i32.i0 += strb[offset + i] ? 1 : 0;
                }
            }
            // TODO: assuming consecutive write (not scattered)
            auto strobe = strb.to_uint();
            if(last) {
                auto act_data_len = fsm_hndl->aux.i32.i0;
                gp->set_data_length(act_data_len);
                gp->set_byte_enable_length(act_data_len);
                gp->set_streaming_width(act_data_len);
            }
            auto tp = this->w_last->read() ? axi::fsm::protocol_time_point_e::BegReqE : axi::fsm::protocol_time_point_e::BegPartReqE;
            react(tp, fsm_hndl);
            // notified in EndPartReqE/EndReq
            wait(wdata_end_req_evt);
            this->w_ready.write(true);
            wait(clk_i.posedge_event());
            this->w_ready.write(false);
            if(last)
                active_req[tlm::TLM_WRITE_COMMAND] = nullptr;
        }
    }
}

template <typename CFG> inline void axi::pin::ace_lite_target<CFG>::bresp_t() {
    this->b_valid.write(false);
    wait(sc_core::SC_ZERO_TIME);
    fsm_handle* fsm_hndl;
    uint8_t val;
    while(true) {
        std::tie(val, fsm_hndl) = wresp_vl.get();
        SCCTRACE(SCMOD) << "got write response of trans " << *fsm_hndl->trans;
        auto ext = fsm_hndl->trans->get_extension<axi::ace_extension>();
        this->b_resp.write(axi::to_int(ext->get_resp()));
        this->b_valid.write(true);
        this->b_id->write(ext->get_id());
        SCCTRACE(SCMOD) << "got write response";
        do {
            wait(this->b_ready.posedge_event() | clk_delayed);
            if(this->b_ready.read()) {
                react(axi::fsm::protocol_time_point_e::EndRespE, active_resp_beat[tlm::TLM_WRITE_COMMAND]);
            }
        } while(!this->b_ready.read());
        SCCTRACE(SCMOD) << "finished write response of trans [" << fsm_hndl->trans << "]";
        wait(clk_i.posedge_event());
        this->b_valid.write(false);
    }
}

#endif /* _BUS_AXI_PIN_ace_lite_TARGET_H_ */
