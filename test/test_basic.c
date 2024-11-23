#include "unity.h"
#include "segman.h"

#define SLOT_CNT SM_MIN_SLOT_CNT

typedef struct
{
    union
    {
        st_t    ptr;
        st_id_t id;
    };
    char name[ 24 ];
} my_slot_t;
typedef my_slot_t* my_slot_p;


void dbg_break( void )
{
}


my_slot_p nth( sm_t sm, int n )
{
    st_t ptr;
    ptr = sm->tail->base + n * sm->slot_size;
    return (my_slot_p)ptr;
}

/*
 * Tests:
 * - basic (queries, factor)
 * - memory env (factor, corner cases).
 * - random.
 */

st_size_t host_extra_in_slots( st_size_t slot_size )
{
    st_size_t host_slot_cnt;
    st_size_t host_extra;
    host_extra = sm_host_size() - sm_tail_size();
    host_slot_cnt = host_extra / slot_size + ( host_extra % slot_size != 0 );

    return host_slot_cnt;
}


int get_cnt = 0;
int put_cnt = 0;

void get_cb( sm_t sm, st_t slot )
{
    sm = sm;
    slot = slot;
    get_cnt++;
}

void put_cb( sm_t sm, st_t slot )
{
    sm = sm;
    if ( slot != NULL )
        put_cnt++;
}


void test_basic( void )
{
    sm_t      sm;
    my_slot_p ptr[ SLOT_CNT ];
    st_size_t slot_size = sizeof( my_slot_t );

    sm = sm_new( SLOT_CNT, slot_size );

    sm_set_resize_factor( sm, 0 );

    sm_set_get_cb( sm, get_cb );
    sm_set_put_cb( sm, put_cb );

    // TEST_ASSERT( sm_slot_cnt( sm ) == SLOT_CNT + host_extra_in_slots( slot_size ) );
    // TEST_ASSERT( sm_host_slot_cnt( sm ) == SLOT_CNT );
    TEST_ASSERT( sm_slot_size( sm ) == slot_size );

    TEST_ASSERT( sm_total_cnt( sm ) == SLOT_CNT );
    TEST_ASSERT( sm_free_cnt( sm ) == SLOT_CNT );
    TEST_ASSERT( sm_used_cnt( sm ) == 0 );
    TEST_ASSERT( sm_host_size() == sizeof( sm_s ) );
    TEST_ASSERT( sm_tail_size() == sizeof( sm_tail_s ) );

    st_t    slot;
    st_id_t i;

    for ( i = 0; i < SLOT_CNT; i++ ) {
        TEST_ASSERT( sm_total_cnt( sm ) == SLOT_CNT );
        TEST_ASSERT( sm_free_cnt( sm ) == ( SLOT_CNT - i ) );
        TEST_ASSERT( sm_used_cnt( sm ) == i );
        TEST_ASSERT( sm->tail->init_cnt == i );
        slot = sm_get( sm );
        ( (my_slot_p)slot )->id = i;
        ptr[ i ] = slot;
    }

    slot = sm_get( sm );
    TEST_ASSERT( slot == NULL );


    st_id_t map[ SLOT_CNT ] = { 1, 3, 0, 2 };

    for ( i = 0; i < SLOT_CNT - 1; i++ ) {
        TEST_ASSERT( sm_total_cnt( sm ) == SLOT_CNT );
        TEST_ASSERT( sm_free_cnt( sm ) == i );
        TEST_ASSERT( sm_used_cnt( sm ) == ( SLOT_CNT - i ) );
        sm_put( sm, ptr[ map[ i ] ] );
    }

    TEST_ASSERT( nth( sm, 2 )->id == 2 );
    TEST_ASSERT( nth( sm, 3 )->ptr == ptr[ 1 ] );
    TEST_ASSERT( nth( sm, 0 )->ptr == ptr[ 3 ] );
    TEST_ASSERT( sm->head == ptr[ 0 ] );

    sm_put( sm, ptr[ map[ i ] ] );

    TEST_ASSERT( nth( sm, 2 )->ptr == ptr[ 0 ] );
    TEST_ASSERT( sm->head == ptr[ 2 ] );

    for ( i = 0; i < SLOT_CNT; i++ ) {
        TEST_ASSERT( sm_total_cnt( sm ) == SLOT_CNT );
        TEST_ASSERT( sm_free_cnt( sm ) == ( SLOT_CNT - i ) );
        TEST_ASSERT( sm_used_cnt( sm ) == i );
        TEST_ASSERT( sm->tail->init_cnt == SLOT_CNT );
        slot = sm_get( sm );
        ( (my_slot_p)slot )->id = i;
        ptr[ i ] = slot;
    }

    for ( i = 0; i < SLOT_CNT; i++ ) {
        TEST_ASSERT( sm_total_cnt( sm ) == SLOT_CNT );
        TEST_ASSERT( sm_free_cnt( sm ) == i );
        TEST_ASSERT( sm_used_cnt( sm ) == ( SLOT_CNT - i ) );
        sm_put( sm, ptr[ i ] );
    }

    TEST_ASSERT_EQUAL( 2*SLOT_CNT+1, get_cnt );
    TEST_ASSERT_EQUAL( 2*SLOT_CNT, put_cnt );

    TEST_ASSERT( sm_used_cnt( sm ) == 0 );
    slot = sm_get( sm );
    TEST_ASSERT( sm_used_cnt( sm ) == 1 );
//     sm_reset( sm );
//     TEST_ASSERT( sm_used_cnt( sm ) == 0 );

    sm_del( sm );
}


void test_random( void )
{
    st_id_t rounds = 100000;
    st_id_t i;
    int     rnd;

    sm_t      sm;
    my_slot_p ptr[ SLOT_CNT + 1 ];
    st_size_t mem_size;
    st_t mem;
    

    dbg_break();

    // mem_size = sm_host_size() + SLOT_CNT * sizeof( my_slot_t );
    // mem_size = SLOT_CNT * sizeof( my_slot_t );
    mem_size = sm_block_head_segment_size( SLOT_CNT, sizeof( my_slot_t ) );
    // mem = st_alloc( 2*sm_host_size() + mem_size );
    mem = st_alloc( mem_size );

#if 0
    sm_fill( sm, mem, mem_size, sizeof( my_slot_t ) );
#endif
    sm = sm_use_block( mem, mem_size, sizeof( my_slot_t ) );
    sm_set_resize_factor( sm, 0 );
    

    st_id_t used = 0;
    st_t    ret;

    i = 0;
    while ( i < rounds ) {
        rnd = rand();

        if ( ( rnd % 2 ) == 0 ) {
            ptr[ used ] = sm_get( sm );
            if ( used < SLOT_CNT )
                used++;
            else {
                // printf( "Empty sat\n" );
                TEST_ASSERT( ptr[ used ] == NULL );
            }
            TEST_ASSERT( sm_used_cnt( sm ) == used );
        } else {
            if ( used > 0 ) {
                used--;
                ret = sm_put( sm, ptr[ used ] );
                TEST_ASSERT( ret == sm );
            } else {
                // printf( "Full sat\n" );
                ret = sm_put( sm, ptr[ used ] );
                TEST_ASSERT( ret == NULL );
            }
            TEST_ASSERT( sm_used_cnt( sm ) == used );
        }

        // printf( "Used: %ld\n", used );
        i++;
    }
}
