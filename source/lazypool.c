#include "lazypool.h"
#include "lazypool_codes.h"
#include <stdlib.h>
#include <assert.h>

void _set_err_(int code, int *err)
{
    if (err)
    {
        *err = code;
    }
}

int _is_valid_ptr_(void *ptr, struct _lazysubpool_ *pool)
{
    assert(ptr && "ptr can't be NULL");
    assert(pool && "pool can't be NULL");

    void *min_ptr = pool->slots + (sizeof(struct _lazyslot_) * pool->slot_count);
    void *max_ptr = min_ptr + (pool->slot_size * (pool->slot_count - 1));

    return ptr >= min_ptr && ptr <= max_ptr;
}

void _init_slots_(struct _lazysubpool_ *subpool)
{
    void *data = subpool->slots;
    unsigned long slot_size = subpool->slot_size;
    unsigned long slot_count = subpool->slot_count;
    struct _lazyslot_ *actual = subpool->free_slots;

    for (unsigned long i = 0; i < slot_count; i++)
    {
        if (i + 1 < slot_count)
        {
            actual->next = data + (sizeof(struct _lazyslot_) * (i + 1));
        }
        else
        {
            actual->next = NULL;
        }

        actual->slot = data + ((sizeof(struct _lazyslot_) * slot_count) + (slot_size * i));
        actual = actual->next;
    }
}

int _create_subpool_(unsigned long slot_size, unsigned long slot_count, struct _lazysubpool_ **out_subpool)
{
    void *data = NULL;
    struct _lazysubpool_ *subpool = NULL;

    subpool = (struct _lazysubpool_ *)malloc(sizeof(struct _lazysubpool_));

    if (!subpool)
    {
        return LAZYPOOL_ERR_ALLOC_MEM;
    }

    data = malloc((sizeof(struct _lazyslot_) * slot_count) + (slot_size * slot_count));

    if (!data)
    {
        free(subpool);
        return LAZYPOOL_ERR_ALLOC_MEM;
    }

    subpool->slot_size = slot_size;
    subpool->slot_count = slot_count;
    subpool->used_count = 0;
    subpool->slots = data;
    subpool->used_slots = NULL;
    subpool->free_slots = data;
    subpool->next = NULL;

    _init_slots_(subpool);

    *out_subpool = subpool;

    return LAZYPOOL_OK;
}

void _destroy_subpool_(struct _lazysubpool_ *subpool)
{
    if (!subpool)
    {
        return;
    }

    free(subpool->slots);

    subpool->slot_size = 0;
    subpool->slot_count = 0;
    subpool->used_count = 0;
    subpool->slots = NULL;
    subpool->used_slots = NULL;
    subpool->free_slots = NULL;
    subpool->next = NULL;

    free(subpool);
}

int _create_pool_(struct _lazypool_ **out_pool)
{
    struct _lazypool_ *pool = (struct _lazypool_ *)malloc(sizeof(struct _lazypool_));

    if (!pool)
    {
        return LAZYPOOL_ERR_ALLOC_MEM;
    }

    pool->slot_count = 0;
    pool->used_count = 0;
    pool->subpool_count = 0;
    pool->used_subpools = NULL;
    pool->free_subpools = NULL;

    *out_pool = pool;

    return LAZYPOOL_OK;
}

void _destroy_pool_(struct _lazypool_ *pool)
{
    if (!pool)
    {
        return;
    }

    pool->slot_count = 0;
    pool->used_count = 0;
    pool->subpool_count = 0;
    pool->free_subpools = NULL;

    free(pool);

    return;
}

int _add_subpool_(unsigned long slot_size, unsigned long slot_count, struct _lazypool_ *pool, struct _lazysubpool_ **out_subpool)
{
    struct _lazysubpool_ *subpool = NULL;

    if (_create_subpool_(slot_size, slot_count, &subpool))
    {
        return LAZYPOOL_ERR_ALLOC_MEM;
    }

    struct _lazysubpool_ *looser = pool->free_subpools;

    if (looser)
    {
        looser->next = NULL;
    }

    if (out_subpool)
    {
        *out_subpool = looser;
    }

    pool->slot_count += slot_count;
    pool->subpool_count += 1;
    pool->free_subpools = subpool;

    return LAZYPOOL_OK;
}

void _remove_from_head_(struct _lazysubpool_ **head, struct _lazypool_ *pool)
{
    assert(head || *head && "head can't be NULL");
    assert(pool && "pool can't be NULL");

    struct _lazysubpool_ *next = (*head)->next;
    unsigned long slot_count = (*head)->slot_count;

    _destroy_subpool_(*head);

    pool->slot_count -= slot_count;
    pool->subpool_count -= 1;

    *head = next;
}

void _remove_used_head_pool(struct _lazypool_ *pool)
{
    assert(pool && "pool can't be NULL");

    if (pool->used_subpools)
    {
        _remove_from_head_(&pool->used_subpools, pool);
    }
}

