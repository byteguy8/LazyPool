// LazyPool 1.0.0

#ifndef _LAZYPOOL_H_
#define _LAZYPOOL_H_

struct _lazyslot_
{
    struct _lazyslot_ *next;
    void *slot;
};

struct _lazysubpool_
{
    unsigned long slot_size;
    unsigned long slot_count;
    unsigned long used_count;
    void *slots;
    struct _lazyslot_ *used_slots;
    struct _lazyslot_ *free_slots;
    struct _lazysubpool_ *next;
};

struct _lazypool_
{
    unsigned long slot_count;
    unsigned long used_count;
    unsigned long subpool_count;
    struct _lazysubpool_ *used_subpools;
    struct _lazysubpool_ *free_subpools;
};

typedef struct _lazypool_ LazyPool;

/**
 * @brief Returns the number of bytes allocated from the
 * specified pool.
 *
 * @param pool Pointer to the pool. Must not be NULL.
 * @return The space, in bytes, have been allocated, by the
 * user, from this pool.
 */
unsigned long lazypool_used_bytes(struct _lazypool_ *pool);

/**
 * @brief Returns the number available bytes that the specified pool
 * has.
 *
 * @param pool Pointer to the pool. Must not be NULL.
 * @return The space, in bytes, this pool has available.
 */
unsigned long lazypool_available_bytes(struct _lazypool_ *pool);

/**
 * @brief Returns the number of bytes that the specified pool
 * had allocated from the system. This is not, necessary, the
 * current space available in the pool.
 *
 * @param pool Pointer to the pool. Must not be NULL.
 * @return The space, in bytes, this pool owns.
 */
unsigned long lazypool_total_bytes(struct _lazypool_ *pool);

/**
 * @brief Creates a new pool with capacity specified in the arguments
 *
 * @param slot_size This is the size of the data, individually, you want
 * the pool to store.
 * @param slot_count This is the count of the individual data you want
 * the pool to store.
 * @param out_pool Pointer to a pointer with the new created pool. Must not be NULL.
 * @return 0 in case of no error, and a value
 * greater than 0, specifying the type of error, otherwise.
 * Possible values: LAZYPOOL_ERR_ALLOC_MEM in case of error, and LAZYPOOL_OK otherwise.
 */
int lazypool_create(unsigned long slot_size, unsigned long slot_count, struct _lazypool_ **out_pool);

/**
 * @brief Allocates a pointer from the pool.
 * If space not left, then tries to allocate more from the system.
 * The resulting new allocated space will be the same size of the first
 * specified when the pool were create.
 *
 * @param pool Pointer to the pool from which allocated the pointer.
 * Must not be NULL.
 * @param err A pointer in which the result of the calling will
 * be saved. The value is 0 in case of no error, and a value
 * greater than 0, specifying the type of error, otherwise.
 * Possible values: LAZYPOOL_ERR_ALLOC_MEM in case of error and LAZYPOOL_OK otherwise.
 * @return void* NULL, in case of error, a new allocated pointer, otherwise.
 */
void *lazypool_allocate(struct _lazypool_ *pool, int *err);

/**
 * @brief Deallocates a pointer from the pool. This function
 * will fail if the specified pointer is not owned by the specifie pool.
 *
 * @param ptr Pointer of pointer to the pointer to deallocate.
 * This function assign NULL to the pointer o pointer, cleaning
 * its state in a properly way.
 * @param pool Pointer to the pool from which allocated the pointer.
 * Must not be NULL.
 * @return 0 in case of no error, and a value
 * greater than 0, specifying the type of error, otherwise.
 * Possible values: LAZYPOOL_ERR_ILLEGAL_PTR in case the pointer
 * is not owned by the specified pool, and LAZYPOOL_OK otherwise.
 */
int lazypool_deallocate(void **ptr, struct _lazypool_ *pool);

/**
 * @brief Deallocates all pointer from the pool, leave clean.
 * All the space allocated from the system by the specified pool
 * remains in the pool. That space is only marked as available.
 * You should use this function with care.
 *
 * @param pool Pointer to the pool from which allocated the pointer.
 * Must not be NULL.
 */
void lazypool_deallocate_all(struct _lazypool_ *pool);

/**
 * @brief Returns to the system the allocated, not used, space
 * that the pool has. This doesn't return individual block of
 * pointer, it returns to the system a complete row of them but,
 * that row must be empty in order to be returned. The pool always
 * will remains with the intial space allocated when were created.
 * The returned space is only those allocated when a call to lazypool_allocate
 * got space from the system.
 *
 * @param pool
 * @return unsigned long
 */
unsigned long lazypool_free_unused(struct _lazypool_ *pool);

/**
 * @brief Destroys the specified pool, returning any allocated
 * space from the system.
 *
 * @param pool Pointer to the pool from which allocated the pointer.
 * Must not be NULL.
 */
void lazypool_destroy(struct _lazypool_ **pool);

#endif