#include "tag.h"

#include "fun.h"      // Fun, fun_*
#include "list.h"     // List, list_*
#include "mem.h"      // mem_*
#include "safemath.h" // i64_*_over
#include "str.h"      // String, Slice, string_*, slice_*
#include "table.h"    // Table, table_*

#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define SAFE_CAST(FROM, TO, VAL)                                                                   \
    (union {                                                                                       \
        FROM f;                                                                                    \
        TO t;                                                                                      \
    }){.f = VAL}                                                                                   \
        .t

static Tag error(const char *fmt, ...) {
    // TODO handle errors, and len > buf_size
    char buf[256];
    size_t buf_size = sizeof(buf) / sizeof(buf[0]);
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(&buf[0], buf_size, fmt, args);
    assert(len > 0);
    assert((unsigned int)len < buf_size);
    va_end(args);
    String *err_msg = string_new(buf, (unsigned int)len);
    Tag *error = mem_allocate(sizeof(*error));
    *error = string_to_tag(err_msg);
    return error_to_tag(error);
}

void tag_free_ptr(Tag t) {
    assert(tag_is_own(t));
    switch (tag_type(t)) {
    case TYPE_STRING:
        string_free(tag_to_string(t));
        break;
    case TYPE_TABLE:
        table_free(tag_to_table(t));
        break;
    case TYPE_LIST:
        list_free(tag_to_list(t));
        break;
    case TYPE_I64: {
        int64_t *i = tag_to_i64(t);
        mem_free(i, sizeof(*i));
        break;
    }
    case TYPE_ERROR: {
        Tag *error = tag_to_error(t);
        t = *error;
        mem_free(error, sizeof(*error));
        tag_free(t);
        break;
    }
    case TYPE_SLICE:
        slice_free(tag_to_slice(t));
        break;
    case TYPE_FUN:
        fun_free(tag_to_fun(t));
        break;
    case TYPE_I49P:
    case TYPE_I49N:
    case TYPE_SYMBOL:
    case TYPE_DOUBLE:
        assert(0 && "free on data tags");
    }
}

static const char *symbols[] = {"<false>", "<true>", "<nil>", "<ok>"};
static void print(FILE *f, Tag t, bool is_repr) {
    // TODO: OK to ignore output errors?
    switch (tag_type(t)) {
    case TYPE_STRING:
        if (is_repr) {
            string_reprf(f, tag_to_string(t));
        } else {
            string_printf(f, tag_to_string(t));
        }
        break;
    case TYPE_TABLE:
        table_printf(f, tag_to_table(t));
        break;
    case TYPE_LIST:
        list_printf(f, tag_to_list(t));
        break;
    case TYPE_I64:
        fprintf(f, "%" PRId64, *tag_to_i64(t));
        if (is_repr) {
            fputc('L', f);
        }
        break;
    case TYPE_ERROR:
        fputs("error: ", f);
        print(f, *tag_to_error(t), is_repr);
        break;
    case TYPE_SLICE:
        if (is_repr) {
            slice_reprf(f, tag_to_slice(t));
        } else {
            slice_printf(f, tag_to_slice(t));
        }
        break;
    case TYPE_FUN:
        fun_printf(f, tag_to_fun(t));
        break;
    case TYPE_DOUBLE: {
        double d = tag_to_double(t);
        fprintf(f, floor(d) == d ? "%.1f" : "%.16g", d);
        break;
    }
    case TYPE_SYMBOL: {
        Symbol s = tag_to_symbol(t);
        if (s >= SYM__COUNT) {
            fprintf(f, "<symbol: %d>", s);
        } else {
            fputs(symbols[s], f);
        }
        break;
    }
    case TYPE_I49P:
    case TYPE_I49N:
        fprintf(f, "%" PRId64, tag_to_i49(t));
        break;
    }
#ifdef SLANG_DEBUG
    if (is_repr && tag_is_ptr(t)) {
        char ownership_flag = tag_is_own(t) ? 'O' : 'R';
        fputc(ownership_flag, f);
    }
#endif
}

void tag_printf(FILE *f, Tag t) { print(f, t, false); }
void tag_reprf(FILE *f, Tag t) { print(f, t, true); }

