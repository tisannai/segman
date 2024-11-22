/**
 * @file   segman.c
 * @author Tero Isannainen <tero.isannainen@gmail.com>
 * @date   Sun Jun 24 10:13:11 2018
 *
 * @brief  Memory segment pooler.
 *
 */

#include <sixten_ass.h>
#include "segman.h"

/* Internal functions: */
static st_none   sm_prepare_slot( sm_t sm );
static st_none   sm_new_seg( sm_t sm, st_size_t slot_cnt );
static st_none   sm_init_host( sm_t sm, st_t mem, st_size_t slot_cnt, st_size_t slot_size );
static st_size_t sm_host_extra( st_size_t slot_size );



/* ------------------------------------------------------------
 * User API:
 */

sm_t sm_new( st_size_t slot_cnt, st_size_t slot_size )
{
    sm_t sm;
    st_t mem;

    sm = st_alloc( sizeof( sm_s ) + slot_cnt * slot_size );
    mem = sm + sizeof( sm_s );
    sm_use( sm, mem, slot_cnt, slot_size );

    return sm;
}


st_none sm_use( sm_t sm, st_t mem, st_size_t slot_cnt, st_size_t slot_size )
{
    assert( slot_size >= sizeof( st_t ) );
    assert( slot_cnt >= SM_MIN_SLOT_CNT );
    sm_init_host( sm, mem, slot_cnt, slot_size );
}


st_size_t sm_fill( sm_t sm, st_t mem, st_size_t mem_size, st_size_t slot_size )
{
    //     st_assert_q( slot_size >= sizeof( st_t ) );
    //     st_assert_q( mem_size >= sizeof( sm_s ) + SM_MIN_SLOT_CNT * slot_size );
    assert( slot_size >= sizeof( st_t ) );
    // assert( mem_size >= sizeof( sm_s ) + SM_MIN_SLOT_CNT * slot_size );
    assert( mem_size >= SM_MIN_SLOT_CNT * slot_size );

    st_size_t slot_cnt;
    st_size_t slot_mem;

    // slot_mem = mem_size - sm_host_size();
    slot_mem = mem_size;
    // slot_cnt = slot_mem / slot_size - ( slot_mem % slot_size != 0 );
    slot_cnt = slot_mem / slot_size - ( ( slot_mem % slot_size == 0 ) ? 0 : 1 );

    sm_init_host( sm, mem, slot_cnt, slot_size );

    return slot_cnt;
}


sm_t sm_reset( sm_t sm )
{
    st_size_t slot_cnt;
    st_size_t slot_size;

    slot_cnt = sm->slot_cnt;
    slot_size = sm->slot_size;

//     st_memclr( sm, sizeof( sm_s ) + slot_cnt * slot_size );
//     sm_use( sm, slot_cnt, slot_size );

    return sm;
}


sm_t sm_del( sm_t sm )
{
    sm_tail_t cur;
    sm_tail_t next;

    cur = sm->host.next;

    while ( cur ) {
        next = cur->next;
        st_del( cur );
        cur = next;
    }

    st_del( sm );

    return NULL;
}


st_size_t sm_set_resize_factor( sm_t sm, st_size_t factor )
{
    if ( factor == 0 || ( factor * sm->slot_cnt / 100 ) >= SM_MIN_SLOT_CNT ) {
        sm->resize = factor;
        return 1;
    } else {
        return 0;
    }
}


st_size_t sm_slot_cnt( sm_t sm )
{
    return sm->slot_cnt;
}


st_size_t sm_slot_size( sm_t sm )
{
    return sm->slot_size;
}


st_size_t sm_total_cnt( sm_t sm )
{
    return sm->used_cnt + sm->free_cnt;
}


st_size_t sm_free_cnt( sm_t sm )
{
    return sm->free_cnt;
}


st_size_t sm_used_cnt( sm_t sm )
{
    return sm->used_cnt;
}


st_size_t sm_host_size( void )
{
    return sizeof( sm_s );
}


st_size_t sm_tail_size( void )
{
    return sizeof( sm_tail_s );
}


st_t sm_get( sm_t sm )
{

#ifdef SEGMAN_USE_HOOKS
    if ( sm->get_cb )
        sm->get_cb( sm, NULL );
#endif

retry:

    if ( sm->tail->init_cnt < sm->tail->tail_cnt ) {
        sm_prepare_slot( sm );
    }

    st_t ret = NULL;

    if ( sm->free_cnt > 0 ) {

        ret = sm->head;

        sm->used_cnt++;
        sm->free_cnt--;

        if ( sm->free_cnt > 0 ) {

            /* Get the link info from returned slot. */
            sm->head = *( (st_p)sm->head );

        } else {

            /* No more slots, out-of-mem. */
            sm->head = NULL;
        }

    } else if ( sm->resize != 0 ) {

        sm_new_seg( sm, sm->resize * sm->slot_cnt / 100 );
        goto retry;
    }

    return ret;
}


