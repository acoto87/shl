/* =============================================================================
   wstr_test.c
   Exhaustive tests for wstr.h (StringView and String APIs).
   ============================================================================= */

#define SHL_WSTR_IMPLEMENTATION
#include "../wstr.h"
#include "test_common.h"

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

/* =========================================================================
   Unity boilerplate
   ========================================================================= */

void setUp(void)    { /* nothing per-test */ }
void tearDown(void) { /* nothing per-test */ }

/* =========================================================================
   Helper macro: assert StringView content equals a C-string literal
   ========================================================================= */
#define ASSERT_SV_EQ(cstr, sv) \
    do { \
        StringView _expected = wsv_fromCString(cstr); \
        TEST_ASSERT_TRUE_MESSAGE(wsv_equals(_expected, (sv)), \
            "StringView content mismatch for: " cstr); \
    } while (0)

/* =========================================================================
   wsv_empty
   ========================================================================= */

static void test_wsv_empty_has_zero_length(void)
{
    StringView v = wsv_empty();
    TEST_ASSERT_EQUAL_size_t(0, wsv_length(v));
}

static void test_wsv_empty_is_empty(void)
{
    StringView v = wsv_empty();
    TEST_ASSERT_TRUE(wsv_isEmpty(v));
}

static void test_wsv_empty_data_returns_empty_string(void)
{
    StringView v = wsv_empty();
    /* wsv_data on a NULL-data view returns the "" sentinel */
    TEST_ASSERT_NOT_NULL(wsv_data(v));
    TEST_ASSERT_EQUAL_STRING("", wsv_data(v));
}

/* =========================================================================
   wsv_fromCString
   ========================================================================= */

static void test_wsv_fromCString_null_gives_empty(void)
{
    StringView v = wsv_fromCString(NULL);
    TEST_ASSERT_TRUE(wsv_isEmpty(v));
    TEST_ASSERT_EQUAL_size_t(0, v.length);
}

static void test_wsv_fromCString_empty_string(void)
{
    StringView v = wsv_fromCString("");
    TEST_ASSERT_EQUAL_size_t(0, v.length);
    /* data pointer should point to the literal, not NULL */
    TEST_ASSERT_NOT_NULL(v.data);
}

static void test_wsv_fromCString_normal(void)
{
    const char* src = "hello";
    StringView v = wsv_fromCString(src);
    TEST_ASSERT_EQUAL_size_t(5, v.length);
    TEST_ASSERT_EQUAL_PTR(src, v.data);
}

static void test_wsv_fromCString_with_embedded_content(void)
{
    StringView v = wsv_fromCString("warcraft");
    TEST_ASSERT_EQUAL_size_t(8, v.length);
    TEST_ASSERT_EQUAL_MEMORY("warcraft", v.data, 8);
}

/* =========================================================================
   wsv_fromParts
   ========================================================================= */

static void test_wsv_fromParts_null_and_zero_gives_empty(void)
{
    StringView v = wsv_fromParts(NULL, 0);
    TEST_ASSERT_TRUE(wsv_isEmpty(v));
}

static void test_wsv_fromParts_normal(void)
{
    const char buf[] = "abcdef";
    StringView v = wsv_fromParts(buf, 3);
    TEST_ASSERT_EQUAL_size_t(3, v.length);
    TEST_ASSERT_EQUAL_PTR(buf, v.data);
    TEST_ASSERT_EQUAL_MEMORY("abc", v.data, 3);
}

static void test_wsv_fromParts_full_length(void)
{
    const char buf[] = "xyz";
    StringView v = wsv_fromParts(buf, 3);
    TEST_ASSERT_EQUAL_size_t(3, v.length);
}

/* =========================================================================
   wsv_fromRange
   ========================================================================= */

static void test_wsv_fromRange_null_begin_gives_empty(void)
{
    StringView v = wsv_fromRange(NULL, "end");
    TEST_ASSERT_TRUE(wsv_isEmpty(v));
}

static void test_wsv_fromRange_null_end_gives_empty(void)
{
    StringView v = wsv_fromRange("begin", NULL);
    TEST_ASSERT_TRUE(wsv_isEmpty(v));
}

static void test_wsv_fromRange_end_before_begin_gives_empty(void)
{
    const char* p = "hello";
    StringView v = wsv_fromRange(p + 3, p);  /* end < begin */
    TEST_ASSERT_TRUE(wsv_isEmpty(v));
}

static void test_wsv_fromRange_same_pointer_gives_zero_length(void)
{
    const char* p = "hello";
    StringView v = wsv_fromRange(p, p);
    TEST_ASSERT_EQUAL_size_t(0, v.length);
    TEST_ASSERT_EQUAL_PTR(p, v.data);
}

static void test_wsv_fromRange_normal(void)
{
    const char* text = "hello world";
    StringView v = wsv_fromRange(text, text + 5);
    TEST_ASSERT_EQUAL_size_t(5, v.length);
    TEST_ASSERT_EQUAL_MEMORY("hello", v.data, 5);
}

/* =========================================================================
   wsv_fromString
   ========================================================================= */

static void test_wsv_fromString_null_gives_empty(void)
{
    StringView v = wsv_fromString(NULL);
    TEST_ASSERT_TRUE(wsv_isEmpty(v));
}

static void test_wsv_fromString_empty_string(void)
{
    String s = wstr_make();
    StringView v = wsv_fromString(&s);
    TEST_ASSERT_EQUAL_size_t(0, v.length);
}

static void test_wsv_fromString_normal(void)
{
    String s = wstr_fromCString("hello");
    StringView v = wsv_fromString(&s);
    TEST_ASSERT_EQUAL_size_t(5, v.length);
    TEST_ASSERT_EQUAL_MEMORY("hello", v.data, 5);
    wstr_free(s);
}

/* =========================================================================
   wsv_isEmpty / wsv_data / wsv_length
   ========================================================================= */

static void test_wsv_isEmpty_true_for_empty(void)
{
    TEST_ASSERT_TRUE(wsv_isEmpty(wsv_empty()));
    TEST_ASSERT_TRUE(wsv_isEmpty(wsv_fromCString("")));
}

static void test_wsv_isEmpty_false_for_non_empty(void)
{
    TEST_ASSERT_FALSE(wsv_isEmpty(wsv_fromCString("x")));
    TEST_ASSERT_FALSE(wsv_isEmpty(wsv_fromCString("hello")));
}

static void test_wsv_data_non_null_for_real_data(void)
{
    StringView v = wsv_fromCString("hi");
    TEST_ASSERT_NOT_NULL(wsv_data(v));
    TEST_ASSERT_EQUAL_CHAR('h', wsv_data(v)[0]);
}

static void test_wsv_length_zero_for_empty(void)
{
    TEST_ASSERT_EQUAL_size_t(0, wsv_length(wsv_empty()));
}

static void test_wsv_length_correct_for_normal(void)
{
    TEST_ASSERT_EQUAL_size_t(5, wsv_length(wsv_fromCString("hello")));
    TEST_ASSERT_EQUAL_size_t(3, wsv_length(wsv_fromCString("abc")));
}

/* =========================================================================
   wsv_slice
   ========================================================================= */

static void test_wsv_slice_index_at_length_gives_empty(void)
{
    StringView v = wsv_fromCString("hello");
    StringView s = wsv_slice(v, 5, 1);
    TEST_ASSERT_TRUE(wsv_isEmpty(s));
}

static void test_wsv_slice_index_beyond_length_gives_empty(void)
{
    StringView v = wsv_fromCString("hello");
    StringView s = wsv_slice(v, 10, 3);
    TEST_ASSERT_TRUE(wsv_isEmpty(s));
}

static void test_wsv_slice_normal(void)
{
    StringView v = wsv_fromCString("hello world");
    StringView s = wsv_slice(v, 6, 5);
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    TEST_ASSERT_EQUAL_MEMORY("world", s.data, 5);
}

static void test_wsv_slice_length_clamped_to_available(void)
{
    StringView v = wsv_fromCString("hello");
    StringView s = wsv_slice(v, 3, 100);   /* only 2 chars available */
    TEST_ASSERT_EQUAL_size_t(2, s.length);
    TEST_ASSERT_EQUAL_MEMORY("lo", s.data, 2);
}

static void test_wsv_slice_zero_length(void)
{
    StringView v = wsv_fromCString("hello");
    StringView s = wsv_slice(v, 1, 0);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
}

static void test_wsv_slice_full_view(void)
{
    StringView v = wsv_fromCString("hello");
    StringView s = wsv_slice(v, 0, 5);
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    TEST_ASSERT_EQUAL_MEMORY("hello", s.data, 5);
}

/* =========================================================================
   wsv_subview
   ========================================================================= */

static void test_wsv_subview_index_zero_gives_same(void)
{
    StringView v = wsv_fromCString("hello");
    StringView s = wsv_subview(v, 0);
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    TEST_ASSERT_EQUAL_PTR(v.data, s.data);
}

static void test_wsv_subview_index_at_end_gives_empty(void)
{
    StringView v = wsv_fromCString("hello");
    StringView s = wsv_subview(v, 5);
    TEST_ASSERT_TRUE(wsv_isEmpty(s));
}

static void test_wsv_subview_index_beyond_end_gives_empty(void)
{
    StringView v = wsv_fromCString("hello");
    StringView s = wsv_subview(v, 100);
    TEST_ASSERT_TRUE(wsv_isEmpty(s));
}

static void test_wsv_subview_middle(void)
{
    StringView v = wsv_fromCString("hello world");
    StringView s = wsv_subview(v, 6);
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    TEST_ASSERT_EQUAL_MEMORY("world", s.data, 5);
}

/* =========================================================================
   wsv_trimLeft / wsv_trimRight / wsv_trim
   ========================================================================= */

static void test_wsv_trimLeft_no_spaces(void)
{
    StringView v = wsv_fromCString("hello");
    StringView t = wsv_trimLeft(v);
    TEST_ASSERT_EQUAL_size_t(5, t.length);
    ASSERT_SV_EQ("hello", t);
}

static void test_wsv_trimLeft_leading_spaces(void)
{
    StringView v = wsv_fromCString("   hello");
    StringView t = wsv_trimLeft(v);
    ASSERT_SV_EQ("hello", t);
}

static void test_wsv_trimLeft_all_spaces(void)
{
    StringView v = wsv_fromCString("   ");
    StringView t = wsv_trimLeft(v);
    TEST_ASSERT_TRUE(wsv_isEmpty(t));
}

static void test_wsv_trimLeft_empty(void)
{
    StringView t = wsv_trimLeft(wsv_empty());
    TEST_ASSERT_TRUE(wsv_isEmpty(t));
}

static void test_wsv_trimLeft_tabs_and_newlines(void)
{
    StringView v = wsv_fromCString("\t\n  hello  ");
    StringView t = wsv_trimLeft(v);
    TEST_ASSERT_EQUAL_CHAR('h', t.data[0]);
}

static void test_wsv_trimRight_no_spaces(void)
{
    StringView v = wsv_fromCString("hello");
    StringView t = wsv_trimRight(v);
    ASSERT_SV_EQ("hello", t);
}

