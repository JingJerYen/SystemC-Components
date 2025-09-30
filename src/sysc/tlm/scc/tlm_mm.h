/*******************************************************************************
 * Copyright 2020 MINRES Technologies GmbH
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

#ifndef _TLM_TLM_MM_H_
#define _TLM_TLM_MM_H_

#include <tlm>
#include <type_traits>
#include <util/pool_allocator.h>

// #if defined(MSVC)
#define ATTR_UNUSED
// #else
// #define ATTR_UNUSED __attribute__((unused))
// #endif
//! @brief SystemC TLM
namespace tlm {
//! @brief SCC TLM utilities
namespace scc {
/*!
 * \brief Memory management for TLM generic payload data.
 *
 * This extension is used for memory handling for tlm generic payload data. It is
 * designed to be used as an extension for the tlm_payload_base class.
 *
 * \note This extension is used internally by the tlm_gp_shared class, which is
 */
struct tlm_gp_mm : public tlm_extension<tlm_gp_mm> {
    virtual ~tlm_gp_mm() {}

    void copy_from(ATTR_UNUSED tlm_extension_base const& from) override {
        // No need to copy, because this extension is used for memory handling for tlm generic payload data.
        // The copy operation of the data is therefore handled by the tlm functions deep_copy_from and update_original_from.
    }

    tlm_gp_mm* clone() const override { return tlm_gp_mm::create(data_size); }

    size_t const data_size;
    uint8_t* const data_ptr;
    uint8_t* const be_ptr;

    static tlm_gp_mm* create(size_t sz, bool be = false);

    template <typename TYPES = tlm_base_protocol_types>
    static typename TYPES::tlm_payload_type* add_data_ptr(size_t sz, typename TYPES::tlm_payload_type& gp, bool be = false) {
        return add_data_ptr(sz, &gp, be);
    }
    template <typename TYPES = tlm_base_protocol_types>
    static typename TYPES::tlm_payload_type* add_data_ptr(size_t sz, typename TYPES::tlm_payload_type* gp, bool be = false);

protected:
    tlm_gp_mm(size_t sz, uint8_t* data_ptr, uint8_t* be_ptr)
    : data_size(sz)
    , data_ptr(data_ptr)
    , be_ptr(be_ptr) {}
};
/*!
 * \brief Creates a new tlm_gp_mm object with fixed size.
 *
 * \param sz The size of the data to be handled.
 * \param be If true, the data is byte-swapped.
 * \return A new tlm_gp_mm object.
 */
template <size_t SZ, bool BE = false> struct tlm_gp_mm_t : public tlm_gp_mm {

    friend tlm_gp_mm;
    /*!
     * virtual destructor
     */
    virtual ~tlm_gp_mm_t() {}
    /*!
     * frees the extension by returning it to the memory manager
     */
    void free() override { util::pool_allocator<sizeof(tlm_gp_mm_t<SZ, BE>)>::get().free(this); }

protected:
    tlm_gp_mm_t(size_t sz)
    : tlm_gp_mm(sz, data, BE ? be : nullptr) {}
    uint8_t data[SZ];
    uint8_t be[BE ? SZ : 0];
};

struct tlm_gp_mm_v : public tlm_gp_mm {

    friend tlm_gp_mm;

    virtual ~tlm_gp_mm_v() { delete data_ptr; }

protected:
    tlm_gp_mm_v(size_t sz)
    : tlm_gp_mm(sz, new uint8_t[sz], nullptr) {}
};
/*!
 * \brief Creates a new tlm_gp_mm object with a dynamically allocated buffer.
 *
 * \param sz The size of the data to be handled.
 * \param be If true, the extension will also provide a byte-enable array.
 * \return A new tlm_gp_mm object.
 *  @ingroup tlm_extensions
 */