sm_t sm_put( sm_t sm, st_t slot )
{

#ifdef SEGMAN_USE_HOOKS
    if ( sm->put_cb )
        sm->put_cb( sm, slot );
#endif

    // if ( sm->free_cnt >= sm_total_cnt( sm ) ) {
    if ( sm->used_cnt == 0 ) {
        return NULL;
    }

    if ( sm->head != NULL ) {

        /* Store index of previous free slot to freed slot. */
        *( (st_p)slot ) = sm->head;

        /* Update free slot to freed slot. */
        sm->head = slot;

    } else {

        /*
         * First free after out-of-mem. Store a "dummy" index
         * (out-of-bounds).
         */
        *( (st_p)slot ) = NULL;
        sm->head = slot;
    }

    sm->used_cnt--;
    sm->free_cnt++;

    return sm;
}


#ifdef SEGMAN_USE_HOOKS

void sm_set_get_cb( sm_t sm, sm_hook_fn cb )
{
    sm->get_cb = cb;
}

void sm_set_put_cb( sm_t sm, sm_hook_fn cb )
{
    sm->put_cb = cb;
}

#endif


/* ------------------------------------------------------------
 * Internal functions:
 * ------------------------------------------------------------ */

/**
 * Prepare the next slot that requires a link.
 *
 * @param sm Segman.
 *
 * @return NA
 */
static st_none sm_prepare_slot( sm_t sm )
{
    /* Make sure that each slot has a link before it is allocated. */
    st_t slot;
    slot = sm->tail->base + ( sm->tail->init_cnt * sm->slot_size );

    /* Make Slot N content to point to Slot N+1. */
    *( (st_p)slot ) = slot + sm->slot_size;

    sm->tail->init_cnt++;
}


/**
 * Allocate new Segman Segment.
 *
 * @param sm       Segman.
 * @param slot_cnt Slot count.
 *
 */
static st_none sm_new_seg( sm_t sm, st_size_t slot_cnt )
{
    sm_tail_t new_seg;

    new_seg = st_alloc( sizeof( sm_tail_s ) + slot_cnt * sm->slot_size );
    new_seg->base = (st_t)new_seg + sizeof( sm_tail_s );
    new_seg->tail_cnt = slot_cnt;
    new_seg->init_cnt = 0;
    new_seg->next = NULL;

    sm->tail->next = new_seg;

    sm->head = new_seg->base;
    sm->tail = new_seg;
    sm->free_cnt += slot_cnt;
}


/**
 * Initialize Segman host structure.
 *
 * @param sm        Segman.
 * @param slot_cnt  Slot count.
 * @param slot_size Slot size (in bytes).
 * @param alloc_fn  Allocation function (if any).
 * @param free_fn   Free function (if any).
 * @param mem_env   Memory env (if any).
 *
 * @return NA
 */
static st_none sm_init_host( sm_t sm, st_t mem, st_size_t slot_cnt, st_size_t slot_size )
{
    // sm->slot_cnt = slot_cnt + sm_host_extra( slot_size );
    sm->slot_cnt = slot_cnt;
    sm->slot_size = slot_size;
    sm->used_cnt = 0;
    sm->free_cnt = slot_cnt;

    //     sm->head = (st_t)sm + sizeof( sm_s );
    sm->head = mem;
    sm->tail = &( sm->host );

    sm->resize = 100;

    sm->tail->base = sm->head;
    sm->tail->tail_cnt = slot_cnt;
    sm->tail->init_cnt = 0;
    sm->tail->next = NULL;

#ifdef SEGMAN_USE_HOOKS
    sm->get_cb = NULL;
    sm->put_cb = NULL;
#endif
}


/**
 * Calculate the slot count of host. Note that some slots are
 * overwritten by host specific data.
 *
 * @param slot_size Slot size.
 *
 * @return Extra size.
 */
static st_size_t sm_host_extra( st_size_t slot_size )
{
    st_size_t host_extra;

    host_extra = sm_host_size() - sm_tail_size();
    return host_extra / slot_size + ( ( host_extra % slot_size == 0 ) ? 0 : 1 );
}
