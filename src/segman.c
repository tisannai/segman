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


st_struct( sm_info )
{
    st_size_t header_size;
    st_size_t slot_area;
};


/* Internal functions: */
static st_size_t sm_size_in_units( st_size_t block_size, st_size_t unit_size );
static sm_info_s sm_host_info( st_size_t slot_cnt, st_size_t block_size, st_size_t slot_size );
static sm_info_s sm_tail_info( st_size_t slot_cnt, st_size_t block_size, st_size_t slot_size );
static st_none   sm_prepare_slot( sm_t sm );
static st_none   sm_new_seg( sm_t sm );
static st_none   sm_init_host( sm_t      sm,
                               st_t      slot_mem,
                               st_size_t slot_cnt,
                               st_size_t block_size,
                               st_size_t slot_size );



/* ------------------------------------------------------------
 * User API:
 */

sm_t sm_new( st_size_t slot_cnt, st_size_t slot_size )
{
    sm_t sm;
    st_t mem;

    sm_info_s info;
    info = sm_host_info( slot_cnt, 0, slot_size );

    mem = st_alloc( info.header_size + info.slot_area );
    sm = mem + info.slot_area;
    sm_use( sm, mem, slot_cnt, slot_size );

    return sm;
}


sm_t sm_new_block( st_size_t block_size, st_size_t slot_size )
{
    st_t mem;

    sm_info_s info;
    info = sm_host_info( 0, block_size, slot_size );

    mem = st_alloc( info.header_size + info.slot_area );
    return sm_use_block( mem, block_size, slot_size );
}


st_none sm_use( sm_t sm, st_t mem, st_size_t slot_cnt, size_t slot_size )
{
    assert( slot_size >= sizeof( st_t ) );
    assert( slot_cnt >= SM_MIN_SLOT_CNT );
    sm_init_host( sm, mem, slot_cnt, 0, slot_size );
}


sm_t sm_use_block( st_t mem, st_size_t block_size, size_t slot_size )
{
    st_t      sm;
    st_size_t slot_cnt;

    assert( slot_size >= sizeof( st_t ) );
    //     assert( slot_cnt >= SM_MIN_SLOT_CNT );

    sm_info_s info;
    info = sm_host_info( 0, block_size, slot_size );

    slot_cnt = ( info.slot_area / slot_size );
    assert( slot_cnt >= SM_MIN_SLOT_CNT );

    sm = mem + info.slot_area;
    sm_init_host( sm, mem, slot_cnt, block_size, slot_size );

    return sm;
}


sm_t sm_reset( sm_t sm )
{
    sm_tail_t cur;
    sm_tail_t next;

    cur = sm->host.next;

    while ( cur ) {
        cur->init_cnt = 0;
        cur = cur->next;
    }

    sm->host.init_cnt = 0;

    sm->used_cnt = 0;
    /*
      Give free_cnt as free in Head, since the tail is going to be
      used gradually.
     */
    sm->free_cnt = sm->slot_cnt;

    sm->tail = &sm->host;
    sm->head = sm->tail->base;

    return sm;
}


sm_t sm_del( sm_t sm )
{
    sm_del_tail( sm );
    st_del( sm->host.base );
    return NULL;
}


sm_t sm_del_tail( sm_t sm )
{
    sm_tail_t cur;
    sm_tail_t next;

    cur = sm->host.next;

    while ( cur ) {
        next = cur->next;
        st_del( cur );
        cur = next;
    }

    sm->host.next = NULL;

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


st_size_t sm_block_head_segment_size( st_size_t slot_cnt, st_size_t slot_size )
{
    st_size_t header_slots;
    header_slots = sm_size_in_units( sizeof( sm_s ), slot_size );
    return ( header_slots + slot_cnt ) * slot_size;
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
    if ( sm->get_cb ) {
        sm->get_cb( sm, NULL );
    }
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

    } else if ( sm->tail->next ) {

        /* Pre-existing Tail Segment (left from sm_reset). */
        sm->tail = sm->tail->next;
        sm->head = sm->tail->base;
        sm->free_cnt += sm->tail->tail_cnt;
        goto retry;

    } else if ( sm->resize != 0 ) {

        sm_new_seg( sm );
        goto retry;
    }

    return ret;
}