static size_t int_hash(uint64_t i) { return i * 13 + 37; }
size_t tag_hash(Tag t) {
    switch (tag_type(t)) {
    case TYPE_STRING:
        return 0xFEEDFEED ^ string_hash(tag_to_string(t));
    case TYPE_TABLE:
    case TYPE_LIST:
        return 0xDEADBEEF ^ (((uintptr_t)tag_to_ptr(t)) >> sizeof(max_align_t));
    case TYPE_I64:
        return int_hash(SAFE_CAST(int64_t, uint64_t, *tag_to_i64(t)));
    case TYPE_ERROR:
        return 0xC0FFEE ^ tag_hash(*tag_to_error(t));
    case TYPE_SLICE:
        return 0xFEEDFEED ^ slice_hash(tag_to_slice(t));
    case TYPE_FUN:
        return 0xBAD0F00D ^ (((uintptr_t)tag_to_ptr(t)) >> sizeof(max_align_t));
    case TYPE_DOUBLE: {
        double d = tag_to_double(t);
        if (d == (int64_t)d) {
            return int_hash(SAFE_CAST(int64_t, uint64_t, d));
        }
        return SAFE_CAST(double, size_t, d);
    }
    case TYPE_SYMBOL:
        return 0xCACA0 ^ (tag_to_symbol(t) * 31 + 37);
    case TYPE_I49P:
    case TYPE_I49N:
        return int_hash(SAFE_CAST(int64_t, uint64_t, tag_to_i49(t)));
    }
}

bool tag_is_true(Tag t) {
    switch (tag_type(t)) {
    case TYPE_STRING:
        return string_len(tag_to_string(t));
    case TYPE_TABLE:
        return table_len(tag_to_table(t));
    case TYPE_LIST:
        return list_len(tag_to_list(t));
    case TYPE_I64:
        return *tag_to_i64(t);
    case TYPE_ERROR:
        return false;
    case TYPE_SLICE:
        return slice_len(tag_to_slice(t));
    case TYPE_FUN:
        return true;
    case TYPE_DOUBLE:
        return tag_to_double(t);
    case TYPE_SYMBOL:
        switch (tag_to_symbol(t)) {
        case SYM_TRUE:
        case SYM_OK:
            return true;
        default:
            return false;
        }
    case TYPE_I49P:
    case TYPE_I49N:
        return tag_to_i49(t);
    }
}

bool tag_eq(Tag a, Tag b) {
    if (tag_biteq(a, b)) {
        return true;
    }
    if (tag_is_ptr(a) && tag_is_ptr(b) && tag_to_ptr(a) == tag_to_ptr(b)) {
        // compare pointers disregarding ownership
        return true;
    }
    switch (tag_type(a)) {
    case TYPE_STRING:
        switch (tag_type(b)) {
        case TYPE_STRING:
            return string_eq_string(tag_to_string(a), tag_to_string(b));
        case TYPE_SLICE:
            return string_eq_slice(tag_to_string(a), tag_to_slice(b));
        default:
            return false;
        }
    case TYPE_TABLE:
        return tag_is_table(b) && table_eq(tag_to_table(a), tag_to_table(b));
    case TYPE_LIST:
        return tag_is_list(b) && list_eq(tag_to_list(a), tag_to_list(b));
    case TYPE_I64:
        switch (tag_type(b)) {
        case TYPE_I64:
            return *tag_to_i64(a) == *tag_to_i64(b);
        case TYPE_I49N:
        case TYPE_I49P:
            return *tag_to_i64(a) == tag_to_i49(b);
        case TYPE_DOUBLE:
            return *tag_to_i64(a) == tag_to_double(b);
        default:
            return false;
        }
    case TYPE_ERROR:
        return tag_is_error(b) && tag_eq(*tag_to_error(a), *tag_to_error(b));
    case TYPE_SLICE:
        switch (tag_type(b)) {
        case TYPE_STRING:
            return string_eq_slice(tag_to_string(b), tag_to_slice(a));
        case TYPE_SLICE:
            return slice_eq_slice(tag_to_slice(a), tag_to_slice(b));
        default:
            return false;
        }
    case TYPE_FUN:
        return tag_is_fun(b) && tag_to_ptr(a) == tag_to_ptr(b);
    case TYPE_DOUBLE:
        switch (tag_type(b)) {
        case TYPE_I64:
            return tag_to_double(a) == *tag_to_i64(b);
        case TYPE_I49N:
        case TYPE_I49P:
            return tag_to_double(a) == tag_to_i49(b);
        case TYPE_DOUBLE:
            return tag_to_double(a) == tag_to_double(b);
        default:
            return false;
        }
    case TYPE_I49P:
    case TYPE_I49N:
        switch (tag_type(b)) {
        case TYPE_I64:
            return tag_to_i49(a) == *tag_to_i64(b);
        case TYPE_I49N:
        case TYPE_I49P:
            return false; // sholud be tag_biteq() called at the top
        case TYPE_DOUBLE:
            return tag_to_i49(a) == tag_to_double(b);
        default:
            return false;
        }
    case TYPE_SYMBOL:
        return false; // should be tag_biteq() called at the top
    }
}