void _remove_free_head_pool_(struct _lazypool_ *pool)
{
    assert(pool && "pool can't be NULL");

    if (pool->free_subpools)
    {
        _remove_from_head_(&pool->free_subpools, pool);
    }
}

struct _lazyslot_ *_get_slot_(unsigned long index, struct _lazysubpool_ *subpool)
{
    assert(index < subpool->slot_count && "index is out of bounds!");
    assert(subpool && "subpool can't be NULL");

    return (((struct _lazyslot_ *)subpool->slots) + index);
}

unsigned long _get_index_from_ptr_(void *ptr, struct _lazysubpool_ *subpool)
{
    assert(ptr && "ptr can't be NULL");
    assert(subpool && "subpool can't be NULL");

    struct _lazyslot_ *last = _get_slot_(subpool->slot_count - 1, subpool);

    return ((struct _lazyslot_ *)ptr) - last;
}

void *_allocate_from_subpool_(struct _lazysubpool_ *subpool, int *err)
{
    assert(subpool && "subpool can't be NULL");

    if (subpool->used_count == subpool->slot_count)
    {
        _set_err_(LAZYPOOL_ERR_FULL, err);
        return NULL;
    }

    struct _lazyslot_ *winner = subpool->free_slots;
    void *slot = winner->slot;

    subpool->free_slots = winner->next;

    winner->next = NULL;
    winner->slot = NULL;

    subpool->used_count += 1;

    _set_err_(LAZYPOOL_OK, err);
    return slot;
}

int _deallocate_from_subpool_(void **ptr, struct _lazysubpool_ *subpool)
{
    assert(ptr || *ptr && "ptr can't be NULL");
    assert(subpool && "subpool can't be NULL");

    if (!_is_valid_ptr_(*ptr, subpool))
    {
        return LAZYPOOL_ERR_ILLEGAL_PTR;
    }

    unsigned long index = _get_index_from_ptr_(*ptr, subpool);
    struct _lazyslot_ *winner = _get_slot_(index, subpool);

    winner->slot = *ptr;
    winner->next = subpool->free_slots;

    subpool->used_count -= 1;
    subpool->free_slots = winner;

    return LAZYPOOL_OK;
}

int _deallocate_all_from_subpool_(struct _lazysubpool_ *pool)
{
    assert(pool && "pool can't be NULL");

    int count = pool->used_count;

    pool->used_count = 0;
    pool->used_slots = NULL;
    pool->free_slots = pool->slots;

    _init_slots_(pool);

    return count;
}

int _deallocate_from_used_subpools(void **ptr, struct _lazypool_ *pool)
{
    assert(ptr || *ptr && "ptr can't be NULL");
    assert(pool && "pool can't be NULL");

    struct _lazysubpool_ *actual = pool->used_subpools;
    struct _lazysubpool_ *before = NULL;

    while (actual)
    {
        struct _lazysubpool_ *next = actual->next;

        if (_is_valid_ptr_(*ptr, actual))
        {
            int err = _deallocate_from_subpool_(ptr, actual);

            if (err)
            {
                return err;
            }

            pool->used_count -= 1;

            actual->next = pool->free_subpools;
            pool->free_subpools = actual;

            if (before)
            {
                before->next = next;
            }
            else
            {
                pool->used_subpools = next;
            }

            return LAZYPOOL_OK;
        }

        before = actual;
        actual = next;
    }

    return LAZYPOOL_ERR_ILLEGAL_PTR;
}

int _deallocate_from_free_subpools(void **ptr, struct _lazypool_ *pool)
{
    assert(ptr || *ptr && "ptr can't be NULL");
    assert(pool && "pool can't be NULL");

    struct _lazysubpool_ *actual = pool->free_subpools;
    struct _lazysubpool_ *before = NULL;

    while (actual)
    {
        struct _lazysubpool_ *next = actual->next;

        if (_is_valid_ptr_(*ptr, actual))
        {
            int err = _deallocate_from_subpool_(ptr, actual);

            if (err)
            {
                return err;
            }

            pool->used_count -= 1;

            return LAZYPOOL_OK;
        }

        before = actual;
        actual = next;
    }

    return LAZYPOOL_ERR_ILLEGAL_PTR;
}

unsigned long lazypool_used_bytes(struct _lazypool_ *pool)
{
    unsigned long slot_size = pool->free_subpools->slot_size;
    return pool->used_count * slot_size;
}

unsigned long lazypool_available_bytes(struct _lazypool_ *pool)
{
    unsigned long slot_size = pool->free_subpools->slot_size;
    return (pool->slot_count - pool->used_count) * slot_size;
}

unsigned long lazypool_total_bytes(struct _lazypool_ *pool)
{
    unsigned long slot_size = pool->free_subpools->slot_size;
    return pool->slot_count * slot_size;
}

