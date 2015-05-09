// Copyright (C) 2015 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#ifndef FOONATHAN_MEMORY_TRACKING_HPP_INCLUDED
#define FOONATHAN_MEMORY_TRACKING_HPP_INCLUDED

/// \file
/// \brief Methods for tracking allocations.

#include <cstddef>

#include "allocator_traits.hpp"

namespace foonathan { namespace memory
{    
    namespace detail
    {
        template <class Tracker, class ImplRawAllocator>
        class tracked_impl_allocator : ImplRawAllocator
        {
            using traits = allocator_traits<ImplRawAllocator>;
        public:
            using raw_allocator = ImplRawAllocator;
            using tracker = Tracker;  

            using is_stateful = std::true_type;
            
            tracked_impl_allocator(tracker &t, raw_allocator allocator = {})
            : t_(&t),
              raw_allocator(std::move(allocator)) {}
            
            void* allocate_node(std::size_t size, std::size_t alignment)
            {
                auto mem = traits::allocate_node(*this, size, alignment);
                t_->on_allocator_growth(mem, size);
                return mem;
            }
            
            void* allocate_array(std::size_t count, std::size_t size, std::size_t alignment)
            {
                auto mem = traits::allocate_array(*this, count, size, alignment);
                t_->on_allocator_growth(mem, size * count);
                return mem;
            }
            
            void deallocate_node(void *ptr,
                                  std::size_t size, std::size_t alignment) FOONATHAN_NOEXCEPT
            {
                traits::deallocate_node(*this, ptr, size, alignment);
                t_->on_allocator_shrinking(ptr, size);
            }
            
            void deallocate_array(void *ptr, std::size_t count,
                                  std::size_t size, std::size_t alignment) FOONATHAN_NOEXCEPT
            {
                traits::deallocate_array(*this, ptr, count, size, alignment);
                t_->on_allocator_shrinking(ptr, size * count);
            }

            std::size_t max_node_size() const
            {
                return traits::max_node_size(*this);
            }
            
            std::size_t max_array_size() const
            {
                return traits::max_array_size(*this);
            }
            
            std::size_t max_alignment() const
            {
                return traits::max_alignment(*this);
            }
           
        private:
            Tracker *t_;
        };
    } // namespace detail

    /// \brief A wrapper around an \ref concept::RawAllocator that allows logging.
    /// \details The \c Tracker must provide the following, \c FOONATHAN_NOEXCEPT functions:
    /// * \c on_node_allocation(void *memory, std::size_t size, std::size_t alignment)
    /// * \c on_node_deallocation(void *memory, std::size_t size, std::size_t alignment)
    /// * \c on_array_allocation(void *memory, std::size_t count, std::size_t size, std::size_t alignment)
    /// * \c on_array_deallocation(void *memory, std::size_t count, std::size_t size, std::size_t alignment)
    /// <br>If you use a deeply tracked allocator via the appropriate \ref make_tracked_allocator() overload,
    /// the \c Tracker must also provide the following two, \c FOONATHAN_NOEXCEPT functions:
    /// * \c on_allocator_growth(void *memory, std::size_t total_size)
    /// * \c on_allocator_shrinking(void *memory, std::size_t total_size)
    /// <br>They are called on the allocation/deallocation functions of the implementation allocator.
    /// <br>The \c RawAllocator functions are called via the \ref allocator_traits.
    /// \ingroup memory
    template <class Tracker, class RawAllocator>
    class tracked_allocator : Tracker, RawAllocator
    {
        using traits = allocator_traits<RawAllocator>;
    public:
        using raw_allocator = RawAllocator;
        using tracker = Tracker;  

        /// \brief The allocator is stateful if the \ref raw_allocator is or the \ref tracker non-empty.
        using is_stateful = std::integral_constant<bool,
                            traits::is_stateful::value || !std::is_empty<Tracker>::value>;
        
        explicit tracked_allocator(tracker t = {}, raw_allocator&& allocator = {})
        : tracker(std::move(t)), raw_allocator(std::move(allocator)) {}
        
        /// @{
        /// \brief (De-)Allocation functions call the appropriate tracker function.
        void* allocate_node(std::size_t size, std::size_t alignment)
        {
            auto mem = traits::allocate_node(get_allocator(), size, alignment);
            this->on_node_allocation(mem, size, alignment);
            return mem;
        }
        
