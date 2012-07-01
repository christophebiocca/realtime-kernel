#import <debug.h>

#define HEAP(heaptype, valuetype, value_func, height, comp)\
    struct heaptype {\
        valuetype heap[1+(1<<height)];\
        int count;\
    };\
    \
    void heaptype ## SiftUp(struct heaptype *heap, int position){\
        if(position == 1) return;\
        if(value_func(heap->heap[position]) comp value_func(heap->heap[position/2])){\
            valuetype tmp = heap->heap[position];\
            heap->heap[position] = heap->heap[position/2];\
            heap->heap[position/2] = tmp;\
            heaptype ## SiftUp(heap, position/2);\
        }\
    }\
    \
    void heaptype ## SiftDown(struct heaptype *heap, int position){\
        if(position * 2 > heap->count) return;\
        if(value_func(heap->heap[position*2]) comp value_func(heap->heap[position])){\
            valuetype tmp = heap->heap[position];\
            heap->heap[position] = heap->heap[position*2];\
            heap->heap[position*2] = tmp;\
            heaptype ## SiftDown(heap, position*2);\
        } else if(position*2+1 <= heap->count &&\
            value_func(heap->heap[position*2+1]) comp value_func(heap->heap[position])){\
            valuetype tmp = heap->heap[position];\
            heap->heap[position] = heap->heap[position*2+1];\
            heap->heap[position*2+1] = tmp;\
            heaptype ## SiftDown(heap, position*2+1);\
        }\
    }\
    \
    void heaptype ## Push(struct heaptype *heap, valuetype value){\
        assert(heap->count < (1<<height));\
        heap->heap[++heap->count] = value;\
        heaptype ## SiftUp(heap, heap->count);\
    }\
    \
    valuetype heaptype ## Pop(struct heaptype *heap){\
        assert(heap->count > 0);\
        valuetype ret = heap->heap[1];\
        heap->heap[1] = heap->heap[heap->count--];\
        heaptype ## SiftDown(heap, 1);\
        return ret;\
    }