Tag i64_new(int64_t i) {
    int64_t *p = mem_allocate(sizeof(*p));
    *p = i;
    return i64_to_tag(p);
}

#define BINARY_MATH(mathf_i64_reuse, mathf_i64, mathf_i49, mathf_double)                           \
    case TYPE_I64: {                                                                               \
        int64_t l = *tag_to_i64(left);                                                             \
        switch (tag_type(right)) {                                                                 \
        case TYPE_I64: {                                                                           \
            int64_t r = *tag_to_i64(right);                                                        \
            if (tag_is_own(left)) {                                                                \
                tag_free(right);                                                                   \
                return mathf_i64_reuse(l, r, left);                                                \
            }                                                                                      \
            if (tag_is_own(right)) {                                                               \
                /* tag_free(left); -- not owned */                                                 \
                return mathf_i64_reuse(l, r, right);                                               \
            }                                                                                      \
            /* tag_free(left); -- not owned */                                                     \
            /* tag_free(right); -- not owned */                                                    \
            return mathf_i64(l, r);                                                                \
        }                                                                                          \
        case TYPE_I49P:                                                                            \
        case TYPE_I49N: {                                                                          \
            if (tag_is_own(left)) {                                                                \
                return mathf_i64_reuse(l, tag_to_i49(right), left);                                \
            }                                                                                      \
            /* tag_free(left); -- not owned */                                                     \
            return mathf_i64(l, tag_to_i49(right));                                                \
        }                                                                                          \
        case TYPE_DOUBLE: {                                                                        \
            tag_free(left);                                                                        \
            return mathf_double((double)l, tag_to_double(right));                                  \
        }                                                                                          \
        default:                                                                                   \
            break;                                                                                 \
        }                                                                                          \
        break;                                                                                     \
    }                                                                                              \
    case TYPE_I49P:                                                                                \
    case TYPE_I49N: {                                                                              \
        int64_t l = tag_to_i49(left);                                                              \
        switch (tag_type(right)) {                                                                 \
        case TYPE_I64: {                                                                           \
            int64_t r = *tag_to_i64(right);                                                        \
            if (tag_is_own(right)) {                                                               \
                return mathf_i64_reuse(l, r, right);                                               \
            }                                                                                      \
            /* tag_free(right); -- not owned */                                                    \
            return mathf_i64(l, r);                                                                \
        }                                                                                          \
        case TYPE_I49P:                                                                            \
        case TYPE_I49N:                                                                            \
            return mathf_i49(l, tag_to_i49(right));                                                \
        case TYPE_DOUBLE:                                                                          \
            return mathf_double((double)l, tag_to_double(right));                                  \
        default:                                                                                   \
            break;                                                                                 \
        }                                                                                          \
        break;                                                                                     \
    }                                                                                              \
    case TYPE_DOUBLE: {                                                                            \
        double l = tag_to_double(left);                                                            \
        switch (tag_type(right)) {                                                                 \
        case TYPE_I64: {                                                                           \
            int64_t r = *tag_to_i64(right);                                                        \
            tag_free(right);                                                                       \
            return mathf_double(l, (double)r);                                                     \
        }                                                                                          \
        case TYPE_I49P:                                                                            \
        case TYPE_I49N:                                                                            \
            return mathf_double(l, (double)tag_to_i49(right));                                     \
        case TYPE_DOUBLE:                                                                          \
            return mathf_double(l, tag_to_double(right));                                          \
        default:                                                                                   \
            break;                                                                                 \
        }                                                                                          \
        break;                                                                                     \
    }

static Tag add_i64(int64_t left, int64_t right) {
    int64_t result;
    if (i64_add_over(left, right, &result)) {
        return error("addition overflows");
    }
    return int_to_tag(result);
}