static void test_wsv_trimRight_trailing_spaces(void)
{
    StringView v = wsv_fromCString("hello   ");
    StringView t = wsv_trimRight(v);
    ASSERT_SV_EQ("hello", t);
}

static void test_wsv_trimRight_all_spaces(void)
{
    StringView v = wsv_fromCString("   ");
    StringView t = wsv_trimRight(v);
    TEST_ASSERT_TRUE(wsv_isEmpty(t));
}

static void test_wsv_trimRight_empty(void)
{
    StringView t = wsv_trimRight(wsv_empty());
    TEST_ASSERT_TRUE(wsv_isEmpty(t));
}

static void test_wsv_trim_both_sides(void)
{
    StringView v = wsv_fromCString("  hello  ");
    StringView t = wsv_trim(v);
    ASSERT_SV_EQ("hello", t);
}

static void test_wsv_trim_no_spaces(void)
{
    StringView v = wsv_fromCString("hello");
    StringView t = wsv_trim(v);
    ASSERT_SV_EQ("hello", t);
}

static void test_wsv_trim_all_spaces(void)
{
    StringView v = wsv_fromCString("   \t\n  ");
    StringView t = wsv_trim(v);
    TEST_ASSERT_TRUE(wsv_isEmpty(t));
}

static void test_wsv_trim_empty(void)
{
    StringView t = wsv_trim(wsv_empty());
    TEST_ASSERT_TRUE(wsv_isEmpty(t));
}

static void test_wsv_trim_only_left_spaces(void)
{
    StringView v = wsv_fromCString("  hello");
    StringView t = wsv_trim(v);
    ASSERT_SV_EQ("hello", t);
}

static void test_wsv_trim_only_right_spaces(void)
{
    StringView v = wsv_fromCString("hello  ");
    StringView t = wsv_trim(v);
    ASSERT_SV_EQ("hello", t);
}

/* =========================================================================
   wsv_equals
   ========================================================================= */

static void test_wsv_equals_same_content(void)
{
    StringView a = wsv_fromCString("hello");
    StringView b = wsv_fromCString("hello");
    TEST_ASSERT_TRUE(wsv_equals(a, b));
}

static void test_wsv_equals_different_content(void)
{
    StringView a = wsv_fromCString("hello");
    StringView b = wsv_fromCString("world");
    TEST_ASSERT_FALSE(wsv_equals(a, b));
}

static void test_wsv_equals_different_lengths(void)
{
    StringView a = wsv_fromCString("hello");
    StringView b = wsv_fromCString("hell");
    TEST_ASSERT_FALSE(wsv_equals(a, b));
}

static void test_wsv_equals_both_empty(void)
{
    TEST_ASSERT_TRUE(wsv_equals(wsv_empty(), wsv_empty()));
}

static void test_wsv_equals_one_empty_one_not(void)
{
    TEST_ASSERT_FALSE(wsv_equals(wsv_fromCString("a"), wsv_empty()));
    TEST_ASSERT_FALSE(wsv_equals(wsv_empty(), wsv_fromCString("a")));
}

static void test_wsv_equals_case_sensitive(void)
{
    StringView a = wsv_fromCString("Hello");
    StringView b = wsv_fromCString("hello");
    TEST_ASSERT_FALSE(wsv_equals(a, b));
}

static void test_wsv_equals_single_char(void)
{
    TEST_ASSERT_TRUE(wsv_equals(wsv_fromCString("a"), wsv_fromCString("a")));
    TEST_ASSERT_FALSE(wsv_equals(wsv_fromCString("a"), wsv_fromCString("b")));
}

/* =========================================================================
   wsv_equalsIgnoreCase
   ========================================================================= */

static void test_wsv_equalsIgnoreCase_same_case(void)
{
    TEST_ASSERT_TRUE(wsv_equalsIgnoreCase(wsv_fromCString("hello"), wsv_fromCString("hello")));
}

static void test_wsv_equalsIgnoreCase_upper_lower(void)
{
    TEST_ASSERT_TRUE(wsv_equalsIgnoreCase(wsv_fromCString("Hello"), wsv_fromCString("hello")));
    TEST_ASSERT_TRUE(wsv_equalsIgnoreCase(wsv_fromCString("HELLO"), wsv_fromCString("hello")));
    TEST_ASSERT_TRUE(wsv_equalsIgnoreCase(wsv_fromCString("HeLLo"), wsv_fromCString("hElLO")));
}

static void test_wsv_equalsIgnoreCase_different_content(void)
{
    TEST_ASSERT_FALSE(wsv_equalsIgnoreCase(wsv_fromCString("Hello"), wsv_fromCString("World")));
}

static void test_wsv_equalsIgnoreCase_different_lengths(void)
{
    TEST_ASSERT_FALSE(wsv_equalsIgnoreCase(wsv_fromCString("Hello"), wsv_fromCString("Hell")));
}

static void test_wsv_equalsIgnoreCase_both_empty(void)
{
    TEST_ASSERT_TRUE(wsv_equalsIgnoreCase(wsv_empty(), wsv_empty()));
}

/* =========================================================================
   wsv_startsWith / wsv_startsWithIgnoreCase
   ========================================================================= */

static void test_wsv_startsWith_matching_prefix(void)
{
    TEST_ASSERT_TRUE(wsv_startsWith(wsv_fromCString("hello world"), wsv_fromCString("hello")));
}

static void test_wsv_startsWith_full_match(void)
{
    TEST_ASSERT_TRUE(wsv_startsWith(wsv_fromCString("hello"), wsv_fromCString("hello")));
}

static void test_wsv_startsWith_empty_prefix_always_true(void)
{
    TEST_ASSERT_TRUE(wsv_startsWith(wsv_fromCString("hello"), wsv_empty()));
    TEST_ASSERT_TRUE(wsv_startsWith(wsv_empty(), wsv_empty()));
}

static void test_wsv_startsWith_prefix_longer_than_view(void)
{
    TEST_ASSERT_FALSE(wsv_startsWith(wsv_fromCString("hi"), wsv_fromCString("hillo")));
}

static void test_wsv_startsWith_no_match(void)
{
    TEST_ASSERT_FALSE(wsv_startsWith(wsv_fromCString("hello"), wsv_fromCString("world")));
}

static void test_wsv_startsWithIgnoreCase_matching(void)
{
    TEST_ASSERT_TRUE(wsv_startsWithIgnoreCase(wsv_fromCString("Hello World"), wsv_fromCString("HELLO")));
}

static void test_wsv_startsWithIgnoreCase_no_match(void)
{
    TEST_ASSERT_FALSE(wsv_startsWithIgnoreCase(wsv_fromCString("Hello"), wsv_fromCString("World")));
}

static void test_wsv_startsWithIgnoreCase_empty_prefix(void)
{
    TEST_ASSERT_TRUE(wsv_startsWithIgnoreCase(wsv_fromCString("Hello"), wsv_empty()));
}

/* =========================================================================
   wsv_findChar
   ========================================================================= */

static void test_wsv_findChar_found_at_start(void)
{
    StringView v = wsv_fromCString("hello");
    TEST_ASSERT_EQUAL_size_t(0, wsv_findChar(v, 'h'));
}

static void test_wsv_findChar_found_in_middle(void)
{
    StringView v = wsv_fromCString("hello");
    TEST_ASSERT_EQUAL_size_t(2, wsv_findChar(v, 'l'));   /* first 'l' */
}

static void test_wsv_findChar_found_at_end(void)
{
    StringView v = wsv_fromCString("hello");
    TEST_ASSERT_EQUAL_size_t(4, wsv_findChar(v, 'o'));
}

static void test_wsv_findChar_not_found(void)
{
    StringView v = wsv_fromCString("hello");
    TEST_ASSERT_EQUAL_size_t(WSV_NPOS, wsv_findChar(v, 'z'));
}

static void test_wsv_findChar_empty_view(void)
{
    TEST_ASSERT_EQUAL_size_t(WSV_NPOS, wsv_findChar(wsv_empty(), 'a'));
}

static void test_wsv_findChar_null_char(void)
{
    /* searching for '\0' in a view that has no null terminator in the data range */
    StringView v = wsv_fromCString("hello");
    TEST_ASSERT_EQUAL_size_t(WSV_NPOS, wsv_findChar(v, '\0'));
}

/* =========================================================================
   wsv_find
   ========================================================================= */

static void test_wsv_find_empty_needle_returns_zero(void)
{
    StringView v = wsv_fromCString("hello");
    TEST_ASSERT_EQUAL_size_t(0, wsv_find(v, wsv_empty()));
}

static void test_wsv_find_needle_found_at_start(void)
{
    StringView v = wsv_fromCString("hello world");
    StringView n = wsv_fromCString("hello");
    TEST_ASSERT_EQUAL_size_t(0, wsv_find(v, n));
}

static void test_wsv_find_needle_found_in_middle(void)
{
    StringView v = wsv_fromCString("hello world");
    StringView n = wsv_fromCString("world");
    TEST_ASSERT_EQUAL_size_t(6, wsv_find(v, n));
}

static void test_wsv_find_needle_not_found(void)
{
    StringView v = wsv_fromCString("hello world");
    StringView n = wsv_fromCString("xyz");
    TEST_ASSERT_EQUAL_size_t(WSV_NPOS, wsv_find(v, n));
}

static void test_wsv_find_needle_longer_than_view(void)
{
    StringView v = wsv_fromCString("hi");
    StringView n = wsv_fromCString("hello");
    TEST_ASSERT_EQUAL_size_t(WSV_NPOS, wsv_find(v, n));
}

static void test_wsv_find_needle_equals_view(void)
{
    StringView v = wsv_fromCString("hello");
    TEST_ASSERT_EQUAL_size_t(0, wsv_find(v, v));
}

static void test_wsv_find_single_char_needle(void)
{
    StringView v = wsv_fromCString("abcabc");
    StringView n = wsv_fromCString("b");
    TEST_ASSERT_EQUAL_size_t(1, wsv_find(v, n));
}

static void test_wsv_find_repeated_pattern(void)
{
    StringView v = wsv_fromCString("ababab");
    StringView n = wsv_fromCString("bab");
    TEST_ASSERT_EQUAL_size_t(1, wsv_find(v, n));
}

/* =========================================================================
   wsv_findAny
   ========================================================================= */

static void test_wsv_findAny_found_first(void)
{
    StringView v = wsv_fromCString("hello");
    StringView chars = wsv_fromCString("aeiou");
    TEST_ASSERT_EQUAL_size_t(1, wsv_findAny(v, chars));  /* 'e' at index 1 */
}

static void test_wsv_findAny_found_at_start(void)
{
    StringView v = wsv_fromCString("apple");
    StringView chars = wsv_fromCString("aeiou");
    TEST_ASSERT_EQUAL_size_t(0, wsv_findAny(v, chars));  /* 'a' at index 0 */
}

static void test_wsv_findAny_not_found(void)
{
    StringView v = wsv_fromCString("ghjkl");
    StringView chars = wsv_fromCString("aeiou");
    TEST_ASSERT_EQUAL_size_t(WSV_NPOS, wsv_findAny(v, chars));
}

static void test_wsv_findAny_empty_view(void)
{
    TEST_ASSERT_EQUAL_size_t(WSV_NPOS, wsv_findAny(wsv_empty(), wsv_fromCString("abc")));
}