inline tlm_gp_mm* tlm::scc::tlm_gp_mm::create(size_t sz, bool be) {
    if(sz > 4096) {
        return new tlm_gp_mm_v(sz);
    } else if(sz > 1024) {
        if(be) {
            return new(util::pool_allocator<sizeof(tlm_gp_mm_t<4096, true>)>::get().allocate()) tlm_gp_mm_t<4096, true>(sz);
        } else {
            return new(util::pool_allocator<sizeof(tlm_gp_mm_t<4096, false>)>::get().allocate()) tlm_gp_mm_t<4096, false>(sz);
        }
    } else if(sz > 256) {
        if(be) {
            return new(util::pool_allocator<sizeof(tlm_gp_mm_t<1024, true>)>::get().allocate()) tlm_gp_mm_t<1024, true>(sz);
        } else {
            return new(util::pool_allocator<sizeof(tlm_gp_mm_t<1024, false>)>::get().allocate()) tlm_gp_mm_t<1024, false>(sz);
        }
    } else if(sz > 64) {
        if(be) {
            return new(util::pool_allocator<sizeof(tlm_gp_mm_t<256, true>)>::get().allocate()) tlm_gp_mm_t<256, true>(sz);
        } else {
            return new(util::pool_allocator<sizeof(tlm_gp_mm_t<256, false>)>::get().allocate()) tlm_gp_mm_t<256, false>(sz);
        }
    } else if(sz > 16) {
        if(be) {
            return new(util::pool_allocator<sizeof(tlm_gp_mm_t<64, true>)>::get().allocate()) tlm_gp_mm_t<64, true>(sz);
        } else {
            return new(util::pool_allocator<sizeof(tlm_gp_mm_t<64, false>)>::get().allocate()) tlm_gp_mm_t<64, false>(sz);
        }
    } else if(be) {
        return new(util::pool_allocator<sizeof(tlm_gp_mm_t<16, true>)>::get().allocate()) tlm_gp_mm_t<16, true>(sz);
    } else {
        return new(util::pool_allocator<sizeof(tlm_gp_mm_t<16, false>)>::get().allocate()) tlm_gp_mm_t<16, false>(sz);
    }
}
/*!
 * \brief Adds a data pointer to a tlm_gp_mm object.
 *
 * \param sz The size of the data.
 * \param gp The tlm_generic_payload object to add the data pointer to.
 * \param be If true, the byte-enable pointer will be populated.
 * \return The tlm_generic_payload object with the data pointer added.
 */
template <typename TYPES>
inline typename TYPES::tlm_payload_type* tlm::scc::tlm_gp_mm::add_data_ptr(size_t sz, typename TYPES::tlm_payload_type* gp, bool be) {
    auto* ext = create(sz, be);
    gp->set_auto_extension(ext);
    gp->set_data_ptr(ext->data_ptr);
    gp->set_data_length(sz);
    gp->set_byte_enable_ptr(ext->be_ptr);
    if(be)
        gp->set_byte_enable_length(sz);
    return gp;
}
/*!
+ Class tlm_ext_mm prides a memory manager for TLM extension
*/
template <typename EXT> struct tlm_ext_mm : public EXT {

    friend tlm_gp_mm;

    ~tlm_ext_mm() {}

    void free() override { util::pool_allocator<sizeof(tlm_ext_mm<EXT>)>::get().free(this); }

    EXT* clone() const override { return create(*this); }

    template <typename... Args> static EXT* create(Args... args) {
        return new(util::pool_allocator<sizeof(tlm_ext_mm<EXT>)>::get().allocate()) tlm_ext_mm<EXT>(args...);
    }

protected:
    template <typename... Args>
    tlm_ext_mm(Args... args)
    : EXT(args...) {}
};
template <typename TYPES> struct tlm_mm_traits { using mm_if_type = tlm::tlm_mm_interface; };
/**
 * @class tlm_mm
 * @brief a tlm memory manager
 *
 * This memory manager can be used as singleton or as local memory manager. It uses the pool_allocator
 * as singleton to maximize reuse
 */
template <typename TYPES, bool CLEANUP_DATA, typename BASE> class tlm_mm_t : public BASE {
    using payload_type = typename TYPES::tlm_payload_type;
    using payload_base = typename tlm_mm_traits<TYPES>::payload_base;
    static_assert(!std::is_base_of<tlm::tlm_generic_payload, typename TYPES::tlm_payload_type>::value, "Using cxs::tlm_network_cxs_types");

public:
    tlm_mm_t()
    : allocator(util::pool_allocator<sizeof(payload_type)>::get()) {}

    tlm_mm_t(const tlm_mm_t&) = delete;

    tlm_mm_t(tlm_mm_t&&) = delete;

    tlm_mm_t& operator=(const tlm_mm_t& other) = delete;

    tlm_mm_t& operator=(tlm_mm_t&& other) = delete;

    ~tlm_mm_t() = default;
    /**
     * @brief get a plain tlm_payload_type without extensions
     * @return the tlm_payload_type
     */
    payload_type* allocate() {
        auto* ptr = allocator.allocate(sc_core::sc_time_stamp().value());
        return new(ptr) payload_type(this);
    }
    /**
     * @brief get a tlm_payload_type with registered extension
     * @return the tlm_payload_type
     */
    template <typename PEXT> payload_type* allocate() {
        auto* ptr = allocate();
        ptr->set_auto_extension(new PEXT);
        return ptr;
    }
    /**
     * @brief return the extension into the memory pool (removing the extensions)
     * @param trans the returning transaction
     */
    void free(payload_base* trans) {
        trans->~payload_base();
        allocator.free(trans);
    }

private:
    util::pool_allocator<sizeof(payload_type)>& allocator;
};
/*!
 * @class tlm_mm_t
 * @brief a tlm payload memory manager
 *
 * This memory manager can be used as singleton or as local memory manager. It uses the pool_allocator
 * as singleton to maximize reuse
 */
