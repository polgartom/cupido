#ifndef H_NEW_STRING
#define H_NEW_STRING

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <cstdlib>

#define IS_ALPHA(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
#define IS_DIGIT(c) (c >= '0' && c <= '9')
#define IS_ALNUM(c) (IS_ALPHA(c) || IS_DIGIT(c) || c == '_')
#define IS_SPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n')

#define SFMT "%.*s"
#define SARG(__s) (int) (__s).count, (__s).data 
#define SARGC(__s, __c) (int)__c, (__s).data 
// Usage: printf("This is an example: " SFMT "\n", SARG(value));

#define SCHAR(s) (*s.data)

const int STRING_MEM_ALIGN = 8;

struct String {
    // static constexpr const unsigned int byte_padding = 8;

    char *_sdata = nullptr; // the 'data' ptr can be vary, so we keep the initial data ptr
    char *data = nullptr;
    s64 count;
    
    char *alloc_location = nullptr; // If allocated on the heap
    s64 allocated_size;
    s64 used_size; // the count can vary

    String () {}
    
    String (const char *s)
    {
        data      = (char *)s;
        _sdata    = data;
        count     = strlen(s);
        used_size = count;
    }
    
    String (const char *s, u32 size)
    {
        data      = (char *)s;
        _sdata    = data;
        count     = size;
        used_size = count;
    }

    inline void operator=(const char *rhs)
    {
        String new_s = String(rhs);
        memcpy(&new_s, this, sizeof(String));
    }
    
};

inline void free(String s)
{
    if (s.alloc_location != nullptr) {
        free(s.alloc_location);
        s.alloc_location = nullptr;
        s.data = nullptr;
        s._sdata = nullptr;
        s.count = 0;
        s.used_size = 0;
    }
}

inline void free(String *s) 
{
    if (s->alloc_location != nullptr) {
        free(s->alloc_location);
        s->alloc_location = nullptr;
        s->data = nullptr;
        s->_sdata = nullptr;
        s->count = 0;
        s->used_size = 0;
    }
}


s64 find_index_from_left(String a, char *_b)
{
    if (_b == NULL) return -1;
    
    String b(_b);
    if (b.count > a.count) return -1;
    
    // @Speed: SIMD or just align the memory properly
    if (a.count == b.count) {
        for (s64 i = 0; i < a.count; i++) {
            if (a.data[i] != b.data[i]) return -1;
        }
        return 0;
    } else {
        for (s64 i = 0; i < a.count; i++) {
            if (a.data[i] == b.data[0]) {
                if (i + b.count-1 >= a.count) return -1;
                
                for (s64 j = 0; j < b.count; j++) {
                    if (a.data[i+j] != b.data[j]) {
                        goto _not_found;
                    }
                }
                
                return i;
            }
            
            _not_found:;
        }
    }
    
    return -1;
}

inline String advance(String s, unsigned int step = 1)
{
    assert(s.count >= step && step >= 0);

    s.data   = s.data + step; 
    s.count -= step;
    
    return s;
}

inline void advance(String *s, unsigned int step = 1) 
{
    String r = advance(*s, step);
    s->data = r.data;
    s->count = r.count;
}

inline String chop(String s, int at, String *rem = NULL)
{
    assert(s.count > at);
    
    if (rem) {
        *rem = advance(s, at);
    }
    
    s.count = at;
    
    return s;
}

inline String split(String s, char *delimeter, String *rem = nullptr, bool *success = nullptr)
{
    assert(delimeter);
    
    int at = find_index_from_left(s, delimeter);
    if (at == -1) {
        if (success != nullptr) *success = false;
        return s;
    }
    
    String r = chop(s, at, rem);
    if (rem) *rem = advance(*rem, strlen(delimeter));
    if (success != nullptr) *success = true;
    
    return r;
}

void alloc(String *s, s64 amount, float headroom_percent = 1.5)
{
    assert(amount >= 0);
    if (amount == 0) return;

    // The 'data' can moved by 'advance' procedures, so we need to restore the location if
    // the realloc is "moved" the memory to elsewhere. 
    u64 backup_data_offset = s->alloc_location ? (s->data - s->alloc_location) : 0;
    
    s64 new_size = (s->used_size + amount) * headroom_percent;
    s->alloc_location = (char *)realloc(s->alloc_location, new_size);
    assert(s->alloc_location);
    s->allocated_size = new_size;
    
    s->_sdata = s->alloc_location;
    s->data = s->alloc_location + backup_data_offset;
}

void join(String *a, char *b)
{
    assert(b);
    auto b_len = strlen(b);
    if (b_len == 0) return;
    
    if (a->used_size + b_len >= a->allocated_size) {
        alloc(a, b_len);
    }
    
    memcpy_s(a->data + a->used_size, a->allocated_size - a->used_size, b, b_len);
    assert(errno == 0);
    
    a->count += b_len;
    a->used_size += b_len;
}

inline void string_switch_to_heap(String *s)
{
    join(s, s->data);
}

inline char *string_to_new_cstr(String s)
{
    char *c_str = (char *)malloc(s.count+1);
    assert(c_str);
    ZERO_MEMORY(c_str, s.count+1);
    memcpy(c_str, s.data, s.count);
    return c_str;
}

bool string_equal(String a, String b)
{
    if (a.count != b.count) return false;

    for (int i = 0; i < a.count; i++) {
        if (a.data[i] != b.data[i]) {
            return false;
        }
    }
    
    return true;
}

inline bool string_equal_cstr(String a, char *b)
{
    return string_equal(a, String(b));
}

inline bool string_starts_with(String a, char *b)
{
    return find_index_from_left(a, b) == 0;
}

inline bool string_starts_with_and_step(String *s, char *b)
{
    int i = find_index_from_left(*s, b);
    if (i != 0) return false;
    *s = advance(*s, strlen(b));
    return true;
}

inline String string_trim_white_left(String s)
{
    while (s.count && IS_SPACE(*s.data)) {
        s.data += 1;
        s.count -= 1;
    }
    
    return s;
}

inline String string_trim_white_right(String s)
{
    while (s.count && IS_SPACE(s.data[s.count-1])) {
        s.count -= 1;
    }
    
    return s;
}

inline String string_trim_white(String s)
{
    s = string_trim_white_left(s);
    s = string_trim_white_right(s);
    
    return s;
}

inline int string_to_int(String s, String *remained = nullptr, int base = 0)
{
    // @Todo: return the remained data
    // @Speed: Make sure the s.data+1 is '\0'
    char *temp = string_to_new_cstr(s);
    int r = strtol(s.data, nullptr, base);
    free(temp);
    
    return r;
}

inline float string_to_float(String s, String *remained = nullptr)
{
    // @Todo: return the remained data
    // @Speed: Make sure the s.data+1 is '\0'
    char *temp = string_to_new_cstr(s);
    float r = strtof(temp, nullptr);
    free(temp);

    return r;
}

inline String string_eat_until(String s, const char c)
{
    // @Speed
    while (*s.data && *s.data != c) {
        advance(&s);
    }
    
    return s;
}

inline bool operator==(String &lhs, String &rhs)
{
    return string_equal(lhs, rhs);
}

inline bool operator==(String &lhs, char *rhs)
{
    return string_equal_cstr(lhs, rhs);
}

#endif

