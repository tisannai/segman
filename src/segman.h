#ifndef SEGMAN_H
#define SEGMAN_H


/**
 * @file   segman.h
 * @author Tero Isannainen <tero.isannainen@gmail.com>
 * @date   Sun Jun 24 10:13:11 2018
 *
 * @brief  Memory segment pooler.
 *
 */

#include <sixten.h>

#ifndef SM_MIN_SLOT_CNT
#define SM_MIN_SLOT_CNT 4
#endif


st_struct_type( sm );
st_struct_type( sm_tail );


typedef void ( *sm_hook_fn )( sm_t sm, st_t slot );


/** Segman Tail structure. */
st_struct_body( sm_tail )
{
    st_t      base;     /**< Base slot (first). */
    st_size_t tail_cnt; /**< Number of slots in last segment. */
    st_size_t init_cnt; /**< Number of initialized slots. */
    sm_tail_t next;     /**< Next Segment (null for tail). */
};

/** Segman Host structure. */
st_struct_body( sm )
{
    st_size_t slot_cnt;  /**< Number of valid (usable) slots in pool. */
    st_size_t slot_size; /**< Size of each slot. */
    st_size_t used_cnt;  /**< Number of used slots. */
    st_size_t free_cnt;  /**< Number of available slots. */

    st_t      head; /**< Head slot. */
    sm_tail_t tail; /**< Tail segment. */
    sm_tail_s host; /**< Segment spec. */

    st_size_t resize; /**< Resize factor percentage. */

    st_mem_cb_fn alloc_fn; /**< Allocation function. */
    st_mem_cb_fn free_fn;  /**< Free function. */
    st_t         mem_env;  /**< Memory env. */

#ifdef SEGMAN_USE_HOOKS
    sm_hook_fn get_cb; /**< Callback for get. */
    sm_hook_fn put_cb; /**< Callback for put. */
#endif
};


/**
 * Create Segman.
 *
 * @param slot_cnt  Number of memory slots.
 * @param slot_size Memory slot size.
 *
 * @return Segman.
 */
sm_t sm_new( st_size_t slot_cnt, st_size_t slot_size );


/**
 * Initialize pre-allocated memory with Segman.
 *
 * @param mem       Memory.
 * @param slot_cnt  Slot count.
 * @param slot_size Slot size.
 * @param alloc_fn  Allocation function (if any).
 * @param free_fn   Free function (if any).
 * @param mem_env   Memory env (if any).
 *
 * @return NA
 */
st_none sm_new_use( st_t         mem,
                    st_size_t    slot_cnt,
                    st_size_t    slot_size,
                    st_mem_cb_fn alloc_fn,
                    st_mem_cb_fn free_fn,
                    st_t         mem_env );


/**
 * Create Segman and fill pre-allocated memory.
 *
 * @param mem       Segman memory.
 * @param mem_size  Memory size.
 * @param slot_size Slot size.
 *
 * @return Slot count.
 */
st_size_t sm_new_fill( st_t mem, st_size_t mem_size, st_size_t slot_size );


/**
 * Free all slots, but leave Segman allocations.
 *
 * @param sm Segman.
 *
 * @return NULL.
 */
sm_t sm_reset( sm_t sm );


/**
 * Destroy memory Segman.
 *
 * @param sm Segman.
 *
 * @return NULL.
 */
sm_t sm_del( sm_t sm );


/**
 * Set memory management environment.
 *
 * @param sm       Segman.
 * @param alloc_fn Allocation function.
 * @param free_fn  De-allocation function.
 * @param mem_env  Allocator env.
 *
 * @return NA
 */
st_none sm_set_memory_env( sm_t sm, st_mem_cb_fn alloc_fn, st_mem_cb_fn free_fn, st_t mem_env );


/**
 * Set Segman resize factor percentage. 0 (the default) means no
 * automatic resizing at out-of-mem condition. Typically factor of
 * 200% is used.
 *
 * Factor of 100% (or anything that does not increase the size) is
 * considered illegal value.
 *
 * @param sm     Segman.
 * @param factor Factor as percentage.
 *
 * @return 1 on success (0 on failure).
 */
st_size_t sm_set_resize_factor( sm_t sm, st_size_t factor );


/**
 * Return Segment Slot count.
 *
 * @param sm Segman.
 *
 * @return Count.
 */
st_size_t sm_slot_cnt( sm_t sm );


/**
 * Return Slot size.
 *
 * @param sm Segman.
 *
 * @return Size.
 */
st_size_t sm_slot_size( sm_t sm );


/**
 * Return total slot count in Segman.
 *
 * @param sm Segman.
 *
 * @return Count.
 */
st_size_t sm_total_cnt( sm_t sm );


/**
 * Return number of available slots.
 *
 * @param sm Segman.
 *
 * @return Count.
 */
st_size_t sm_free_cnt( sm_t sm );


/**
 * Return number of used slots.
 *
 * @param sm Segman.
 *
 * @return Count.
 */
st_size_t sm_used_cnt( sm_t sm );


/**
 * Return Host Segment struct size in bytes.
 *
 * @return Size.
 */
st_size_t sm_host_size( void );


/**
 * Return Tail Segment struct size in bytes.
 *
 * @return Size.
 */
st_size_t sm_tail_size( void );


/**
 * Allocate (get) a slot of memory.
 *
 * @param sm Segman.
 *
 * @return Memory slot (or NULL if memory pool is exhausted).
 */
st_t sm_get( sm_t sm );


/**
 * De-allocate (put back) a slot of memory.
 *
 * @param sm   Segman.
 * @param slot Slot to return to pool.
 *
 * @return Pool on success (NULL otherwise).
 */
sm_t sm_put( sm_t sm, st_t slot );


/* ------------------------------------------------------------
 * SEGMAN_USE_HOOKS
 */

/**
 * Register callback to be called at allocation.
 *
 * @param sm Segman.
 * @param cb Callback function pointer.
 */
void sm_set_get_cb( sm_t sm, sm_hook_fn cb );


/**
 * Register callback to be called at de-allocation.
 *
 * @param sm Segman.
 * @param cb Callback function pointer.
 */
void sm_set_put_cb( sm_t sm, sm_hook_fn cb );

#endif
