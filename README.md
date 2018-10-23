# Segman

## Basics

Segman is a memory pooler for fixed size Slots. When Segman is
created, the user specifies size of the used Slot and the number of
Slots that exist initially. Segman is able grow by itself when there
are no free Slots available, but it can be configured to be limited as
well.

Segman consists of one or more Segments. Segment contains a header and
a collection of Slots. The first Segment in Segman is called Host and
the rest are called Tail Segments. The header in Host is somewhat
bigger than the header of a Tail Segment. In fact Tail header is
contained within Host header.

Segman is created with:

    sm_t sm;
    sm = sm_new( slot_cnt, slot_size );

This will create a Host Segment with `slot_cnt` number of Slot with
size `slot_size`.

Slot can be reserved with:

    slot = sm_get( sm );

and it can be released with:

    sm_put( sm, slot );

If current Segment has no available Slots, either `NULL` is returned
or a new Segment is allocated. Segman growth is defined by `resize`
factor. `resize` factor is a percentage to use for growth. `100`
(i.e. 100%) means that the newly allocated Tail Segment will have the
same amount of Slots as the nominal Slot count for Segment. Note that
nominal Slot count is somewhat smaller for the Host Segment than for
the Tail Segments. `50` means that half of the nominal Slots are
provided. Nominal Slot count is given:

    slots_per_tail_segment = sm_slot_cnt( sm );

If `resize` is `0`, new Segments are not allocated and `sm_get`
returns `NULL` in out-of-slots event.

Segman has query functions: `sm_slot_cnt`, `sm_slot_size`,
`sm_total_cnt`, `sm_free_cnt`, `sm_used_cnt`, `sm_host_size`, and
`sm_tail_size`.


## Details

Segman reuses released slots for future reservations. Each available
Slot contains a link to the next available Slot. Segman is hence a
singly linked list of available Slots. Whenever a Slot is released, it
is placed as head of the list. Whenever a Slot is reserved, the head
Slot is returned, and head is updated to the next free Slot. The
returned Slot includes link to the next free Slot.

                                   head
                                     |
                                     v
     0     1     2     3     4     5
    +-----+-----+-----+-----+-----+-----+
    |     |     |     |     |     |     |
    |  2  | used|  X  | used| used|  0  |
    |     |     |     |     |     |     |
    +-----+-----+-----+-----+-----+-----+
       ^ |        ^                 |
       | +--------+                 |
       +----------------------------+

NOTE: the minimum size of Slot is thus a pointer (i.e. 64-bits).

In the above figure, we have 6 Slots. `head` is pointing to `S5`, and
`S5` is pointing to `S0`. `S0` is pointing to `S2`, which is now the
last free Slot. Slots 1, 3, and 4 are reserved.

If `S3` is released, `head` would then point to `S3`, and `S3` would
be initialized with link to (previous head) `S5`. If Slot is reserved,
the Slot pointed by `head` (i.e. `S5`) would be returned, and `head`
would be updated with the returned Slot's link value, which is
pointing to `S0`.

Segment header includes a counter which counts how many Slots have
been initialized with a link. Each time `sm_get` is performed, one
more link is added, i.e. lazy initialization.

If Segment allocation is allowed, the new Segment is allocated and
linked to the previous Segment. Segments constitute another singly
linked list. Host includes the total count of used and free Slots and
each Segment have the local counters and details.

Segman allows user hooks for `get` and `put` events. If Segman is
compiled with `SEGMAN_USE_HOOKS` option, the hooks are active.

If custom memory management is preferred, the Segman can be configured
to use user allocation and de-allocation functions. 

See Doxygen docs and `segman.h` for details about Segman API. Also
consult the test directory for usage examples.


## Segman API documentation

See Doxygen documentation. Documentation can be created with:

    shell> doxygen .doxygen


## Examples

All functions and their use is visible in tests. Please refer `test`
directory for testcases.


## Building

Ceedling based flow is in use:

    shell> ceedling

Testing:

    shell> ceedling test:all

User defines can be placed into `project.yml`. Please refer to
Ceedling documentation for details.


## Ceedling

Segman uses Ceedling for building and testing. Standard Ceedling files
are not in GIT. These can be added by executing:

    shell> ceedling new segman

in the directory above Segman. Ceedling prompts for file
overwrites. You should answer NO in order to use the customized files.