unsigned long lazypool_free_unused(struct _lazypool_ *pool)
{
    if (pool->subpool_count == 1)
    {
        return 0;
    }

    unsigned long freeded_count = 0;
    struct _lazysubpool_ *actual = pool->free_subpools;
    struct _lazysubpool_ *before = NULL;

    while (actual)
    {
        struct _lazysubpool_ *next = actual->next;

        if (actual->used_count == 0)
        {

            freeded_count += actual->slot_count;

            pool->subpool_count -= 1;
            pool->slot_count -= actual->slot_count;

            _destroy_subpool_(actual);

            if (before)
            {
                before->next = next;
            }
            else
            {
                pool->free_subpools = next;
            }
        }

        before = actual;
        actual = next;
    }

    return freeded_count;
}

int lazypool_create(unsigned long slot_size, unsigned long slot_count, struct _lazypool_ **out_pool)
{
    struct _lazypool_ *pool = NULL;

    if (_create_pool_(&pool))
    {
        return LAZYPOOL_ERR_ALLOC_MEM;
    }

    if (_add_subpool_(slot_size, slot_count, pool, NULL))
    {
        free(pool);
        return LAZYPOOL_ERR_ALLOC_MEM;
    }

    *out_pool = pool;

    return LAZYPOOL_OK;
}

void *lazypool_allocate(struct _lazypool_ *pool, int *err)
{
    // If the pool is full, lets create a new subpool
    if (pool->used_count == pool->slot_count)
    {
        struct _lazysubpool_ *filled_subpool = NULL; // The already filled subpool will be saved here

        // We use the same size and slot count for all the subpools
        unsigned slot_size = pool->free_subpools->slot_size;
        unsigned slot_count = pool->free_subpools->slot_count;

        // In case of error, return the error NULL and set the error to err variable
        if (_add_subpool_(slot_size, slot_count, pool, &filled_subpool))
        {
            _set_err_(LAZYPOOL_ERR_ALLOC_MEM, err);
            return NULL;
        }

        // filled_subpool should never be null due to from the begining
        // the pool is created with a empty subpool. _add_subpool_ replace the filled
        // one with the new created, and the filled should be in filled_subpool variable
        assert(filled_subpool && "filled subpool can't be NULL");

        // We move the already filled subpool to the list of filled subpools
        filled_subpool->next = pool->used_subpools;
        pool->used_subpools = filled_subpool;
    }

    // Allocating from the first subpool of the list of subpools with available space
    struct _lazysubpool_ *subpool = pool->free_subpools;
    void *ptr = _allocate_from_subpool_(subpool, err);

    // If we reach this point must be space avaible. So, we assert that
    assert(ptr && "ptr can't be NULL");

    pool->used_count += 1;

    return ptr;
}

int lazypool_deallocate(void **ptr, struct _lazypool_ *pool)
{
    if (!ptr || !*ptr)
    {
        return LAZYPOOL_OK;
    }

    // When a subpool filled spool get space, it's inmediately, moved
    // to the list of subpools with available space in order to always
    // have fast allocations. Due to that, some allocated information
    // reside in the list of "free subpools". That's why we chech the
    // pointer in both list of subpools, used and free.

    // But first, we look inside the list of used (or totally filled subpools).
    int err = -1;
    err = _deallocate_from_used_subpools(ptr, pool);

    // If err is greater than 0, means a error, which is the case when the
    // pointer were not found in any of the filled subpools. If that happens,
    // we need to check inside the list of totally or partially freed subpools.
    if (err)
    {
        // We return the result from trying to free the pointer from the
        // list of totally or partially freed subpools directly because
        // a error indicate that the pointer is owned by the pool.
        // If result is not a error, the the pointer where located and "freeded".
        return _deallocate_from_free_subpools(ptr, pool);
    }

    return LAZYPOOL_OK;
}

void lazypool_deallocate_all(struct _lazypool_ *pool)
{
    struct _lazysubpool_ *actual_free = pool->free_subpools;
    struct _lazysubpool_ *actual_used = pool->used_subpools;

    struct _lazysubpool_ *last_free = actual_free;

    while (actual_free || actual_used)
    {
        if (actual_free)
        {
            pool->used_count -= _deallocate_all_from_subpool_(actual_free);
            last_free = actual_free;
            actual_free = actual_free->next;
        }

        if (actual_used)
        {
            pool->used_count -= _deallocate_all_from_subpool_(actual_used);
            actual_used = actual_used->next;
        }
    }

    if (last_free && pool->used_subpools)
    {
        last_free->next = pool->used_subpools;
    }

    pool->used_subpools = NULL;
}

void lazypool_destroy(struct _lazypool_ **pool)
{
    if (!pool || !*pool)
    {
        return;
    }

    while ((*pool)->free_subpools || (*pool)->used_subpools)
    {

        _remove_used_head_pool(*pool);
        _remove_free_head_pool_(*pool);
    }

    _destroy_pool_(*pool);

    *pool = NULL;
}