sm_t sm_put( sm_t sm, st_t slot )
{

#ifdef SEGMAN_USE_HOOKS
    if ( sm->put_cb ) {
        sm->put_cb( sm, slot );
    }
#endif

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
 * Return size of block in terms of the unit.
 *
 * @param block_size Block size;
 * @param unit_size  Unit size.
 *
 * @return Block size in units.
 */
static st_size_t sm_size_in_units( st_size_t block_size, st_size_t unit_size )
{
    st_size_t odd;
    st_size_t slots;

    slots = ( block_size / unit_size );
    odd = ( ( block_size % unit_size ) != 0 );

    return slots + odd;
}


/**
 * Return host segment info.
 *
 * @param slot_cnt   Slot count.
 * @param block_size Block size;
 * @param slot_size  Slot size.
 *
 * @return Segment info.
 */
static sm_info_s sm_host_info( st_size_t slot_cnt, st_size_t block_size, st_size_t slot_size )
{
    sm_info_s info;

    if ( block_size == 0 ) {

        info.header_size = sizeof( sm_s );
        info.slot_area = ( slot_cnt * slot_size );

    } else {

        st_size_t header_slots;
        st_size_t slot_cnt;

        header_slots = sm_size_in_units( sizeof( sm_s ), slot_size );
        info.header_size = ( header_slots * slot_size );
        slot_cnt = ( block_size - info.header_size ) / slot_size;
        info.slot_area = slot_cnt * slot_size;
    }

    return info;
}


/**
 * Return tail segment info.
 *
 * @param slot_cnt   Slot count.
 * @param block_size Block size;
 * @param slot_size  Slot size.
 *
 * @return Segment info.
 */
static sm_info_s sm_tail_info( st_size_t slot_cnt, st_size_t block_size, st_size_t slot_size )
{
    sm_info_s info;

    if ( block_size == 0 ) {

        info.header_size = sizeof( sm_tail_s );
        info.slot_area = ( slot_cnt * slot_size );

    } else {

        st_size_t header_slots;
        st_size_t slot_cnt;

        header_slots = sm_size_in_units( sizeof( sm_tail_s ), slot_size );
        info.header_size = ( header_slots * slot_size );
        slot_cnt = ( block_size - info.header_size ) / slot_size;
        info.slot_area = slot_cnt * slot_size;
    }

    return info;
}


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
static st_none sm_new_seg( sm_t sm )
{
    st_size_t slot_cnt;
    sm_tail_t new_seg;

    sm_info_s info;
    info = sm_tail_info( sm->slot_cnt, sm->block_size, sm->slot_size );

    if ( sm->block_size == 0 ) {
        slot_cnt = ( sm->resize * sm->slot_cnt ) / 100;
        new_seg = st_alloc( info.header_size + ( slot_cnt * sm->slot_size ) );
        new_seg->base = (st_t)new_seg + info.header_size;
    } else {
        slot_cnt = info.slot_area / sm->slot_size;
        new_seg = st_alloc( info.header_size + info.slot_area );
        new_seg->base = (st_t)new_seg + info.header_size;
    }

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
 * @param sm         Segman.
 * @param slot_mem   Slot memory.
 * @param slot_cnt   Slot count.
 * @param block_size Fixed block size (in bytes).
 * @param slot_size  Slot size (in bytes).
 *
 * @return NA
 */
static st_none sm_init_host( sm_t      sm,
                             st_t      slot_mem,
                             st_size_t slot_cnt,
                             st_size_t block_size,
                             st_size_t slot_size )
{

    sm->slot_cnt = slot_cnt;
    sm->block_size = block_size;
    sm->slot_size = slot_size;

    sm->used_cnt = 0;
    sm->free_cnt = slot_cnt;

    sm->head = slot_mem;
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