static void test_wsv_findAny_empty_chars_never_matches(void)
{
    StringView v = wsv_fromCString("hello");
    TEST_ASSERT_EQUAL_size_t(WSV_NPOS, wsv_findAny(v, wsv_empty()));
}

/* =========================================================================
   wsv_skipChars
   ========================================================================= */

static void test_wsv_skipChars_all_match(void)
{
    StringView v = wsv_fromCString("aaa");
    StringView s = wsv_skipChars(v, wsv_fromCString("a"));
    TEST_ASSERT_TRUE(wsv_isEmpty(s));
}

static void test_wsv_skipChars_none_match(void)
{
    StringView v = wsv_fromCString("hello");
    StringView s = wsv_skipChars(v, wsv_fromCString("xyz"));
    ASSERT_SV_EQ("hello", s);
}

static void test_wsv_skipChars_some_prefix_match(void)
{
    StringView v = wsv_fromCString("   hello");
    StringView s = wsv_skipChars(v, wsv_fromCString(" "));
    ASSERT_SV_EQ("hello", s);
}

static void test_wsv_skipChars_empty_view(void)
{
    StringView s = wsv_skipChars(wsv_empty(), wsv_fromCString("abc"));
    TEST_ASSERT_TRUE(wsv_isEmpty(s));
}

static void test_wsv_skipChars_empty_chars_no_skip(void)
{
    StringView v = wsv_fromCString("hello");
    StringView s = wsv_skipChars(v, wsv_empty());
    ASSERT_SV_EQ("hello", s);
}

static void test_wsv_skipChars_multiple_char_set(void)
{
    StringView v = wsv_fromCString("abc123");
    StringView s = wsv_skipChars(v, wsv_fromCString("cba"));
    ASSERT_SV_EQ("123", s);
}

/* =========================================================================
   wsv_takeUntilAny
   ========================================================================= */

static void test_wsv_takeUntilAny_match_at_start(void)
{
    StringView v = wsv_fromCString(",hello");
    StringView t = wsv_takeUntilAny(v, wsv_fromCString(","));
    TEST_ASSERT_TRUE(wsv_isEmpty(t));
}

static void test_wsv_takeUntilAny_match_in_middle(void)
{
    StringView v = wsv_fromCString("hello,world");
    StringView t = wsv_takeUntilAny(v, wsv_fromCString(","));
    ASSERT_SV_EQ("hello", t);
}

static void test_wsv_takeUntilAny_no_match_returns_full(void)
{
    StringView v = wsv_fromCString("hello");
    StringView t = wsv_takeUntilAny(v, wsv_fromCString(",;"));
    ASSERT_SV_EQ("hello", t);
}

static void test_wsv_takeUntilAny_empty_view(void)
{
    StringView t = wsv_takeUntilAny(wsv_empty(), wsv_fromCString(","));
    TEST_ASSERT_TRUE(wsv_isEmpty(t));
}

static void test_wsv_takeUntilAny_empty_chars_returns_full(void)
{
    StringView v = wsv_fromCString("hello");
    StringView t = wsv_takeUntilAny(v, wsv_empty());
    ASSERT_SV_EQ("hello", t);
}

/* =========================================================================
   wsv_splitOnce
   ========================================================================= */

static void test_wsv_splitOnce_null_left_returns_false(void)
{
    StringView right;
    bool result = wsv_splitOnce(wsv_fromCString("a=b"), wsv_fromCString("="), NULL, &right);
    TEST_ASSERT_FALSE(result);
}

static void test_wsv_splitOnce_null_right_returns_false(void)
{
    StringView left;
    bool result = wsv_splitOnce(wsv_fromCString("a=b"), wsv_fromCString("="), &left, NULL);
    TEST_ASSERT_FALSE(result);
}

static void test_wsv_splitOnce_empty_separator(void)
{
    StringView left, right;
    bool result = wsv_splitOnce(wsv_fromCString("hello"), wsv_empty(), &left, &right);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(wsv_isEmpty(left));
    ASSERT_SV_EQ("hello", right);
}

static void test_wsv_splitOnce_separator_found(void)
{
    StringView left, right;
    bool result = wsv_splitOnce(wsv_fromCString("key=value"), wsv_fromCString("="), &left, &right);
    TEST_ASSERT_TRUE(result);
    ASSERT_SV_EQ("key", left);
    ASSERT_SV_EQ("value", right);
}

static void test_wsv_splitOnce_separator_not_found(void)
{
    StringView left, right;
    bool result = wsv_splitOnce(wsv_fromCString("keyvalue"), wsv_fromCString("="), &left, &right);
    TEST_ASSERT_FALSE(result);
    ASSERT_SV_EQ("keyvalue", left);
    TEST_ASSERT_TRUE(wsv_isEmpty(right));
}

static void test_wsv_splitOnce_separator_at_start(void)
{
    StringView left, right;
    bool result = wsv_splitOnce(wsv_fromCString("=value"), wsv_fromCString("="), &left, &right);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(wsv_isEmpty(left));
    ASSERT_SV_EQ("value", right);
}

static void test_wsv_splitOnce_separator_at_end(void)
{
    StringView left, right;
    bool result = wsv_splitOnce(wsv_fromCString("key="), wsv_fromCString("="), &left, &right);
    TEST_ASSERT_TRUE(result);
    ASSERT_SV_EQ("key", left);
    TEST_ASSERT_TRUE(wsv_isEmpty(right));
}

static void test_wsv_splitOnce_multi_char_separator(void)
{
    StringView left, right;
    bool result = wsv_splitOnce(wsv_fromCString("hello::world"), wsv_fromCString("::"), &left, &right);
    TEST_ASSERT_TRUE(result);
    ASSERT_SV_EQ("hello", left);
    ASSERT_SV_EQ("world", right);
}

static void test_wsv_splitOnce_uses_first_occurrence(void)
{
    StringView left, right;
    bool result = wsv_splitOnce(wsv_fromCString("a=b=c"), wsv_fromCString("="), &left, &right);
    TEST_ASSERT_TRUE(result);
    ASSERT_SV_EQ("a", left);
    ASSERT_SV_EQ("b=c", right);
}

/* =========================================================================
   wsv_chopByDelimiter
   ========================================================================= */

static void test_wsv_chopByDelimiter_null_remaining_returns_empty(void)
{
    StringView token = wsv_chopByDelimiter(NULL, ',');
    TEST_ASSERT_TRUE(wsv_isEmpty(token));
}

static void test_wsv_chopByDelimiter_normal(void)
{
    StringView remaining = wsv_fromCString("hello,world");
    StringView token = wsv_chopByDelimiter(&remaining, ',');
    ASSERT_SV_EQ("hello", token);
    ASSERT_SV_EQ("world", remaining);
}

static void test_wsv_chopByDelimiter_no_delimiter(void)
{
    StringView remaining = wsv_fromCString("hello");
    StringView token = wsv_chopByDelimiter(&remaining, ',');
    ASSERT_SV_EQ("hello", token);
    TEST_ASSERT_TRUE(wsv_isEmpty(remaining));
}

static void test_wsv_chopByDelimiter_delimiter_at_start(void)
{
    StringView remaining = wsv_fromCString(",world");
    StringView token = wsv_chopByDelimiter(&remaining, ',');
    TEST_ASSERT_TRUE(wsv_isEmpty(token));
    ASSERT_SV_EQ("world", remaining);
}

static void test_wsv_chopByDelimiter_delimiter_at_end(void)
{
    StringView remaining = wsv_fromCString("hello,");
    StringView token = wsv_chopByDelimiter(&remaining, ',');
    ASSERT_SV_EQ("hello", token);
    TEST_ASSERT_TRUE(wsv_isEmpty(remaining));
}

static void test_wsv_chopByDelimiter_empty_remaining(void)
{
    StringView remaining = wsv_empty();
    StringView token = wsv_chopByDelimiter(&remaining, ',');
    TEST_ASSERT_TRUE(wsv_isEmpty(token));
    TEST_ASSERT_TRUE(wsv_isEmpty(remaining));
}

static void test_wsv_chopByDelimiter_multiple_calls(void)
{
    StringView remaining = wsv_fromCString("a,b,c");
    StringView t1 = wsv_chopByDelimiter(&remaining, ',');
    StringView t2 = wsv_chopByDelimiter(&remaining, ',');
    StringView t3 = wsv_chopByDelimiter(&remaining, ',');
    ASSERT_SV_EQ("a", t1);
    ASSERT_SV_EQ("b", t2);
    ASSERT_SV_EQ("c", t3);
    TEST_ASSERT_TRUE(wsv_isEmpty(remaining));
}

/* =========================================================================
   wsv_nextToken
   ========================================================================= */

static void test_wsv_nextToken_null_remaining_returns_false(void)
{
    StringView token;
    bool result = wsv_nextToken(NULL, wsv_fromCString(" "), &token);
    TEST_ASSERT_FALSE(result);
}

static void test_wsv_nextToken_null_token_returns_false(void)
{
    StringView remaining = wsv_fromCString("hello");
    bool result = wsv_nextToken(&remaining, wsv_fromCString(" "), NULL);
    TEST_ASSERT_FALSE(result);
}

static void test_wsv_nextToken_empty_remaining_returns_false(void)
{
    StringView remaining = wsv_empty();
    StringView token;
    bool result = wsv_nextToken(&remaining, wsv_fromCString(" "), &token);
    TEST_ASSERT_FALSE(result);
}

static void test_wsv_nextToken_single_token(void)
{
    StringView remaining = wsv_fromCString("hello");
    StringView token;
    bool result = wsv_nextToken(&remaining, wsv_fromCString(" "), &token);
    TEST_ASSERT_TRUE(result);
    ASSERT_SV_EQ("hello", token);
    TEST_ASSERT_TRUE(wsv_isEmpty(remaining));
}

static void test_wsv_nextToken_multiple_tokens(void)
{
    StringView remaining = wsv_fromCString("hello world foo");
    StringView separators = wsv_fromCString(" ");
    StringView t1, t2, t3;

    TEST_ASSERT_TRUE(wsv_nextToken(&remaining, separators, &t1));
    ASSERT_SV_EQ("hello", t1);

    TEST_ASSERT_TRUE(wsv_nextToken(&remaining, separators, &t2));
    ASSERT_SV_EQ("world", t2);

    TEST_ASSERT_TRUE(wsv_nextToken(&remaining, separators, &t3));
    ASSERT_SV_EQ("foo", t3);

    TEST_ASSERT_FALSE(wsv_nextToken(&remaining, separators, &t1));
}

static void test_wsv_nextToken_skips_multiple_consecutive_separators(void)
{
    StringView remaining = wsv_fromCString("  hello   world  ");
    StringView separators = wsv_fromCString(" ");
    StringView t1, t2;

    TEST_ASSERT_TRUE(wsv_nextToken(&remaining, separators, &t1));
    ASSERT_SV_EQ("hello", t1);

    TEST_ASSERT_TRUE(wsv_nextToken(&remaining, separators, &t2));
    ASSERT_SV_EQ("world", t2);

    TEST_ASSERT_FALSE(wsv_nextToken(&remaining, separators, &t1));
}

