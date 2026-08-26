#include <stdlib.h>

#include "timer_heap.h"

#define BB_TIMER_HEAP_INITIAL_CAPACITY 16

struct bb_timer_heap {
    bb_timer_t *items;
    int count;
    int capacity;
};

bb_timer_heap_t *bb_timer_heap_create(void)
{
    bb_timer_heap_t *heap = malloc(sizeof(*heap));

    if (!heap)
    {
        return NULL;
    }

    heap->items = malloc(BB_TIMER_HEAP_INITIAL_CAPACITY * sizeof(*heap->items));

    if (!heap->items)
    {
        free(heap);
        return NULL;
    }

    heap->count = 0;
    heap->capacity = BB_TIMER_HEAP_INITIAL_CAPACITY;

    return heap;
}

void bb_timer_heap_destroy(bb_timer_heap_t *heap)
{
    if (!heap)
    {
        return;
    }

    free(heap->items);
    free(heap);
}

static void _swap(bb_timer_heap_t *heap, int a, int b)
{
    bb_timer_t tmp = heap->items[a];

    heap->items[a] = heap->items[b];
    heap->items[b] = tmp;
}

static void _sift_up(bb_timer_heap_t *heap, int idx)
{
    while (idx > 0)
    {
        int parent = (idx - 1) / 2;

        if (heap->items[parent].next_fire_ms <= heap->items[idx].next_fire_ms)
        {
            break;
        }

        _swap(heap, idx, parent);
        idx = parent;
    }
}

static void _sift_down(bb_timer_heap_t *heap, int idx)
{
    for (;;)
    {
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;
        int smallest = idx;

        if (left < heap->count && heap->items[left].next_fire_ms < heap->items[smallest].next_fire_ms)
        {
            smallest = left;
        }
        if (right < heap->count && heap->items[right].next_fire_ms < heap->items[smallest].next_fire_ms)
        {
            smallest = right;
        }
        if (smallest == idx)
        {
            break;
        }

        _swap(heap, idx, smallest);
        idx = smallest;
    }
}

static int _grow(bb_timer_heap_t *heap)
{
    int new_capacity = heap->capacity * 2;
    bb_timer_t *grown = realloc(heap->items, (size_t)new_capacity * sizeof(*grown));

    if (!grown)
    {
        return -1;
    }

    heap->items = grown;
    heap->capacity = new_capacity;

    return 0;
}

int bb_timer_heap_push(bb_timer_heap_t *heap, uint64_t next_fire_ms, uint64_t interval_ms, int repeating, bb_task_t *task)
{
    if (!heap || !task)
    {
        return -1;
    }

    if (heap->count >= heap->capacity && _grow(heap) != 0)
    {
        return -1;
    }

    int idx = heap->count;

    heap->items[idx].next_fire_ms = next_fire_ms;
    heap->items[idx].interval_ms = interval_ms;
    heap->items[idx].repeating = repeating;
    heap->items[idx].task = task;

    heap->count++;

    _sift_up(heap, idx);

    return 0;
}

bb_timer_t *bb_timer_heap_peek(bb_timer_heap_t *heap)
{
    if (!heap || heap->count == 0)
    {
        return NULL;
    }

    return &heap->items[0];
}

static void _remove_at(bb_timer_heap_t *heap, int idx)
{
    heap->count--;

    if (idx == heap->count)
    {
        return; // removed the last element in place, nothing to re-settle
    }

    heap->items[idx] = heap->items[heap->count];

    // The element that took idx's place could belong either above or below
    // it in fire-time order, so try both directions; only one can move it.
    _sift_down(heap, idx);
    _sift_up(heap, idx);
}

void bb_timer_heap_pop(bb_timer_heap_t *heap)
{
    if (!heap || heap->count == 0)
    {
        return;
    }

    _remove_at(heap, 0);
}

void bb_timer_heap_reschedule_top(bb_timer_heap_t *heap, uint64_t new_next_fire_ms)
{
    if (!heap || heap->count == 0)
    {
        return;
    }

    heap->items[0].next_fire_ms = new_next_fire_ms;

    _sift_down(heap, 0);
}

int bb_timer_heap_remove_task(bb_timer_heap_t *heap, bb_task_t *task)
{
    if (!heap || !task)
    {
        return 0;
    }

    int removed = 0;

    for (int i = 0; i < heap->count;)
    {
        if (heap->items[i].task == task)
        {
            _remove_at(heap, i);
            removed++;
            // The element now at i hasn't been checked yet.
        }
        else
        {
            i++;
        }
    }

    return removed;
}

bool bb_timer_heap_is_empty(bb_timer_heap_t *heap)
{
    return !heap || heap->count == 0;
}

int bb_timer_heap_count(bb_timer_heap_t *heap)
{
    return heap ? heap->count : 0;
}

bb_timer_t *bb_timer_heap_at(bb_timer_heap_t *heap, int idx)
{
    if (!heap || idx < 0 || idx >= heap->count)
    {
        return NULL;
    }

    return &heap->items[idx];
}
