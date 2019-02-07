/**
 * Cache simulation using a functional system simulator.
 *
 * Course: Advanced Computer Architecture, Uppsala University
 * Course Part: Lab assignment 1
 *
 * Original authors: UART 1.0(?)
 * Modified by: Andreas Sandberg <andreas.sandberg@it.uu.se>
 * Revision (2015, 2016, 2017, 2018): German Ceballos, Johan Janzen, Chris Sakalis
 *
 */

#include "avdark-cache.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#ifdef SIMICS
/* Simics stuff  */
#include <simics/api.h>
#include <simics/alloc.h>
#include <simics/utils.h>

#define AVDC_MALLOC(nelems, type) (type *) MM_MALLOC(nelems, type)
#define AVDC_FREE(p) MM_FREE(p)

#else

#define AVDC_MALLOC(nelems, type) (type *) malloc(nelems * sizeof(type))
#define AVDC_FREE(p) free(p)

#endif

int timestamp = 0;

/**
 * Cache block information.
 *
 * TODO: Fill in the data structure
 * HINT: You will probably need to change this structure
 */
struct avdc_cache_line {
    avdc_tag_t tag;
    int valid;
    int timestamp = 0;
};

/**
 * Extract the cache line tag from a physical address.
 *
 * You probably don't want to change this function, instead you may
 * want to change how the tag_shift field is calculated in
 * avdc_resize().
 */
static inline avdc_pa_t
tag_from_pa(avdark_cache_t *self, avdc_pa_t pa) {
    return pa >> self->tag_shift;
}

void direct_mapped_cache(avdark_cache_t *self, const avdc_pa_t &pa, const avdc_access_type_t &type,
                         const avdc_tag_t &tag, int index);

/**
 * Calculate the cache line index from a physical address.
 *
 * Feel free to experiment and change this function
 */
static inline int
index_from_pa(avdark_cache_t *self, avdc_pa_t pa) {
    return (pa >> self->block_size_log2) & (self->number_of_sets - 1);
}

/**
 * Computes the log2 of a 32 bit integer value. Used in dc_init
 *
 * Do NOT modify!
 */
static int
log2_int32(uint32_t value) {
    int i;

    for (i = 0; i < 32; i++) {
        value >>= 1;
        if (value == 0)
            break;
    }
    return i;
}

/**
 * Check if a number is a power of 2. Used for cache parameter sanity
 * checks.
 *
 * Do NOT modify!
 */
static int
is_power_of_two(uint64_t val) {
    return ((((val) & (val - 1)) == 0) && (val > 0));
}

void
avdc_dbg_log(avdark_cache_t *self, const char *msg, ...) {
    va_list ap;

    if (self->dbg) {
        const char *name = self->dbg_name ? self->dbg_name : "AVDC";
        fprintf(stderr, "[%s] dbg: ", name);
        va_start(ap, msg);
        vfprintf(stderr, msg, ap);
        va_end(ap);
    }
}

void print_cache_state(avdark_cache_t *self) {
    fprintf(stderr, "\n\nCache status: \n");
    int lines = self->assoc * self->number_of_sets;
    for (int j = 0; j < lines; ++j) {
        fprintf(stderr, "Cacheline %d (Valid: %d) - timestamp: %d \n", j,
                self->lines[j].valid, self->lines[j]
                        .timestamp);
    }
}

int lru(avdark_cache_t *self, int index) {
    // find out which cacheline has this timestamp and return its index
    int smallest_timestamp = 65535;
    int cacheline = self->lines[index].timestamp;
    int lines = self->assoc * self->number_of_sets;
    for (int i = index; i < lines; i += self->number_of_sets) {
        if (self->lines[i].timestamp < smallest_timestamp) {
            smallest_timestamp = self->lines[i].timestamp;
            cacheline = i;
        }
    }

    //fprintf(stderr, "LRU Result: cacheline: %d\n", cacheline);
    return cacheline;
}

bool all_misses(int hits[], int associativity) {
    bool all_misses = true;

    for (int i = 0; i < associativity; i++) {
        if (hits[i] == 1) {
            all_misses = false;
        }
    }

    return all_misses;
}

void check_hits_in_sets(avdark_cache_t *self, int hits[], int index, int associativity, const avdc_tag_t tag) {
    int _index = index;
    for (int i = 0; i < associativity; i++) {
        hits[i] = self->lines[_index].valid && self->lines[_index].tag == tag;

        _index += self->number_of_sets;
    }
}

void fill_cache_line(avdark_cache_t *self, int associativity, int index, const avdc_tag_t tag, int timestamp) {
    int position = associativity == 1 ? index : lru(self, index);
    self->lines[position].valid = 1;
    self->lines[position].tag = tag;
    self->lines[position].timestamp = timestamp;
}