static void test_wsv_nextToken_only_separators_returns_false(void)
{
    StringView remaining = wsv_fromCString("   ");
    StringView token;
    bool result = wsv_nextToken(&remaining, wsv_fromCString(" "), &token);
    TEST_ASSERT_FALSE(result);
}

static void test_wsv_nextToken_multiple_separator_chars(void)
{
    StringView remaining = wsv_fromCString("a,b;c");
    StringView separators = wsv_fromCString(",;");
    StringView t1, t2, t3;

    TEST_ASSERT_TRUE(wsv_nextToken(&remaining, separators, &t1));
    ASSERT_SV_EQ("a", t1);

    TEST_ASSERT_TRUE(wsv_nextToken(&remaining, separators, &t2));
    ASSERT_SV_EQ("b", t2);

    TEST_ASSERT_TRUE(wsv_nextToken(&remaining, separators, &t3));
    ASSERT_SV_EQ("c", t3);

    TEST_ASSERT_FALSE(wsv_nextToken(&remaining, separators, &t1));
}

/* =========================================================================
   wsv_hashFNV32
   ========================================================================= */

#define FNV_OFFSET_32_VAL 0x811c9dc5u

static void test_wsv_hashFNV32_empty_returns_offset(void)
{
    uint32_t hash = wsv_hashFNV32(wsv_empty());
    TEST_ASSERT_EQUAL_UINT32(FNV_OFFSET_32_VAL, hash);
}

static void test_wsv_hashFNV32_same_input_same_output(void)
{
    StringView v = wsv_fromCString("hello");
    uint32_t h1 = wsv_hashFNV32(v);
    uint32_t h2 = wsv_hashFNV32(v);
    TEST_ASSERT_EQUAL_UINT32(h1, h2);
}

static void test_wsv_hashFNV32_different_strings_differ(void)
{
    uint32_t h1 = wsv_hashFNV32(wsv_fromCString("hello"));
    uint32_t h2 = wsv_hashFNV32(wsv_fromCString("world"));
    /* Not guaranteed but certain for these well-separated strings */
    TEST_ASSERT_NOT_EQUAL_UINT32(h1, h2);
}

static void test_wsv_hashFNV32_case_sensitive(void)
{
    uint32_t h1 = wsv_hashFNV32(wsv_fromCString("Hello"));
    uint32_t h2 = wsv_hashFNV32(wsv_fromCString("hello"));
    TEST_ASSERT_NOT_EQUAL_UINT32(h1, h2);
}

static void test_wsv_hashFNV32_single_byte_a(void)
{
    /* FNV-1a for "a": (0x61 ^ 0x811c9dc5) * 0x01000193 */
    uint32_t expected = ((uint32_t)((unsigned char)'a') ^ FNV_OFFSET_32_VAL) * 0x01000193u;
    uint32_t actual = wsv_hashFNV32(wsv_fromCString("a"));
    TEST_ASSERT_EQUAL_UINT32(expected, actual);
}

static void test_wsv_hashFNV32_different_lengths_differ(void)
{
    uint32_t h1 = wsv_hashFNV32(wsv_fromCString("a"));
    uint32_t h2 = wsv_hashFNV32(wsv_fromCString("ab"));
    TEST_ASSERT_NOT_EQUAL_UINT32(h1, h2);
}

/* =========================================================================
   wsv_parseS32 / wsv_tryParseS32
   ========================================================================= */

static void test_wsv_parseS32_zero(void)
{
    TEST_ASSERT_EQUAL_INT32(0, wsv_parseS32(wsv_fromCString("0")));
}

static void test_wsv_parseS32_positive(void)
{
    TEST_ASSERT_EQUAL_INT32(42, wsv_parseS32(wsv_fromCString("42")));
}

static void test_wsv_parseS32_negative(void)
{
    TEST_ASSERT_EQUAL_INT32(-42, wsv_parseS32(wsv_fromCString("-42")));
}

static void test_wsv_parseS32_with_plus_sign(void)
{
    TEST_ASSERT_EQUAL_INT32(99, wsv_parseS32(wsv_fromCString("+99")));
}

static void test_wsv_parseS32_leading_whitespace(void)
{
    TEST_ASSERT_EQUAL_INT32(5, wsv_parseS32(wsv_fromCString("  5")));
}

static void test_wsv_parseS32_trailing_whitespace(void)
{
    TEST_ASSERT_EQUAL_INT32(5, wsv_parseS32(wsv_fromCString("5  ")));
}

static void test_wsv_parseS32_both_whitespace(void)
{
    TEST_ASSERT_EQUAL_INT32(123, wsv_parseS32(wsv_fromCString("  123  ")));
}

static void test_wsv_parseS32_max_value(void)
{
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, wsv_parseS32(wsv_fromCString("2147483647")));
}

static void test_wsv_parseS32_min_value(void)
{
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, wsv_parseS32(wsv_fromCString("-2147483648")));
}

static void test_wsv_parseS32_hex(void)
{
    int32_t value = 0;
    TEST_ASSERT_TRUE(wsv_tryParseS32(wsv_fromCString("0x1F"), &value));
    TEST_ASSERT_EQUAL_INT32(31, value);
}

static void test_wsv_parseS32_hex_uppercase(void)
{
    int32_t value = 0;
    TEST_ASSERT_TRUE(wsv_tryParseS32(wsv_fromCString("0XFF"), &value));
    TEST_ASSERT_EQUAL_INT32(255, value);
}

static void test_wsv_parseS32_octal(void)
{
    int32_t value = 0;
    /* "010" in octal = 8 in decimal */
    TEST_ASSERT_TRUE(wsv_tryParseS32(wsv_fromCString("010"), &value));
    TEST_ASSERT_EQUAL_INT32(8, value);
}

static void test_wsv_tryParseS32_overflow_returns_false(void)
{
    int32_t value = 99;
    bool result = wsv_tryParseS32(wsv_fromCString("2147483648"), &value);  /* INT32_MAX + 1 */
    TEST_ASSERT_FALSE(result);
}

static void test_wsv_tryParseS32_underflow_returns_false(void)
{
    int32_t value = 99;
    bool result = wsv_tryParseS32(wsv_fromCString("-2147483649"), &value);  /* INT32_MIN - 1 */
    TEST_ASSERT_FALSE(result);
}

static void test_wsv_tryParseS32_invalid_chars_returns_false(void)
{
    int32_t value = 99;
    TEST_ASSERT_FALSE(wsv_tryParseS32(wsv_fromCString("abc"), &value));
    TEST_ASSERT_FALSE(wsv_tryParseS32(wsv_fromCString("12abc"), &value));
    TEST_ASSERT_FALSE(wsv_tryParseS32(wsv_fromCString(""), &value));
    TEST_ASSERT_FALSE(wsv_tryParseS32(wsv_fromCString("   "), &value));
}

static void test_wsv_tryParseS32_sign_only_returns_false(void)
{
    int32_t value = 0;
    TEST_ASSERT_FALSE(wsv_tryParseS32(wsv_fromCString("-"), &value));
    TEST_ASSERT_FALSE(wsv_tryParseS32(wsv_fromCString("+"), &value));
}

static void test_wsv_tryParseS32_null_value_ptr_still_returns_true(void)
{
    /* According to impl: if value is NULL, it validates but doesn't write */
    bool result = wsv_tryParseS32(wsv_fromCString("42"), NULL);
    TEST_ASSERT_TRUE(result);
}

static void test_wsv_tryParseS32_invalid_octal_digit(void)
{
    /* '8' and '9' are not valid octal digits */
    int32_t value = 0;
    TEST_ASSERT_FALSE(wsv_tryParseS32(wsv_fromCString("08"), &value));
    TEST_ASSERT_FALSE(wsv_tryParseS32(wsv_fromCString("09"), &value));
}

static void test_wsv_parseS32_invalid_returns_zero(void)
{
    /* wsv_parseS32 returns 0 on failure */
    TEST_ASSERT_EQUAL_INT32(0, wsv_parseS32(wsv_fromCString("abc")));
}

/* =========================================================================
   wsv_parseS64 / wsv_tryParseS64
   ========================================================================= */

static void test_wsv_parseS64_zero(void)
{
    TEST_ASSERT_EQUAL_INT64(0LL, wsv_parseS64(wsv_fromCString("0")));
}

static void test_wsv_parseS64_large_positive(void)
{
    TEST_ASSERT_EQUAL_INT64(9000000000LL, wsv_parseS64(wsv_fromCString("9000000000")));
}

static void test_wsv_parseS64_large_negative(void)
{
    TEST_ASSERT_EQUAL_INT64(-9000000000LL, wsv_parseS64(wsv_fromCString("-9000000000")));
}

static void test_wsv_parseS64_max_value(void)
{
    int64_t value = 0;
    bool result = wsv_tryParseS64(wsv_fromCString("9223372036854775807"), &value);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT64((int64_t)INT64_MAX, value);
}

static void test_wsv_parseS64_min_value(void)
{
    int64_t value = 0;
    bool result = wsv_tryParseS64(wsv_fromCString("-9223372036854775808"), &value);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT64((int64_t)INT64_MIN, value);
}

static void test_wsv_tryParseS64_overflow_returns_false(void)
{
    int64_t value = 0;
    /* INT64_MAX + 1 */
    TEST_ASSERT_FALSE(wsv_tryParseS64(wsv_fromCString("9223372036854775808"), &value));
}

static void test_wsv_tryParseS64_invalid_returns_false(void)
{
    int64_t value = 0;
    TEST_ASSERT_FALSE(wsv_tryParseS64(wsv_fromCString("xyz"), &value));
}

static void test_wsv_tryParseS64_hex(void)
{
    int64_t value = 0;
    TEST_ASSERT_TRUE(wsv_tryParseS64(wsv_fromCString("0xFF"), &value));
    TEST_ASSERT_EQUAL_INT64(255LL, value);
}

static void test_wsv_tryParseS64_null_value_ptr(void)
{
    TEST_ASSERT_TRUE(wsv_tryParseS64(wsv_fromCString("100"), NULL));
}

/* =========================================================================
   wsv_copyToBuffer
   ========================================================================= */

static void test_wsv_copyToBuffer_normal(void)
{
    char buf[16] = {0};
    bool result = wsv_copyToBuffer(wsv_fromCString("hello"), buf, sizeof(buf));
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("hello", buf);
}

static void test_wsv_copyToBuffer_exact_capacity(void)
{
    /* view.length == capacity - 1  (fits exactly, null term at buf[5]) */
    char buf[6];
    bool result = wsv_copyToBuffer(wsv_fromCString("hello"), buf, 6);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("hello", buf);
}

static void test_wsv_copyToBuffer_too_small_returns_false(void)
{
    char buf[4] = {1, 2, 3, 4};
    /* "hello" needs 6 bytes (5 + null), buf only has 4 */
    bool result = wsv_copyToBuffer(wsv_fromCString("hello"), buf, 4);
    TEST_ASSERT_FALSE(result);
    /* On failure the buffer is zeroed at index 0 */
    TEST_ASSERT_EQUAL_CHAR(0, buf[0]);
}

static void test_wsv_copyToBuffer_null_buffer_returns_false(void)
{
    bool result = wsv_copyToBuffer(wsv_fromCString("hello"), NULL, 16);
    TEST_ASSERT_FALSE(result);
}