        void* allocate_array(std::size_t count, std::size_t size, std::size_t alignment)
        {
            auto mem = traits::allocate_array(get_allocator(), count, size, alignment);
            this->on_array_allocation(mem, count, size, alignment);
            return mem;
        }
        
        void deallocate_node(void *ptr,
                              std::size_t size, std::size_t alignment) FOONATHAN_NOEXCEPT
        {
            this->on_node_deallocation(ptr, size, alignment);
            traits::deallocate_node(get_allocator(), ptr, size, alignment);
        }
        
        void deallocate_array(void *ptr, std::size_t count,
                              std::size_t size, std::size_t alignment) FOONATHAN_NOEXCEPT
        {
            this->on_array_deallocation(ptr, count, size, alignment);
            traits::deallocate_array(get_allocator(), ptr, count, size, alignment);
        }
        /// @}        
        
        /// @{
        /// \brief Forwards to the allocator.
        std::size_t max_node_size() const
        {
            return traits::max_node_size(get_allocator());
        }
        
        std::size_t max_array_size() const
        {
            return traits::max_array_size(get_allocator());
        }
        
        std::size_t max_alignment() const
        {
            return traits::max_alignment(get_allocator());
        }
        /// @}
        
        /// @{
        /// \brief Returns a reference to the allocator.
        raw_allocator& get_allocator() FOONATHAN_NOEXCEPT
        {
            return *this;
        }
        
        const raw_allocator& get_allocator() const FOONATHAN_NOEXCEPT
        {
            return *this;
        }
        /// @}
        
        /// @{
        /// \brief Returns a reference to the tracker.
        tracker& get_tracker() FOONATHAN_NOEXCEPT
        {
            return *this;
        }
        
        const tracker& get_tracker() const FOONATHAN_NOEXCEPT
        {
            return *this;
        }
        /// @}
      
    // my g++ has an issue with the friend declaration, this constructor must be public for now
    //private:
        template <class ImplRawAllocator, typename ... Args>
        tracked_allocator(tracker t, ImplRawAllocator impl,
                        Args&&... args)
        : tracker(std::move(t)),
          raw_allocator(std::forward<Args>(args)...,
                detail::tracked_impl_allocator<tracker, ImplRawAllocator>(*this, std::move(impl)))
        {}
        
        template <template <class> class A, class T, class I, class ... Args>
        friend tracked_allocator<T, A<detail::tracked_impl_allocator<T, I>>> make_tracked_allocator(T t, I impl, Args&&... args);
    };
    
    /// \brief Creates a \ref tracked_allocator.
    /// \relates tracked_allocator
    template <class Tracker, class RawAllocator>
    auto make_tracked_allocator(Tracker t, RawAllocator &&alloc)
    -> tracked_allocator<Tracker, typename std::decay<RawAllocator>::type>
    {
        return tracked_allocator<Tracker, typename std::decay<RawAllocator>::type>{std::move(t), std::forward<RawAllocator>(alloc)};
    }
    
    /// \brief Creates a deeply tracked \ref tracked_allocator.
    /// \details It also tracks allocator growth, that is, when allocators with implementation allocator (e.g. \ref memory_stack),
    /// run out of memory blocks and need to allocate new, slow memory.<br>
    /// It is detected by wrapping the implementation allocator into an adapter and calling the appropriate tracker functions
    /// on allocation/deallocation of the implementation allocator.<br>
    /// The \c RawAllocator must take as single template argument an implementation allocator,
    /// \c args are passed to its constructor followed by the implementation allocator.
    /// \relates tracked_allocator
    template <template <class> class RawAllocator, class Tracker, class ImplRawAllocator, class ... Args>
    auto make_tracked_allocator(Tracker t, ImplRawAllocator impl, Args&&... args)
    -> tracked_allocator<Tracker, RawAllocator<detail::tracked_impl_allocator<Tracker, ImplRawAllocator>>>
    {
        return {std::move(t), std::move(impl), std::forward<Args&&>(args)...};
    }
}} // namespace foonathan::memory

#endif // FOONATHAN_MEMORY_TRACKING_HPP_INCLUDED