static Tag add_i64_reuse(int64_t left, int64_t right, Tag out) {
    int64_t result;
    if (i64_add_over(left, right, &result)) {
        tag_free_ptr(out);
        return error("addition overflows");
    }
    if (I49_MIN <= result && result <= I49_MAX) {
        tag_free_ptr(out);
        return i49_to_tag(result);
    }
    *tag_to_i64(out) = result;
    return out;
}

static Tag add_i49(int64_t left, int64_t right) {
    // can't oveflow
    return int_to_tag(left + right);
}

static Tag add_double(double left, double right) { return double_to_tag(left + right); }

Tag tag_add(Tag left, Tag right) {
    switch (tag_type(left)) {
        BINARY_MATH(add_i64_reuse, add_i64, add_i49, add_double)
    case TYPE_SLICE: {
        Slice *l = tag_to_slice(left);
        switch (tag_type(right)) {
        case TYPE_SLICE: {
            Slice *r = tag_to_slice(right);
            Tag result = string_to_tag(str_concat(l->c, l->len, r->c, r->len));
            tag_free(left);
            tag_free(right);
            return result;
        }
        case TYPE_STRING: {
            String *r = tag_to_string(right);
            Tag result = string_to_tag(str_concat(l->c, l->len, r->c, r->len));
            tag_free(left);
            tag_free(right);
            return result;
        }
        default:
            break;
        }
        break;
    }
    case TYPE_STRING: {
        String *l = tag_to_string(left);
        switch (tag_type(right)) {
        case TYPE_SLICE: {
            String *result;
            Slice *r = tag_to_slice(right);
            if (tag_is_own(left)) {
                result = string_append(l, r->c, r->len);
            } else {
                result = str_concat(l->c, l->len, r->c, r->len);
                // tag_free(left) -- not owned
            }
            tag_free(right);
            return string_to_tag(result);
        }
        case TYPE_STRING: {
            String *result;
            String *r = tag_to_string(right);
            if (tag_is_own(left)) {
                result = string_append(l, r->c, r->len);
            } else {
                result = str_concat(l->c, l->len, r->c, r->len);
                // tag_free(left) -- not owned
            }
            tag_free(right);
            return string_to_tag(result);
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
    tag_free(left);
    tag_free(right);
    const char *left_type = tag_type_str(tag_type(left));
    const char *right_type = tag_type_str(tag_type(right));
    return error("cannot add %s to %s", left_type, right_type);
}

static Tag mul_i64(int64_t left, int64_t right) {
    int64_t result;
    if (i64_mul_over(left, right, &result)) {
        return error("multiplication overflows");
    }
    return int_to_tag(result);
}

static Tag mul_i64_reuse(int64_t left, int64_t right, Tag out) {
    int64_t result;
    if (i64_mul_over(left, right, &result)) {
        tag_free_ptr(out);
        return error("multiplication overflows");
    }
    if (I49_MIN <= result && result <= I49_MAX) {
        tag_free_ptr(out);
        return i49_to_tag(result);
    }
    *tag_to_i64(out) = result;
    return out;
}

static Tag mul_double(double left, double right) { return double_to_tag(left * right); }

Tag tag_mul(Tag left, Tag right) {
    switch (tag_type(left)) {
        BINARY_MATH(mul_i64_reuse, mul_i64, mul_i64, mul_double)
    default:
        break;
    }
    tag_free(left);
    tag_free(right);
    const char *left_type = tag_type_str(tag_type(left));
    const char *right_type = tag_type_str(tag_type(right));
    return error("cannot multiply %s to %s", left_type, right_type);
}

static Tag div_i64(int64_t left, int64_t right) {
    int64_t result;
    if (i64_div_over(left, right, &result)) {
        return error(right == 0 ? "division by zero" : "division overflows");
    }
    return int_to_tag(result);
}

static Tag div_i64_reuse(int64_t left, int64_t right, Tag out) {
    int64_t result;
    if (i64_div_over(left, right, &result)) {
        tag_free_ptr(out);
        return error(right == 0 ? "division by zero" : "division overflows");
    }
    if (I49_MIN <= result && result <= I49_MAX) {
        tag_free_ptr(out);
        return i49_to_tag(result);
    }
    *tag_to_i64(out) = result;
    return out;
}

static Tag div_i49(int64_t left, int64_t right) {
    // can't oveflow
    return right == 0 ? error("division by zero") : i49_to_tag(left / right);
}

static Tag div_double(double left, double right) {
    return right == 0 ? error("division by zero") : double_to_tag(left / right);
}

Tag tag_div(Tag left, Tag right) {
    switch (tag_type(left)) {
        BINARY_MATH(div_i64_reuse, div_i64, div_i49, div_double)
    default:
        break;
    }
    tag_free(left);
    tag_free(right);
    const char *left_type = tag_type_str(tag_type(left));
    const char *right_type = tag_type_str(tag_type(right));
    return error("cannot divide %s to %s", left_type, right_type);
}

static Tag mod_i64(int64_t left, int64_t right) {
    int64_t result;
    if (i64_mod_over(left, right, &result)) {
        return error(right == 0 ? "division by zero" : "division overflows");
    }
    return int_to_tag(result);
}

static Tag mod_i64_reuse(int64_t left, int64_t right, Tag out) {
    int64_t result;
    if (i64_mod_over(left, right, &result)) {
        tag_free_ptr(out);
        return error(right == 0 ? "division by zero" : "division overflows");
    }
    if (I49_MIN <= result && result <= I49_MAX) {
        tag_free_ptr(out);
        return i49_to_tag(result);
    }
    *tag_to_i64(out) = result;
    return out;
}

static Tag mod_i49(int64_t left, int64_t right) {
    return right == 0 ? error("division by zero") : i49_to_tag(left % right);
}

static Tag mod_double(double left, double right) {
    return right == 0 ? error("division by zero") : double_to_tag(fmod(left, right));
}

Tag tag_mod(Tag left, Tag right) {
    switch (tag_type(left)) {
        BINARY_MATH(mod_i64_reuse, mod_i64, mod_i49, mod_double)
    default:
        break;
    }
    tag_free(left);
    tag_free(right);
    const char *left_type = tag_type_str(tag_type(left));
    const char *right_type = tag_type_str(tag_type(right));
    // Should the message say cannot modulo instead of divide?
    return error("cannot divide %s to %s", left_type, right_type);
}

#define STR_CMP(cmpf)                                                                              \
    case TYPE_STRING: {                                                                            \
        String *l = tag_to_string(left);                                                           \
        switch (tag_type(right)) {                                                                 \
        case TYPE_STRING: {                                                                        \
            String *r = tag_to_string(right);                                                      \
            Tag result = cmpf(l->c, l->len, r->c, r->len);                                         \
            tag_free(left);                                                                        \
            tag_free(right);                                                                       \
            return result;                                                                         \
        }                                                                                          \
        case TYPE_SLICE: {                                                                         \
            Slice *r = tag_to_slice(right);                                                        \
            Tag result = cmpf(l->c, l->len, r->c, r->len);                                         \
            tag_free(left);                                                                        \
            tag_free(right);                                                                       \
            return result;                                                                         \
        }                                                                                          \
        default:                                                                                   \
            break;                                                                                 \
        }                                                                                          \
        break;                                                                                     \
    }                                                                                              \
    case TYPE_SLICE: {                                                                             \
        Slice *l = tag_to_slice(left);                                                             \
        switch (tag_type(right)) {                                                                 \
        case TYPE_STRING: {                                                                        \
            String *r = tag_to_string(right);                                                      \
            Tag result = cmpf(l->c, l->len, r->c, r->len);                                         \
            tag_free(left);                                                                        \
            tag_free(right);                                                                       \
            return result;                                                                         \
        }                                                                                          \
        case TYPE_SLICE: {                                                                         \
            Slice *r = tag_to_slice(right);                                                        \
            Tag result = cmpf(l->c, l->len, r->c, r->len);                                         \
            tag_free(left);                                                                        \
            tag_free(right);                                                                       \
            return result;                                                                         \
        }                                                                                          \
        default:                                                                                   \
            break;                                                                                 \
        }                                                                                          \
        break;                                                                                     \
    }

static Tag less_i64(int64_t left, int64_t right) { return left < right ? TAG_TRUE : TAG_FALSE; }

static Tag less_i64_reuse(int64_t left, int64_t right, Tag out) {
    tag_free(out);
    return less_i64(left, right);
}

static Tag less_double(double left, double right) { return left < right ? TAG_TRUE : TAG_FALSE; }

static Tag less_str(const char *l, size_t l_len, const char *r, size_t r_len) {
    return str_cmp(l, l_len, r, r_len) < 0 ? TAG_TRUE : TAG_FALSE;
}

Tag tag_less(Tag left, Tag right) {
    switch (tag_type(left)) {
        BINARY_MATH(less_i64_reuse, less_i64, less_i64, less_double)
        STR_CMP(less_str)
    default:
        break;
    }
    tag_free(left);
    tag_free(right);
    const char *left_type = tag_type_str(tag_type(left));
    const char *right_type = tag_type_str(tag_type(right));
    return error("cannot compare %s to %s", left_type, right_type);
}

static Tag greater_i64(int64_t left, int64_t right) { return left > right ? TAG_TRUE : TAG_FALSE; }

static Tag greater_i64_reuse(int64_t left, int64_t right, Tag out) {
    tag_free(out);
    return greater_i64(left, right);
}

static Tag greater_double(double left, double right) { return left > right ? TAG_TRUE : TAG_FALSE; }

static Tag greater_str(const char *l, size_t l_len, const char *r, size_t r_len) {
    return str_cmp(l, l_len, r, r_len) > 0 ? TAG_TRUE : TAG_FALSE;
}

Tag tag_greater(Tag left, Tag right) {
    switch (tag_type(left)) {
        BINARY_MATH(greater_i64_reuse, greater_i64, greater_i64, greater_double)
        STR_CMP(greater_str)
    default:
        break;
    }
    tag_free(left);
    tag_free(right);
    const char *left_type = tag_type_str(tag_type(left));
    const char *right_type = tag_type_str(tag_type(right));
    return error("cannot compare %s to %s", left_type, right_type);
}

Tag tag_negate(Tag t) {
    switch (tag_type(t)) {
    case TYPE_I64:
        if (tag_is_own(t)) {
            *tag_to_i64(t) *= -1;
            return t;
        } else {
            // expect the negation to not fit an i49
            return i64_new(-*tag_to_i64(t));
            // free_tag(t) -- not owned
        }
        break;
    case TYPE_I49N:
    case TYPE_I49P:
        return i49_negate(t);
    case TYPE_DOUBLE:
        return double_to_tag(-tag_to_double(t));
    default: {
        Tag result = error("cannot negate %s", tag_type_str(tag_type(t)));
        tag_free(t);
        return result;
    }
    }
}

const char *tag_type_names[] = {"String", "Table", "List",  "Integer", "Integer",  "Integer",
                                "Symbol", "",      "Error", "String",  "Function", "Float"};

extern inline bool tag_biteq(Tag, Tag);
extern inline bool tag_is_ptr(Tag);
extern inline bool tag_is_data(Tag);
extern inline bool tag_is_own(Tag);
extern inline bool tag_is_ref(Tag);
extern inline Tag tag_to_ref(Tag);
extern inline void *tag_to_ptr(Tag);

extern inline bool tag_is_string(Tag);
extern inline Tag string_to_tag(const String *);
extern inline String *tag_to_string(Tag);

extern inline bool tag_is_table(Tag);
extern inline Tag table_to_tag(const Table *);
extern inline Table *tag_to_table(Tag);

extern inline bool tag_is_list(Tag);
extern inline Tag list_to_tag(const List *);
extern inline List *tag_to_list(Tag);

extern inline bool tag_is_i64(Tag);
extern inline Tag i64_to_tag(const int64_t *);
extern inline int64_t *tag_to_i64(Tag);

extern inline bool tag_is_error(Tag);
extern inline Tag error_to_tag(const Tag *);
extern inline Tag *tag_to_error(Tag);

extern inline bool tag_is_slice(Tag);
extern inline Tag slice_to_tag(const Slice *);
extern inline Slice *tag_to_slice(Tag);

extern inline bool tag_is_fun(Tag);
extern inline Tag fun_to_tag(const Fun *);
extern inline Fun *tag_to_fun(Tag);

extern inline bool tag_is_i49(Tag);
extern inline Tag i49_to_tag(int64_t);
extern inline int64_t tag_to_i49(Tag);
extern inline Tag i49_negate(Tag);

extern inline Tag int_to_tag(int64_t);

extern inline bool tag_is_symbol(Tag);
extern inline Symbol tag_to_symbol(Tag);

extern inline bool tag_is_double(Tag);
extern inline Tag double_to_tag(double);
extern inline double tag_to_double(Tag);

extern inline TagType tag_type(Tag);
extern inline const char *tag_type_str(TagType);

extern inline void tag_free(Tag);

extern inline void tag_print(Tag);
extern inline void tag_repr(Tag);

extern inline Tag tag_to_bool(Tag);
extern inline Tag tag_equals(Tag, Tag);

extern inline bool as_int(Tag, int64_t*);