static void test_wsv_copyToBuffer_zero_capacity_returns_false(void)
{
    char buf[4];
    bool result = wsv_copyToBuffer(wsv_fromCString("hello"), buf, 0);
    TEST_ASSERT_FALSE(result);
}

static void test_wsv_copyToBuffer_empty_view(void)
{
    char buf[4] = {1, 2, 3, 4};
    bool result = wsv_copyToBuffer(wsv_empty(), buf, sizeof(buf));
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_CHAR(0, buf[0]);
}

/* =========================================================================
   wsv_toString (delegates to wstr_fromView)
   ========================================================================= */

static void test_wsv_toString_creates_copy(void)
{
    StringView v = wsv_fromCString("hello");
    String s = wsv_toString(v);
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    /* Data pointer is a fresh allocation, not the same as v.data */
    TEST_ASSERT_TRUE(v.data != s.data);
    wstr_free(s);
}

static void test_wsv_toString_empty_view(void)
{
    String s = wsv_toString(wsv_empty());
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    wstr_free(s);
}

/* =========================================================================
   wstr_make
   ========================================================================= */

static void test_wstr_make_zeroed(void)
{
    String s = wstr_make();
    TEST_ASSERT_NULL(s.data);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    TEST_ASSERT_EQUAL_size_t(0, s.capacity);
}

static void test_wstr_make_isEmpty(void)
{
    String s = wstr_make();
    TEST_ASSERT_TRUE(wstr_isEmpty(&s));
}

/* =========================================================================
   wstr_withCapacity
   ========================================================================= */

static void test_wstr_withCapacity_capacity_set(void)
{
    String s = wstr_withCapacity(32);
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(32, s.capacity);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    TEST_ASSERT_NOT_NULL(s.data);
    wstr_free(s);
}

static void test_wstr_withCapacity_zero(void)
{
    String s = wstr_withCapacity(0);
    /* Requesting 0 capacity is fine; may or may not allocate */
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    wstr_free(s);
}

/* =========================================================================
   wstr_fromCString
   ========================================================================= */

static void test_wstr_fromCString_null_gives_empty(void)
{
    String s = wstr_fromCString(NULL);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    wstr_free(s);
}

static void test_wstr_fromCString_empty_string(void)
{
    String s = wstr_fromCString("");
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    wstr_free(s);
}

static void test_wstr_fromCString_normal(void)
{
    String s = wstr_fromCString("hello");
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    wstr_free(s);
}

static void test_wstr_fromCString_null_terminated(void)
{
    String s = wstr_fromCString("hi");
    TEST_ASSERT_EQUAL_CHAR(0, s.data[s.length]);
    wstr_free(s);
}

static String test_wstr__fromCStringFormatv_helper(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String string = wstr_fromCStringFormatv(fmt, args);
    va_end(args);
    return string;
}

static void test_wstr_fromCStringFormat_simple_string(void)
{
    String s = wstr_fromCStringFormat("hello %s %d", "world", 42);
    TEST_ASSERT_EQUAL_size_t(14, s.length);
    TEST_ASSERT_EQUAL_STRING("hello world 42", s.data);
    TEST_ASSERT_EQUAL_CHAR(0, s.data[s.length]);
    wstr_free(s);
}

static void test_wstr_fromCStringFormat_null_format_gives_empty(void)
{
    String s = wstr_fromCStringFormat(NULL);
    TEST_ASSERT_NULL(s.data);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    TEST_ASSERT_EQUAL_size_t(0, s.capacity);
}

static void test_wstr_fromCStringFormatv_formats_arguments(void)
{
    String s = test_wstr__fromCStringFormatv_helper("%s:%04d", "id", 7);
    TEST_ASSERT_EQUAL_size_t(7, s.length);
    TEST_ASSERT_EQUAL_STRING("id:0007", s.data);
    TEST_ASSERT_EQUAL_CHAR(0, s.data[s.length]);
    wstr_free(s);
}