void record_statistics(avdark_cache_t *self, avdc_pa_t pa, avdc_access_type_t type, int only_misses, int index,
                       int hits[], const avdc_tag_t tag) {
    switch (type) {
        case AVDC_READ:
            /*avdc_dbg_log(self,
                         "read: pa: 0x%.16lx, tag: 0x%.16lx, index: %d, hit: %d\n",
                         (unsigned long) pa, (unsigned long) tag, index, hits[0]);*/
            self->stat_data_read += 1;
            if (only_misses)
                self->stat_data_read_miss += 1;
            break;

        case AVDC_WRITE:
            /*avdc_dbg_log(self,
                         "write: pa: 0x%.16lx, tag: 0x%.16lx, index: %d, hit: %d\n",
                         (unsigned long) pa, (unsigned long) tag, index, hits[0]);*/
            self->stat_data_write += 1;
            if (only_misses)
                self->stat_data_write_miss += 1;
            break;
    }
}

void
avdc_access(avdark_cache_t *self, avdc_pa_t pa, avdc_access_type_t type) {
    avdc_tag_t tag = tag_from_pa(self, pa);
    int index = index_from_pa(self, pa);
    int associativity = (int) self->assoc;
    int hits[associativity] = {-1};

    check_hits_in_sets(self, hits, index, associativity, tag);
    bool only_misses = all_misses(hits, associativity);

    if (only_misses) {
        fill_cache_line(self, associativity, index, tag, timestamp);
    }

    record_statistics(self, pa, type, only_misses, index, hits, tag);

    timestamp++;
}

void
avdc_flush_cache(avdark_cache_t *self) {

    fprintf(stderr, "cache flushed! \n\n");
    int lines = self->assoc * self->number_of_sets;
    fprintf(stderr, "%d\n", lines);
    for (int i = 0; i < lines; i++) {
        if (i % 1000 == 0) fprintf(stderr, "i = %d\n", i);
        self->lines[i].valid = 0;
        self->lines[i].tag = 0;
        self->lines[i].timestamp = 0;
    }
}


int
avdc_resize(avdark_cache_t *self,
            avdc_size_t size, avdc_block_size_t block_size, avdc_assoc_t assoc) {
    /* TODO: Update this function */
    /* HINT: This function precomputes some common values and
     * allocates the self->lines array. You will need to update
     * this to reflect any changes to how this array is supposed
     * to be allocated.
     */

    /* Verify that the parameters are sane */
    if (!is_power_of_two(size) ||
        !is_power_of_two(block_size) ||
        !is_power_of_two(assoc)) {
        fprintf(stderr, "size, block-size and assoc all have to be powers of two and > zero\n");
        return 0;
    }

    /* Update the stored parameters */
    self->size = size;
    self->block_size = block_size;
    self->assoc = assoc;

    /* Cache some common values */
    self->number_of_sets = (self->size / self->block_size) / self->assoc;
    self->block_size_log2 = log2_int32(self->block_size);
    self->tag_shift = self->block_size_log2 + log2_int32(self->number_of_sets);

    /* (Re-)Allocate space for the tags array */
    if (self->lines)
        AVDC_FREE(self->lines);
    /* HINT: If you change this, you may have to update
     * avdc_delete() to reflect changes to how thie self->lines
     * array is allocated. */
    self->lines = AVDC_MALLOC(self->number_of_sets * self->assoc, avdc_cache_line_t);

    /* Flush the cache, this initializes the tag array to a known state */
    avdc_flush_cache(self);

    return 1;
}

void
avdc_print_info(avdark_cache_t *self) {
    fprintf(stderr, "Cache Info\n");
    fprintf(stderr, "size: %d, assoc: %d, line-size: %d\n",
            self->size, self->assoc, self->block_size);
}

void
avdc_print_internals(avdark_cache_t *self) {
    int i;

    fprintf(stderr, "Cache Internals\n");
    fprintf(stderr, "size: %d, assoc: %d, line-size: %d\n",
            self->size, self->assoc, self->block_size);

    for (i = 0; i < self->number_of_sets; i++)
        fprintf(stderr, "tag: <0x%.16lx> valid: %d\n",
                (long unsigned int) self->lines[i].tag,
                self->lines[i].valid);
}

void
avdc_reset_statistics(avdark_cache_t *self) {
    self->stat_data_read = 0;
    self->stat_data_read_miss = 0;
    self->stat_data_write = 0;
    self->stat_data_write_miss = 0;
}

avdark_cache_t *
avdc_new(avdc_size_t size, avdc_block_size_t block_size,
         avdc_assoc_t assoc) {
    avdark_cache_t *self;

    self = AVDC_MALLOC(1, avdark_cache_t);

    memset(self, 0, sizeof(*self));
    self->dbg = 0;

    if (!avdc_resize(self, size, block_size, assoc)) {
        AVDC_FREE(self);
        return NULL;
    }

    return self;
}

void
avdc_delete(avdark_cache_t *self) {
    if (self->lines)
        AVDC_FREE(self->lines);

    AVDC_FREE(self);
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 8
 * indent-tabs-mode: nil
 * c-file-style: "linux"
 * compile-command: "make -k -C ../../"
 * End:
 */
