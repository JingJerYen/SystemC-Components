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

#ifndef SC_COMPONENTS_INCL_TLM_TLM_EXTENSIONS_H_
#define SC_COMPONENTS_INCL_TLM_TLM_EXTENSIONS_H_

#ifdef CWR_SYSTEMC
#include <tlm_h/tlm_generic_payload/tlm_gp.h>
#else
#include <tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h>
#endif
#include "tlm_gp_shared.h"

//! @brief SystemC TLM
namespace tlm {
//! @brief SCC TLM utilities
namespace scc {
/*!
 * \brief A unmanaged extension for TLM transactions.
 *
 * This class provides a basic implementation of an unmanaged  TLM extension.
 *
 * \tparam T The type of the extension.
 */
template <typename T> struct tlm_unmanaged_extension : public tlm_extension<T> {
    using type = T;

    tlm_extension_base* clone() const override { return new type(static_cast<const T&>(*this)); }

    void copy_from(tlm_extension_base const& other) override { this->operator=(static_cast<const type&>(other)); }

protected:
    tlm_unmanaged_extension(){};
};
/*!
 * \brief A managed extension for TLM transactions.
 *
 * This class provides a basic implementation of a managed TLM extension.
 *
 * \tparam T The type of the extension.
 */
template <typename T> struct tlm_managed_extension {

    using type = T;

    template <typename... Args> static type* allocate(Args&&... args) {
        auto* ret = new(pool::allocate()) type(std::forward<Args>(args)...);
        ret->is_pooled = true;
        return ret;
    }

    static type* allocate() {
        auto* ret = new(pool::allocate()) type();
        ret->is_pooled = true;
        return ret;
    }

    tlm_extension_base* clone() const {
        return allocate(); // Maybe static_cast<const T&>(*this)
    }

    void copy_from(tlm_extension_base const& other) { this->operator=(static_cast<const type&>(other)); }

    void free() {
        if(is_pooled) {
            this->~type();
            pool::dealllocate(this);
        } else {
            delete this;
        }
    }
    struct pool {
        static void* allocate() {
            if(free_list.size() > 0) {
                auto ret = free_list.back();
                free_list.pop_back();
                return ret;
            } else
                return calloc(1, sizeof(type));
        }

        static void dealllocate(void* p) { free_list.push_back(p); }

    private:
        static std::vector<void*> free_list;
    };

protected:
    tlm_managed_extension() = default;
    tlm_managed_extension(const tlm_managed_extension&) = default;
    tlm_managed_extension& operator=(const tlm_managed_extension& other) { return *this; }

private:
    bool is_pooled{false};
};
/*!
 * \brief Extension for data buffering.
 *
 * This class provides an extension for a data buffer. It can be used to populate a generic payloads data pointer with automatic memory
 * management as the extension is deleted once the payload is deleted or returned to the memory manager.
 *
 * \note This extension is unmanaged and must be freed manually.
 */
struct data_buffer : public tlm::tlm_extension<data_buffer> {

    tlm_extension_base* clone() const override {
        data_buffer* ext = new data_buffer;
        return ext;
    }
    void copy_from(tlm_extension_base const& from) override { buffer_ = static_cast<data_buffer const&>(from).buffer_; }

    void set_size(uint32_t size) { buffer_.resize(size); }
    unsigned char* get_buf_ptr() { return buffer_.data(); }

private:
    std::vector<unsigned char> buffer_;
};
/*!
 * \brief Extension for a TLM payload.
 *
 * This class provides an extension for a TLM payload. It holds a shared pointer to another TLM payload.
 *
 * \note This extension is unmanaged and must be freed automatically.
 */
struct tlm_payload_extension : public tlm::tlm_extension<tlm_payload_extension> {

    tlm_extension_base* clone() const override {
        tlm_payload_extension* ext = new tlm_payload_extension(*this);
        return ext;
    }

    void copy_from(tlm_extension_base const& from) override { gp = static_cast<tlm_payload_extension const&>(from).gp; }

    tlm::scc::tlm_gp_shared_ptr gp;
};

} // namespace scc
} // namespace tlm
#endif /* SC_COMPONENTS_INCL_TLM_TLM_EXTENSIONS_H_ */