static void test_wstr_fromCStringFormatv_long_string_grows_buffer(void)
{
    String s = test_wstr__fromCStringFormatv_helper(
        "%s%s",
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");
    TEST_ASSERT_EQUAL_size_t(128, s.length);
    TEST_ASSERT_EQUAL_CHAR('A', s.data[0]);
    TEST_ASSERT_EQUAL_CHAR('B', s.data[127]);
    TEST_ASSERT_EQUAL_CHAR(0, s.data[s.length]);
    wstr_free(s);
}

/* =========================================================================
   wstr_fromView
   ========================================================================= */

static void test_wstr_fromView_normal(void)
{
    StringView v = wsv_fromCString("world");
    String s = wstr_fromView(v);
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    TEST_ASSERT_EQUAL_STRING("world", s.data);
    wstr_free(s);
}

static void test_wstr_fromView_empty(void)
{
    String s = wstr_fromView(wsv_empty());
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    wstr_free(s);
}

/* =========================================================================
   wstr_adopt
   ========================================================================= */

static void test_wstr_adopt_null_buffer_gives_empty(void)
{
    String s = wstr_adopt(NULL, 5, 10);
    TEST_ASSERT_NULL(s.data);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    TEST_ASSERT_EQUAL_size_t(0, s.capacity);
}

static void test_wstr_adopt_normal(void)
{
    /* Allocate capacity+1 bytes so adopt can write the null terminator */
    size_t cap = 8;
    char* buf = (char*)malloc(cap + 1);
    memcpy(buf, "hello", 5);
    String s = wstr_adopt(buf, 5, cap);
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    TEST_ASSERT_EQUAL_size_t(cap, s.capacity);
    TEST_ASSERT_EQUAL_CHAR(0, s.data[5]);    /* null terminator set */
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    wstr_free(s);
}

static void test_wstr_adopt_length_clamped_to_capacity(void)
{
    size_t cap = 4;
    char* buf = (char*)malloc(cap + 1);
    memcpy(buf, "hell", 4);
    buf[4] = 'o';   /* intentionally no null term */
    /* length > capacity -> clamped */
    String s = wstr_adopt(buf, 10, cap);
    TEST_ASSERT_EQUAL_size_t(cap, s.length);
    TEST_ASSERT_EQUAL_CHAR(0, s.data[cap]);
    wstr_free(s);
}

/* =========================================================================
   wstr_free
   ========================================================================= */

static void test_wstr_free_ptr_null_is_safe(void)
{
    wstr_freePtr(NULL);  /* must not crash */
    TEST_PASS();
}

static void test_wstr_free_ptr_resets_string(void)
{
    String s = wstr_fromCString("hello");
    wstr_freePtr(&s);
    TEST_ASSERT_NULL(s.data);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    TEST_ASSERT_EQUAL_size_t(0, s.capacity);
}

static void test_wstr_free_null_string_is_safe(void)
{
    wstr_free(wstr_make());  /* must not crash */
    TEST_PASS();
}

static void test_wstr_free_releases_owned_buffer(void)
{
    String s = wstr_fromCString("hello");
    char* data = s.data;

    TEST_ASSERT_NOT_NULL(data);
    wstr_free(s);
    TEST_PASS();
}

/* =========================================================================
   wstr_clear
   ========================================================================= */

static void test_wstr_clear_null_is_safe(void)
{
    wstr_clear(NULL);  /* must not crash */
    TEST_PASS();
}

static void test_wstr_clear_resets_length(void)
{
    String s = wstr_fromCString("hello");
    wstr_clear(&s);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    TEST_ASSERT_EQUAL_CHAR(0, s.data[0]);
    /* capacity is unchanged after clear */
    TEST_ASSERT_GREATER_THAN_size_t(0, s.capacity);
    wstr_free(s);
}

static void test_wstr_clear_empty_string_is_safe(void)
{
    String s = wstr_make();
    wstr_clear(&s);  /* must not crash */
    TEST_ASSERT_EQUAL_size_t(0, s.length);
}

/* =========================================================================
   wstr_view / wstr_cstr / wstr_isEmpty
   ========================================================================= */

static void test_wstr_view_null_gives_empty(void)
{
    StringView v = wstr_view(NULL);
    TEST_ASSERT_TRUE(wsv_isEmpty(v));
}

static void test_wstr_view_normal(void)
{
    String s = wstr_fromCString("hello");
    StringView v = wstr_view(&s);
    ASSERT_SV_EQ("hello", v);
    TEST_ASSERT_EQUAL_PTR(s.data, v.data);
    wstr_free(s);
}

static void test_wstr_cstr_null_gives_empty_string(void)
{
    const char* cs = wstr_cstr(NULL);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_EQUAL_STRING("", cs);
}

static void test_wstr_cstr_normal(void)
{
    String s = wstr_fromCString("world");
    const char* cs = wstr_cstr(&s);
    TEST_ASSERT_EQUAL_STRING("world", cs);
    wstr_free(s);
}

static void test_wstr_isEmpty_null_is_true(void)
{
    TEST_ASSERT_TRUE(wstr_isEmpty(NULL));
}

static void test_wstr_isEmpty_empty_string_is_true(void)
{
    String s = wstr_make();
    TEST_ASSERT_TRUE(wstr_isEmpty(&s));
}

static void test_wstr_isEmpty_non_empty_is_false(void)
{
    String s = wstr_fromCString("x");
    TEST_ASSERT_FALSE(wstr_isEmpty(&s));
    wstr_free(s);
}

/* =========================================================================
   wstr_reserve
   ========================================================================= */

static void test_wstr_reserve_grows_capacity(void)
{
    String s = wstr_make();
    bool ok = wstr_reserve(&s, 64);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(64, s.capacity);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    wstr_free(s);
}

static void test_wstr_reserve_already_sufficient(void)
{
    String s = wstr_withCapacity(128);
    size_t capBefore = s.capacity;
    bool ok = wstr_reserve(&s, 16);  /* less than existing */
    TEST_ASSERT_TRUE(ok);
    /* capacity should not shrink */
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(capBefore, s.capacity);
    wstr_free(s);
}

/* =========================================================================
   wstr_resize
   ========================================================================= */

static void test_wstr_resize_null_returns_false(void)
{
    TEST_ASSERT_FALSE(wstr_resize(NULL, 10));
}

static void test_wstr_resize_grow_zero_fills(void)
{
    String s = wstr_fromCString("hi");
    bool ok = wstr_resize(&s, 5);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    /* new bytes are zeroed */
    TEST_ASSERT_EQUAL_CHAR(0, s.data[2]);
    TEST_ASSERT_EQUAL_CHAR(0, s.data[3]);
    TEST_ASSERT_EQUAL_CHAR(0, s.data[4]);
    TEST_ASSERT_EQUAL_CHAR(0, s.data[5]);  /* null terminator */
    wstr_free(s);
}

static void test_wstr_resize_shrink(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_resize(&s, 3);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_size_t(3, s.length);
    TEST_ASSERT_EQUAL_CHAR(0, s.data[3]);
    TEST_ASSERT_EQUAL_MEMORY("hel", s.data, 3);
    wstr_free(s);
}

static void test_wstr_resize_to_same_length(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_resize(&s, 5);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    wstr_free(s);
}

static void test_wstr_resize_to_zero(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_resize(&s, 0);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    wstr_free(s);
}

/* =========================================================================
   wstr_assign / wstr_assignCString
   ========================================================================= */

static void test_wstr_assign_null_string_returns_false(void)
{
    StringView v = wsv_fromCString("hello");
    TEST_ASSERT_FALSE(wstr_assign(NULL, v));
}

static void test_wstr_assign_normal(void)
{
    String s = wstr_make();
    bool ok = wstr_assign(&s, wsv_fromCString("hello"));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    wstr_free(s);
}

static void test_wstr_assign_overwrites_existing_content(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_assign(&s, wsv_fromCString("world!"));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("world!", s.data);
    TEST_ASSERT_EQUAL_size_t(6, s.length);
    wstr_free(s);
}

static void test_wstr_assign_empty_view_clears(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_assign(&s, wsv_empty());
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    wstr_free(s);
}

static void test_wstr_assign_self_aliased(void)
{
    /* Assigning a substring of a string to itself (aliasing) */
    String s = wstr_fromCString("hello world");
    StringView suffix = wsv_fromCString("world");
    /* Create a view that points into s's buffer */
    StringView alias = wsv_fromParts(s.data + 6, 5);
    bool ok = wstr_assign(&s, alias);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("world", s.data);
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    /* Suppress unused variable warning */
    (void)suffix;
    wstr_free(s);
}

static void test_wstr_assignCString_normal(void)
{
    String s = wstr_make();
    bool ok = wstr_assignCString(&s, "hello");
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    wstr_free(s);
}

static void test_wstr_assignCString_null_clears(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_assignCString(&s, NULL);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    wstr_free(s);
}

/* =========================================================================
   wstr_append / wstr_appendCString / wstr_appendChar
   ========================================================================= */

static void test_wstr_append_null_string_returns_false(void)
{
    TEST_ASSERT_FALSE(wstr_append(NULL, wsv_fromCString("hello")));
}

static void test_wstr_append_to_empty(void)
{
    String s = wstr_make();
    bool ok = wstr_append(&s, wsv_fromCString("hello"));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    wstr_free(s);
}

static void test_wstr_append_to_existing(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_append(&s, wsv_fromCString(" world"));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello world", s.data);
    TEST_ASSERT_EQUAL_size_t(11, s.length);
    wstr_free(s);
}

static void test_wstr_append_empty_view_is_noop(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_append(&s, wsv_empty());
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    wstr_free(s);
}

static void test_wstr_append_self_aliased(void)
{
    /* Appending the string to itself */
    String s = wstr_fromCString("hello");
    bool ok = wstr_append(&s, wstr_view(&s));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hellohello", s.data);
    TEST_ASSERT_EQUAL_size_t(10, s.length);
    wstr_free(s);
}

static void test_wstr_append_self_suffix_aliased(void)
{
    /* Appending a suffix view that aliases into the same buffer */
    String s = wstr_fromCString("hello");
    StringView suffix = wsv_fromParts(s.data + 1, 4);  /* "ello" */
    bool ok = wstr_append(&s, suffix);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("helloello", s.data);
    wstr_free(s);
}

static void test_wstr_appendCString_normal(void)
{
    String s = wstr_fromCString("foo");
    bool ok = wstr_appendCString(&s, "bar");
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("foobar", s.data);
    wstr_free(s);
}

static void test_wstr_appendCString_null_is_noop(void)
{
    String s = wstr_fromCString("foo");
    bool ok = wstr_appendCString(&s, NULL);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("foo", s.data);
    wstr_free(s);
}

static void test_wstr_appendChar_normal(void)
{
    String s = wstr_fromCString("hel");
    bool ok = wstr_appendChar(&s, 'l');
    TEST_ASSERT_TRUE(ok);
    ok = wstr_appendChar(&s, 'o');
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    wstr_free(s);
}

static void test_wstr_appendChar_to_empty(void)
{
    String s = wstr_make();
    bool ok = wstr_appendChar(&s, 'x');
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("x", s.data);
    TEST_ASSERT_EQUAL_size_t(1, s.length);
    wstr_free(s);
}

/* =========================================================================
   wstr_insert
   ========================================================================= */

static void test_wstr_insert_null_string_returns_false(void)
{
    TEST_ASSERT_FALSE(wstr_insert(NULL, 0, wsv_fromCString("x")));
}

static void test_wstr_insert_index_beyond_length_returns_false(void)
{
    String s = wstr_fromCString("hello");
    TEST_ASSERT_FALSE(wstr_insert(&s, 10, wsv_fromCString("x")));
    wstr_free(s);
}

static void test_wstr_insert_at_start(void)
{
    String s = wstr_fromCString("world");
    bool ok = wstr_insert(&s, 0, wsv_fromCString("hello "));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello world", s.data);
    wstr_free(s);
}

static void test_wstr_insert_in_middle(void)
{
    String s = wstr_fromCString("helo");
    bool ok = wstr_insert(&s, 3, wsv_fromCString("l"));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    wstr_free(s);
}

static void test_wstr_insert_at_end(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_insert(&s, 5, wsv_fromCString(" world"));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello world", s.data);
    wstr_free(s);
}

static void test_wstr_insert_empty_view_is_noop(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_insert(&s, 2, wsv_empty());
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    wstr_free(s);
}

static void test_wstr_insert_self_aliased(void)
{
    /* Insert a view of the string itself */
    String s = wstr_fromCString("ab");
    StringView self = wstr_view(&s);
    bool ok = wstr_insert(&s, 1, self);  /* "a" + "ab" + "b" = "aabb" */
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("aabb", s.data);
    wstr_free(s);
}

/* =========================================================================
   wstr_removeRange
   ========================================================================= */

static void test_wstr_removeRange_null_returns_false(void)
{
    TEST_ASSERT_FALSE(wstr_removeRange(NULL, 0, 3));
}

static void test_wstr_removeRange_index_beyond_length_returns_false(void)
{
    String s = wstr_fromCString("hello");
    TEST_ASSERT_FALSE(wstr_removeRange(&s, 10, 1));
    wstr_free(s);
}

static void test_wstr_removeRange_zero_length_is_noop(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_removeRange(&s, 2, 0);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    wstr_free(s);
}

static void test_wstr_removeRange_from_start(void)
{
    String s = wstr_fromCString("hello world");
    bool ok = wstr_removeRange(&s, 0, 6);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("world", s.data);
    TEST_ASSERT_EQUAL_size_t(5, s.length);
    wstr_free(s);
}

static void test_wstr_removeRange_from_middle(void)
{
    String s = wstr_fromCString("hello world");
    bool ok = wstr_removeRange(&s, 5, 6);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    wstr_free(s);
}

static void test_wstr_removeRange_length_clamped_at_end(void)
{
    /* length > remaining bytes: should clamp, not overrun */
    String s = wstr_fromCString("hello");
    bool ok = wstr_removeRange(&s, 3, 100);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_size_t(3, s.length);
    TEST_ASSERT_EQUAL_MEMORY("hel", s.data, 3);
    wstr_free(s);
}

static void test_wstr_removeRange_entire_string(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_removeRange(&s, 0, 5);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_size_t(0, s.length);
    TEST_ASSERT_EQUAL_CHAR(0, s.data[0]);
    wstr_free(s);
}

static void test_wstr_removeRange_at_exact_end_is_noop(void)
{
    /* index == string.length: valid, but no bytes to remove */
    String s = wstr_fromCString("hello");
    bool ok = wstr_removeRange(&s, 5, 3);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello", s.data);
    wstr_free(s);
}

/* =========================================================================
   wstr_setFormat / wstr_appendFormat
   ========================================================================= */

static void test_wstr_setFormat_null_string_returns_false(void)
{
    TEST_ASSERT_FALSE(wstr_setFormat(NULL, "hello"));
}

static void test_wstr_setFormat_simple_string(void)
{
    String s = wstr_make();
    bool ok = wstr_setFormat(&s, "hello %s", "world");
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello world", s.data);
    wstr_free(s);
}

static void test_wstr_setFormat_replaces_existing_content(void)
{
    String s = wstr_fromCString("old content");
    bool ok = wstr_setFormat(&s, "new %d", 42);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("new 42", s.data);
    wstr_free(s);
}

static void test_wstr_setFormat_integers(void)
{
    String s = wstr_make();
    wstr_setFormat(&s, "%d %d %d", -1, 0, 99);
    TEST_ASSERT_EQUAL_STRING("-1 0 99", s.data);
    wstr_free(s);
}

static void test_wstr_setFormat_long_string(void)
{
    /* Force the retry loop in wstr__appendFormatAt */
    String s = wstr_make();
    bool ok = wstr_setFormat(&s,
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_size_t(128, s.length);
    wstr_free(s);
}

static void test_wstr_appendFormat_appends_to_existing(void)
{
    String s = wstr_fromCString("hello");
    bool ok = wstr_appendFormat(&s, " %s %d", "world", 42);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("hello world 42", s.data);
    wstr_free(s);
}

static void test_wstr_appendFormat_null_string_returns_false(void)
{
    TEST_ASSERT_FALSE(wstr_appendFormat(NULL, "hello"));
}

static void test_wstr_appendFormat_to_empty(void)
{
    String s = wstr_make();
    bool ok = wstr_appendFormat(&s, "n=%d", 7);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("n=7", s.data);
    wstr_free(s);
}

/* =========================================================================
   Capacity growth (wstr__grow via wstr_append)
   ========================================================================= */

static void test_wstr_capacity_grows_geometrically(void)
{
    /* Verify that repeated appends don't cause quadratic behavior
       and that capacity stays at or above length */
    String s = wstr_make();
    for (int i = 0; i < 100; i++)
    {
        TEST_ASSERT_TRUE(wstr_appendChar(&s, 'x'));
    }
    TEST_ASSERT_EQUAL_size_t(100, s.length);
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(100, s.capacity);
    wstr_free(s);
}

static void test_wstr_null_term_always_present_after_mutations(void)
{
    String s = wstr_make();
    wstr_appendCString(&s, "hello");
    TEST_ASSERT_EQUAL_CHAR(0, s.data[s.length]);
    wstr_appendCString(&s, " world");
    TEST_ASSERT_EQUAL_CHAR(0, s.data[s.length]);
    wstr_removeRange(&s, 0, 6);
    TEST_ASSERT_EQUAL_CHAR(0, s.data[s.length]);
    wstr_insert(&s, 0, wsv_fromCString("hi "));
    TEST_ASSERT_EQUAL_CHAR(0, s.data[s.length]);
    wstr_free(s);
}

/* =========================================================================
   WSV_LITERAL macro
   ========================================================================= */

static void test_wsv_literal_correct_length(void)
{
    StringView v = WSV_LITERAL("hello");
    TEST_ASSERT_EQUAL_size_t(5, v.length);
}

static void test_wsv_literal_correct_content(void)
{
    StringView v = WSV_LITERAL("hello");
    TEST_ASSERT_EQUAL_MEMORY("hello", v.data, 5);
}

static void test_wsv_literal_empty(void)
{
    StringView v = WSV_LITERAL("");
    TEST_ASSERT_EQUAL_size_t(0, v.length);
}

/* =========================================================================
   Integration / Round-trip tests
   ========================================================================= */

static void test_roundtrip_string_to_view_and_back(void)
{
    String original = wstr_fromCString("round-trip test");
    StringView view = wstr_view(&original);
    String copy = wstr_fromView(view);
    TEST_ASSERT_TRUE(wsv_equals(wstr_view(&original), wstr_view(&copy)));
    TEST_ASSERT_TRUE(original.data != copy.data);
    wstr_free(original);
    wstr_free(copy);
}

static void test_chop_csv_line(void)
{
    /* Parse a comma-separated line using wsv_chopByDelimiter */
    StringView remaining = wsv_fromCString("10,20,30,40");
    int values[4];
    for (int i = 0; i < 4; i++)
    {
        StringView field = wsv_chopByDelimiter(&remaining, ',');
        int32_t v = 0;
        wsv_tryParseS32(field, &v);
        values[i] = (int)v;
    }
    TEST_ASSERT_EQUAL_INT(10, values[0]);
    TEST_ASSERT_EQUAL_INT(20, values[1]);
    TEST_ASSERT_EQUAL_INT(30, values[2]);
    TEST_ASSERT_EQUAL_INT(40, values[3]);
}

static void test_tokenize_whitespace_sentence(void)
{
    /* Count tokens in a multi-space string */
    StringView text = wsv_fromCString("  the  quick  brown  fox  ");
    StringView sep  = wsv_fromCString(" ");
    StringView token;
    int count = 0;
    while (wsv_nextToken(&text, sep, &token))
    {
        count++;
    }
    TEST_ASSERT_EQUAL_INT(4, count);
}

static void test_build_path_with_format(void)
{
    String path = wstr_make();
    wstr_setFormat(&path, "%s/%s.%s", "assets", "data", "bin");
    TEST_ASSERT_EQUAL_STRING("assets/data.bin", path.data);
    wstr_free(path);
}

static void test_wstr_stress_append_and_trim_pipeline(void)
{
    String s = wstr_make();

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
    {
        TEST_ASSERT_TRUE(wstr_appendCString(&s, " x"));
    }

    TEST_ASSERT_TRUE(s.length > (size_t)SHL_TEST_MEDIUM_COUNT);
    StringView trimmed = wsv_trim(wstr_view(&s));
    TEST_ASSERT_EQUAL_CHAR('x', trimmed.data[0]);
    TEST_ASSERT_EQUAL_CHAR('x', trimmed.data[trimmed.length - 1]);
    wstr_free(s);
}

static void test_wsv_nextToken_stress_counts_all_tokens(void)
{
    String s = wstr_make();
    StringView separators = wsv_fromCString(", ");
    StringView token;
    int tokenCount = 0;

    for (int i = 0; i < SHL_TEST_MEDIUM_COUNT; i++)
    {
        TEST_ASSERT_TRUE(wstr_appendFormat(&s, "item%d, ", i));
    }

    StringView remaining = wstr_view(&s);
    while (wsv_nextToken(&remaining, separators, &token))
    {
        tokenCount++;
    }

    TEST_ASSERT_EQUAL_INT(SHL_TEST_MEDIUM_COUNT, tokenCount);
    wstr_free(s);
}

/* =========================================================================
   main
   ========================================================================= */

int main(void)
{
    UNITY_BEGIN();

    /* wsv_empty */
    RUN_TEST(test_wsv_empty_has_zero_length);
    RUN_TEST(test_wsv_empty_is_empty);
    RUN_TEST(test_wsv_empty_data_returns_empty_string);

    /* wsv_fromCString */
    RUN_TEST(test_wsv_fromCString_null_gives_empty);
    RUN_TEST(test_wsv_fromCString_empty_string);
    RUN_TEST(test_wsv_fromCString_normal);
    RUN_TEST(test_wsv_fromCString_with_embedded_content);

    /* wsv_fromParts */
    RUN_TEST(test_wsv_fromParts_null_and_zero_gives_empty);
    RUN_TEST(test_wsv_fromParts_normal);
    RUN_TEST(test_wsv_fromParts_full_length);

    /* wsv_fromRange */
    RUN_TEST(test_wsv_fromRange_null_begin_gives_empty);
    RUN_TEST(test_wsv_fromRange_null_end_gives_empty);
    RUN_TEST(test_wsv_fromRange_end_before_begin_gives_empty);
    RUN_TEST(test_wsv_fromRange_same_pointer_gives_zero_length);
    RUN_TEST(test_wsv_fromRange_normal);

    /* wsv_fromString */
    RUN_TEST(test_wsv_fromString_null_gives_empty);
    RUN_TEST(test_wsv_fromString_empty_string);
    RUN_TEST(test_wsv_fromString_normal);

    /* wsv_isEmpty / wsv_data / wsv_length */
    RUN_TEST(test_wsv_isEmpty_true_for_empty);
    RUN_TEST(test_wsv_isEmpty_false_for_non_empty);
    RUN_TEST(test_wsv_data_non_null_for_real_data);
    RUN_TEST(test_wsv_length_zero_for_empty);
    RUN_TEST(test_wsv_length_correct_for_normal);

    /* wsv_slice */
    RUN_TEST(test_wsv_slice_index_at_length_gives_empty);
    RUN_TEST(test_wsv_slice_index_beyond_length_gives_empty);
    RUN_TEST(test_wsv_slice_normal);
    RUN_TEST(test_wsv_slice_length_clamped_to_available);
    RUN_TEST(test_wsv_slice_zero_length);
    RUN_TEST(test_wsv_slice_full_view);

    /* wsv_subview */
    RUN_TEST(test_wsv_subview_index_zero_gives_same);
    RUN_TEST(test_wsv_subview_index_at_end_gives_empty);
    RUN_TEST(test_wsv_subview_index_beyond_end_gives_empty);
    RUN_TEST(test_wsv_subview_middle);

    /* wsv_trimLeft */
    RUN_TEST(test_wsv_trimLeft_no_spaces);
    RUN_TEST(test_wsv_trimLeft_leading_spaces);
    RUN_TEST(test_wsv_trimLeft_all_spaces);
    RUN_TEST(test_wsv_trimLeft_empty);
    RUN_TEST(test_wsv_trimLeft_tabs_and_newlines);

    /* wsv_trimRight */
    RUN_TEST(test_wsv_trimRight_no_spaces);
    RUN_TEST(test_wsv_trimRight_trailing_spaces);
    RUN_TEST(test_wsv_trimRight_all_spaces);
    RUN_TEST(test_wsv_trimRight_empty);

    /* wsv_trim */
    RUN_TEST(test_wsv_trim_both_sides);
    RUN_TEST(test_wsv_trim_no_spaces);
    RUN_TEST(test_wsv_trim_all_spaces);
    RUN_TEST(test_wsv_trim_empty);
    RUN_TEST(test_wsv_trim_only_left_spaces);
    RUN_TEST(test_wsv_trim_only_right_spaces);

    /* wsv_equals */
    RUN_TEST(test_wsv_equals_same_content);
    RUN_TEST(test_wsv_equals_different_content);
    RUN_TEST(test_wsv_equals_different_lengths);
    RUN_TEST(test_wsv_equals_both_empty);
    RUN_TEST(test_wsv_equals_one_empty_one_not);
    RUN_TEST(test_wsv_equals_case_sensitive);
    RUN_TEST(test_wsv_equals_single_char);

    /* wsv_equalsIgnoreCase */
    RUN_TEST(test_wsv_equalsIgnoreCase_same_case);
    RUN_TEST(test_wsv_equalsIgnoreCase_upper_lower);
    RUN_TEST(test_wsv_equalsIgnoreCase_different_content);
    RUN_TEST(test_wsv_equalsIgnoreCase_different_lengths);
    RUN_TEST(test_wsv_equalsIgnoreCase_both_empty);

    /* wsv_startsWith */
    RUN_TEST(test_wsv_startsWith_matching_prefix);
    RUN_TEST(test_wsv_startsWith_full_match);
    RUN_TEST(test_wsv_startsWith_empty_prefix_always_true);
    RUN_TEST(test_wsv_startsWith_prefix_longer_than_view);
    RUN_TEST(test_wsv_startsWith_no_match);

    /* wsv_startsWithIgnoreCase */
    RUN_TEST(test_wsv_startsWithIgnoreCase_matching);
    RUN_TEST(test_wsv_startsWithIgnoreCase_no_match);
    RUN_TEST(test_wsv_startsWithIgnoreCase_empty_prefix);

    /* wsv_findChar */
    RUN_TEST(test_wsv_findChar_found_at_start);
    RUN_TEST(test_wsv_findChar_found_in_middle);
    RUN_TEST(test_wsv_findChar_found_at_end);
    RUN_TEST(test_wsv_findChar_not_found);
    RUN_TEST(test_wsv_findChar_empty_view);
    RUN_TEST(test_wsv_findChar_null_char);

    /* wsv_find */
    RUN_TEST(test_wsv_find_empty_needle_returns_zero);
    RUN_TEST(test_wsv_find_needle_found_at_start);
    RUN_TEST(test_wsv_find_needle_found_in_middle);
    RUN_TEST(test_wsv_find_needle_not_found);
    RUN_TEST(test_wsv_find_needle_longer_than_view);
    RUN_TEST(test_wsv_find_needle_equals_view);
    RUN_TEST(test_wsv_find_single_char_needle);
    RUN_TEST(test_wsv_find_repeated_pattern);

    /* wsv_findAny */
    RUN_TEST(test_wsv_findAny_found_first);
    RUN_TEST(test_wsv_findAny_found_at_start);
    RUN_TEST(test_wsv_findAny_not_found);
    RUN_TEST(test_wsv_findAny_empty_view);
    RUN_TEST(test_wsv_findAny_empty_chars_never_matches);

    /* wsv_skipChars */
    RUN_TEST(test_wsv_skipChars_all_match);
    RUN_TEST(test_wsv_skipChars_none_match);
    RUN_TEST(test_wsv_skipChars_some_prefix_match);
    RUN_TEST(test_wsv_skipChars_empty_view);
    RUN_TEST(test_wsv_skipChars_empty_chars_no_skip);
    RUN_TEST(test_wsv_skipChars_multiple_char_set);

    /* wsv_takeUntilAny */
    RUN_TEST(test_wsv_takeUntilAny_match_at_start);
    RUN_TEST(test_wsv_takeUntilAny_match_in_middle);
    RUN_TEST(test_wsv_takeUntilAny_no_match_returns_full);
    RUN_TEST(test_wsv_takeUntilAny_empty_view);
    RUN_TEST(test_wsv_takeUntilAny_empty_chars_returns_full);

    /* wsv_splitOnce */
    RUN_TEST(test_wsv_splitOnce_null_left_returns_false);
    RUN_TEST(test_wsv_splitOnce_null_right_returns_false);
    RUN_TEST(test_wsv_splitOnce_empty_separator);
    RUN_TEST(test_wsv_splitOnce_separator_found);
    RUN_TEST(test_wsv_splitOnce_separator_not_found);
    RUN_TEST(test_wsv_splitOnce_separator_at_start);
    RUN_TEST(test_wsv_splitOnce_separator_at_end);
    RUN_TEST(test_wsv_splitOnce_multi_char_separator);
    RUN_TEST(test_wsv_splitOnce_uses_first_occurrence);

    /* wsv_chopByDelimiter */
    RUN_TEST(test_wsv_chopByDelimiter_null_remaining_returns_empty);
    RUN_TEST(test_wsv_chopByDelimiter_normal);
    RUN_TEST(test_wsv_chopByDelimiter_no_delimiter);
    RUN_TEST(test_wsv_chopByDelimiter_delimiter_at_start);
    RUN_TEST(test_wsv_chopByDelimiter_delimiter_at_end);
    RUN_TEST(test_wsv_chopByDelimiter_empty_remaining);
    RUN_TEST(test_wsv_chopByDelimiter_multiple_calls);

    /* wsv_nextToken */
    RUN_TEST(test_wsv_nextToken_null_remaining_returns_false);
    RUN_TEST(test_wsv_nextToken_null_token_returns_false);
    RUN_TEST(test_wsv_nextToken_empty_remaining_returns_false);
    RUN_TEST(test_wsv_nextToken_single_token);
    RUN_TEST(test_wsv_nextToken_multiple_tokens);
    RUN_TEST(test_wsv_nextToken_skips_multiple_consecutive_separators);
    RUN_TEST(test_wsv_nextToken_only_separators_returns_false);
    RUN_TEST(test_wsv_nextToken_multiple_separator_chars);

    /* wsv_hashFNV32 */
    RUN_TEST(test_wsv_hashFNV32_empty_returns_offset);
    RUN_TEST(test_wsv_hashFNV32_same_input_same_output);
    RUN_TEST(test_wsv_hashFNV32_different_strings_differ);
    RUN_TEST(test_wsv_hashFNV32_case_sensitive);
    RUN_TEST(test_wsv_hashFNV32_single_byte_a);
    RUN_TEST(test_wsv_hashFNV32_different_lengths_differ);

    /* wsv_parseS32 / wsv_tryParseS32 */
    RUN_TEST(test_wsv_parseS32_zero);
    RUN_TEST(test_wsv_parseS32_positive);
    RUN_TEST(test_wsv_parseS32_negative);
    RUN_TEST(test_wsv_parseS32_with_plus_sign);
    RUN_TEST(test_wsv_parseS32_leading_whitespace);
    RUN_TEST(test_wsv_parseS32_trailing_whitespace);
    RUN_TEST(test_wsv_parseS32_both_whitespace);
    RUN_TEST(test_wsv_parseS32_max_value);
    RUN_TEST(test_wsv_parseS32_min_value);
    RUN_TEST(test_wsv_parseS32_hex);
    RUN_TEST(test_wsv_parseS32_hex_uppercase);
    RUN_TEST(test_wsv_parseS32_octal);
    RUN_TEST(test_wsv_tryParseS32_overflow_returns_false);
    RUN_TEST(test_wsv_tryParseS32_underflow_returns_false);
    RUN_TEST(test_wsv_tryParseS32_invalid_chars_returns_false);
    RUN_TEST(test_wsv_tryParseS32_sign_only_returns_false);
    RUN_TEST(test_wsv_tryParseS32_null_value_ptr_still_returns_true);
    RUN_TEST(test_wsv_tryParseS32_invalid_octal_digit);
    RUN_TEST(test_wsv_parseS32_invalid_returns_zero);

    /* wsv_parseS64 / wsv_tryParseS64 */
    RUN_TEST(test_wsv_parseS64_zero);
    RUN_TEST(test_wsv_parseS64_large_positive);
    RUN_TEST(test_wsv_parseS64_large_negative);
    RUN_TEST(test_wsv_parseS64_max_value);
    RUN_TEST(test_wsv_parseS64_min_value);
    RUN_TEST(test_wsv_tryParseS64_overflow_returns_false);
    RUN_TEST(test_wsv_tryParseS64_invalid_returns_false);
    RUN_TEST(test_wsv_tryParseS64_hex);
    RUN_TEST(test_wsv_tryParseS64_null_value_ptr);

    /* wsv_copyToBuffer */
    RUN_TEST(test_wsv_copyToBuffer_normal);
    RUN_TEST(test_wsv_copyToBuffer_exact_capacity);
    RUN_TEST(test_wsv_copyToBuffer_too_small_returns_false);
    RUN_TEST(test_wsv_copyToBuffer_null_buffer_returns_false);
    RUN_TEST(test_wsv_copyToBuffer_zero_capacity_returns_false);
    RUN_TEST(test_wsv_copyToBuffer_empty_view);

    /* wsv_toString */
    RUN_TEST(test_wsv_toString_creates_copy);
    RUN_TEST(test_wsv_toString_empty_view);

    /* wstr_make */
    RUN_TEST(test_wstr_make_zeroed);
    RUN_TEST(test_wstr_make_isEmpty);

    /* wstr_withCapacity */
    RUN_TEST(test_wstr_withCapacity_capacity_set);
    RUN_TEST(test_wstr_withCapacity_zero);

    /* wstr_fromCString */
    RUN_TEST(test_wstr_fromCString_null_gives_empty);
    RUN_TEST(test_wstr_fromCString_empty_string);
    RUN_TEST(test_wstr_fromCString_normal);
    RUN_TEST(test_wstr_fromCString_null_terminated);
    RUN_TEST(test_wstr_fromCStringFormat_simple_string);
    RUN_TEST(test_wstr_fromCStringFormat_null_format_gives_empty);
    RUN_TEST(test_wstr_fromCStringFormatv_formats_arguments);
    RUN_TEST(test_wstr_fromCStringFormatv_long_string_grows_buffer);

    /* wstr_fromView */
    RUN_TEST(test_wstr_fromView_normal);
    RUN_TEST(test_wstr_fromView_empty);

    /* wstr_adopt */
    RUN_TEST(test_wstr_adopt_null_buffer_gives_empty);
    RUN_TEST(test_wstr_adopt_normal);
    RUN_TEST(test_wstr_adopt_length_clamped_to_capacity);

    /* wstr_free_ptr / wstr_free */
    RUN_TEST(test_wstr_free_ptr_null_is_safe);
    RUN_TEST(test_wstr_free_ptr_resets_string);
    RUN_TEST(test_wstr_free_null_string_is_safe);
    RUN_TEST(test_wstr_free_releases_owned_buffer);

    /* wstr_clear */
    RUN_TEST(test_wstr_clear_null_is_safe);
    RUN_TEST(test_wstr_clear_resets_length);
    RUN_TEST(test_wstr_clear_empty_string_is_safe);

    /* wstr_view / wstr_cstr / wstr_isEmpty */
    RUN_TEST(test_wstr_view_null_gives_empty);
    RUN_TEST(test_wstr_view_normal);
    RUN_TEST(test_wstr_cstr_null_gives_empty_string);
    RUN_TEST(test_wstr_cstr_normal);
    RUN_TEST(test_wstr_isEmpty_null_is_true);
    RUN_TEST(test_wstr_isEmpty_empty_string_is_true);
    RUN_TEST(test_wstr_isEmpty_non_empty_is_false);

    /* wstr_reserve */
    RUN_TEST(test_wstr_reserve_grows_capacity);
    RUN_TEST(test_wstr_reserve_already_sufficient);

    /* wstr_resize */
    RUN_TEST(test_wstr_resize_null_returns_false);
    RUN_TEST(test_wstr_resize_grow_zero_fills);
    RUN_TEST(test_wstr_resize_shrink);
    RUN_TEST(test_wstr_resize_to_same_length);
    RUN_TEST(test_wstr_resize_to_zero);

    /* wstr_assign / wstr_assignCString */
    RUN_TEST(test_wstr_assign_null_string_returns_false);
    RUN_TEST(test_wstr_assign_normal);
    RUN_TEST(test_wstr_assign_overwrites_existing_content);
    RUN_TEST(test_wstr_assign_empty_view_clears);
    RUN_TEST(test_wstr_assign_self_aliased);
    RUN_TEST(test_wstr_assignCString_normal);
    RUN_TEST(test_wstr_assignCString_null_clears);

    /* wstr_append / wstr_appendCString / wstr_appendChar */
    RUN_TEST(test_wstr_append_null_string_returns_false);
    RUN_TEST(test_wstr_append_to_empty);
    RUN_TEST(test_wstr_append_to_existing);
    RUN_TEST(test_wstr_append_empty_view_is_noop);
    RUN_TEST(test_wstr_append_self_aliased);
    RUN_TEST(test_wstr_append_self_suffix_aliased);
    RUN_TEST(test_wstr_appendCString_normal);
    RUN_TEST(test_wstr_appendCString_null_is_noop);
    RUN_TEST(test_wstr_appendChar_normal);
    RUN_TEST(test_wstr_appendChar_to_empty);

    /* wstr_insert */
    RUN_TEST(test_wstr_insert_null_string_returns_false);
    RUN_TEST(test_wstr_insert_index_beyond_length_returns_false);
    RUN_TEST(test_wstr_insert_at_start);
    RUN_TEST(test_wstr_insert_in_middle);
    RUN_TEST(test_wstr_insert_at_end);
    RUN_TEST(test_wstr_insert_empty_view_is_noop);
    RUN_TEST(test_wstr_insert_self_aliased);

    /* wstr_removeRange */
    RUN_TEST(test_wstr_removeRange_null_returns_false);
    RUN_TEST(test_wstr_removeRange_index_beyond_length_returns_false);
    RUN_TEST(test_wstr_removeRange_zero_length_is_noop);
    RUN_TEST(test_wstr_removeRange_from_start);
    RUN_TEST(test_wstr_removeRange_from_middle);
    RUN_TEST(test_wstr_removeRange_length_clamped_at_end);
    RUN_TEST(test_wstr_removeRange_entire_string);
    RUN_TEST(test_wstr_removeRange_at_exact_end_is_noop);

    /* wstr_setFormat / wstr_appendFormat */
    RUN_TEST(test_wstr_setFormat_null_string_returns_false);
    RUN_TEST(test_wstr_setFormat_simple_string);
    RUN_TEST(test_wstr_setFormat_replaces_existing_content);
    RUN_TEST(test_wstr_setFormat_integers);
    RUN_TEST(test_wstr_setFormat_long_string);
    RUN_TEST(test_wstr_appendFormat_appends_to_existing);
    RUN_TEST(test_wstr_appendFormat_null_string_returns_false);
    RUN_TEST(test_wstr_appendFormat_to_empty);

    /* Capacity / null terminator invariants */
    RUN_TEST(test_wstr_capacity_grows_geometrically);
    RUN_TEST(test_wstr_null_term_always_present_after_mutations);

    /* WSV_LITERAL */
    RUN_TEST(test_wsv_literal_correct_length);
    RUN_TEST(test_wsv_literal_correct_content);
    RUN_TEST(test_wsv_literal_empty);

    /* Integration */
    RUN_TEST(test_roundtrip_string_to_view_and_back);
    RUN_TEST(test_chop_csv_line);
    RUN_TEST(test_tokenize_whitespace_sentence);
    RUN_TEST(test_build_path_with_format);
    RUN_TEST(test_wstr_stress_append_and_trim_pipeline);
    RUN_TEST(test_wsv_nextToken_stress_counts_all_tokens);

    return UNITY_END();
}