template <typename TYPES, bool CLEANUP_DATA> class tlm_mm_t<TYPES, CLEANUP_DATA, tlm::tlm_mm_interface> : public tlm::tlm_mm_interface {
    using payload_type = typename TYPES::tlm_payload_type;

public:
    tlm_mm_t()
    : allocator(util::pool_allocator<sizeof(payload_type)>::get()) {}

    tlm_mm_t(const tlm_mm_t&) = delete;

    tlm_mm_t(tlm_mm_t&&) = delete;

    tlm_mm_t& operator=(const tlm_mm_t& other) = delete;

    tlm_mm_t& operator=(tlm_mm_t&& other) = delete;

    ~tlm_mm_t() = default;
    /**
     * @brief get a plain tlm_payload_type without extensions
     * @return the tlm_payload_type
     */
    payload_type* allocate() {
        auto* ptr = allocator.allocate(sc_core::sc_time_stamp().value());
        return new(ptr) payload_type(this);
    }
    /**
     * @brief get a tlm_payload_type with registered extension
     * @return the tlm_payload_type
     */
    template <typename PEXT> payload_type* allocate() {
        auto* ptr = allocate();
        ptr->set_auto_extension(new PEXT);
        return ptr;
    }
    /**
     * @brief get a plain tlm_payload_type without extensions but initialized data and byte enable
     * @return the tlm_payload_type
     */
    payload_type* allocate(size_t sz, bool be = false) { return sz ? tlm_gp_mm::add_data_ptr(sz, allocate(), be) : allocate(); }
    /**
     * @brief get a tlm_payload_type with registered extension and initialize data pointer
     *
     * @return the tlm_payload_type
     */
    template <typename PEXT> payload_type* allocate(size_t sz, bool be = false) {
        auto* ptr = allocate(sz, be);
        ptr->set_auto_extension(tlm_ext_mm<PEXT>::create());
        return ptr;
    }
    /**
     * @brief return the extension into the memory pool (removing the extensions)
     * @param trans the returning transaction
     */
    void free(tlm::tlm_generic_payload* trans) {
        if(CLEANUP_DATA && !trans->get_extension<tlm_gp_mm>()) {
            if(trans->get_data_ptr())
                delete[] trans->get_data_ptr();
            if(trans->get_byte_enable_ptr())
                delete[] trans->get_byte_enable_ptr();
        }
        trans->set_data_ptr(nullptr);
        trans->set_byte_enable_ptr(nullptr);
        trans->reset();
        trans->~tlm_generic_payload();
        allocator.free(trans);
    }

private:
    util::pool_allocator<sizeof(payload_type)>& allocator;
};
/*!
 * @class tlm_mm
 * @brief a tlm payload memory manager as singleton
 *
 * This memory manager can be used as singleton. It uses the pool_allocator
 * as singleton to maximize reuse
 */
template <typename TYPES = tlm_base_protocol_types, bool CLEANUP_DATA = true>
struct tlm_mm
: public tlm_mm_t<TYPES, CLEANUP_DATA,
                  typename std::conditional<std::is_base_of<tlm::tlm_generic_payload, typename TYPES::tlm_payload_type>::value,
                                            tlm::tlm_mm_interface, typename tlm_mm_traits<TYPES>::mm_if_type>::type> {
    /**
     * @brief accessor function of the singleton
     * @return
     */
    static tlm_mm& get();
};

template <typename TYPES, bool CLEANUP_DATA> inline tlm_mm<TYPES, CLEANUP_DATA>& tlm_mm<TYPES, CLEANUP_DATA>::get() {
    static tlm_mm<TYPES, CLEANUP_DATA> mm;
    return mm;
}
} // namespace scc
} // namespace tlm

#endif /* _TLM_TLM_MM_H_ */
