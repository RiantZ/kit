#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

// Bounded lock-free MPSC queue based on the Vyukov bounded ring buffer.
namespace kit
{
/// @brief Bounded, lock-free multi-producer / single-consumer queue.
///
/// The queue is implemented as a fixed-capacity ring buffer where every slot
/// carries its own atomic sequence number acting as a per-slot handshake.
/// Multiple producers contend only on an atomic enqueue position via a CAS loop,
/// while the (single) consumer advances a separate dequeue position. Producers
/// and the consumer never block each other, so the data structure is lock-free.
///
/// The algorithm is correct for the full MPMC case as well, which makes the
/// single-consumer usage trivially safe.
///
/// @tparam t_value_type The type of elements stored in the queue.
template <typename t_value_type> class c_mpsc_queue
{
    /// @brief A single ring-buffer slot.
    struct s_cell
    {
        std::atomic<size_t> mc_sequence; ///< Per-slot handshake counter
        t_value_type        mo_data;     ///< Payload stored in the slot
    };

    // Avoid false sharing between the producer and consumer positions.
    static constexpr size_t gz_cache_line = 64;

    s_cell *mp_buffer;
    size_t  mz_capacity;
    size_t  mz_mask;

    alignas(gz_cache_line) std::atomic<size_t> mc_enqueue_pos;
    alignas(gz_cache_line) std::atomic<size_t> mc_dequeue_pos;

public:
    /// @brief Constructs the queue with a fixed capacity.
    /// @param iz_capacity Number of elements the queue can hold. Must be a
    ///        power of two and greater than zero.
    explicit c_mpsc_queue(size_t iz_capacity)
        : mp_buffer(nullptr)
        , mz_capacity(iz_capacity)
        , mz_mask(iz_capacity - 1)
    {
        assert((iz_capacity >= 2) && (0 == (iz_capacity & (iz_capacity - 1)))
               && "Capacity must be a power of two >= 2");

        mp_buffer = new s_cell[mz_capacity];

        for(size_t lz_i = 0; lz_i < mz_capacity; lz_i++)
        {
            mp_buffer[lz_i].mc_sequence.store(lz_i, std::memory_order_relaxed);
        }

        mc_enqueue_pos.store(0, std::memory_order_relaxed);
        mc_dequeue_pos.store(0, std::memory_order_relaxed);
    }

    c_mpsc_queue(const c_mpsc_queue &)            = delete;
    c_mpsc_queue &operator=(const c_mpsc_queue &) = delete;

    /// @brief Destructor, releases the ring buffer.
    ~c_mpsc_queue()
    {
        delete[] mp_buffer;
        mp_buffer = nullptr;
    }

    /// @brief Attempts to enqueue an element (copy).
    /// @param ir_data The element to copy into the queue.
    /// @return true if the element was enqueued, false if the queue is full.
    bool try_push(const t_value_type &ir_data)
    {
        s_cell *lp_cell = nullptr;
        size_t  lz_pos  = mc_enqueue_pos.load(std::memory_order_relaxed);

        for(;;)
        {
            lp_cell         = &mp_buffer[lz_pos & mz_mask];
            size_t   lz_seq = lp_cell->mc_sequence.load(std::memory_order_acquire);
            intptr_t li_dif = (intptr_t)lz_seq - (intptr_t)lz_pos;

            if(0 == li_dif)
            {
                if(mc_enqueue_pos.compare_exchange_weak(lz_pos, lz_pos + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if(li_dif < 0)
            {
                return false;
            }
            else
            {
                lz_pos = mc_enqueue_pos.load(std::memory_order_relaxed);
            }
        }

        lp_cell->mo_data = ir_data;
        lp_cell->mc_sequence.store(lz_pos + 1, std::memory_order_release);
        return true;
    }

    /// @brief Attempts to enqueue an element (move).
    /// @param iv_data The element to move into the queue.
    /// @return true if the element was enqueued, false if the queue is full.
    bool try_push(t_value_type &&iv_data)
    {
        s_cell *lp_cell = nullptr;
        size_t  lz_pos  = mc_enqueue_pos.load(std::memory_order_relaxed);

        for(;;)
        {
            lp_cell         = &mp_buffer[lz_pos & mz_mask];
            size_t   lz_seq = lp_cell->mc_sequence.load(std::memory_order_acquire);
            intptr_t li_dif = (intptr_t)lz_seq - (intptr_t)lz_pos;

            if(0 == li_dif)
            {
                if(mc_enqueue_pos.compare_exchange_weak(lz_pos, lz_pos + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if(li_dif < 0)
            {
                return false;
            }
            else
            {
                lz_pos = mc_enqueue_pos.load(std::memory_order_relaxed);
            }
        }

        lp_cell->mo_data = std::move(iv_data);
        lp_cell->mc_sequence.store(lz_pos + 1, std::memory_order_release);
        return true;
    }

    /// @brief Attempts to dequeue an element.
    /// @param or_data Receives the dequeued element on success.
    /// @return true if an element was dequeued, false if the queue is empty.
    bool try_pop(t_value_type &or_data)
    {
        s_cell *lp_cell = nullptr;
        size_t  lz_pos  = mc_dequeue_pos.load(std::memory_order_relaxed);

        for(;;)
        {
            lp_cell         = &mp_buffer[lz_pos & mz_mask];
            size_t   lz_seq = lp_cell->mc_sequence.load(std::memory_order_acquire);
            intptr_t li_dif = (intptr_t)lz_seq - (intptr_t)(lz_pos + 1);

            if(0 == li_dif)
            {
                if(mc_dequeue_pos.compare_exchange_weak(lz_pos, lz_pos + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if(li_dif < 0)
            {
                return false;
            }
            else
            {
                lz_pos = mc_dequeue_pos.load(std::memory_order_relaxed);
            }
        }

        or_data = std::move(lp_cell->mo_data);
        lp_cell->mc_sequence.store(lz_pos + mz_mask + 1, std::memory_order_release);
        return true;
    }

    /// @brief Returns the maximum number of elements the queue can hold.
    /// @return The fixed capacity of the queue.
    inline size_t capacity() const noexcept
    {
        return mz_capacity;
    }

    /// @brief Returns an approximate number of elements currently queued.
    ///
    /// The value is a snapshot that may be stale in the presence of concurrent
    /// producers/consumer; it is intended for diagnostics only.
    /// @return Approximate element count.
    inline size_t size() const noexcept
    {
        size_t lz_enqueue = mc_enqueue_pos.load(std::memory_order_relaxed);
        size_t lz_dequeue = mc_dequeue_pos.load(std::memory_order_relaxed);

        return (lz_enqueue > lz_dequeue) ? (lz_enqueue - lz_dequeue) : 0;
    }

    /// @brief Reports whether the queue appears to be empty.
    /// @return true if no elements appear to be queued.
    inline bool empty() const noexcept
    {
        return 0 == size();
    }
};
}
