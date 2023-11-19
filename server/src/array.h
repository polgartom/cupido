#ifndef H_ARRAY
#define H_ARRAY

#include <assert.h>

template <typename T>
struct Array {
    T *data = nullptr;
    long allocated = 0;
    long count = 0;
    long iterator_index = 0;

    // @Todo: Refcount!!
    // ~Array() {
    //     if (this->allocated != 0) {
    //         free(this->data);
    //     }
    // };

    T& operator[](unsigned long index) {
        // ASSERT(index < this->allocated || index >= this->allocated, "Array index out of bound");  
        ASSERT(index < this->count || index >= this->count, "Array index out of bound");  
        return this->data[index];
    }

    T* begin() { return data; }
    T* begin() const { return data; }
    T* end() { return count ? &data[count-1] : this->begin(); }
    T* end() const { return count ? &data[count-1] : this->begin(); }
};

template <typename T>
void array_add(Array<T> *arr, T item)
{
    if (arr->count >= arr->allocated) {
        long reserve = 2 * arr->allocated;
        if (reserve < 8) reserve = 8;
        if (!arr->data) {        
            arr->data = static_cast<T *>(malloc(sizeof(T) * reserve));
        } else {
            arr->data = static_cast<T *>(realloc(arr->data, sizeof(T) * reserve));
        }        
        arr->allocated = reserve;
    }

    arr->data[arr->count] = item;
    arr->count += 1;    
}

template <typename T>
Array<T> array_copy(Array<T> arr)
{
    auto new_arr = arr;
    auto size_in_bytes = sizeof(T) * arr.allocated;
    new_arr.data = static_cast<T *>(malloc(size_in_bytes));
    memcpy(new_arr.data, arr.data, size_in_bytes); // or arr.count
    assert(&new_arr.data[0] != &arr.data[0]);
    
    return new_arr;
}

template <typename T>
T array_last_item(Array<T> *arr)
{
    if (arr->count == 0) return nullptr;

    return arr->data[arr->count-1];
}

template <typename T>
T array_pop(Array<T> *arr)
{
    if (arr->count == 0) return nullptr;

    T *r = arr->data[arr->count-1];
    arr->count -= 1;

    return r;
}

#define For(_arr) for (auto it = _arr.begin(); it != _arr.end()+1; it++)
#define For_Index(_arr) for (auto it_index = 0; it_index < _arr.count; it_index++)

#endif 