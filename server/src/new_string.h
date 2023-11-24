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

struct String {
    // static constexpr const unsigned int byte_padding = 8;

    char *data = nullptr;
    int count;
    
    char *alloc_location = nullptr; // If allocated on the heap
    int allocated_size;

    String () {}
    
    String (const char *s)
    {
        data  = (char *)s;
        count = strlen(s);
    }

    inline void operator=(const char *rhs)
    {
        String new_s = String(rhs);
        memcpy(&new_s, this, sizeof(String));
    }
};

inline char schar(String &s)
{
    return *s.data;
}

inline String string_allocate(char *data)
{
    String s;
    s.alloc_location = (char *)malloc(padded_size);
    s.data           = (char *)memset(s.alloc_location, 0, (padded_size));
    s.count          = size;

    return s;
}

void free(String s)
{
    if (s.alloc_location != nullptr) {
        free(s.alloc_location);
        s.alloc_location = nullptr;
        s.data = nullptr;
    }
}

// inline String string_make_alloc(unsigned int size)
// {   
//     auto padded_size = (String::byte_padding - (size % String::byte_padding)) + size;
//     assert((padded_size % String::byte_padding) == 0);

//     String s;
//     s.alloc_location = (char *)malloc(padded_size);
//     s.data           = (char *)memset(s.alloc_location, 0, (padded_size));
//     s.count          = size;
    
//     return s;
// }

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

inline String chop(String s, unsigned int index)
{
    assert(s.count > index);
    s.count = index;
    return s;
}

void join(String *a, char *b)
{
    // @XXX: if we use the 'advance', then it'll cause memleak by incrementing the "data" pointer and decrementing
    //  the "count" field, therefore this calculations aren't accurate!!!

    assert(b);
    if (*b == '\0') return; // strlen is 0
    
    if (a->alloc_location == nullptr) {
        a->alloc_location = (char *)malloc((a->count + b_len)*1.5);
        memcpy(a->alloc_location, a->data, a->count); // copy it, if we have a constant string
        a->data = a->alloc_location;
        a->allocated_size = (a->count + b_len)*1.5;
    } 
    else if (a->count + b_len >= a->allocated_size) {
        a->alloc_location = (char *)realloc(a->alloc_location, (a->count + b_len)*1.5);
        a->allocated_size = (a->count + b_len)*1.5;
    }
    
    memcpy(a->data + a->count, b, b_len);
}  

inline char *string_to_cstr(String s)
{
    char *c_str = (char *)memset(((char *)malloc(s.count+1)), 0, (s.count+1));
    memcpy(c_str, s.data, s.count);
    return c_str;
}

inline char *to_cstr(String s)
{
    char *c_str = (char *)malloc(s.count+1);
    assert(c_str != NULL);
    memset(c_str, 0, s.count+1);
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
    return string_equal(a, string_create(b));
}

int find_index_from_left(String a, char *_b)
{
    if (!_b) return -1;
    
    String b(_b);
    if (b.count > a.count) return -1;
    
    // @Speed: SIMD or just align the memory properly
    if (a.count == b.count) {
        for (int i = 0; i < a.count; i++) {
            if (a.data[i] != b.data[i]) return -1;
        }
        
        return 0;
    } else {
        int len = a.count - b.count;
        bool found = false;
        for (int i = 0; i < len; i++) {
            found = true;
            for (int j = 0; j < b.count; j++) {
                if (a.data[i+j] != b.data[j]) {
                    found = false;
                    break;
                }
            }
            
            if (found) return i;
        } 
    
    }
    
    return -1;
}

inline bool string_starts_with(String a, char *b)
{
    return find_index_from_left(a, b) == 0;
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

int string_to_int(String s, bool *success, String *remained)
{
    assert(success != NULL);
    if (s.count == 0 || !s.data) {
        *success = false;
        return 0.0f;
    }

    char *o = remained->data;
    int i = strtol(s.data, &remained->data, 0);

    int bef = remained->count;    
    remained->count -= remained->data - o;

    *success = true;
    return i;
}

float string_to_float(String s, bool *success, String *remained)
{
    assert(success != NULL);
    if (s.count == 0 || !s.data) {
        *success = false;
        return 0.0f;
    }

    char *o = remained->data;
    float f = strtof(s.data, &remained->data);

    int bef = remained->count;    
    remained->count -= remained->data - o;

    *success = true;    
    return f;
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

