/* libxmq - Copyright (C) 2023 Fredrik Öhrström (spdx: MIT)

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include"xmq.h"
#include"version.h"

#include<assert.h>
#include<ctype.h>
#include<errno.h>
#include<math.h>
#include<setjmp.h>
#include<stdarg.h>
#include<stdbool.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#include<libxml/tree.h>
#include<libxml/parser.h>
#include<libxml/HTMLparser.h>
#include<libxml/xmlreader.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

// DEFAULT COLORS ///////////////////////////////////////////////

enum _ColorType
{
    COLORTYPE_xmq_c = 0, // Comments
    COLORTYPE_xmq_q = 1, // Standalone quote.
    COLORTYPE_xmq_e = 2, // Entity
    COLORTYPE_xmq_ens = 3, // Element Namespace
    COLORTYPE_xmq_en = 4, // Element Name
    COLORTYPE_xmq_ek = 5, // Element Key
    COLORTYPE_xmq_ekv = 6, // Element Key Value
    COLORTYPE_xmq_ans = 7, // Attribute NameSpace
    COLORTYPE_xmq_ak = 8, // Attribute Key
    COLORTYPE_xmq_akv = 9, // Attribute Key Value
    COLORTYPE_xmq_cp = 10, // Compound Parentheses
    COLORTYPE_xmq_uw = 11, // Unicode Whitespace
    COLORTYPE_xmq_tw = 12, // Tab Whitespace
};
typedef enum _ColorType ColorType;

const char *color_names[13] = {
    "xmq_c",
    "xmq_q",
    "xmq_e",
    "xmq_ens",
    "xmq_en",
    "xmq_ek",
    "xmq_ekv",
    "xmq_ans",
    "xmq_ak",
    "xmq_akv",
    "xmq_cp",
    "xmq_uw",
    "xmq_tw",
};

// DECLARATIONS /////////////////////////////////////////////////

#define LIST_OF_XMQ_TOKENS  \
    X(whitespace)           \
    X(equals)               \
    X(brace_left)           \
    X(brace_right)          \
    X(apar_left)            \
    X(apar_right)           \
    X(cpar_left)            \
    X(cpar_right)           \
    X(quote)                \
    X(entity)               \
    X(comment)              \
    X(comment_continuation) \
    X(element_ns)           \
    X(element_name)         \
    X(element_key)          \
    X(element_value_text)   \
    X(element_value_quote)  \
    X(element_value_entity) \
    X(element_value_compound_quote)  \
    X(element_value_compound_entity) \
    X(attr_ns)             \
    X(attr_key)            \
    X(attr_value_text)     \
    X(attr_value_quote)    \
    X(attr_value_entity)   \
    X(attr_value_compound_quote)    \
    X(attr_value_compound_entity)   \
    X(ns_colon)  \

#define MAGIC_COOKIE 7287528
#define PRINT_STDOUT(...) printf(__VA_ARGS__)
#define PRINT_ERROR(...) fprintf(stderr, __VA_ARGS__)

struct XMQNode
{
    xmlNodePtr node;
};

struct XMQDoc
{
    union {
        xmlDocPtr xml;
        htmlDocPtr html;
    } docptr_; // Is both xmlDocPtr and htmlDocPtr.
    const char *source_name_; // File name or url from which the documented was fetched.
    int errno_; // A parse error is assigned a number.
    const char *error_; // And the error is explained here.
    XMQNode root_; // The root node.
};

enum Level {
    LEVEL_XMQ = 0,
    LEVEL_ELEMENT_VALUE = 1,
    LEVEL_ELEMENT_VALUE_COMPOUND = 2,
    LEVEL_ATTR_VALUE = 3,
    LEVEL_ATTR_VALUE_COMPOUND = 4
};
typedef enum Level Level;

typedef struct StackElement StackElement;
struct StackElement
{
    void *data;
    StackElement *below; // When this element is popped, below becomes top element.
};

struct Stack
{
    StackElement *bottom;
    StackElement *top;
    size_t size;
};
typedef struct Stack Stack;

struct XMQParseState
{
    char *source_name; // Only used for generating any error messages.
    void *out;
    const char *buffer_start; // Points to first byte in buffer.
    const char *buffer_stop;   // Points to byte >after< last byte in buffer.
    const char *i;
    size_t line;
    size_t col;
    int error_nr;
    char *generated_error_msg;
    jmp_buf error_handler;
    bool simulated;
    XMQParseCallbacks *parse;
    XMQDoc *doq;
    const char *implicit_root; // Assume that this is the first element name
    Stack *element_stack; // Top is last created node
    void *element_last; // Last added sibling to stack top node.
    bool parsing_doctype;
    XMQOutputSettings *output_settings; // Used when coloring existing text using the tokenizer.
    int magic_cookie;

    // These are used for better error reporting.
    const char *last_body_start;
    size_t last_body_start_line;
    size_t last_body_start_col;
    const char *last_attr_start;
    size_t last_attr_start_line;
    size_t last_attr_start_col;
    const char *last_quote_start;
    size_t last_quote_start_line;
    size_t last_quote_start_col;
    const char *last_compound_start;
    size_t last_compound_start_line;
    size_t last_compound_start_col;
    const char *last_equals_start;
    size_t last_equals_start_line;
    size_t last_equals_start_col;
};

/**
   XMQPrintState:
   @current_indent: The current_indent stores how far we have printed on the current line.
                    0 means nothing has been printed yet.
   @line_indent:  The line_indent stores the current indentation level from which print
                  starts for each new line. The line_indent is 0 when we start printing the xmq.
   @last_char: the last_char remembers the most recent printed character. or 0 if no char has been printed yet.
   @color_pre: The active color prefix.
   @prev_color_pre: The previous color prefix, used for restoring utf8 text after coloring unicode whitespace.
   @restart_line: after nl_and_indent print this to restart coloring of line.
   @output_settings: the output settings.
   @doc: The xmq document that is being printed.
*/
struct XMQPrintState
{
    size_t current_indent;
    size_t line_indent;
    int last_char;
    const char *color_pre;
    const char *prev_color_pre;
    const char *restart_line;
    XMQOutputSettings *output_settings;
    XMQDoc *doq;
};
typedef struct XMQPrintState XMQPrintState;

/**
    MemBuffer:
    @max_: Current size of malloced buffer.
    @used_: Number of bytes actually used of buffer.
    @buffer_: Start of buffer data.
*/
typedef struct
{
    size_t max_; // Current size of malloc for buffer.
    size_t used_; // How much is actually used.
    char *buffer_; // The malloced data.
} MemBuffer;

/**
    XMQQuoteSettings:
    @force:           Always add single quotes. More quotes if necessary.
    @compact:         Generate compact quote on a single line. Using &#10; and no superfluous whitespace.
    @value_after_key: If enties are introduced by the quoting, then use compound (( )) around the content.

    @indentation_space: Use this as the indentation character. If NULL default to " ".
    @explicit_space: Use this as the explicit space/indentation character. If NULL default to " ".
    @newline:     Use this as the newline character. If NULL default to "\n".
    @prefix_line: Prepend each line with this. If NULL default to empty string.
    @postfix_line Suffix each line whith this. If NULL default to empty string.
    @prefix_entity: Prepend each entity with this. If NULL default to empty string.
    @postfix_entity: Suffix each entity whith this. If NULL default to empty string.
    @prefix_doublep: Prepend each ( ) with this. If NULL default to empty string.
    @postfix_doublep: Suffix each ( ) whith this. If NULL default to empty string.
*/
struct XMQQuoteSettings
{
    bool force; // Always add single quotes. More quotes if necessary.
    bool compact; // Generate compact quote on a single line. Using &#10; and no superfluous whitespace.
    bool value_after_key; // If enties are introduced by the quoting, then use compound (( )) around the content.

    const char *indentation_space;  // Use this as the indentation character. If NULL default to " ".
    const char *explicit_space;  // Use this as the explicit space character inside quotes. If NULL default to " ".
    const char *explicit_nl;      // Use this as the newline character. If NULL default to "\n".
    const char *explicit_tab;      // Use this as the tab character. If NULL default to "\t".
    const char *explicit_cr;      // Use this as the cr character. If NULL default to "\r".
    const char *prefix_line;  // Prepend each line with this. If NULL default to empty string.
    const char *postfix_line; // Append each line whith this, before any newline.
    const char *prefix_entity;  // Prepend each entity with this. If NULL default to empty string.
    const char *postfix_entity; // Append each entity whith this. If NULL default to empty string.
    const char *prefix_doublep;  // Prepend each ( ) with this. If NULL default to empty string.
    const char *postfix_doublep; // Append each ( ) whith this. If NULL default to empty string.
};
typedef struct XMQQuoteSettings XMQQuoteSettings;

// An utf8 char is at most 4 bytes since the max unicode nr is capped at U+10FFFF:
#define MAX_NUM_UTF8_BYTES 4
typedef struct
{
    char bytes[MAX_NUM_UTF8_BYTES];
} UTF8Char;

// UTILITY FUNCTION DECLARATIONS ///////////////////////////////////////////////////

Stack *new_stack();
void free_stack(Stack *stack);
void push_stack(Stack *s, void *);
size_t size_stack();
void *pop_stack(Stack *s);

/**
   decode_utf8: Peek 1 to 4 chars from start and calculate unicode codepoint.
   @start: Read utf8 from this string.
   @stop: Points to byte after last byte in string. If NULL assume start is null terminated.
   @out_char: Store the unicode code point here.
   @out_len: How many bytes the utf8 char used.

   Return true if valid utf8 char.
*/
bool decode_utf8(const char *start, const char *stop, int *out_char, size_t *out_len);

/**
   peek_utf8_char: Peek 1 to 4 chars from s belonging to the next utf8 code point and store them in uc.
   @start: Read utf8 from this string.
   @stop: Points to byte after last byte in string. If NULL assume start is null terminated.
   @uc: Store the UTF8 char here.

   Return the number of bytes peek UTF8 char, use this number to skip ahead to next char in s.
*/
size_t peek_utf8_char(const char *start, const char *stop, UTF8Char *uc);

/**
   utf8_char_to_codepoint_string: Decode a utf8 char and store as a string "U+123"
   @uc: The utf8 char to decode.
   @buf: Store the codepoint string here.

*/
bool utf8_char_to_codepoint_string(UTF8Char *uc, char *buf);

void increment(char c, size_t num_bytes, const char **i, size_t *line, size_t *col);
bool is_xml_whitespace(char c);
bool is_all_xml_whitespace(const char *start);
size_t count_whitespace(const char *i, const char *stop);
bool is_unicode_whitespace(const char *start, const char *stop);
const char *has_leading_nl_whitespace(const char *start, const char *stop);
const char *has_ending_nl_whitespace(const char *start, const char *stop);
bool has_newlines(const char *start, const char *stop);
bool has_all_quotes(const char *start, const char *stop);
bool has_all_whitespace(const char *start, const char *stop, bool *all_space);
bool is_lowercase_hex(char c);
bool is_xmq_quote_start(char c);
bool is_xmq_entity_start(char c);
bool is_xmq_attribute_key_start(char c);
bool is_xmq_compound_start(char c);
bool is_xmq_comment_start(char c, char cc);
bool is_xmq_doctype_start(const char *start, const char *stop);
bool is_xmq_text_value_char(const char *i, const char *stop);
bool is_xmq_text_value(const char *i, const char *stop);
bool is_xmq_text_name(char c);
bool is_xmq_element_start(char c);
static const char *build_error_message(const char *fmt, ...);

size_t count_xmq_slashes(const char *i, const char *stop, bool *found_asterisk);
size_t count_necessary_quotes(const char *start, const char *stop, bool forbid_nl, bool *add_nls, bool *add_compound);
size_t count_necessary_slashes(const char *start, const char *stop);

bool contains_newline(const char *start, const char *stop);
Level enter_compound_level(Level l);
XMQColor level_to_quote_color(Level l);
XMQColor level_to_entity_color(Level l);
void str_b_u_len(const char *start, const char *stop, size_t *b_len, size_t *u_len);
void attr_strlen_name_prefix(xmlAttr *attr, const char **name, const char **prefix, size_t *total_u_len);
void element_strlen_name_prefix(xmlNode *attr, const char **name, const char **prefix, size_t *total_u_len);
void namespace_strlen_prefix(xmlNs *ns, const char **prefix, size_t *total_u_len);
size_t find_attr_key_max_u_width(xmlAttr *a);
size_t find_namespace_max_u_width(size_t max, xmlNs *ns);
size_t find_element_key_max_width(xmlNodePtr node, xmlNodePtr *restart_find_at_node);
bool is_hex(char c);
const char *find_word_ignore_case(const char *start, const char *stop, const char *word);

XMQParseState *xmqAllocateParseState(XMQParseCallbacks *callbacks);
void xmqFreeParseState(XMQParseState *state);
bool xmqTokenizeBuffer(XMQParseState *state, const char *start, const char *stop);
bool xmqTokenizeFile(XMQParseState *state, const char *file);

void setup_terminal_coloring(XMQColoring *c, bool dark_mode, bool use_color, bool render_raw);
void setup_html_coloring(XMQColoring *c, bool dark_mode, bool use_color, bool render_raw);
void setup_tex_coloring(XMQColoring *c, bool dark_mode, bool use_color, bool render_raw);

// XMQ tokenizer functions ///////////////////////////////////////////////////////////

void eat_whitespace(XMQParseState *state, const char **start, const char **stop);
void eat_xmq_entity(XMQParseState *state, const char **content_start, const char **content_stop);
void eat_xmq_comment_to_eol(XMQParseState *state, const char **content_start, const char **content_stop);
void eat_xmq_comment_to_close(XMQParseState *state, const char **content_start, const char **content_stop, size_t n, bool *found_asterisk);
void eat_xmq_text_name(XMQParseState *state, const char **content_start, const char **content_stop);
void eat_xmq_text_value(XMQParseState *state, const char **content_start, const char **content_stop);
bool peek_xmq_next_is_equal(XMQParseState *state);
size_t count_xmq_quotes(const char *i, const char *stop);
size_t eat_xmq_quote(XMQParseState *state, const char **content_start, const char **content_stop);
char *xmq_trim_quote(size_t indent, char space, const char *start, const char *stop);
void xmq_fixup_html_before_writeout(XMQDoc *doq);

char *xmq_quote_as_c(const char *start, const char *stop);
char *xmq_unquote_as_c(const char *start, const char *stop);
char *xmq_comment(int indent,
                 const char *start,
                 const char *stop,
                 XMQQuoteSettings *settings);
char *xmq_un_comment(size_t indent, char space, const char *start, const char *stop);
char *xmq_un_quote(size_t indent, char space, const char *start, const char *stop, bool remove_qs);

// XMQ syntax parser functions ///////////////////////////////////////////////////////////

void parse_xmq(XMQParseState *state);
void parse_xmq_attribute(XMQParseState *state);
void parse_xmq_attributes(XMQParseState *state);
void parse_xmq_comment(XMQParseState *state, char cc);
void parse_xmq_compound(XMQParseState *state, Level level);
void parse_xmq_compound_children(XMQParseState *state, Level level);
void parse_xmq_element_internal(XMQParseState *state, bool doctype, bool pi);
void parse_xmq_element(XMQParseState *state);
void parse_xmq_doctype(XMQParseState *state);
void parse_xmq_pi(XMQParseState *state);
void parse_xmq_entity(XMQParseState *state, Level level);
void parse_xmq_quote(XMQParseState *state, Level level);
void parse_xmq_text_any(XMQParseState *state);
void parse_xmq_text_name(XMQParseState *state);
void parse_xmq_text_value(XMQParseState *state, Level level);
void parse_xmq_value(XMQParseState *state, Level level);
void parse_xmq_whitespace(XMQParseState *state);

// XML/HTML dom functions ///////////////////////////////////////////////////////////////

xmlDtdPtr parse_doctype_raw(XMQDoc *doq, const char *start, const char *stop);
xmlNode *xml_first_child(xmlNode *node);
xmlNode *xml_last_child(xmlNode *node);
xmlNode *xml_prev_sibling(xmlNode *node);
xmlNode *xml_next_sibling(xmlNode *node);
xmlAttr *xml_first_attribute(xmlNode *node);
xmlNs *xml_first_namespace_def(xmlNode *node);
xmlNs *xml_next_namespace_def(xmlNs *ns);
xmlAttr *xml_next_attribute(xmlAttr *attr);
const char*xml_element_name(xmlNode *node);
const char*xml_element_content(xmlNode *node);
const char *xml_element_ns_prefix(const xmlNode *node);
const char*xml_attr_key(xmlAttr *attr);
const char*xml_namespace_href(xmlNs *ns);
bool is_entity_node(const xmlNode *node);
bool is_content_node(const xmlNode *node);
bool is_doctype_node(const xmlNode *node);
bool is_comment_node(const xmlNode *node);
bool is_leaf_node(xmlNode *node);
bool is_key_value_node(xmlNode *node);
void trim_node(xmlNode *node, XMQTrimType tt);
void trim_text_node(xmlNode *node, XMQTrimType tt);

// Output buffer functions ////////////////////////////////////////////////////////

MemBuffer *new_membuffer();
char *free_membuffer_but_return_trimmed_content(MemBuffer *mb);
void free_membuffer_and_free_content(MemBuffer *mb);
size_t pick_buffer_new_size(size_t max, size_t used, size_t add);
void membuffer_append_region(MemBuffer *mb, const char *start, const char *stop);
void membuffer_append(MemBuffer *mb, const char *start);
void membuffer_append_char(MemBuffer *mb, char c);
void membuffer_append_null(MemBuffer *mb);
void membuffer_append_entity(MemBuffer *mb, char c);

// Output buffer functions ////////////////////////////////////////////////////////

bool need_separation_before_attribute_key(XMQPrintState *ps);
bool need_separation_before_element_name(XMQPrintState *ps);
bool need_separation_before_quote(XMQPrintState *ps);
bool need_separation_before_comment(XMQPrintState *ps);

void check_space_before_attribute(XMQPrintState *ps);
void check_space_before_comment(XMQPrintState *ps);
void check_space_before_entity_node(XMQPrintState *ps);
void check_space_before_key(XMQPrintState *ps);
void check_space_before_opening_brace(XMQPrintState *ps);
void check_space_before_closing_brace(XMQPrintState *ps);
void check_space_before_quote(XMQPrintState *ps, Level level);

bool quote_needs_compounded(XMQPrintState *ps, const char *start, const char *stop);

// Printing the DOM as xmq/htmq ///////////////////////////////////////////////////

size_t print_char_entity(XMQPrintState *ps, XMQColor c, const char *start, const char *stop);
void print_color_post(XMQPrintState *ps, XMQColor c);
void print_color_pre(XMQPrintState *ps, XMQColor c);
void print_comment_line(XMQPrintState *ps, const char *start, const char *stop, bool compact);
void print_comment_lines(XMQPrintState *ps, const char *start, const char *stop, bool compact);
void print_nl_and_indent(XMQPrintState *ps, const char *prefix, const char *postfix);
void print_quote_lines_and_color_uwhitespace(XMQPrintState *ps, XMQColor c, const char *start, const char *stop);
void print_quoted_spaces(XMQPrintState *ps, XMQColor c, int n);
void print_quotes(XMQPrintState *ps, size_t num, XMQColor c);
void print_slashes(XMQPrintState *ps, const char *pre, const char *post, size_t n);
void print_white_spaces(XMQPrintState *ps, int n);
void print_all_whitespace(XMQPrintState *ps, const char *start, const char *stop, Level level);

void print_nodes(XMQPrintState *ps, xmlNode *from, xmlNode *to, size_t align);

void print_node(XMQPrintState *ps, xmlNode *node, size_t align);
void print_entity_node(XMQPrintState *ps, xmlNode *node);
void print_content_node(XMQPrintState *ps, xmlNode *node);
void print_comment_node(XMQPrintState *ps, xmlNode *node);
void print_doctype(XMQPrintState *ps, xmlNode *node);
void print_key_node(XMQPrintState *ps, xmlNode *node, size_t align);
void print_leaf_node(XMQPrintState *ps, xmlNode *node);

void print_element_with_children(XMQPrintState *ps, xmlNode *node, size_t align);

size_t print_element_name_and_attributes(XMQPrintState *ps, xmlNode *node);
void print_attribute(XMQPrintState *ps, xmlAttr *a, size_t align);
void print_attributes(XMQPrintState *ps, xmlNodePtr node);

void print_value(XMQPrintState *ps, xmlNode *node, Level level);
void print_value_internal_text(XMQPrintState *ps, const char *start, const char *stop, Level level);
void print_value_internal(XMQPrintState *ps, xmlNode *node, Level level);
const char *needs_escape(XMQRenderFormat f, const char *start, const char *stop);
size_t print_utf8_internal(XMQPrintState *ps, const char *start, const char *stop);
size_t print_utf8(XMQPrintState *ps, XMQColor c, size_t num_pairs, ...);
size_t print_utf8_char(XMQPrintState *ps, const char *start, const char *stop);
void print_quote(XMQPrintState *ps, XMQColor c, const char *start, const char *stop);

// DEFINITIONS ///////////////////////////////////////////////////////////////////////////////

void check_malloc(void *a)
{
    if (!a)
    {
        PRINT_ERROR("libxmq: Out of memory!\n");
        exit(1);
    }
}

/** Allocate a automatically growing membuffer. */
MemBuffer *new_membuffer()
{
    MemBuffer *mb = (MemBuffer*)malloc(sizeof(MemBuffer));
    check_malloc(mb);
    memset(mb, 0, sizeof(*mb));
    return mb;
}

/** Free the MemBuffer support struct but return the actual contents. */
char *free_membuffer_but_return_trimmed_content(MemBuffer *mb)
{
    char *b = mb->buffer_;
    b = (char*)realloc(b, mb->used_);
    free(mb);
    return b;
}

void free_membuffer_and_free_content(MemBuffer *mb)
{
    if (mb->buffer_) free(mb->buffer_);
    mb->buffer_ = NULL;
    free(mb);
}

size_t pick_buffer_new_size(size_t max, size_t used, size_t add)
{
    assert(used <= max);
    if (used + add > max)
    {
        // Increment buffer with 1KiB.
        max = max + 1024;
    }
    if (used + add > max)
    {
        // Still did not fit? The add was more than 1 Kib. Lets add that.
        max = max + add;
    }
    assert(used + add <= max);

    return max;
}

void membuffer_append_region(MemBuffer *mb, const char *start, const char *stop)
{
    if (!start) return;
    if (!stop) stop = start + strlen(start);
    size_t add = stop-start;
    size_t max = pick_buffer_new_size(mb->max_, mb->used_, add);
    if (max > mb->max_)
    {
        mb->buffer_ = (char*)realloc(mb->buffer_, max);
        mb->max_ = max;
    }
    memcpy(mb->buffer_+mb->used_, start, add);
    mb->used_ += add;
}

void membuffer_append(MemBuffer *mb, const char *start)
{
    const char *i = start;
    char *to = mb->buffer_+mb->used_;
    const char *stop = mb->buffer_+mb->max_;

    while (*i)
    {
        if (to >= stop)
        {
            mb->used_ = to - mb->buffer_;
            size_t max = pick_buffer_new_size(mb->max_, mb->used_, 1);
            assert(max >= mb->max_);
            mb->buffer_ = (char*)realloc(mb->buffer_, max);
            mb->max_ = max;
            stop = mb->buffer_+mb->max_;
            to = mb->buffer_+mb->used_;
        }
        *to = *i;
        i++;
        to++;
    }
    mb->used_ = to-mb->buffer_;
}

void membuffer_append_char(MemBuffer *mb, char c)
{
    size_t max = pick_buffer_new_size(mb->max_, mb->used_, 1);
    if (max > mb->max_)
    {
        mb->buffer_ = (char*)realloc(mb->buffer_, max);
        mb->max_ = max;
    }
    memcpy(mb->buffer_+mb->used_, &c, 1);
    mb->used_ ++;
}

void membuffer_append_null(MemBuffer *mb)
{
    membuffer_append_char(mb, 0);
}

void membuffer_append_entity(MemBuffer *mb, char c)
{
    if (c == ' ') membuffer_append(mb, "&#32;");
    else if (c == '\n') membuffer_append(mb, "&#10;");
    else if (c == '\t') membuffer_append(mb, "&#9;");
    else if (c == '\r') membuffer_append(mb, "&#13;");
    else
    {
        assert(false);
    }
}

typedef void (*XMQContentCallback)(XMQParseState *state,
                                   size_t start_line,
                                   size_t start_col,
                                   const char *start,
                                   size_t content_start_col,
                                   const char *content_start,
                                   const char *content_stop,
                                   const char *stop);

struct XMQParseCallbacks
{
#define X(TYPE) \
    XMQContentCallback handle_##TYPE;
LIST_OF_XMQ_TOKENS
#undef X

    void (*init)(XMQParseState*);
    void (*done)(XMQParseState*);

    int magic_cookie;
};

struct XMQPrintCallbacks
{
    void (*writeIndent)(int n);
    void (*writeElementName)(char *start, char *stop);
    void (*writeElementContent)(char *start, char *stop);
};

#define DO_CALLBACK(TYPE, state, start_line, start_col, start, content_start_col, content_start, content_stop, stop) \
    { if (state->parse->handle_##TYPE != NULL) state->parse->handle_##TYPE(state,start_line,start_col,start,content_start_col,content_start,content_stop,stop); }

#define DO_CALLBACK_SIM(TYPE, state, start_line, start_col, start, content_start_col, content_start, content_stop, stop) \
    { if (state->parse->handle_##TYPE != NULL) { state->simulated=true; state->parse->handle_##TYPE(state,start_line,start_col,start,content_start_col,content_start,content_stop,stop); state->simulated=false; } }

typedef struct
{
    char *buf;   // Malloced memory, that might be reallocated.
    size_t size; // Total allocated
    size_t used; // Actually used.
}  InternalBuffer;

void new_buffer(InternalBuffer *ib, size_t l);
void free_buffer(InternalBuffer *ib);
void append_buffer(InternalBuffer *ib, const char *start, const char *stop);
void trim_buffer(InternalBuffer *ib);

bool debug_enabled();

void xmq_setup_parse_callbacks(XMQParseCallbacks *callbacks);

char ansi_reset_color[] = "\033[0m";

#define NOCOLOR      "\033[0m"
#define GREEN        "\033[0;32m"
#define DARK_GREEN   "\033[0;1;32m"
#define BLUE         "\033[0;38;5;27m"
#define BLUE_UNDERLINE "\033[0;4;38;5;27m"
#define LIGHT_BLUE   "\033[0;38;5;39m"
#define LIGHT_BLUE_UNDERLINE   "\033[0;4;38;5;39m"
#define DARK_BLUE    "\033[0;1;34m"
#define ORANGE       "\033[0;38;5;166m"
#define ORANGE_UNDERLINE "\033[0;4;38;5;166m"
#define DARK_ORANGE  "\033[0;38;5;130m"
#define DARK_ORANGE_UNDERLINE  "\033[0;4;38;5;130m"
#define MAGENTA      "\033[0;38;5;13m"
#define CYAN         "\033[0;1;36m"
#define DARK_CYAN    "\033[0;38;5;21m"
#define DARK_RED     "\033[0;31m"
#define RED          "\033[0;31m"
#define RED_UNDERLINE  "\033[0;4;31m"
#define UNDERLINE    "\033[0;1;4m"

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

void xmqSetupDefaultColors(XMQOutputSettings *output_settings, bool dark_mode)
{
    XMQColoring *c = &output_settings->coloring;
    memset(c, 0, sizeof(XMQColoring));
    c->indentation_space = " ";
    c->explicit_space = " ";
    c->explicit_nl = "\n";
    c->explicit_tab = "\t";
    c->explicit_cr = "\r";

    if (output_settings->render_to == XMQ_RENDER_PLAIN)
    {
    }
    else
    if (output_settings->render_to == XMQ_RENDER_TERMINAL)
    {
        setup_terminal_coloring(c, dark_mode, output_settings->use_color, output_settings->render_raw);
    }
    else if (output_settings->render_to == XMQ_RENDER_HTML)
    {
        setup_html_coloring(c, dark_mode, output_settings->use_color, output_settings->render_raw);
    }
    else if (output_settings->render_to == XMQ_RENDER_TEX)
    {
        setup_tex_coloring(c, dark_mode, output_settings->use_color, output_settings->render_raw);
    }

    if (output_settings->only_style)
    {
        printf("%s\n", c->style.pre);
        exit(0);
    }

}

void setup_terminal_coloring(XMQColoring *c, bool dark_mode, bool use_color, bool render_raw)
{
    if (!use_color) return;
    if (dark_mode)
    {
        c->whitespace.pre  = NOCOLOR;
        c->unicode_whitespace.pre  = RED_UNDERLINE;
        c->equals.pre      = NOCOLOR;
        c->brace_left.pre  = NOCOLOR;
        c->brace_right.pre = NOCOLOR;
        c->apar_left.pre    = NOCOLOR;
        c->apar_right.pre   = NOCOLOR;
        c->cpar_left.pre    = MAGENTA;
        c->cpar_right.pre   = MAGENTA;
        c->quote.pre = GREEN;
        c->entity.pre = MAGENTA;
        c->comment.pre = CYAN;
        c->comment_continuation.pre = CYAN;
        c->element_ns.pre = ORANGE_UNDERLINE;
        c->element_name.pre = ORANGE;
        c->element_key.pre = LIGHT_BLUE;
        c->element_value_text.pre = GREEN;
        c->element_value_quote.pre = GREEN;
        c->element_value_entity.pre = MAGENTA;
        c->element_value_compound_quote.pre = GREEN;
        c->element_value_compound_entity.pre = MAGENTA;
        c->attr_ns.pre = LIGHT_BLUE_UNDERLINE;
        c->attr_key.pre = LIGHT_BLUE;
        c->attr_value_text.pre = BLUE;
        c->attr_value_quote.pre = BLUE;
        c->attr_value_entity.pre = MAGENTA;
        c->attr_value_compound_quote.pre = BLUE;
        c->attr_value_compound_entity.pre = MAGENTA;
        c->ns_colon.pre = NOCOLOR;
    }
    else
    {
        c->whitespace.pre  = NOCOLOR;
        c->unicode_whitespace.pre  = RED_UNDERLINE;
        c->equals.pre      = NOCOLOR;
        c->brace_left.pre  = NOCOLOR;
        c->brace_right.pre = NOCOLOR;
        c->apar_left.pre    = NOCOLOR;
        c->apar_right.pre   = NOCOLOR;
        c->cpar_left.pre = MAGENTA;
        c->cpar_right.pre = MAGENTA;
        c->quote.pre = DARK_GREEN;
        c->entity.pre = MAGENTA;
        c->comment.pre = CYAN;
        c->comment_continuation.pre = CYAN;
        c->element_ns.pre = DARK_ORANGE_UNDERLINE;
        c->element_name.pre = DARK_ORANGE;
        c->element_key.pre = BLUE;
        c->element_value_text.pre = DARK_GREEN;
        c->element_value_quote.pre = DARK_GREEN;
        c->element_value_entity.pre = MAGENTA;
        c->element_value_compound_quote.pre = DARK_GREEN;
        c->element_value_compound_entity.pre = MAGENTA;
        c->attr_ns.pre = BLUE_UNDERLINE;
        c->attr_key.pre = BLUE;
        c->attr_value_text.pre = DARK_BLUE;
        c->attr_value_quote.pre = DARK_BLUE;
        c->attr_value_entity.pre = MAGENTA;
        c->attr_value_compound_quote.pre = DARK_BLUE;
        c->attr_value_compound_entity.pre = MAGENTA;
        c->ns_colon.pre = NOCOLOR;
    }
}

void setup_html_coloring(XMQColoring *c, bool dark_mode, bool use_color, bool render_raw)
{
    c->indentation_space = " ";
    c->explicit_nl = "\n";
    if (!render_raw)
    {
        c->document.pre =
            "<!DOCTYPE html><html>";
        c->document.post =
            "</html>";
        c->header.pre =
            "<head><style>";
        c->header.post =
            "</style></head>";
        c->style.pre =
            "pre.xmq_dark{font-size:16px;border-radius:1em;background-color:#263338;display:inline-block;padding:1em;color:white;}\n"
            "pre.xmq_light{font-size:16px;border-radius:1em;border:solid 2px #263338;display:inline-block;padding:1em;color:black;}\n"
            "xmq_c{color:#536f78;}\n"
            "xmq_q{color:darkgreen;}\n"
            "xmq_e{color:magenta;}\n"
            "xmq_ens{text-decoration:underline; color:darkorange;}\n"
            "xmq_en{color:darkorange;}\n"
            "xmq_ek{color:#88b4f7;}\n"
            "xmq_ekv{color:darkgreen;}\n"
            "pre.xmq_dark { xmq_ekv{color:lightgreen;}}\n"
            "xmq_ans{text-decoration:underline;color:#88b4f7;}\n"
            "xmq_ak{color:#88b4f7;}\n"
            "xmq_akv{color:#3166cc;}\n"
            "xmq_cp{color:magenta;}";

        c->body.pre =
            "<body>";
        c->body.post =
            "</body>";
    }

    c->content.pre = "<pre>";
    c->content.post = "</pre>";

    if (dark_mode)
    {
        c->content.pre = "<pre class=\"xmq xmq_dark\">";
    }
    else
    {
        c->content.pre = "<pre class=\"xmq xmq_light\">";
    }

    c->whitespace.pre  = NULL;
    c->indentation_whitespace.pre = NULL;
    c->unicode_whitespace.pre  = "<xmq_uw>";
    c->unicode_whitespace.post  = "</xmq_uw>";
    c->equals.pre      = NULL;
    c->brace_left.pre  = NULL;
    c->brace_right.pre = NULL;
    c->apar_left.pre    = NULL;
    c->apar_right.pre   = NULL;
    c->cpar_left.pre = "<xmq_cp>";
    c->cpar_left.post = "</xmq_cp>";
    c->cpar_right.pre = "<xmq_cp>";
    c->cpar_right.post = "</xmq_cp>";
    c->quote.pre = "<xmq_q>";
    c->quote.post = "</xmq_q>";
    c->entity.pre = "<xmq_e>";
    c->entity.post = "</xmq_e>";
    c->comment.pre = "<xmq_c>";
    c->comment.post = "</xmq_c>";
    c->comment_continuation.pre = "<xmq_c>";
    c->comment_continuation.post = "</xmq_c>";
    c->element_ns.pre = "<xmq_ens>";
    c->element_ns.post = "</xmq_ens>";
    c->element_name.pre = "<xmq_en>";
    c->element_name.post = "</xmq_en>";
    c->element_key.pre = "<xmq_ek>";
    c->element_key.post = "</xmq_ek>";
    c->element_value_text.pre = "<xmq_ekv>";
    c->element_value_text.post = "</xmq_ekv>";
    c->element_value_quote.pre = "<xmq_ekv>";
    c->element_value_quote.post = "</xmq_ekv>";
    c->element_value_entity.pre = "<xmq_e>";
    c->element_value_entity.post = "</xmq_e>";
    c->element_value_compound_quote.pre = "<xmq_kv>";
    c->element_value_compound_quote.post = "</xmq_kv>";
    c->element_value_compound_entity.pre = "<xmq_e>";
    c->element_value_compound_entity.post = "</xmq_e>";
    c->attr_ns.pre = "<xmq_ans>";
    c->attr_ns.post = "</xmq_ans>";
    c->attr_key.pre = "<xmq_ak>";
    c->attr_key.post = "</xmq_ak>";
    c->attr_value_text.pre = "<xmq_akv>";
    c->attr_value_text.post = "</xmq_kav>";
    c->attr_value_quote.pre = "<xmq_akv>";
    c->attr_value_quote.post = "</xmq_akv>";
    c->attr_value_entity.pre = "<xmq_e>";
    c->attr_value_entity.post = "</xmq_e>";
    c->attr_value_compound_quote.pre = "<xmq_akv>";
    c->attr_value_compound_quote.post = "</xmq_akv>";
    c->attr_value_compound_entity.pre = "<xmq_e>";
    c->attr_value_compound_entity.post = "</xmq_e>";
    c->ns_colon.pre = NULL;
}

void setup_htmq_coloring(XMQColoring *c, bool dark_mode, bool use_color, bool render_raw)
{
}

void setup_tex_coloring(XMQColoring *c, bool dark_mode, bool use_color, bool render_raw)
{
    c->indentation_space = "\\xmqI ";
    c->explicit_space = " ";
    c->explicit_nl = "\\linebreak\n";

    if (!render_raw)
    {
        c->document.pre =
            "\\documentclass[10pt,a4paper]{article}\n"
            "\\usepackage{color}\n";

        c->style.pre =
            "\\definecolor{Brown}{rgb}{0.86,0.38,0.0}\n"
            "\\definecolor{Blue}{rgb}{0.0,0.37,1.0}\n"
            "\\definecolor{DarkSlateBlue}{rgb}{0.28,0.24,0.55}\n"
            "\\definecolor{Green}{rgb}{0.0,0.46,0.0}\n"
            "\\definecolor{Red}{rgb}{0.77,0.13,0.09}\n"
            "\\definecolor{LightBlue}{rgb}{0.40,0.68,0.89}\n"
            "\\definecolor{MediumBlue}{rgb}{0.21,0.51,0.84}\n"
            "\\definecolor{LightGreen}{rgb}{0.54,0.77,0.43}\n"
            "\\definecolor{Grey}{rgb}{0.5,0.5,0.5}\n"
            "\\definecolor{Purple}{rgb}{0.69,0.02,0.97}\n"
            "\\definecolor{Yellow}{rgb}{0.5,0.5,0.1}\n"
            "\\definecolor{Cyan}{rgb}{0.3,0.7,0.7}\n"
            "\\newcommand{\\xmq_c}[1]{{\\color{Cyan}#1}}\n"
            "\\newcommand{\\xmq_q}[1]{{\\color{Green}#1}}\n"
            "\\newcommand{\\xmq_e}[1]{{\\color{Purple}#1}}\n"
            "\\newcommand{\\xmq_ens}[1]{{\\color{Blue}#1}}\n"
            "\\newcommand{\\xmq_en}[1]{{\\color{Blue}#1}}\n"
            "\\newcommand{\\xmq_ek}[1]{{\\color{Blue}#1}}\n"
            "\\newcommand{\\xmq_ekv}[1]{{\\color{Green}#1}}\n"
            "\\newcommand{\\xmq_ans}[1]{{\\color{Blue}#1}}\n"
            "\\newcommand{\\xmq_ak}[1]{{\\color{Blue}#1}}\n"
            "\\newcommand{\\xmq_akv}[1]{{\\color{Blue}#1}}\n"
            "\\newcommand{\\xmq_cp}[1]{{\\color{Purple}#1}}\n"
            "\\newcommand{\\xmqI}[0]{{\\mbox{\\ }}}\n";

        c->body.pre = "\n\\begin{document}\n";
        c->body.post = "\n\\end{document}\n";
    }

    c->content.pre = "\\texttt{\\flushleft\\noindent ";
    c->content.post = "\n}\n";
    c->whitespace.pre  = NULL;
    c->indentation_whitespace.pre = NULL;
    c->unicode_whitespace.pre  = "\\xmq_uw{";
    c->unicode_whitespace.post  = "}";
    c->equals.pre      = NULL;
    c->brace_left.pre  = NULL;
    c->brace_right.pre = NULL;
    c->apar_left.pre    = NULL;
    c->apar_right.pre   = NULL;
    c->cpar_left.pre = "\\xmq_cp{";
    c->cpar_left.post = "}";
    c->cpar_right.pre = "\\xmq_cp{";
    c->cpar_right.post = "}";
    c->quote.pre = "\\xmq_q{";
    c->quote.post = "}";
    c->entity.pre = "\\xmq_e{";
    c->entity.post = "}";
    c->comment.pre = "\\xmq_c{";
    c->comment.post = "}";
    c->comment_continuation.pre = "\\xmq_c{";
    c->comment_continuation.post = "}";
    c->element_ns.pre = "\\xmq_ens{";
    c->element_ns.post = "}";
    c->element_name.pre = "\\xmq_en{";
    c->element_name.post = "}";
    c->element_key.pre = "\\xmq_ek{";
    c->element_key.post = "}";
    c->element_value_text.pre = "\\xmq_ekv{";
    c->element_value_text.post = "}";
    c->element_value_quote.pre = "\\xmq_ekv{";
    c->element_value_quote.post = "}";
    c->element_value_entity.pre = "\\xmq_e{";
    c->element_value_entity.post = "}";
    c->element_value_compound_quote.pre = "\\xmq_ekv{";
    c->element_value_compound_quote.post = "}";
    c->element_value_compound_entity.pre = "\\xmq_e{";
    c->element_value_compound_entity.post = "}";
    c->attr_ns.pre = "\\xmq_ans{";
    c->attr_ns.post = "}";
    c->attr_key.pre = "\\xmq_ak{";
    c->attr_key.post = "}";
    c->attr_value_text.pre = "\\xmq_akv{";
    c->attr_value_text.post = "}";
    c->attr_value_quote.pre = "\\xmq_akv{";
    c->attr_value_quote.post = "}";
    c->attr_value_entity.pre = "\\xmq_e{";
    c->attr_value_entity.post = "}";
    c->attr_value_compound_quote.pre = "\\xmq_akv{";
    c->attr_value_compound_quote.post = "}";
    c->attr_value_compound_entity.pre = "\\xmq_e{";
    c->attr_value_compound_entity.post = "}";
    c->ns_colon.pre = NULL;
}

int xmqStateErrno(XMQParseState *state)
{
    return (int)state->error_nr;
}

void get_color(XMQColoring *coloring, XMQColor c, const char **pre, const char **post)
{
    switch(c)
    {
#define X(TYPE) case COLOR_##TYPE: *pre = coloring->TYPE.pre; *post = coloring->TYPE.post; return;
LIST_OF_XMQ_TOKENS
#undef X
    case COLOR_unicode_whitespace: *pre = coloring->unicode_whitespace.pre; *post = coloring->unicode_whitespace.post; return;
    case COLOR_indentation_whitespace: *pre = coloring->indentation_whitespace.pre; *post = coloring->indentation_whitespace.post; return;
    default:
        *pre = NULL;
        *post = NULL;
        return;
    }
    assert(false);
    *pre = "";
    *post = "";
}

#define X(TYPE) \
    void colorize_##TYPE(XMQParseState*state, size_t line, size_t col,const char *start, size_t indent,const char *cstart, const char *cstop, const char *stop) { \
        if (!state->simulated) { \
            const char *pre, *post;  \
            get_color(&state->output_settings->coloring, COLOR_##TYPE, &pre, &post); \
            if (pre) state->output_settings->content.write(state->output_settings->content.writer_state, pre, NULL); \
            state->output_settings->content.write(state->output_settings->content.writer_state, start, stop); \
            if (post) state->output_settings->content.write(state->output_settings->content.writer_state, post, NULL); \
        } \
    }
LIST_OF_XMQ_TOKENS
#undef X


const char *xmqStateErrorMsg(XMQParseState *state)
{
    return state->generated_error_msg;
}

void reset_ansi(XMQParseState *state)
{
    state->output_settings->content.write(state->output_settings->content.writer_state, ansi_reset_color, NULL);
}

void reset_ansi_nl(XMQParseState *state)
{
    state->output_settings->content.write(state->output_settings->content.writer_state, ansi_reset_color, NULL);
    state->output_settings->content.write(state->output_settings->content.writer_state, "\n", NULL);
}

void add_nl(XMQParseState *state)
{
    state->output_settings->content.write(state->output_settings->content.writer_state, "\n", NULL);
}

XMQOutputSettings *xmqNewOutputSettings()
{
    XMQOutputSettings *s = (XMQOutputSettings*)malloc(sizeof(XMQOutputSettings));
    memset(s, 0, sizeof(XMQOutputSettings));
    s->coloring.indentation_space = " ";
    s->coloring.explicit_space = " ";
    s->coloring.explicit_nl = "\n";
    s->coloring.explicit_tab = "\t";
    s->coloring.explicit_cr = "\r";
    s->add_indent = 4;
    s->use_color = false;
    return s;
}

void xmqFreeOutputSettings(XMQOutputSettings *s)
{
    free(s);
}

bool write_print_stdout(void *writer_state_ignored, const char *start, const char *stop)
{
    if (!start) return true;
    if (!stop)
    {
        fputs(start, stdout);
    }
    else
    {
        assert(stop > start);
        fwrite(start, stop-start, 1, stdout);
    }
    return true;
}

bool write_print_stderr(void *writer_state_ignored, const char *start, const char *stop)
{
    if (!start) return true;
    if (!stop)
    {
        fputs(start, stderr);
    }
    else
    {
        fwrite(start, stop-start, 1, stderr);
    }
    return true;
}

void xmqSetupPrintStdOutStdErr(XMQOutputSettings *ps)
{
    ps->content.writer_state = NULL; // Not needed
    ps->content.write = write_print_stdout;
    ps->error.writer_state = NULL; // Not needed
    ps->error.write = write_print_stderr;
}

XMQParseCallbacks *xmqNewParseCallbacks()
{
    XMQParseCallbacks *callbacks = (XMQParseCallbacks*)malloc(sizeof(XMQParseCallbacks));
    memset(callbacks, 0, sizeof(sizeof(XMQParseCallbacks)));
    return callbacks;
}

XMQParseState *xmqNewParseState(XMQParseCallbacks *callbacks, XMQOutputSettings *output_settings)
{
    if (!callbacks)
    {
        PRINT_ERROR("xmqNewParseState is given a NULL callback structure!\n");
        assert(0);
        exit(1);
    }
    if (!output_settings)
    {
        PRINT_ERROR("xmqNewParseState is given a NULL print output_settings structure!\n");
        assert(0);
        exit(1);
    }
    if (callbacks->magic_cookie != MAGIC_COOKIE)
    {
        PRINT_ERROR("xmqNewParseState is given a callback structure which is not initialized!\n");
        assert(0);
        exit(1);
    }
    XMQParseState *state = (XMQParseState*)malloc(sizeof(XMQParseState));
    memset(state, 0, sizeof(XMQParseState));
    state->parse = callbacks;
    state->output_settings = output_settings;
    state->magic_cookie = MAGIC_COOKIE;
    state->element_stack = new_stack();

    return state;
}

void build_state_error_message(XMQParseState *state, const char *start, const char *stop)
{
    // Error detected during parsing and this is where the longjmp will end up!
    state->generated_error_msg = (char*)malloc(2048);

    XMQParseError error_nr = (XMQParseError)state->error_nr;
    const char *error = xmqParseErrorToString(error_nr);

    const char *statei = state->i;
    size_t line = state->line;
    size_t col = state->col;

    if (error_nr == XMQ_ERROR_BODY_NOT_CLOSED)
    {
        statei = state->last_body_start;
        line = state->last_body_start_line;
        col = state->last_body_start_col;
    }
    if (error_nr == XMQ_ERROR_ATTRIBUTES_NOT_CLOSED)
    {
        statei = state->last_attr_start;
        line = state->last_attr_start_line;
        col = state->last_attr_start_col;
    }
    if (error_nr == XMQ_ERROR_QUOTE_NOT_CLOSED)
    {
        statei = state->last_quote_start;
        line = state->last_quote_start_line;
        col = state->last_quote_start_col;
    }
    if (error_nr == XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS)
    {
        statei = state->last_equals_start;
        line = state->last_equals_start_line;
        col = state->last_equals_start_col;
    }

    size_t n = 0;
    size_t offset = 0;
    const char *line_start = statei;
    while (line_start > start && *(line_start-1) != '\n' && n < 1024)
    {
        n++;
        offset++;
        line_start--;
    }

    const char *i = statei;
    while (i < stop && *i && *i != '\n' && n < 1024)
    {
        n++;
        i++;
    }
    const char *char_error = "";
    char buf[32];

    if (error_nr == XMQ_ERROR_INVALID_CHAR)
    {
        UTF8Char utf8_char;
        peek_utf8_char(statei, stop, &utf8_char);
        char utf8_codepoint[8];
        utf8_char_to_codepoint_string(&utf8_char, utf8_codepoint);

        snprintf(buf, 32, " \"%s\" %s", utf8_char.bytes, utf8_codepoint);
        char_error = buf;
    }

    char line_error[1024];
    line_error[0] = 0;
    if (statei < stop)
    {
        snprintf(line_error, 1024, "\n%.*s\n %*s",
                 (int)n,
                 line_start,
                 (int)offset,
                 "^");
    }

    snprintf(state->generated_error_msg, 2048,
             "%s:%zu:%zu: error: %s%s%s",
             state->source_name,
             line, col,
             error,
             char_error,
             line_error
        );
    state->generated_error_msg[2047] = 0;
}

bool xmqTokenizeBuffer(XMQParseState *state, const char *start, const char *stop)
{
    if (state->magic_cookie != MAGIC_COOKIE)
    {
        PRINT_ERROR("Parser state not initialized!\n");
        assert(0);
        exit(1);
    }

    XMQContentType detected_ct = xmqDetectContentType(start, stop);
    if (detected_ct != XMQ_CONTENT_XMQ)
    {
        state->generated_error_msg = strdup("You can only tokenize xmq!");
        state->error_nr = XMQ_ERROR_NOT_XMQ;
        return false;
    }

    state->buffer_start = start;
    state->buffer_stop = stop;
    state->i = start;
    state->line = 1;
    state->col = 1;
    state->error_nr = 0;

    if (state->parse->init) state->parse->init(state);

    if (!setjmp(state->error_handler))
    {
        // Start parsing!
        parse_xmq(state);
        if (state->i < state->buffer_stop)
        {
            state->error_nr = XMQ_ERROR_UNEXPECTED_CLOSING_BRACE;
            longjmp(state->error_handler, 1);
        }
    }
    else
    {
        build_state_error_message(state, start, stop);
        return false;
    }

    if (state->parse->done) state->parse->done(state);
    return true;
}

bool xmqTokenizeFile(XMQParseState *state, const char *file)
{
    bool rc = false;
    char *buffer = NULL;
    long fsize = 0;
    size_t n = 0;
    XMQContentType content = XMQ_CONTENT_XMQ;

    FILE *f = fopen(file, "rb");
    if (!f) {
        state->error_nr = XMQ_ERROR_CANNOT_READ_FILE;
        goto exit;
    }

    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = (char*)malloc(fsize + 1);
    if (!buffer) return false;

    n = fread(buffer, fsize, 1, f);

    if (n != 1) {
        rc = false;
        state->error_nr = XMQ_ERROR_CANNOT_READ_FILE;
        goto exit;
    }
    fclose(f);
    buffer[fsize] = 0;

    xmqSetStateSourceName(state, file);

    content = xmqDetectContentType(buffer, buffer+fsize);
    if (content != XMQ_CONTENT_XMQ)
    {
        state->generated_error_msg = strdup("You can only tokenize xmq!");
        state->error_nr = XMQ_ERROR_NOT_XMQ;
        rc = false;
        goto exit;
    }

    rc = xmqTokenizeBuffer(state, buffer, buffer+fsize);

    exit:

    free(buffer);

    return rc;
}

/** This function is used only for detecting the kind of content: xmq, xml, html, json. */
const char *find_word_ignore_case(const char *start, const char *stop, const char *word)
{
    const char *i = start;
    size_t len = strlen(word);

    while (i < stop && is_xml_whitespace(*i)) i++;
    if (!strncasecmp(i, word, len))
    {
        const char *next = i+len;
        if (next <= stop && (is_xml_whitespace(*next) || *next == 0 || !isalnum(*next)))
        {
            // The word was properly terminated with a 0, or a whitespace or something not alpha numeric.
            return i+strlen(word);
        }
    }
    return NULL;
}

XMQContentType xmqDetectContentType(const char *start, const char *stop)
{
    const char *i = start;

    while (i < stop)
    {
        char c = *i;
        if (!is_xml_whitespace(c))
        {
            if (c == '<')
            {
                // Starts with <html or < html
                const char *is_html = find_word_ignore_case(i+1, stop, "html");
                if (is_html) return XMQ_CONTENT_HTML;

                // Starts with <!doctype  html
                const char *is_doctype = find_word_ignore_case(i, stop, "<!doctype");
                if (is_doctype)
                {
                    i = is_doctype;
                    is_html = find_word_ignore_case(is_doctype+1, stop, "html");
                    if (is_html) return XMQ_CONTENT_HTML;
                }
                // Otherwise we assume it is xml. If you are working with html content inside
                // the html, then use --informat=html
                return XMQ_CONTENT_XML; // Or HTML...
            }
            if (c == '{' || c == '"' || c == '[' || (c >= '0' && c <= '9')) return XMQ_CONTENT_JSON;
            // The strings true, false and null are valid json, but are also valid xmq.
            // We default to xmq here.
            return XMQ_CONTENT_XMQ;
        }
        i++;
    }

    return XMQ_CONTENT_XMQ;
}

void increment(char c, size_t num_bytes, const char **i, size_t *line, size_t *col)
{
    if ((c & 0xc0) != 0x80) // Just ignore UTF8 parts since they do not change the line or col.
    {
        (*col)++;
        if (c == '\n')
        {
            (*line)++;
            (*col) = 1;
        }
    }
    assert(num_bytes > 0);
    (*i)+=num_bytes;
}

bool is_lowercase_hex(char c)
{
    return
        (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f');
}

bool is_hex(char c)
{
    return
        (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F');
}

size_t num_utf8_bytes(char c)
{
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xe0) == 0xc0) return 2;
    if ((c & 0xf0) == 0xe0) return 3;
    if ((c & 0xf8) == 0xf0) return 4;
    return 0; // Error
}

size_t peek_utf8_char(const char *start, const char *stop, UTF8Char *uc)
{
    char a = *start;
    size_t n = num_utf8_bytes(a);

    if (n == 1)
    {
        uc->bytes[0] = a;
        uc->bytes[1] = 0;
        uc->bytes[2] = 0;
        uc->bytes[3] = 0;
        return 1;
    }

    char b = *(start+1);
    if (n == 2)
    {
        uc->bytes[0] = a;
        uc->bytes[1] = b;
        uc->bytes[2] = 0;
        uc->bytes[3] = 0;
        return 2;
    }

    char c = *(start+2);
    if (n == 3)
    {
        uc->bytes[0] = a;
        uc->bytes[1] = b;
        uc->bytes[2] = c;
        uc->bytes[3] = 0;
        return 3;
    }

    char d = *(start+3);
    if (n == 4)
    {
        uc->bytes[0] = a;
        uc->bytes[1] = b;
        uc->bytes[2] = c;
        uc->bytes[3] = d;
        return 4;
    }

    return 0;
}

bool utf8_char_to_codepoint_string(UTF8Char *uc, char *buf)
{
    int cp = 0;
    size_t len = 0;
    bool ok = decode_utf8(uc->bytes, uc->bytes+4, &cp, &len);
    if (!ok)
    {
        snprintf(buf, 16, "U+error");
        return false;
    }
    snprintf(buf, 16, "U+%X", cp);
    return true;
}

bool is_xml_whitespace(char c)
{
    if (c == ' ' || c == '\n' || c == '\t' || c == '\r')
    {
        return true;
    }
    return false;
}

bool is_all_xml_whitespace(const char *s)
{
    if (!s) return false;

    for (const char *i = s; *i; ++i)
    {
        if (!is_xml_whitespace(*i)) return false;
    }
    return true;
}

size_t count_whitespace(const char *i, const char *stop)
{
    unsigned char c = *i;
    if (c == ' ' || c == '\n' || c == '\t' || c == '\r')
    {
        return 1;
    }

    if (i+1 >= stop) return 0;

    unsigned char cc = *(i+1);

    if (c == 0xC2 && cc == 0xA0)
    {
        // Unbreakable space. 0xA0
        return 2;
    }
    if (c == 0xE2 && cc == 0x80)
    {
        if (i+2 >= stop) return 0;

        unsigned char ccc = *(i+2);

        if (ccc == 0x80)
        {
            // EN quad. &#8192; U+2000 utf8 E2 80 80
            return 3;
        }
        if (ccc == 0x81)
        {
            // EM quad. &#8193; U+2001 utf8 E2 80 81
            return 3;
        }
        if (ccc == 0x82)
        {
            // EN space. &#8194; U+2002 utf8 E2 80 82
            return 3;
        }
        if (ccc == 0x83)
        {
            // EM space. &#8195; U+2003 utf8 E2 80 83
            return 3;
        }
    }
    return 0;
}

bool is_unicode_whitespace(const char *start, const char *stop)
{
    size_t n = count_whitespace(start, stop);

    // Single char whitespace is ' ' '\t' '\n' '\r'
    // First unicode whitespace is 160 nbsp require two or more chars.
    return n > 1;
}

const char *has_leading_nl_whitespace(const char *start, const char *stop)
{
    const char *i = start;
    bool found_nl = false;

    while (i < stop)
    {
        if (*i == '\n') found_nl = true;
        if (!is_xml_whitespace(*i)) break;
        i++;
    }
    // No newline found before content, so leading spaces/tabs will not be trimmed.
    if (!found_nl) return 0;
    return i;
}

const char *has_ending_nl_whitespace(const char *start, const char *stop)
{
    const char *i = stop;
    bool found_nl = false;

    while (i > start)
    {
        i--;
        if (*i == '\n') found_nl = true;
        if (!is_xml_whitespace(*i)) break;
    }
    // No newline found after content, so ending spaces/tabs will not be trimmed.
    if (!found_nl) return 0;
    i++;
    return i;
}

bool is_xmq_quote_start(char c)
{
    return c == '\'';
}

bool is_xmq_entity_start(char c)
{
    return c == '&';
}

bool is_xmq_attribute_key_start(char c)
{
    bool t =
        c == '\'' ||
        c == '"' ||
        c == '(' ||
        c == ')' ||
        c == '{' ||
        c == '}' ||
        c == '/' ||
        c == '=' ||
        c == '&';

    return !t;
}

bool is_xmq_compound_start(char c)
{
    return (c == '(');
}

bool is_xmq_comment_start(char c, char cc)
{
    return c == '/' && (cc == '/' || cc == '*');
}

bool is_xmq_doctype_start(const char *start, const char *stop)
{
    if (*start != '!') return false;
    // !DOCTYPE
    if (start+8 > stop) return false;
    if (strncmp(start, "!DOCTYPE", 8)) return false;
    // Ooups, !DOCTYPE must have some value.
    if (start+8 == stop) return false;
    char c = *(start+8);
    // !DOCTYPE= or !DOCTYPE = etc
    if (c != '=' && c != ' ' && c != '\t' && c != '\n' && c != '\r') return false;
    return true;
}

size_t count_xmq_slashes(const char *i, const char *stop, bool *found_asterisk)
{
    const char *start = i;

    while (i < stop && *i == '/') i++;

    if (*i == '*') *found_asterisk = true;
    else *found_asterisk = false;

    return i-start;
}

bool is_xmq_text_value_char(const char *i, const char *stop)
{
    char c = *i;
    if (count_whitespace(i, stop) > 0 ||
        c == '\'' ||
        c == '"' ||
        c == '(' ||
        c == ')' ||
        c == '{' ||
        c == '}')
    {
        return false;
    }
    return true;
}

bool is_xmq_text_value(const char *start, const char *stop)
{
    for (const char *i = start; i < stop; ++i)
    {
        if (!is_xmq_text_value_char(i, stop))
        {
            return false;
        }
    }
    return true;
}

bool is_xmq_text_name(char c)
{
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    if (c == '-' || c == '_' || c == '.' || c == ':' || c == '#') return true;
    return false;
}

bool is_xmq_element_start(char c)
{
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c == '_') return true;
    return false;
}

bool peek_xmq_next_is_equal(XMQParseState *state)
{
    const char *i = state->i;
    const char *stop = state->buffer_stop;
    char c = 0;

    while (i < stop)
    {
        c = *i;
        if (!is_xml_whitespace(c)) break;
        i++;
    }
    return c == '=';
}

void eat_whitespace(XMQParseState *state, const char **start, const char **stop)
{
    const char *i = state->i;
    const char *buffer_stop = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;
    *start = i;

    while (i < buffer_stop)
    {
        size_t nw = count_whitespace(i, *stop);
        if (!nw) break;
        // Pass the first char, needed to detect '\n' which increments line and set cols to 1.
        increment(*i, nw, &i, &line, &col);
    }

    *stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
}

size_t count_xmq_quotes(const char *i, const char *stop)
{
    const char *start = i;

    while (i < stop && *i == '\'') i++;

    return i-start;
}

/** Scan a line, ie until \n or NULL.
    Return true if a newline was found. */
bool find_line(const char *start, // Start scanning the line from here.
               const char *stop,  // Points to char after end of buffer.
               size_t *indent,  // Store indentation in number of spaces.
               const char **after_last_non_space, // Points to char after last non-space char on line, start if whole line is spaces.
               const char **eol)    // Points to char after '\n' or to stop.
{
    assert(start <= stop);

    bool has_nl = false;
    size_t ndnt = 0;
    const char *lnws = start;
    const char *i = start;

    // Skip spaces as indententation.
    while (i < stop && (*i == ' ' || *i == '\t'))
    {
        if (*i == ' ') ndnt++;
        else ndnt += 8; // Count tab as 8 chars.
        i++;
    }
    *indent = ndnt;

    // Find eol \n and the last non-space char.
    while (i < stop)
    {
        if (*i == '\n')
        {
            i++;
            has_nl = true;
            break;
        }
        if (*i != ' ' && *i != '\t') lnws = i+1;
        i++;
    }

    *after_last_non_space = lnws;
    *eol = i;

    return has_nl;
}

static bool debug_enabled_ = false;
static bool verbose_enabled_ = false;

void xmqSetDebug(bool e)
{
    debug_enabled_ = e;
}

bool xmqDebugging()
{
    return debug_enabled_;
}

void xmqSetVerbose(bool e)
{
    verbose_enabled_ = e;
}

bool xmqVerbose() {
    return verbose_enabled_;
}

static const char *build_error_message(const char* fmt, ...)
{
    char *buf = (char*)malloc(1024);
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1024, fmt, args);
    va_end(args);
    buf[1023] = 0;
    buf = (char*)realloc(buf, strlen(buf)+1);
    return buf;
}

static void debug(const char* fmt, ...)
{
    if (debug_enabled_) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

static void verbose(const char* fmt, ...)
{
    if (verbose_enabled_) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

/** Check if the quote begins with a nl or spaces nl.
    For example just newline:
'
abc'
    Or spaces newline, but this cannot be shown here since
    it is invisible and often trimmed by the source editor.
*/
bool begins_with_spaces_or_tabs_then_nl(const char *start, const char *stop)
{
    const char *i = start;
    while (i < stop-1 && (*i == ' ' || *i == '\t')) i++;
    return *i == '\n';
}

/** Check if the quote ends with a nl or nl spaces.
    For example just newline:
'abc
'
    Or newline spaces:
'abc
   '
*/
bool ends_with_nl_then_sp_tb_cr(const char *start, const char *stop)
{
    const char *i = stop-1;
    while (i > start && (*i == ' ' || *i == '\t' || *i == '\r')) i--;
    return *i == '\n';
}

/**
    xmq_un_quote:
    @indent: The number of chars before the quote starts on the first line.
    @space: Use this space to prepend the first line if needed due to indent.
    @start: Start of buffer to unquote.
    @stop: Points to byte after buffer.

    Do the reverse of xmq_quote, take a quote (with or without the surrounding single quotes)
    and removes any incindental indentation. Returns a newly malloced buffer
    that must be free:ed later.

    Use indent 0 if the quote ' is first on the line.
    The indent is 1 if there is a single space before the starting quote ' etc.

    As a special case, if both indent is 0 and space is 0, then the indent of the
    first line is picked from the second line.
*/
char *xmq_un_quote(size_t indent, char space, const char *start, const char *stop, bool remove_qs)
{
    if (!stop) stop = start+strlen(start);

    // Remove the surrounding quotes.
    size_t j = 0;
    if (remove_qs)
    {
        while (*(start+j) == '\'' && *(stop-j-1) == '\'' && (start+j) < (stop-j)) j++;
    }

    indent += j;
    start = start+j;
    stop = stop-j;

    return xmq_trim_quote(indent, space, start, stop);
}

/**
    xmq_un_comment:

    Do the reverse of xmq_comment, Takes a comment (including /✻ ✻/ ///✻ ✻///) and removes any incindental
    indentation and trailing spaces. Returns a newly malloced buffer
    that must be free:ed later.

    The indent is 0 if the / first on the line.
    The indent is 1 if there is a single space before the starting / etc.
*/
char *xmq_un_comment(size_t indent, char space, const char *start, const char *stop)
{
    assert(start < stop);
    assert(*start == '/');

    const char *i = start;
    while (i < stop && *i == '/') i++;

    if (i == stop)
    {
        // Single line all slashes. Drop the two first slashes which are the single line comment.
        return xmq_trim_quote(indent, space, start+2, stop);
    }

    if (*i != '*')
    {
        // No asterisk * after the slashes. This is a single line comment.
        // If there is a space after //, skip it.
        if (*i == ' ') i++;
        // Remove trailing spaces.
        while (i < stop && *(stop-1) == ' ') stop--;
        assert(i <= stop);
        return xmq_trim_quote(indent, space, i, stop);
    }

    // There is an asterisk after the slashes. A standard /* */ comment
    // Remove the surrounding / slashes.
    size_t j = 0;
    while (*(start+j) == '/' && *(stop-j-1) == '/' && (start+j) < (stop-j)) j++;

    indent += j;
    start = start+j;
    stop = stop-j;

    // Check that the star is there.
    assert(*start == '*' && *(stop-1) == '*');
    indent++;
    start++;
    stop--;

    // Skip any space
    if (*start == ' ')
    {
        indent++;
        start++;
    }
    if (*(stop-1) == ' ')
    {
        if (stop-1 >= start)
        {
            stop--;
        }
    }

    assert(start <= stop);
    return xmq_trim_quote(indent, space, start, stop);
}

char *xmq_trim_quote(size_t indent, char space, const char *start, const char *stop)
{
    // Special case, find the next indent and use as original indent.
    if (indent == 0 && space == 0)
    {
        size_t i;
        const char *after;
        const char *eol;
        // Skip first line.
        bool found_nl = find_line(start, stop, &i, &after, &eol);
        if (found_nl && eol != stop)
        {
            // Now grab the indent from the second line.
            find_line(eol, stop, &indent, &after, &eol);
        }
    }
    // If the first line is all spaces and a newline, then
    // the first_indent should be ignored.
    bool ignore_first_indent = false;

    // For each found line, the found indent is the number of spaces before the first non space.
    size_t found_indent;

    // For each found line, this is where the line ends after trimming line ending whitespace.
    const char *after_last_non_space;

    // This is where the newline char was found.
    const char *eol;

    // Did the found line end with a newline or NULL.
    bool has_nl = false;

    // Lets examine the first line!
    has_nl = find_line(start, stop, &found_indent, &after_last_non_space, &eol);

    // We override the found indent (counting from start)
    // with the actual source indent from the beginning of the line.
    found_indent = indent;

    if (!has_nl)
    {
        // No newline was found, then do not trim! Return as is.
        size_t n = 1+stop-start;
        char *buf = (char*)malloc(n);
        memcpy(buf, start, n-1);
        buf[n-1] = 0;
        return buf;
    }

    // Check if the final line is all spaces.
    if (ends_with_nl_then_sp_tb_cr(start, stop))
    {
        // So it is, now trim from the end.
        while (stop > start)
        {
            char c = *(stop-1);
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
            stop--;
        }
    }

    if (stop == start)
    {
        // Oups! Quote was all space and newlines. I.e. it is an empty quote.
        char *buf = (char*)malloc(1);
        buf[0] = 0;
        return buf;
    }

    // Check if the first line is all spaces.
    if (begins_with_spaces_or_tabs_then_nl(start, stop))
    {
        // The first line is all spaces, trim leading spaces and newlines!
        ignore_first_indent = true;
        // Skip the already scanned first line.
        start = eol;
        const char *i = start;
        while (i < stop)
        {
            char c = *i;
            if (c == '\n') start = i+1; // Restart find lines from here.
            else if (c != ' ' && c != '\t' && c != '\r') break;
            i++;
        }
    }
    size_t incindental = (size_t)-1;

    if (!ignore_first_indent)
    {
        incindental = indent;
    }

    // Now scan remaining lines at the first line.
    const char *i = start;
    bool first_line = true;
    while (i < stop)
    {
        has_nl = find_line(i, stop, &found_indent, &after_last_non_space, &eol);

        if (after_last_non_space != i)
        {
            // There are non-space chars.
            if (found_indent < incindental)  // Their indent is lesser than the so far found.
            {
                // Yep, remember it.
                if (!first_line || ignore_first_indent)
                {
                    incindental = found_indent;
                }
                debug("FOUND incindental %zu\n", incindental);
            }
            first_line = false;
        }
        i = eol; // Start at next line, or end at stop.
    }

    size_t prepend = 0;

    if (!ignore_first_indent &&
        indent >= incindental)
    {
        // The first indent is relevant and it is bigger than the incindental.
        // We need to prepend the output line with spaces that are not in the source!
        // But, only if there is more than one line with actual non spaces!
        prepend = indent - incindental;
        debug("ADJUSTING prepend=%zu first_indent=%zu incindental=%zu\n", prepend, indent, incindental);
    }

    // Allocate max size of output buffer, it usually becomes smaller
    // when incindental indentation and trailing whitespace is removed.
    size_t n = stop-start+prepend+1;
    char *buf = (char*)malloc(n);
    char *o = buf;

    // Insert any necessary prepended spaces due to source indentation of the line.
    while (prepend) { *o++ = space; prepend--; }

    // Start scanning the lines from the beginning again.
    // Now use the found incindental to copy the right parts.
    i = start;

    first_line = true;
    while (i < stop)
    {
        bool has_nl = find_line(i, stop, &found_indent, &after_last_non_space, &eol);

        if (!first_line || ignore_first_indent)
        {
            // For all lines except the first. And for the first line if ignore_first_indent is true.
            // Skip the incindental indentation, space counts as one tab counts as 8.
            size_t n = incindental;
            while (n > 0)
            {
                char c = *i;
                i++;
                if (c == ' ') n--;
                else if (c == '\t')
                {
                    if (n >= 8) n -= 8;
                    else break; // safety check.
                }
            }
            debug("ADD INCIDENTAL %zu\n", incindental);
        }
        // Copy content, but not line ending xml whitespace ie space, tab, cr.
        while (i < after_last_non_space) { *o++ = *i++; }

        if (has_nl)
        {
            *o++ = '\n';
            debug("ADDING NL\n");
        }
        else
        {
            // The final line has no nl, here we copy any ending spaces as well!
            while (i < eol) { *o++ = *i++; }
        }
        i = eol;
        first_line = false;
    }
    *o++ = 0;
    size_t real_size = o-buf;
    buf = (char*)realloc(buf, real_size);
    return buf;
}

size_t eat_xmq_quote(XMQParseState *state, const char **content_start, const char **content_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    size_t depth = count_xmq_quotes(i, end);
    size_t return_depth = depth;
    size_t count = depth;

    state->last_quote_start = state->i;
    state->last_quote_start_line = state->line;
    state->last_quote_start_col = state->col;

    while (count > 0)
    {
        increment('\'', 1, &i, &line, &col);
        count--;
    }

    *content_start = i;

    if (depth == 2)
    {
        // The empty quote ''
        state->i = i;
        state->line = line;
        state->col = col;
        *content_stop = i;
        return 1; // Depth is one.
    }

    while (i < end)
    {
        char c = *i;
        if (c != '\'')
        {
            increment(c, 1, &i, &line, &col);
            continue;
        }
        size_t count = count_xmq_quotes(i, end);
        if (count > depth)
        {
            state->error_nr = XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES;
            longjmp(state->error_handler, 1);
        }
        else
        if (count < depth)
        {
            while (count > 0)
            {
                increment('\'', 1, &i, &line, &col);
                count--;
            }
            continue;
        }
        else
        if (count == depth)
        {
            *content_stop = i;
            while (count > 0)
            {
                increment('\'', 1, &i, &line, &col);
                count--;
            }
            depth = 0;
            break;
        }
    }
    if (depth != 0)
    {
        state->error_nr = XMQ_ERROR_QUOTE_NOT_CLOSED;
        longjmp(state->error_handler, 1);
    }
    state->i = i;
    state->line = line;
    state->col = col;

    return return_depth;
}

void eat_xmq_entity(XMQParseState *state, const char **content_start, const char **content_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    increment('&', 1, &i, &line, &col);
    *content_start = i;
    char c = 0;
    bool expect_semicolon = false;

    while (i < end)
    {
        c = *i;
        if (!is_xmq_text_name(c)) break;
        if (!is_lowercase_hex(c)) expect_semicolon = true;
        increment(c, 1, &i, &line, &col);
    }
    if (c == ';')
    {
        increment(c, 1, &i, &line, &col);
        c = *i;
        expect_semicolon = false;
    }
    if (expect_semicolon)
    {
        state->error_nr = XMQ_ERROR_ENTITY_NOT_CLOSED;
        longjmp(state->error_handler, 1);
    }
    *content_stop = i-1;
    state->i = i;
    state->line = line;
    state->col = col;
}

void eat_xmq_comment_to_eol(XMQParseState *state, const char **comment_start, const char **comment_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    increment('/', 1, &i, &line, &col);
    increment('/', 1, &i, &line, &col);

    *comment_start = i;

    char c = 0;
    while (i < end && c != '\n')
    {
        c = *i;
        increment(c, 1, &i, &line, &col);
    }
    if (c == '\n') *comment_stop = i-1;
    else *comment_stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
}

void eat_xmq_comment_to_close(XMQParseState *state, const char **comment_start, const char **comment_stop,
                              size_t num_slashes,
                              bool *found_asterisk)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;

    size_t line = state->line;
    size_t col = state->col;
    size_t n = num_slashes;

    if (*i == '/')
    {
        // Comment starts from the beginning ////* ....
        // Otherwise this is a continuation and *i == '*'
        while (n > 0)
        {
            assert(*i == '/');
            increment('/', 1, &i, &line, &col);
            n--;
        }
    }
    assert(*i == '*');
    increment('*', 1, &i, &line, &col);

    *comment_start = i;

    char c = 0;
    char cc = 0;
    n = 0;
    while (i < end)
    {
        cc = c;
        c = *i;
        if (cc != '*' || c != '/')
        {
            // Not a possible end marker */ or *///// continue eating.
            increment(c, 1, &i, &line, &col);
            continue;
        }
        // We have found */ or *//// not count the number of slashes.
        n = count_xmq_slashes(i, end, found_asterisk);

        if (n < num_slashes) continue; // Not a balanced end marker continue eating,

        if (n > num_slashes)
        {
            // Oups, too many slashes.
            state->error_nr = XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES;
            longjmp(state->error_handler, 1);
        }

        assert(n == num_slashes);
        // Found the ending slashes!
        *comment_stop = i-1;
        while (n > 0)
        {
            cc = c;
            c = *i;
            assert(*i == '/');
            increment(c, 1, &i, &line, &col);
            n--;
        }
        state->i = i;
        state->line = line;
        state->col = col;
        return;
    }
    // We reached the end of the xmq and no */ was found!
    state->error_nr = XMQ_ERROR_COMMENT_NOT_CLOSED;
    longjmp(state->error_handler, 1);
}

void eat_xmq_text_name(XMQParseState *state, const char **text_start, const char **text_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;
    *text_start = i;

    while (i < end)
    {
        char c = *i;
        if (!is_xmq_text_name(c)) break;
        increment(c, 1, &i, &line, &col);
    }

    *text_stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
}

void eat_xmq_text_value(XMQParseState *state, const char **text_start, const char **text_stop)
{
    const char *i = state->i;
    const char *stop = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;
    *text_start = i;

    while (i < stop)
    {
        char c = *i;
        if (!is_xmq_text_value_char(i, stop)) break;
        increment(c, 1, &i, &line, &col);
    }

    *text_stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
}

void eat_xmq_doctype(XMQParseState *state, const char **text_start, const char **text_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;
    *text_start = i;

    assert(*i == '!');
    increment('!', 1, &i, &line, &col);
    while (i < end)
    {
        char c = *i;
        if (!is_xmq_text_name(c)) break;
        increment(c, 1, &i, &line, &col);
    }


    *text_stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
}

bool possibly_lost_content_after_equals(XMQParseState *state)
{
    char c = *(state->i);

    // Look for unexpected = before 123 since beta was gobbled into being alfa:s value.
    // alfa = <newline>
    // beta = 123
    // Look for unexpected { since beta was gobbled into being alfa:s value.
    // alfa = <newline>
    // beta { 'more' }
    // Look for unexpected ( since beta was gobbled into being alfa:s value.
    // alfa = <newline>
    // beta(attr)

    // Not {(= then not lost content, probably.
    if (!(c == '{' || c == '(' || c == '=')) return false;

    const char *i = state->i-1;
    const char *start = state->buffer_start;

    // Scan backwards for newline accepting only texts and xml whitespace.
    while (i > start && *i != '\n' && (is_xmq_text_name(*i) || is_xml_whitespace(*i))) i--;
    if (i == start || *i != '\n') return false;

    // We found the newline lets see if the next character backwards is equals...
    while (i > start
           &&
           is_xml_whitespace(*i))
    {
        i--;
    }

    return *i == '=';
}

void parse_xmq(XMQParseState *state)
{
    const char *end = state->buffer_stop;

    while (state->i < end)
    {
        char c = *(state->i);
        char cc = 0;
        if ((c == '/' || c == '(') && state->i+1 < end) cc = *(state->i+1);

        if (is_xml_whitespace(c)) parse_xmq_whitespace(state);
        else if (is_xmq_quote_start(c)) parse_xmq_quote(state, LEVEL_XMQ);
        else if (is_xmq_entity_start(c)) parse_xmq_entity(state, LEVEL_XMQ);
        else if (is_xmq_comment_start(c, cc)) parse_xmq_comment(state, cc);
        else if (is_xmq_element_start(c)) parse_xmq_element(state);
        else if (is_xmq_doctype_start(state->i, end)) parse_xmq_doctype(state);
        else if (c == '}') return;
        else
        {
            if (possibly_lost_content_after_equals(state))
            {
                state->error_nr = XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS;
                longjmp(state->error_handler, 1);
            }

            state->error_nr = XMQ_ERROR_INVALID_CHAR;
            longjmp(state->error_handler, 1);
        }
    }
}

void parse_xmq_whitespace(XMQParseState *state)
{
    size_t start_line = state->line;
    size_t start_col = state->col;
    const char *start;
    const char *stop;
    eat_whitespace(state, &start, &stop);
    DO_CALLBACK(whitespace, state, start_line, start_col, start, start_col, start, stop, stop);
}


void parse_xmq_quote(XMQParseState *state, Level level)
{
    const char *start = state->i;
    size_t start_line = state->line;
    size_t start_col = state->col;
    const char *content_start;
    const char *content_stop;

    size_t depth = eat_xmq_quote(state, &content_start, &content_stop);
    const char *stop = state->i;
    size_t content_start_col = start_col + depth;

    switch(level)
    {
    case LEVEL_XMQ:
       DO_CALLBACK(quote, state, start_line, start_col, start, content_start_col, content_start, content_stop, stop);
       break;
    case LEVEL_ELEMENT_VALUE:
        DO_CALLBACK(element_value_quote, state, start_line, start_col, start, content_start_col, content_start, content_stop, stop);
        break;
    case LEVEL_ELEMENT_VALUE_COMPOUND:
        DO_CALLBACK(element_value_compound_quote, state, start_line, start_col, start, content_start_col, content_start, content_stop, stop);
        break;
    case LEVEL_ATTR_VALUE:
        DO_CALLBACK(attr_value_quote, state, start_line, start_col, start, content_start_col, content_start, content_stop, stop);
        break;
    case LEVEL_ATTR_VALUE_COMPOUND:
        DO_CALLBACK(attr_value_compound_quote, state, start_line, start_col, start, content_start_col, content_start, content_stop, stop);
        break;
    default:
        assert(false);
    }
}

void parse_xmq_entity(XMQParseState *state, Level level)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *content_start;
    const char *content_stop;

    eat_xmq_entity(state, &content_start, &content_stop);
    const char *stop = state->i;
    switch (level) {
    case LEVEL_XMQ:
        DO_CALLBACK(entity, state, start_line, start_col, start, start_col+1, content_start, content_stop, stop);
        break;
    case LEVEL_ELEMENT_VALUE:
        DO_CALLBACK(element_value_entity, state, start_line, start_col, start, start_col+1, content_start, content_stop, stop);
        break;
    case LEVEL_ELEMENT_VALUE_COMPOUND:
        DO_CALLBACK(element_value_compound_entity, state, start_line, start_col, start, start_col+1, content_start, content_stop, stop);
        break;
    case LEVEL_ATTR_VALUE:
        DO_CALLBACK(attr_value_entity, state, start_line, start_col, start, start_col+1, content_start, content_stop, stop);
        break;
    case LEVEL_ATTR_VALUE_COMPOUND:
        DO_CALLBACK(attr_value_compound_entity, state, start_line, start_col, start, start_col+1, content_start, content_stop, stop);
        break;
    default:
        assert(false);
    }
}

void parse_xmq_comment(XMQParseState *state, char cc)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *comment_start;
    const char *comment_stop;
    bool found_asterisk = false;

    size_t n = count_xmq_slashes(start, state->buffer_stop, &found_asterisk);

    if (!found_asterisk)
    {
        // This is a single line asterisk.
        eat_xmq_comment_to_eol(state, &comment_start, &comment_stop);
        const char *stop = state->i;
        DO_CALLBACK(comment, state, start_line, start_col, start, start_col, comment_start, comment_stop, stop);
    }
    else
    {
        // This is a /* ... */ or ////*  ... *//// comment.
        eat_xmq_comment_to_close(state, &comment_start, &comment_stop, n, &found_asterisk);
        const char *stop = state->i;
        DO_CALLBACK(comment, state, start_line, start_col, start, start_col, comment_start, comment_stop, stop);

        while (found_asterisk)
        {
            // Aha, this is a comment continuation /* ... */* ...
            start = state->i;
            start_line = state->line;
            start_col = state->col;
            eat_xmq_comment_to_close(state, &comment_start, &comment_stop, n, &found_asterisk);
            stop = state->i;
            DO_CALLBACK(comment_continuation, state, start_line, start_col, start, start_col, comment_start, comment_stop, stop);
        }
    }
}

void parse_xmq_text_value(XMQParseState *state, Level level)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *text_start;
    const char *text_stop;

    eat_xmq_text_value(state, &text_start, &text_stop);
    const char *stop = state->i;
    assert(level != LEVEL_XMQ);
    if (level == LEVEL_ATTR_VALUE)
    {
        DO_CALLBACK(attr_value_text, state, start_line, start_col, start, start_col, text_start, text_stop, stop);
    }
    else
    {
        DO_CALLBACK(element_value_text, state, start_line, start_col, start, start_col, text_start, text_stop, stop);
    }
}

void parse_xmq_value(XMQParseState *state, Level level)
{
    char c = *state->i;

    if (is_xml_whitespace(c))
    {
        parse_xmq_whitespace(state);
        c = *state->i;
    }

    if (is_xmq_quote_start(c))
    {
        parse_xmq_quote(state, level);
    }
    else if (is_xmq_entity_start(c))
    {
        parse_xmq_entity(state, level);
    }
    else if (is_xmq_compound_start(c))
    {
        parse_xmq_compound(state, level);
    }
    else
    {
        parse_xmq_text_value(state, level);
    }
}

void parse_xmq_element_internal(XMQParseState *state, bool doctype, bool pi)
{
    char c = 0;
    const char *name_start;
    const char *name_stop;

    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    if (doctype)
    {
        eat_xmq_doctype(state, &name_start, &name_stop);
    }
    else
    {
        eat_xmq_text_name(state, &name_start, &name_stop);
    }
    const char *stop = state->i;

    if (peek_xmq_next_is_equal(state))
    {
        DO_CALLBACK(element_key, state, start_line, start_col, start, start_col, name_start, name_stop, stop);
    }
    else
    {
        DO_CALLBACK(element_name, state, start_line, start_col, start, start_col, name_start, name_stop, stop);
    }

    c = *state->i;
    if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }

    if (c == '(')
    {
        const char *start = state->i;
        const char *parentheses_left_start = state->i;
        const char *parentheses_left_stop = state->i+1;
        state->last_attr_start = state->i;
        state->last_attr_start_line = state->line;
        state->last_attr_start_col = state->col;
        increment('(', 1, &state->i, &state->line, &state->col);
        const char *stop = state->i;
        DO_CALLBACK(apar_left, state, start_line, start_col, start, start_col,
                    parentheses_left_start, parentheses_left_stop, stop);

        parse_xmq_attributes(state);

        c = *state->i;
        if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }
        if (c != ')')
        {
            state->error_nr = XMQ_ERROR_ATTRIBUTES_NOT_CLOSED;
            longjmp(state->error_handler, 1);
        }

        start = state->i;
        const char *parentheses_right_start = state->i;
        const char *parentheses_right_stop = state->i+1;

        increment(')', 1, &state->i, &state->line, &state->col);
        stop = state->i;
        DO_CALLBACK(apar_right, state, start_line, start_col, start, start_col,
                    parentheses_right_start, parentheses_right_stop, stop);
    }

    c = *state->i;
    if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }

    if (c == '=')
    {
        state->last_equals_start = state->i;
        state->last_equals_start_line = state->line;
        state->last_equals_start_col = state->col;
        const char *start = state->i;
        const char *equal_start = state->i;
        const char *equal_stop = state->i+1;
        increment('=', 1, &state->i, &state->line, &state->col);
        const char *stop = state->i;
        DO_CALLBACK(equals, state, start_line, start_col, start, start_col, equal_start, equal_stop, stop);
        parse_xmq_value(state, LEVEL_ELEMENT_VALUE);
        return;
    }

    if (c == '{')
    {
        const char *start = state->i;
        const char *brace_left_start = state->i;
        const char *brace_left_stop = state->i+1;
        state->last_body_start = state->i;
        state->last_body_start_line = state->line;
        state->last_body_start_col = state->col;
        increment('{', 1, &state->i, &state->line, &state->col);
        const char *stop = state->i;
        DO_CALLBACK(brace_left, state, start_line, start_col, start, start_col, brace_left_start, brace_left_stop, stop);

        parse_xmq(state);
        c = *state->i;
        if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }
        if (c != '}')
        {
            state->error_nr = XMQ_ERROR_BODY_NOT_CLOSED;
            longjmp(state->error_handler, 1);
        }

        start = state->i;
        const char *brace_right_start = state->i;
        const char *brace_right_stop = state->i+1;

        increment('}', 1, &state->i, &state->line, &state->col);
        stop = state->i;
        DO_CALLBACK(brace_right, state, start_line, start_col, start, start_col, brace_right_start, brace_right_stop, stop);
    }
}

void parse_xmq_element(XMQParseState *state)
{
    parse_xmq_element_internal(state, false, false);
}

void parse_xmq_doctype(XMQParseState *state)
{
    parse_xmq_element_internal(state, true, false);
}

void parse_xmq_pi(XMQParseState *state)
{
    parse_xmq_element_internal(state, false, true);
}


/** Parse a list of attribute key = value, or just key children until a ')' is found. */
void parse_xmq_attributes(XMQParseState *state)
{
    const char *end = state->buffer_stop;

    while (state->i < end)
    {
        char c = *(state->i);

        if (is_xml_whitespace(c)) parse_xmq_whitespace(state);
        else if (c == ')') return;
        else if (is_xmq_attribute_key_start(c)) parse_xmq_attribute(state);
        else break;
    }
}

void parse_xmq_attribute(XMQParseState *state)
{
    char c = 0;
    const char *name_start;
    const char *name_stop;

    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    eat_xmq_text_name(state, &name_start, &name_stop);
    const char *stop = state->i;
    DO_CALLBACK(attr_key, state, start_line, start_col, start, start_col, name_start, name_stop, stop);

    c = *state->i;
    if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }

    if (c == '=')
    {
        const char *start = state->i;
        const char *equal_start = state->i;
        const char *equal_stop = state->i+1;
        increment('=', 1, &state->i, &state->line, &state->col);
        const char *stop = state->i;
        DO_CALLBACK(equals, state, start_line, start_col, start, start_col, equal_start, equal_stop, stop);
        parse_xmq_value(state, LEVEL_ATTR_VALUE);
        return;
    }
}

/** Parse a compound value, ie:  = ( '   ' &#10; '  info ' )
    a compound can only occur after an = (equals) character.
    The normal quoting with single quotes, is enough for all quotes except:
    1) An attribute value with leading/ending whitespace including leading/ending newlines.
    2) An attribute with a mix of quotes and referenced entities.
    3) Compact xmq where actual newlines have to be replaced with &#10;

    Note that an element key = ( ... ) can always  be replaced with key { ... }
    so compound values are not strictly necessary for element key values.
    However they are permitted for symmetry reasons.
*/
void parse_xmq_compound(XMQParseState *state, Level level)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *parentheses_left_start = state->i;
    const char *parentheses_left_stop = state->i+1;
    increment('(', 1, &state->i, &state->line, &state->col);
    const char *stop = state->i;
    DO_CALLBACK(cpar_left, state, start_line, start_col, start, start_col,
                parentheses_left_start, parentheses_left_stop, stop);

    parse_xmq_compound_children(state, enter_compound_level(level));

    char c = *state->i;
    if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }

    if (c != ')')
    {
        state->error_nr = XMQ_ERROR_COMPOUND_NOT_CLOSED;
        longjmp(state->error_handler, 1);
    }

    start = state->i;
    const char *parentheses_right_start = state->i;
    const char *parentheses_right_stop = state->i+1;

    increment(')', 1, &state->i, &state->line, &state->col);
    stop = state->i;
    DO_CALLBACK(cpar_right, state, start_line, start_col, start,
                start_col, parentheses_right_start, parentheses_right_stop, stop);
}

/** Parse each compound child (quote or entity) until end of file or a ')' is found. */
void parse_xmq_compound_children(XMQParseState *state, Level level)
{
    const char *end = state->buffer_stop;

    while (state->i < end)
    {
        char c = *(state->i);

        if (is_xml_whitespace(c)) parse_xmq_whitespace(state);
        else if (c == ')') break;
        else if (is_xmq_quote_start(c)) parse_xmq_quote(state, level);
        else if (is_xmq_entity_start(c)) parse_xmq_entity(state, level);
        else
        {
            state->error_nr = XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN;
            longjmp(state->error_handler, 1);
        }
    }
}

void xmqSetupParseCallbacksNoop(XMQParseCallbacks *callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));

#define X(TYPE) callbacks->handle_##TYPE = NULL;
LIST_OF_XMQ_TOKENS
#undef X

    callbacks->magic_cookie = MAGIC_COOKIE;
}

char to_hex(int c)
{
    if (c >= 0 && c <= 9) return '0'+c;
    return 'A'-10+c;
}
/**
    xmq_quote_as_c:

    Escape the in string using c/json quotes. I.e. Surround with " and newline becomes \n and " become \" etc.
*/

char *xmq_quote_as_c(const char *start, const char *stop)
{
    if (!stop) stop = start+strlen(start);
    if (stop == start)
    {
        char *tmp = (char*)malloc(1);
        tmp[0] = 0;
        return tmp;
    }
    assert(stop > start);
    size_t len = 1+(stop-start)*4; // Worst case expansion of all chars.
    char *buf = (char*)malloc(len);

    const char *i = start;
    char *o = buf;
    size_t real = 0;

    for (; i < stop; ++i)
    {
        char c = *i;
        if (c >= ' ' && c <= 126 && c != '"') { *o++ = *i; real++;}
        else if (c == '"') { *o++ = '\\'; *o++ = '"'; real+=2; }
        else if (c == '\a') { *o++ = '\\'; *o++ = 'a'; real+=2; }
        else if (c == '\b') { *o++ = '\\'; *o++ = 'b'; real+=2; }
        else if (c == '\t') { *o++ = '\\'; *o++ = 't'; real+=2; }
        else if (c == '\n') { *o++ = '\\'; *o++ = 'n'; real+=2; }
        else if (c == '\v') { *o++ = '\\'; *o++ = 'v'; real+=2; }
        else if (c == '\f') { *o++ = '\\'; *o++ = 'f'; real+=2; }
        else if (c == '\r') { *o++ = '\\'; *o++ = 'r'; real+=2; }
        else { *o++ = '\\'; *o++ = 'x'; *o++ = to_hex((c>>4)&0xf); *o++ = to_hex(c&0xf); real+=4; }
    }
    real++;
    *o = 0;
    buf = (char*)realloc(buf, real);
    return buf;
}
/**
    xmq_unquote_as_c:

    Unescape the in string using c/json quotes. I.e. Replace \" with ", \n with newline etc.
*/
char *xmq_unquote_as_c(const char *start, const char *stop)
{
    if (stop == start)
    {
        char *tmp = (char*)malloc(1);
        tmp[0] = 0;
        return tmp;
    }
    assert(stop > start);
    size_t len = 1+stop-start; // It gets shorter when unescaping. Worst case no escape was found.
    char *buf = (char*)malloc(len);

    const char *i = start;
    char *o = buf;
    size_t real = 0;

    for (; i < stop; ++i, real++)
    {
        char c = *i;
        if (c == '\\') {
            i++;
            if (i >= stop) break;
            c = *i;
            if (c == '"') *o++ = '"';
            else if (c == 'n') *o++ = '\n';
            else if (c == 'a') *o++ = '\a';
            else if (c == 'b') *o++ = '\b';
            else if (c == 't') *o++ = '\t';
            else if (c == 'v') *o++ = '\v';
            else if (c == 'f') *o++ = '\f';
            else if (c == 'r') *o++ = '\r';
            // Ignore or what?
        }
        else
        {
            *o++ = *i;
        }
    }
    real++;
    *o = 0;
    buf = (char*)realloc(buf, real);
    return buf;
}

#define X(TYPE) void debug_tokens_##TYPE(XMQParseState*state,size_t line,size_t col,const char*start,size_t indent,const char*cstart,const char*cstop,const char*stop) { PRINT_STDOUT("["#TYPE "%s ", state->simulated?" SIM":""); char *tmp = xmq_quote_as_c(start, stop); PRINT_STDOUT("\"%s\" %zu:%zu]", tmp, line, col); free(tmp); };
LIST_OF_XMQ_TOKENS
#undef X

void xmqSetupParseCallbacksDebugTokens(XMQParseCallbacks *callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));
#define X(TYPE) callbacks->handle_##TYPE = debug_tokens_##TYPE ;
LIST_OF_XMQ_TOKENS
#undef X

    callbacks->done = add_nl;

    callbacks->magic_cookie = MAGIC_COOKIE;
}

void debug_content_value(XMQParseState *state,
                         size_t line,
                         size_t start_col,
                         const char *start,
                         size_t indent,
                         const char *cstart,
                         const char *cstop,
                         const char *stop)
{
    char *tmp = xmq_quote_as_c(cstart, cstop);
    PRINT_STDOUT("{value \"%s\"}", tmp);
    free(tmp);
}


void debug_content_quote(XMQParseState *state,
                         size_t line,
                         size_t start_col,
                         const char *start,
                         size_t inden,
                         const char *cstart,
                         const char *cstop,
                         const char*stop)
{
    size_t indent = start_col-1;
    char *trimmed = xmq_un_quote(indent, ' ', start, stop, true);
    char *tmp = xmq_quote_as_c(trimmed, trimmed+strlen(trimmed));
    PRINT_STDOUT("{quote \"%s\"}", tmp);
    free(tmp);
    free(trimmed);
}

void debug_content_comment(XMQParseState *state,
                           size_t line,
                           size_t start_col,
                           const char *start,
                           size_t inden,
                           const char *cstart,
                           const char *cstop,
                           const char*stop)
{
    size_t indent = start_col-1;
    char *trimmed = xmq_un_comment(indent, ' ', start, stop);
    char *tmp = xmq_quote_as_c(trimmed, trimmed+strlen(trimmed));
    PRINT_STDOUT("{comment \"%s\"}", tmp);
    free(tmp);
    free(trimmed);
}

void xmqSetupParseCallbacksDebugContent(XMQParseCallbacks *callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));
    callbacks->handle_element_value_text = debug_content_value;
    callbacks->handle_attr_value_text = debug_content_value;
    callbacks->handle_quote = debug_content_quote;
    callbacks->handle_comment = debug_content_comment;
    callbacks->handle_element_value_quote = debug_content_quote;
    callbacks->handle_element_value_compound_quote = debug_content_quote;
    callbacks->handle_attr_value_quote = debug_content_quote;
    callbacks->handle_attr_value_compound_quote = debug_content_quote;
    callbacks->done = add_nl;

    callbacks->magic_cookie = MAGIC_COOKIE;
}

void xmqSetupParseCallbacksColorizeTokens(XMQParseCallbacks *callbacks, XMQRenderFormat render_format, bool dark_mode)
{
    memset(callbacks, 0, sizeof(*callbacks));

#define X(TYPE) callbacks->handle_##TYPE = colorize_##TYPE ;
LIST_OF_XMQ_TOKENS
#undef X

    callbacks->magic_cookie = MAGIC_COOKIE;
}

const char *xmqParseErrorToString(XMQParseError e)
{
    switch (e)
    {
    case XMQ_ERROR_CANNOT_READ_FILE: return "cannot read file";
    case XMQ_ERROR_NOT_XMQ: return "input file is not xmq";
    case XMQ_ERROR_QUOTE_NOT_CLOSED: return "quote is not closed";
    case XMQ_ERROR_ENTITY_NOT_CLOSED: return "entity is not closed";
    case XMQ_ERROR_COMMENT_NOT_CLOSED: return "comment is not closed";
    case XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES: return "comment closed with too many slashes";
    case XMQ_ERROR_BODY_NOT_CLOSED: return "body is not closed";
    case XMQ_ERROR_ATTRIBUTES_NOT_CLOSED: return "attributes are not closed";
    case XMQ_ERROR_COMPOUND_NOT_CLOSED: return "compound is not closed";
    case XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN: return "compound may only contain quotes and entities";
    case XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES: return "quote closed with too many quotes";
    case XMQ_ERROR_UNEXPECTED_CLOSING_BRACE: return "unexpected closing brace";
    case XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS: return "expected content after equals";
    case XMQ_ERROR_INVALID_CHAR: return "unexpected character";
    case XMQ_ERROR_BAD_DOCTYPE: return "doctype could not be parsed";
    case XMQ_ERROR_CANNOT_HANDLE_XML: return "cannot handle xml use libxmq-all for this!";
    case XMQ_ERROR_CANNOT_HANDLE_HTML: return "cannot handle html use libxmq-all for this!";
    case XMQ_ERROR_CANNOT_HANDLE_JSON: return "cannot handle json use libxmq-all for this!";
    case XMQ_ERROR_JSON_INVALID_ESCAPE: return "invalid json escape";
    case XMQ_ERROR_JSON_INVALID_CHAR: return "json invalid char";
    case XMQ_ERROR_EXPECTED_XMQ: return "expected xmq source";
    case XMQ_ERROR_EXPECTED_HTMQ: return "expected htmlq source";
    case XMQ_ERROR_EXPECTED_XML: return "expected xml source";
    case XMQ_ERROR_EXPECTED_HTML: return "expected html source";
    case XMQ_ERROR_EXPECTED_JSON: return "expected json source";
    }
    assert(false);
    return "unknown error";
}

XMQDoc *xmqNewDoc()
{
    XMQDoc *d = (XMQDoc*)malloc(sizeof(XMQDoc));
    memset(d, 0, sizeof(XMQDoc));
    d->docptr_.xml = xmlNewDoc((const xmlChar*)"1.0");
    return d;
}

void *xmqGetImplementationDoc(XMQDoc *doq)
{
    return doq->docptr_.xml;
}

void xmqSetDocSourceName(XMQDoc *doq, const char *source_name)
{
    if (source_name)
    {
        char *buf = (char*)malloc(strlen(source_name)+1);
        strcpy(buf, source_name);
        doq->source_name_ = buf;
    }
}

XMQNode *xmqGetRootNode(XMQDoc *doq)
{
    return &doq->root_;
}

void xmqFreeParseCallbacks(XMQParseCallbacks *cb)
{
    free(cb);
}

void xmqFreeParseState(XMQParseState *state)
{
    if (!state) return;
    free(state->source_name);
    state->source_name = NULL;
    free(state->generated_error_msg);
    state->generated_error_msg = NULL;
    free_stack(state->element_stack);
//    free(state->settings);
    state->output_settings = NULL;
    free(state);
}

void free_xml(xmlNode * node)
{
    while(node)
    {
        xmlNode *next = node->next;
        free_xml(node->children);
        xmlFreeNode(node);
        node = next;
    }
}

void xmqFreeDoc(XMQDoc *doq)
{
    if (!doq) return;
    if (doq->source_name_)
    {
        debug("(xmq) freeing source name\n");
        free((void*)doq->source_name_);
        doq->source_name_ = NULL;
    }
    if (doq->error_)
    {
        debug("(xmq) freeing error message\n");
        free((void*)doq->error_);
        doq->error_ = NULL;
    }
    if (doq->docptr_.xml)
    {
        debug("(xmq) freeing xml doc\n");
        xmlFreeDoc(doq->docptr_.xml);
        doq->docptr_.xml = NULL;
    }
    debug("(xmq) freeing xmq doc\n");
    free(doq);
}

const char *skip_any_potential_bom(const char *start, const char *stop)
{
    if (start + 3 < stop)
    {
        int a = (unsigned char)*(start+0);
        int b = (unsigned char)*(start+1);
        int c = (unsigned char)*(start+2);
        if (a == 0xef && b == 0xbb && c == 0xbf)
        {
            // The UTF8 bom, this is fine, just skip it.
            return start+3;
        }
    }

    if (start+2 < stop)
    {
        unsigned char a = *(start+0);
        unsigned char b = *(start+1);
        if ((a == 0xff && b == 0xfe) ||
            (a == 0xfe && b == 0xff))
        {
            // We cannot handle UTF-16 files.
            return NULL;
        }
    }

    // Assume content, no bom.
    return start;
}

bool xmqParseBuffer(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root)
{
    bool rc = true;
    XMQOutputSettings *output_settings = xmqNewOutputSettings();
    XMQParseCallbacks *parse = xmqNewParseCallbacks();

    xmq_setup_parse_callbacks(parse);

    XMQParseState *state = xmqNewParseState(parse, output_settings);
    state->doq = doq;
    xmqSetStateSourceName(state, doq->source_name_);

    if (implicit_root != NULL && implicit_root[0] == 0) implicit_root = NULL;

    state->implicit_root = implicit_root;

    push_stack(state->element_stack, doq->docptr_.xml);
    // Now state->element_stack->top->data points to doq->docptr_;
    state->element_last = NULL; // Will be set when the first node is created.
    // The doc root will be set when the first element node is created.

    // Time to tokenize the buffer and invoke the parse callbacks.
    xmqTokenizeBuffer(state, start, stop);

    if (xmqStateErrno(state))
    {
        rc = false;
        doq->errno_ = xmqStateErrno(state);
        doq->error_ = build_error_message("%s\n", xmqStateErrorMsg(state));
    }

    xmqFreeParseState(state);
    xmqFreeParseCallbacks(parse);
    xmqFreeOutputSettings(output_settings);

    return rc;
}

bool xmqParseFile(XMQDoc *doq, const char *file, const char *implicit_root)
{
    bool ok = true;
    char *buffer = NULL;
    long fsize = 0;
    size_t n = 0;
    XMQContentType content = XMQ_CONTENT_XMQ;

    xmqSetDocSourceName(doq, file);

    FILE *f = fopen(file, "rb");
    if (!f) {
        doq->errno_ = XMQ_ERROR_CANNOT_READ_FILE;
        goto exit;
    }

    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = (char*)malloc(fsize + 1);
    if (!buffer) return false;

    n = fread(buffer, fsize, 1, f);

    if (n != 1) {
        ok = false;
        doq->errno_ = XMQ_ERROR_CANNOT_READ_FILE;
        goto exit;
    }
    fclose(f);
    buffer[fsize] = 0;

    content = xmqDetectContentType(buffer, buffer+fsize);
    if (content != XMQ_CONTENT_XMQ)
    {
        ok = false;
        doq->errno_ = XMQ_ERROR_NOT_XMQ;
        goto exit;
    }

    ok = xmqParseBuffer(doq, buffer, buffer+fsize, implicit_root);

    exit:

    free(buffer);

    return ok;
}

const char *xmqVersion()
{
    return VERSION;
}


const char *xmqCommit()
{
    return COMMIT;
}

void do_whitespace(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   size_t indent,
                   const char *cstart,
                   const char *cstop,
                   const char *stop)
{
}

xmlNodePtr create_quote(XMQParseState *state,
                       size_t l,
                       size_t col,
                       const char *start,
                       size_t ccol,
                       const char *cstart,
                       const char *cstop,
                       const char *stop,
                       xmlNodePtr parent)
{
    size_t indent = col - 1;
    char *trimmed = xmq_un_quote(indent, ' ', start, stop, true);
    xmlNodePtr n = xmlNewDocText(state->doq->docptr_.xml, (const xmlChar *)trimmed);
    xmlAddChild(parent, n);
    free(trimmed);
    return n;
}

void do_quote(XMQParseState *state,
              size_t l,
              size_t col,
              const char *start,
              size_t ccol,
              const char *cstart,
              const char *cstop,
              const char *stop)
{
    state->element_last = create_quote(state, l, col, start, ccol, cstart, cstop, stop, (xmlNode*)state->element_stack->top->data);
}

xmlNodePtr create_entity(XMQParseState *state,
                         size_t l,
                         size_t c,
                         const char *start,
                         size_t indent,
                         const char *cstart,
                         const char *cstop,
                         const char*stop,
                         xmlNodePtr parent)
{
    size_t len = stop-start;
    char *tmp = (char*)malloc(len+1);
    memcpy(tmp, start, len);
    tmp[len] = 0;
    xmlNodePtr n = NULL;
    if (tmp[1] == '#')
    {
        n = xmlNewCharRef(state->doq->docptr_.xml, (const xmlChar *)tmp);
    }
    else
    {
        n = xmlNewReference(state->doq->docptr_.xml, (const xmlChar *)tmp);
    }
    xmlAddChild(parent, n);
    free(tmp);

    return n;
}

void do_entity(XMQParseState *state,
               size_t l,
               size_t c,
               const char *start,
               size_t indent,
               const char *cstart,
               const char *cstop,
               const char*stop)
{
    state->element_last = create_entity(state, l, c, start, indent, cstart, cstop, stop, (xmlNode*)state->element_stack->top->data);
}

void do_comment(XMQParseState*state,
                size_t l,
                size_t c,
                const char *start,
                size_t indent,
                const char *cstart,
                const char *cstop,
                const char *stop)
{
    xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
    char *trimmed = xmq_un_comment(indent, ' ', start, stop);
    xmlNodePtr n = xmlNewDocComment(state->doq->docptr_.xml, (const xmlChar *)trimmed);
    xmlAddChild(parent, n);
    state->element_last = n;
    free(trimmed);
}

void do_comment_continuation(XMQParseState*state,
                             size_t line,
                             size_t col,
                             const char *start,
                             size_t indent,
                             const char *cstart,
                             const char *cstop,
                             const char *stop)
{
    xmlNodePtr last = (xmlNode*)state->element_last;
    // We have ///* alfa *///* beta *///* gamma *///
    // and this function is invoked with "* beta *///"
    const char *i = stop-1;
    // Count n slashes from the end
    size_t n = 0;
    while (i > start && *i == '/') { n++; i--; }
    // Since we know that we are invoked pointing into a buffer with /// before start, we
    // can safely do start-n.
    char *trimmed = xmq_un_comment(indent, ' ', start-n, stop);
    size_t l = strlen(trimmed);
    char *tmp = (char*)malloc(l+2);
    tmp[0] = '\n';
    memcpy(tmp+1, trimmed, l);
    tmp[l+1] = 0;
    xmlNodeAddContent(last, (const xmlChar *)tmp);
    free(trimmed);
    free(tmp);
}

void do_element_value_text(XMQParseState *state,
                           size_t line,
                           size_t col,
                           const char *start,
                           size_t indent,
                           const char *cstart,
                           const char *cstop,
                           const char *stop)
{
    if (!state->parsing_doctype)
    {
        xmlNodePtr n = xmlNewDocTextLen(state->doq->docptr_.xml, (const xmlChar *)start, stop-start);
        xmlAddChild((xmlNode*)state->element_last, n);
    }
    else
    {
        size_t l = cstop-cstart;
        char *tmp = (char*)malloc(l+1);
        memcpy(tmp, cstart, l);
        tmp[l] = 0;
        state->doq->docptr_.xml->intSubset = xmlNewDtd(state->doq->docptr_.xml, (xmlChar*)tmp, NULL, NULL);
        xmlNodePtr n = (xmlNodePtr)state->doq->docptr_.xml->intSubset;
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, n);
        free(tmp);

        state->parsing_doctype = false;
    }
}

void do_element_value_quote(XMQParseState *state,
                            size_t line,
                            size_t col,
                            const char *start,
                            size_t indent,
                            const char *cstart,
                            const char *cstop,
                            const char *stop)
{
    char *trimmed = xmq_un_quote(col-1, ' ', start, stop, true);
    if (!state->parsing_doctype)
    {
        xmlNodePtr n = xmlNewDocText(state->doq->docptr_.xml, (const xmlChar *)trimmed);
        xmlAddChild((xmlNode*)state->element_last, n);
    }
    else
    {
        // "<!DOCTYPE "=10  ">"=1 NULL=1
        size_t tn = strlen(trimmed);
        size_t n = tn+10+1+12;
        char *buf = (char*)malloc(n);
        strcpy(buf, "<!DOCTYPE ");
        memcpy(buf+10, trimmed, tn);
        memcpy(buf+10+tn, "><foo></foo>", 12);
        buf[n-1] = 0;
        xmlDtdPtr dtd = parse_doctype_raw(state->doq, buf, buf+n-1);
        if (!dtd)
        {
            state->error_nr = XMQ_ERROR_BAD_DOCTYPE;
            longjmp(state->error_handler, 1);
        }
        state->doq->docptr_.xml->intSubset = dtd;
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, (xmlNodePtr)dtd);
        state->parsing_doctype = false;
        free(buf);
    }
    free(trimmed);
}

void do_element_value_entity(XMQParseState *state,
                             size_t line,
                             size_t col,
                             const char *start,
                             size_t indent,
                             const char *cstart,
                             const char *cstop,
                             const char *stop)
{
    create_entity(state, line, col, start, indent, cstart, cstop, stop, (xmlNode*)state->element_last);
}

void do_element_value_compound_quote(XMQParseState *state,
                                     size_t line,
                                     size_t col,
                                     const char *start,
                                     size_t indent,
                                     const char *cstart,
                                     const char *cstop,
                                     const char *stop)
{
    do_quote(state, line, col, start, indent, cstart, cstop, stop);
}

void do_element_value_compound_entity(XMQParseState *state,
                                      size_t line,
                                      size_t col,
                                      const char *start,
                                      size_t indent,
                                      const char *cstart,
                                      const char *cstop,
                                      const char *stop)
{
    do_entity(state, line, col, start, indent, cstart, cstop, stop);
}

void do_attr_ns(XMQParseState *state,
                size_t line,
                size_t col,
                const char *start,
                size_t indent,
                const char *cstart,
                const char *cstop,
                const char *stop)
{
}

void do_attr_key(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 size_t indent,
                 const char *cstart,
                 const char *cstop,
                 const char *stop)
{
    size_t n = stop - start;
    char *key = (char*)malloc(n+1);
    memcpy(key, start, n);
    key[n] = 0;

    xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
    xmlAttrPtr attr =  xmlNewProp(parent, (xmlChar*)key, NULL);
    // The new attribute attr should now be added to the parent elements: properties list.
    // Remember this attr so that we can set the value.
    state->element_last = attr;

    free(key);
}

void do_attr_value_text(XMQParseState *state,
                        size_t line,
                        size_t col,
                        const char *start,
                        size_t indent,
                        const char *cstart,
                        const char *cstop,
                        const char *stop)
{
    xmlNodePtr n = xmlNewDocTextLen(state->doq->docptr_.xml, (const xmlChar *)start, stop-start);
    xmlAddChild((xmlNode*)state->element_last, n);
}

void do_attr_value_quote(XMQParseState*state,
                         size_t line,
                         size_t col,
                         const char *start,
                         size_t i,
                         const char *cstart,
                         const char *cstop,
                         const char *stop)
{
    create_quote(state, line, col, start, i, cstart, cstop, stop, (xmlNode*)state->element_last);
}

void do_attr_value_entity(XMQParseState *state,
                          size_t l,
                          size_t c,
                          const char *start,
                          size_t indent,
                          const char *cstart,
                          const char *cstop,
                          const char*stop)
{
    create_entity(state, l, c, start, indent, cstart, cstop, stop, (xmlNode*)state->element_last);
}

void do_attr_value_compound_quote(XMQParseState *state,
                                             size_t l,
                                             size_t c,
                                             const char *start,
                                             size_t indent,
                                             const char *cstart,
                                             const char *cstop,
                                             const char*stop)
{
    do_quote(state, l, c, start, indent, cstart, cstop, stop);
}

void do_attr_value_compound_entity(XMQParseState *state,
                                             size_t l,
                                             size_t c,
                                             const char *start,
                                             size_t indent,
                                             const char *cstart,
                                             const char *cstop,
                                             const char*stop)
{
    do_entity(state, l, c, start, indent, cstart, cstop, stop);
}

void create_node(XMQParseState *state, const char *start, const char *stop)
{
    size_t len = stop-start;
    char *name = (char*)malloc(len+1);
    memcpy(name, start, len);
    name[len] = 0;

    if (!strcmp(name, "!DOCTYPE"))
    {
        state->parsing_doctype = true;
    }
    else
    {
        xmlNodePtr n = xmlNewDocNode(state->doq->docptr_.xml, NULL, (const xmlChar *)name, NULL);
        if (state->element_last == NULL)
        {
            state->element_last = n;
            xmlDocSetRootElement(state->doq->docptr_.xml, n);
            state->doq->root_.node = n;
        }
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, n);
        state->element_last = n;
    }


    free(name);
}

void do_element_ns(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   size_t indent,
                   const char *cstart,
                   const char *cstop,
                   const char *stop)
{
}

void do_ns_colon(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 size_t indent,
                 const char *cstart,
                 const char *cstop,
                 const char *stop)
{
}

void do_element_name(XMQParseState *state,
                     size_t line,
                     size_t col,
                     const char *start,
                     size_t indent,
                     const char *cstart,
                     const char *cstop,
                     const char *stop)
{
    create_node(state, start, stop);
}

void do_element_key(XMQParseState *state,
                    size_t line,
                    size_t col,
                    const char *start,
                    size_t indent,
                    const char *cstart,
                    const char *cstop,
                    const char *stop)
{
    create_node(state, start, stop);
}

void do_equals(XMQParseState *state,
               size_t line,
               size_t col,
               const char *start,
               size_t indent,
               const char *cstart,
               const char *cstop,
               const char *stop)
{
}

void do_brace_left(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   size_t indent,
                   const char *cstart,
                   const char *cstop,
                   const char *stop)
{
    push_stack(state->element_stack, state->element_last);
}

void do_brace_right(XMQParseState *state,
                    size_t line,
                    size_t col,
                    const char *start,
                    size_t indent,
                    const char *cstart,
                    const char *cstop,
                    const char *stop)
{
    state->element_last = pop_stack(state->element_stack);
}

void do_apar_left(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 size_t indent,
                 const char *cstart,
                 const char *cstop,
                 const char *stop)
{
    push_stack(state->element_stack, state->element_last);
}

void do_apar_right(XMQParseState *state,
                  size_t line,
                  size_t col,
                  const char *start,
                  size_t indent,
                  const char *cstart,
                  const char *cstop,
                  const char *stop)
{
    state->element_last = pop_stack(state->element_stack);
}

void do_cpar_left(XMQParseState *state,
                  size_t line,
                  size_t col,
                  const char *start,
                  size_t indent,
                  const char *cstart,
                  const char *cstop,
                  const char *stop)
{
    push_stack(state->element_stack, state->element_last);
}

void do_cpar_right(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   size_t indent,
                   const char *cstart,
                   const char *cstop,
                   const char *stop)
{
    state->element_last = pop_stack(state->element_stack);
}

void xmq_setup_parse_callbacks(XMQParseCallbacks *callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));

#define X(TYPE) callbacks->handle_##TYPE = do_##TYPE;
LIST_OF_XMQ_TOKENS
#undef X

    callbacks->magic_cookie = MAGIC_COOKIE;
}

/**
  Scan the attribute names and find the max unicode character width.
*/
size_t find_attr_key_max_u_width(xmlAttr *a)
{
    size_t max = 0;
    while (a)
    {
        const char *name;
        const char *prefix;
        size_t total_u_len;
        attr_strlen_name_prefix(a, &name, &prefix, &total_u_len);

        if (total_u_len > max) max = total_u_len;
        a = xml_next_attribute(a);
    }
    return max;
}

/**
  Scan the namespace links and find the max unicode character width.
*/
size_t find_namespace_max_u_width(size_t max, xmlNs *ns)
{
    while (ns)
    {
        const char *prefix;
        size_t total_u_len;
        namespace_strlen_prefix(ns, &prefix, &total_u_len);

        // Print only new/overriding namespaces.
        if (total_u_len > max) max = total_u_len;
        ns = ns->next;
    }

    return max;
}

/**
  Scan nodes until there is a node which is not suitable for using the = sign.
  I.e. it has multiple children or no children. This node unsuitable node is stored in
  restart_find_at_node or NULL if all nodes were suitable.
*/
size_t find_element_key_max_width(xmlNodePtr element, xmlNodePtr *restart_find_at_node)
{
    size_t max = 0;
    xmlNodePtr i = element;
    while (i)
    {
        if (!is_key_value_node(i) || xml_first_attribute(i))
        {
            if (i == element) *restart_find_at_node = xml_next_sibling(i);
            else *restart_find_at_node = i;
            return max;
        }
        const char *name;
        const char *prefix;
        size_t total_u_len;
        element_strlen_name_prefix(i, &name, &prefix, &total_u_len);

        if (total_u_len > max) max = total_u_len;
        i = xml_next_sibling(i);
    }
    *restart_find_at_node = NULL;
    return max;
}

void print_white_spaces(XMQPrintState *ps, int num)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQColoring *coloring = &output_settings->coloring;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;
    if (coloring->whitespace.pre) write(writer_state, coloring->whitespace.pre, NULL);
    for (int i=0; i<num; ++i)
    {
        write(writer_state, coloring->indentation_space, NULL);
    }
    ps->current_indent += num;
    if (coloring->whitespace.post) write(writer_state, coloring->whitespace.post, NULL);
}

void print_all_whitespace(XMQPrintState *ps, const char *start, const char *stop, Level level)
{
    const char *i = start;
    while (true)
    {
        if (i >= stop) break;
        if (*i == ' ')
        {
            const char *j = i;
            while (*j == ' ' && j < stop) j++;
            check_space_before_quote(ps, level);
            print_quoted_spaces(ps, level_to_quote_color(level), (size_t)(j-i));
            i += j-i;
        }
        else
        {
            check_space_before_entity_node(ps);
            print_char_entity(ps, level_to_entity_color(level), i, stop);
            i++;
        }
    }
}

void print_explicit_spaces(XMQPrintState *ps, XMQColor c, int num)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQColoring *coloring = &output_settings->coloring;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;

    const char *pre = NULL;
    const char *post = NULL;
    get_color(coloring, c, &pre, &post);

    write(writer_state, pre, NULL);
    for (int i=0; i<num; ++i)
    {
        write(writer_state, coloring->explicit_space, NULL);
    }
    ps->current_indent += num;
    write(writer_state, post, NULL);
}

void print_quoted_spaces(XMQPrintState *ps, XMQColor c, int num)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQColoring *coloring = &output_settings->coloring;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;

    if (coloring->whitespace.pre) write(writer_state, coloring->quote.pre, NULL);
    write(writer_state, "'", NULL);
    for (int i=0; i<num; ++i)
    {
        write(writer_state, coloring->explicit_space, NULL);
    }
    ps->current_indent += num;
    ps->last_char = '\'';
    write(writer_state, "'", NULL);
    if (coloring->whitespace.post) write(writer_state, coloring->quote.post, NULL);
}

void print_quotes(XMQPrintState *ps, size_t num, XMQColor c)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;

    XMQColoring *coloring = &output_settings->coloring;
    const char *pre = NULL;
    const char *post = NULL;
    get_color(coloring, c, &pre, &post);

    if (pre) write(writer_state, pre, NULL);
    for (size_t i=0; i<num; ++i)
    {
        write(writer_state, "'", NULL);
    }
    ps->current_indent += num;
    ps->last_char = '\'';
    if (post) write(writer_state, post, NULL);
}

void print_nl_and_indent(XMQPrintState *ps, const char *prefix, const char *postfix)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;
    if (postfix) write(writer_state, postfix, NULL);
    write(writer_state, output_settings->coloring.explicit_nl, NULL);
    ps->current_indent = 0;
    ps->last_char = 0;
    print_white_spaces(ps, ps->line_indent);
    if (ps->restart_line) write(writer_state, ps->restart_line, NULL);
    if (prefix) write(writer_state, prefix, NULL);
}

void print_color_pre(XMQPrintState *ps, XMQColor c)
{
    XMQColoring *coloring = &ps->output_settings->coloring;
    const char *pre = NULL;
    const char *post = NULL;
    get_color(coloring, c, &pre, &post);

    if (pre)
    {
        XMQWrite write = ps->output_settings->content.write;
        void *writer_state = ps->output_settings->content.writer_state;
        write(writer_state, pre, NULL);
    }
}

void print_color_post(XMQPrintState *ps, XMQColor c)
{
    XMQColoring *coloring = &ps->output_settings->coloring;
    const char *pre = NULL;
    const char *post = NULL;
    get_color(coloring, c, &pre, &post);

    if (post)
    {
        XMQWrite write = ps->output_settings->content.write;
        void *writer_state = ps->output_settings->content.writer_state;
        write(writer_state, post, NULL);
    }
}

const char *needs_escape(XMQRenderFormat f, const char *start, const char *stop)
{
    if (f == XMQ_RENDER_HTML)
    {
        char c = *start;
        if (c == '&') return "&amp;";
        if (c == '<') return "&lt;";
        if (c == '>') return "&gt;";
        return NULL;
    }
    else if (f == XMQ_RENDER_TEX)
    {
        char c = *start;
        if (c == '\\') return "\\backslash;";
        if (c == '&') return "\\&";
        if (c == '#') return "\\#";
        if (c == '{') return "\\{";
        if (c == '}') return "\\}";
        if (c == '_') return "\\_";
        if (c == '\'') return "{'}";

        return NULL;
    }

    return NULL;
}

inline size_t print_utf8_char(XMQPrintState *ps, const char *start, const char *stop)
{
    XMQWrite write = ps->output_settings->content.write;
    void *writer_state = ps->output_settings->content.writer_state;

    const char *i = start;

    // Find next utf8 char....
    const char *j = i+1;
    while (j < stop && (*j & 0xc0) == 0x80) j++;

    // Is the utf8 char a unicode whitespace and not space,tab,cr,nl?
    bool uw = is_unicode_whitespace(i, j);

    // If so, then color it. This will typically red underline the non-breakable space.
    if (uw) print_color_pre(ps, COLOR_unicode_whitespace);

    if (*i == ' ')
    {
        write(writer_state, ps->output_settings->coloring.explicit_space, NULL);
    }
    else
    {
        const char *e = needs_escape(ps->output_settings->render_to, i, j);
        if (!e)
        {
            write(writer_state, i, j);
        }
        else
        {
            write(writer_state, e, NULL);
        }
    }
    if (uw) print_color_post(ps, COLOR_unicode_whitespace);

    ps->last_char = *i;
    ps->current_indent++;

    return j-start;
}

/**
   print_utf8_internal: Print a single string
   ps: The print state.
   num_utf8: If non-zero then print this number of complete utf8 chars.
   start: Points to bytes to be printed.
   stop: Points to byte after last byte to be printed. If NULL then assume start is null-terminated.

   Returns number of bytes printed.
*/
size_t print_utf8_internal(XMQPrintState *ps, const char *start, const char *stop)
{
    XMQWrite write = ps->output_settings->content.write;
    void *writer_state = ps->output_settings->content.writer_state;

    size_t u_len = 0;

    const char *i = start;
    while (*i && (!stop || i < stop))
    {
        // Find next utf8 char....
        const char *j = i+1;
        while (j < stop && (*j & 0xc0) == 0x80) j++;

        // Is the utf8 char a unicode whitespace and not space,tab,cr,nl?
        bool uw = is_unicode_whitespace(i, j);

        // If so, then color it. This will typically red underline the non-breakable space.
        if (uw) print_color_pre(ps, COLOR_unicode_whitespace);

        if (*i == ' ')
        {
            write(writer_state, ps->output_settings->coloring.explicit_space, NULL);
        }
        else
        {
            const char *e = needs_escape(ps->output_settings->render_to, i, j);
            if (!e)
            {
                write(writer_state, i, j);
            }
            else
            {
                write(writer_state, e, NULL);
            }
        }
        if (uw) print_color_post(ps, COLOR_unicode_whitespace);
        u_len++;
        i = j;
    }

    ps->last_char = *(i-1);
    ps->current_indent += u_len;
    return i-start;
}

/**
   print_utf8:
   @ps: The print state.
   @c:  The color.
   @num_utf8:  Number of unicode chars to print. If 0 print all between start and stop.
   @num_pairs:  Number of start, stop pairs.
   @start: First utf8 byte to print.
   @stop: Points to byte after the last utf8 content.

   Returns the number of bytes used after start.
*/
size_t print_utf8(XMQPrintState *ps, XMQColor c, size_t num_pairs, ...)
{
    XMQWrite write = ps->output_settings->content.write;
    void *writer_state = ps->output_settings->content.writer_state;
    XMQColoring *coloring = &ps->output_settings->coloring;

    const char *pre, *post;
    get_color(coloring, c, &pre, &post);

    if (pre) write(writer_state, pre, NULL);

    size_t b_len = 0;

    va_list ap;
    va_start(ap, num_pairs);
    for (size_t x = 0; x < num_pairs; ++x)
    {
        const char *start = va_arg(ap, const char *);
        const char *stop = va_arg(ap, const char *);
        b_len += print_utf8_internal(ps, start, stop);
    }
    va_end(ap);

    if (post) write(writer_state, post, NULL);

    return b_len;
}

bool decode_utf8(const char *start, const char *stop, int *out_char, size_t *out_len)
{
    int c = (int)(unsigned char)(*start);

    if ((c & 0x80) == 0)
    {
        *out_char = c;
        *out_len = 1;
        return true;
    }

    if ((c & 0xe0) == 0xc0)
    {
        if (start+1 < stop)
        {
            unsigned char cc = *(start+1);
            if ((cc & 0xc0) == 0x80)
            {
                *out_char = ((c & 0x1f) << 6) | (cc & 0x3f);
                *out_len = 2;
                return true;
            }
        }
    }
    else if ((c & 0xf0) == 0xe0)
    {
        if (start+2 < stop)
        {
            unsigned char cc = *(start+1);
            unsigned char ccc = *(start+2);
            if (((cc & 0xc0) == 0x80) && ((ccc & 0xc0) == 0x80))
            {
                *out_char = ((c & 0x0f) << 12) | ((cc & 0x3f) << 6) | (ccc & 0x3f) ;
                *out_len = 3;
                return true;
            }
        }
    }
    else if ((c & 0xf8) == 0xf0)
    {
        if (start+3 < stop)
        {
            unsigned char cc = *(start+1);
            unsigned char ccc = *(start+2);
            unsigned char cccc = *(start+3);
            if (((cc & 0xc0) == 0x80) && ((ccc & 0xc0) == 0x80) && ((cccc & 0xc0) == 0x80))
            {
                *out_char = ((c & 0x07) << 18) | ((cc & 0x3f) << 12) | ((ccc & 0x3f) << 6) | (cccc & 0x3f);
                *out_len = 4;
                return true;
            }
        }
    }

    // Illegal utf8.
    *out_char = 1;
    *out_len = 1;
    return false;
}

size_t print_char_entity(XMQPrintState *ps, XMQColor c, const char *start, const char *stop)
{
    XMQWrite write = ps->output_settings->content.write;
    void *writer_state = ps->output_settings->content.writer_state;
    XMQColoring *coloring = &ps->output_settings->coloring;
    const char *pre, *post;
    get_color(coloring, c, &pre, &post);

    int uc = 0;
    size_t bytes = 0;
    if (decode_utf8(start, stop, &uc, &bytes))
    {
        // Max entity &#1114112; max buf is 11 bytes including terminating zero byte.
        char buf[20] = {};
        memset(buf, 0, sizeof(buf));
        snprintf(buf, 11, "&#%d;", uc);

        if (pre) write(writer_state, pre, NULL);
        print_utf8(ps, COLOR_none, 1, buf, NULL);
        if (post) write(writer_state, post, NULL);

        ps->last_char = ';';
        ps->current_indent += strlen(buf);
    }
    else
    {
        if (pre) write(writer_state, pre, NULL);
        write(writer_state, "&badutf8;", NULL);
        if (post) write(writer_state, post, NULL);
    }

    return bytes;
}

void print_slashes(XMQPrintState *ps, const char *pre, const char *post, size_t n)
{
    XMQWrite write = ps->output_settings->content.write;
    void *writer_state = ps->output_settings->content.writer_state;
    XMQColoring *coloring = &ps->output_settings->coloring;
    const char *cpre = NULL;
    const char *cpost = NULL;
    get_color(coloring, COLOR_comment, &cpre, &cpost);

    if (cpre) write(writer_state, cpre, NULL);
    if (pre) write(writer_state, pre, NULL);
    for (size_t i = 0; i < n; ++i) write(writer_state, "/", NULL);
    if (post) write(writer_state, post, NULL);
    if (cpost) write(writer_state, cpost, NULL);
}

bool need_separation_before_attribute_key(XMQPrintState *ps)
{
    // If the previous value was quoted, then no space is needed, ie.
    // 'x y z'key=
    // If the previous text was attribute start, then no space is needed, ie.
    // (key=
    // If the previous text was compound endt, then no space is needed, ie.
    // ))key=
    // If the previous text was entity, then no space is needed, ie.
    // &x10;key=
    // if previous value was text, then a space is necessary, ie.
    // xyz key=
    char c = ps->last_char;
    return c != 0 && c != '\'' && c != '(' && c != ')' && c != ';';
}

bool need_separation_before_entity(XMQPrintState *ps)
{
    // No space needed for:
    // 'x y z'&nbsp;
    // =&nbsp;
    // {&nbsp;
    // }&nbsp;
    // ;&nbsp;
    // Otherwise a space is needed:
    // xyz &nbsp;
    char c = ps->last_char;
    return c != 0 && c != '=' && c != '\'' && c != '{' && c != '}' && c != ';' && c != '(' && c != ')';
}

bool need_separation_before_element_name(XMQPrintState *ps)
{
    // No space needed for:
    // 'x y z'key=
    // {key=
    // }key=
    // ;key=
    // */key=
    // )key=
    // Otherwise a space is needed:
    // xyz key=
    char c = ps->last_char;
    return c != 0 && c != '\'' && c != '{' && c != '}' && c != ';' && c != ')' && c != '/';
}

bool need_separation_before_quote(XMQPrintState *ps)
{
    // If the previous node was quoted, then a space is necessary, ie
    // 'a b c' 'next quote'
    // otherwise last char is the end of a text value, and no space is necessary, ie
    // key=value'next quote'
    char c = ps->last_char;
    return c == '\'';
}

bool need_separation_before_comment(XMQPrintState *ps)
{
    // If the previous value was quoted, then then no space is needed, ie.
    // 'x y z'/*comment*/
    // If the previous value was an entity &...; then then no space is needed, ie.
    // &nbsp;/*comment*/
    // if previous value was text, then a space is necessary, ie.
    // xyz /*comment*/
    // if previous value was } or )) then no space is is needed.
    // }/*comment*/   ((...))/*comment*/
    char c = ps->last_char;
    return c != 0 && c != '\'' && c != '{' && c != ')' && c != '}' && c != ';';
}

void check_space_before_attribute(XMQPrintState *ps)
{
    char c = ps->last_char;
    if (c == '(') return;
    if (!ps->output_settings->compact)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }
    else if (need_separation_before_attribute_key(ps))
    {
        print_white_spaces(ps, 1);
    }
}

void check_space_before_entity_node(XMQPrintState *ps)
{
    char c = ps->last_char;
    if (c == '(') return;
    if (!ps->output_settings->compact && c != '=')
    {
        print_nl_and_indent(ps, NULL, NULL);
    }
    else if (need_separation_before_entity(ps))
    {
        print_white_spaces(ps, 1);
    }
}


void check_space_before_quote(XMQPrintState *ps, Level level)
{
    char c = ps->last_char;
    if (c == 0) return;
    if (!ps->output_settings->compact && (c != '=' || level == LEVEL_XMQ) && c != '(')
    {
        print_nl_and_indent(ps, NULL, NULL);
    }
    else if (need_separation_before_quote(ps))
    {
        print_white_spaces(ps, 1);
    }
}

void check_space_before_key(XMQPrintState *ps)
{
    char c = ps->last_char;
    if (c == 0) return;

    if (!ps->output_settings->compact)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }
    else if (need_separation_before_element_name(ps))
    {
        print_white_spaces(ps, 1);
    }
}

void check_space_before_opening_brace(XMQPrintState *ps)
{
    char c = ps->last_char;

    if (!ps->output_settings->compact)
    {
        if (c == ')')
        {
            print_nl_and_indent(ps, NULL, NULL);
        }
        else
        {
            print_white_spaces(ps, 1);
        }
    }
}

void check_space_before_closing_brace(XMQPrintState *ps)
{
    if (!ps->output_settings->compact)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }
}

void check_space_before_comment(XMQPrintState *ps)
{
    char c = ps->last_char;

    if (c == 0) return;
    if (!ps->output_settings->compact)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }
    else if (need_separation_before_comment(ps))
    {
        print_white_spaces(ps, 1);
    }
}

void copy_quote_settings_from_output_settings(XMQQuoteSettings *qs, XMQOutputSettings *os)
{
    qs->indentation_space = os->coloring.indentation_space;
    qs->explicit_space = os->coloring.explicit_space;
    qs->explicit_nl = os->coloring.explicit_nl;
    qs->prefix_line = os->coloring.prefix_line;
    qs->postfix_line = os->coloring.prefix_line;
    qs->compact = os->compact;
}

void print_attribute(XMQPrintState *ps, xmlAttr *a, size_t align)
{
    check_space_before_attribute(ps);

    const char *key;
    const char *prefix;
    size_t total_u_len;

    attr_strlen_name_prefix(a, &key, &prefix, &total_u_len);

    if (prefix)
    {
        print_utf8(ps, COLOR_attr_ns, 1, prefix, NULL);
        print_utf8(ps, COLOR_ns_colon, 1, ":", NULL);
    }
    print_utf8(ps, COLOR_attr_key, 1, key, NULL);

    if (a->children != NULL)
    {
        if (!ps->output_settings->compact) print_white_spaces(ps, 1+align-total_u_len);

        print_utf8(ps, COLOR_equals, 1, "=", NULL);

        if (!ps->output_settings->compact) print_white_spaces(ps, 1);

        print_value(ps, a->children, LEVEL_ATTR_VALUE);
    }
}

void print_namespace(XMQPrintState *ps, xmlNs *ns, size_t align)
{
    check_space_before_attribute(ps);

    const char *prefix;
    size_t total_u_len;

    namespace_strlen_prefix(ns, &prefix, &total_u_len);

    print_utf8(ps, COLOR_attr_key, 1, "xmlns", NULL);

    if (prefix)
    {
        print_utf8(ps, COLOR_ns_colon, 1, ":", NULL);
        print_utf8(ps, COLOR_attr_ns, 1, prefix, NULL);
    }

    const char *v = xml_namespace_href(ns);

    if (v != NULL)
    {
        if (!ps->output_settings->compact) print_white_spaces(ps, 1+align-total_u_len);

        print_utf8(ps, COLOR_equals, 1, "=", NULL);

        if (!ps->output_settings->compact) print_white_spaces(ps, 1);

        print_utf8(ps, COLOR_attr_value_text, 1, v, NULL);
    }
}


void print_attributes(XMQPrintState *ps,
                      xmlNodePtr node)
{
    xmlAttr *a = xml_first_attribute(node);

    size_t max = 0;
    if (!ps->output_settings->compact) max = find_attr_key_max_u_width(a);

    xmlNs *ns = xml_first_namespace_def(node);
    if (!ps->output_settings->compact) max = find_namespace_max_u_width(max, ns);

    size_t line_indent = ps->line_indent;
    ps->line_indent = ps->current_indent;
    while (a)
    {
        print_attribute(ps, a, max);
        a = xml_next_attribute(a);
    }
    while (ns)
    {
        print_namespace(ps, ns, max);
        ns = xml_next_namespace_def(ns);
    }

    ps->line_indent = line_indent;
}

void print_nodes(XMQPrintState *ps, xmlNode *from, xmlNode *to, size_t align)
{
    xmlNode *i = from;
    xmlNode *restart_find_at_node = from;
    size_t max = 0;

    while (i)
    {
        // We need to search ahead to find the max width of the node names so that we can align the equal signs.
        if (!ps->output_settings->compact && i == restart_find_at_node)
        {
            max = find_element_key_max_width(i, &restart_find_at_node);
        }

        print_node(ps, i, max);
        i = xml_next_sibling(i);
    }
}

void print_content_node(XMQPrintState *ps, xmlNode *node)
{
    print_value(ps, node, LEVEL_XMQ);
}

bool has_leading_ending_quote(const char *start, const char *stop)
{
    return start < stop && ( *start == '\'' || *(stop-1) == '\'');
}

bool has_newlines(const char *start, const char *stop)
{
    for (const char *i = start; i < stop; ++i)
    {
        if (*i == '\n') return true;
    }
    return false;
}

bool has_all_quotes(const char *start, const char *stop)
{
    for (const char *i = start; i < stop; ++i)
    {
        if (*i != '\'') return false;
    }
    return true;
}

bool has_all_whitespace(const char *start, const char *stop, bool *all_space)
{
    *all_space = true;
    for (const char *i = start; i < stop; ++i)
    {
        if (!is_xml_whitespace(*i))
        {
            *all_space = false;
            return false;
        }
        if (*i != ' ')
        {
            *all_space = false;
        }
    }
    return true;
}

void node_strlen_name_prefix(xmlNode *node,
                        const char **name, size_t *name_len,
                        const char **prefix, size_t *prefix_len,
                        size_t *total_len)
{
    *name_len = strlen((const char*)node->name);
    *name = (const char*)node->name;

    if (node->ns && node->ns->prefix)
    {
        *prefix = (const char*)node->ns->prefix;
        *prefix_len = strlen((const char*)node->ns->prefix);
        *total_len = *name_len + *prefix_len +1;
    }
    else
    {
        *prefix = NULL;
        *prefix_len = 0;
        *total_len = *name_len;
    }
    assert(*name != NULL);
}

/**
    str_b_u_len: Count bytes and unicode characters.
    @start:
    @stop
    @b_len:
    @u_len:

    Store the number of actual bytes. Which is stop-start, and strlen(start) if stop is NULL.
    Count actual unicode characters between start and stop (ie all bytes not having the two msb bits set to 0x10xxxxxx).
    This will have to be improved if we want to handle indentation with characters with combining diacritics.
*/
void str_b_u_len(const char *start, const char *stop, size_t *b_len, size_t *u_len)
{
    assert(start);
    if (stop)
    {
        *b_len = stop - start;
        size_t u = 0;
        for (const char *i = start; i < stop; ++i)
        {
            if ((*i & 0xc0) != 0x80) u++;
        }
        *u_len = u;
        return;
    }

    size_t b = 0;
    size_t u = 0;
    for (const char *i = start; *i != 0; ++i)
    {
        if ((*i & 0xc0) != 0x80) u++;
        b++;
    }
    *b_len = b;
    *u_len = u;
}

void attr_strlen_name_prefix(xmlAttr *attr, const char **name, const char **prefix, size_t *total_u_len)
{
    *name = (const char*)attr->name;
    size_t name_b_len;
    size_t name_u_len;
    size_t prefix_b_len;
    size_t prefix_u_len;
    str_b_u_len(*name, NULL, &name_b_len, &name_u_len);

    if (attr->ns && attr->ns->prefix)
    {
        *prefix = (const char*)attr->ns->prefix;
        str_b_u_len(*prefix, NULL, &prefix_b_len, &prefix_u_len);
        *total_u_len = name_u_len + prefix_u_len + 1;
    }
    else
    {
        *prefix = NULL;
        prefix_b_len = 0;
        prefix_u_len = 0;
        *total_u_len = name_u_len;
    }
    assert(*name != NULL);
}

void namespace_strlen_prefix(xmlNs *ns, const char **prefix, size_t *total_u_len)
{
    size_t prefix_b_len;
    size_t prefix_u_len;

    if (ns->prefix)
    {
        *prefix = (const char*)ns->prefix;
        str_b_u_len(*prefix, NULL, &prefix_b_len, &prefix_u_len);
        *total_u_len = /* xmlns */ 5  + prefix_u_len + 1;
    }
    else
    {
        *prefix = NULL;
        prefix_b_len = 0;
        prefix_u_len = 0;
        *total_u_len = /* xmlns */ 5;
    }
}

void element_strlen_name_prefix(xmlNode *element, const char **name, const char **prefix, size_t *total_u_len)
{
    *name = (const char*)element->name;
    if (!*name)
    {
        *name = "";
        *prefix = "";
        *total_u_len = 0;
        return;
    }
    size_t name_b_len;
    size_t name_u_len;
    size_t prefix_b_len;
    size_t prefix_u_len;
    str_b_u_len(*name, NULL, &name_b_len, &name_u_len);

    if (element->ns && element->ns->prefix)
    {
        *prefix = (const char*)element->ns->prefix;
        str_b_u_len(*prefix, NULL, &prefix_b_len, &prefix_u_len);
        *total_u_len = name_u_len + prefix_u_len + 1;
    }
    else
    {
        *prefix = NULL;
        prefix_b_len = 0;
        prefix_u_len = 0;
        *total_u_len = name_u_len;
    }
    assert(*name != NULL);
}

void print_entity_node(XMQPrintState *ps, xmlNode *node)
{
    check_space_before_entity_node(ps);

    print_utf8(ps, COLOR_entity, 1, "&", NULL);
    print_utf8(ps, COLOR_entity, 1, (const char*)node->name, NULL);
    print_utf8(ps, COLOR_entity, 1, ";", NULL);
}

bool contains_newline(const char *start, const char *stop)
{
    for (const char *i = start; i < stop; ++i)
    {
        if (*i == '\n') return true;
    }
    return false;
}

void print_comment_line(XMQPrintState *ps, const char *start, const char *stop, bool compact)
{
    print_utf8(ps, COLOR_comment, 1, start, stop);
}

void print_comment_lines(XMQPrintState *ps, const char *start, const char *stop, bool compact)
{
    const char *i = start;
    const char *line = i;

    size_t num_slashes = count_necessary_slashes(start, stop);

    print_slashes(ps, NULL, "*", num_slashes);
    size_t add_spaces = ps->current_indent + 1 + num_slashes;
    if (!compact) {
        print_white_spaces(ps, 1);
        add_spaces++;
    }

    size_t prev_line_indent = ps->line_indent;
    ps->line_indent = add_spaces;

    for (; i < stop; ++i)
    {
        if (*i == '\n')
        {
            if (line > start) {
                if (compact) {
                    print_slashes(ps, "*", "*", num_slashes);
                }
                else
                {
                    print_nl_and_indent(ps, NULL, NULL);
                }
            }
            print_comment_line(ps, line, i, compact);
            line = i+1;
        }
    }
    if (line == start)
    {
        // No newlines found.
        print_comment_line(ps, line, i, compact);
    }
    else if (line < stop)
    {
        // There is a remaining line that ends with stop and not newline.
        if (line > start) {
            if (compact) {
                print_slashes(ps, "*", "*", num_slashes);
            }
            else
            {
                print_nl_and_indent(ps, NULL, NULL);
            }
        }
        print_comment_line(ps, line, i, compact);
    }
    if (!compact) print_white_spaces(ps, 1);
    print_slashes(ps, "*", NULL, num_slashes);
    ps->last_char = '/';
    ps->line_indent = prev_line_indent;
}

void print_comment_node(XMQPrintState *ps, xmlNode *node)
{
    const char *comment = xml_element_content(node);
    const char *start = comment;
    const char *stop = comment+strlen(comment);

/*    // Remove leading and ending comment whitespace.
    while (start < stop && is_xml_whitespace(*start)) start++;
    while (start < stop && is_xml_whitespace(*(stop-1))) stop--;*/

    check_space_before_comment(ps);

    bool has_newline = contains_newline(start, stop);
    if (!has_newline)
    {
        if (ps->output_settings->compact)
        {
            print_utf8(ps, COLOR_comment, 3, "/*", NULL, start, stop, "*/", NULL);
            ps->last_char = '/';
        }
        else
        {
            print_utf8(ps, COLOR_comment, 2, "// ", NULL, start, stop);
            ps->last_char = 1;
        }
    }
    else
    {
        print_comment_lines(ps, start, stop, ps->output_settings->compact);
        ps->last_char = '/';
    }
}

size_t print_element_name_and_attributes(XMQPrintState *ps, xmlNode *node)
{
    size_t name_len, prefix_len, total_u_len;
    const char *name;
    const char *prefix;

    check_space_before_key(ps);

    node_strlen_name_prefix(node, &name, &name_len, &prefix, &prefix_len, &total_u_len);

    if (prefix)
    {
        print_utf8(ps, COLOR_element_ns, 1, prefix, NULL);
        print_utf8(ps, COLOR_ns_colon, 1, ":", NULL);
    }

    if (is_key_value_node(node) && !xml_first_attribute(node))
    {
        // Only print using key color if = and no attributes.
        // I.e. alfa=1
        print_utf8(ps, COLOR_element_key, 1, name, NULL);
    }
    else
    {
        // All other cases print with node color.
        // I.e. alfa{a b} alfa(x=1)=1
        print_utf8(ps, COLOR_element_name, 1, name, NULL);
    }

    if (xml_first_attribute(node) || xml_first_namespace_def(node))
    {
        print_utf8(ps, COLOR_apar_left, 1, "(", NULL);
        print_attributes(ps, node);
        print_utf8(ps, COLOR_apar_right, 1, ")", NULL);
    }

    return total_u_len;
}

void print_leaf_node(XMQPrintState *ps,
                     xmlNode *node)
{
    print_element_name_and_attributes(ps, node);
}

void print_key_node(XMQPrintState *ps,
                    xmlNode *node,
                    size_t align)
{
    print_element_name_and_attributes(ps, node);

    if (!ps->output_settings->compact)
    {
        size_t len = ps->current_indent - ps->line_indent;
        size_t pad = 1;
        if (len < align) pad = 1+align-len;
        print_white_spaces(ps, pad);
    }
    print_utf8(ps, COLOR_equals, 1, "=", NULL);
    if (!ps->output_settings->compact) print_white_spaces(ps, 1);

    print_value(ps, xml_first_child(node), LEVEL_ELEMENT_VALUE);
}

void print_element_with_children(XMQPrintState *ps,
                                 xmlNode *node,
                                 size_t align)
{
    print_element_name_and_attributes(ps, node);

    void *from = xml_first_child(node);
    void *to = xml_last_child(node);

    check_space_before_opening_brace(ps);
    print_utf8(ps, COLOR_brace_left, 1, "{", NULL);

    ps->line_indent += ps->output_settings->add_indent;

    while (xml_prev_sibling((xmlNode*)from)) from = xml_prev_sibling((xmlNode*)from);
    assert(from != NULL);

    print_nodes(ps, (xmlNode*)from, (xmlNode*)to, align);

    ps->line_indent -= ps->output_settings->add_indent;

    check_space_before_closing_brace(ps);
    print_utf8(ps, COLOR_brace_right, 1, "}", NULL);
}

void print_doctype(XMQPrintState *ps, xmlNode *node)
{
    if (!node) return;

    check_space_before_key(ps);
    print_utf8(ps, COLOR_element_key, 1, "!DOCTYPE", NULL);
    if (!ps->output_settings->compact) print_white_spaces(ps, 1);
    print_utf8(ps, COLOR_equals, 1, "=", NULL);
    if (!ps->output_settings->compact) print_white_spaces(ps, 1);

    xmlBuffer *buffer = xmlBufferCreate();
    xmlNodeDump(buffer, (xmlDocPtr)ps->doq->docptr_.xml, (xmlNodePtr)node, 0, 0);
    char *c = (char*)xmlBufferContent(buffer);
    if (ps->output_settings->compact)
    {
        size_t n = strlen(c);
        char *end = c+n;
        for (char *i = c; i < end; ++i)
        {
            if (*i == '\n') *i = ' ';
        }
    }
    print_value_internal_text(ps, c+10, c+strlen(c)-1, LEVEL_ELEMENT_VALUE);
    xmlBufferFree(buffer);
}

void print_node(XMQPrintState *ps, xmlNode *node, size_t align)
{
    // Standalone quote must be quoted: 'word' 'some words'
    if (is_content_node(node))
    {
        return print_content_node(ps, node);
    }

    // This is an entity reference node. &something;
    if (is_entity_node(node))
    {
        return print_entity_node(ps, node);
    }

    // This is a comment // or /* ... */
    if (is_comment_node(node))
    {
        return print_comment_node(ps, node);
    }

    // This is doctype node.
    if (is_doctype_node(node))
    {
        return print_doctype(ps, node);
    }

    // This is a node with no children, ie br
    if (is_leaf_node(node))
    {
        return print_leaf_node(ps, node);
    }

    // This is a key = value or key = 'value value' node and there are no attributes.
    if (is_key_value_node(node))
    {
        return print_key_node(ps, node, align);
    }

    // All other nodes are printed
    return print_element_with_children(ps, node, align);
}

void xmq_print_xml(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    xmlNodePtr child = doq->docptr_.xml->children;
    xmlBuffer *buffer = xmlBufferCreate();
    while (child != NULL)
    {
        xmlNodeDump(buffer, doq->docptr_.xml, child, 0, 0);
        child = child->next;
    }
    char *c = (char*)xmlBufferContent(buffer);
    fputs(c, stdout);
    xmlBufferFree(buffer);
}

void xmq_print_html(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    xmq_fixup_html_before_writeout(doq);
    xmlNodePtr child = doq->docptr_.xml->children;
    xmlBuffer *buffer = xmlBufferCreate();
    while (child != NULL)
    {
        xmlNodeDump(buffer, doq->docptr_.xml, child, 0, 0);
        child = child->next;
    }
    const char *c = (const char *)xmlBufferContent(buffer);
    fputs(c, stdout);
    xmlBufferFree(buffer);
}

void xmq_print_xmq(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    void *first = doq->docptr_.xml->children;
    if (!doq || !first) return;
    void *last = doq->docptr_.xml->last;

    XMQPrintState ps = {};
    ps.doq = doq;
    if (output_settings->compact) output_settings->escape_newlines = true;
    ps.output_settings = output_settings;
    assert(output_settings->content.write);

    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;
    XMQColoring *coloring = &output_settings->coloring;

    if (coloring->document.pre) write(writer_state, coloring->document.pre, NULL);
    if (coloring->header.pre) write(writer_state, coloring->header.pre, NULL);
    if (coloring->style.pre) write(writer_state, coloring->style.pre, NULL);
    if (coloring->header.post) write(writer_state, coloring->header.post, NULL);
    if (coloring->body.pre) write(writer_state, coloring->body.pre, NULL);

    if (coloring->content.pre) write(writer_state, coloring->content.pre, NULL);
    print_nodes(&ps, (xmlNode*)first, (xmlNode*)last, 0);
    if (coloring->content.post) write(writer_state, coloring->content.post, NULL);

    if (coloring->body.post) write(writer_state, coloring->body.post, NULL);
    if (coloring->document.post) write(writer_state, coloring->document.post, NULL);
}

void xmqPrint(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    if (output_settings->output_format == XMQ_CONTENT_XML)
    {
        xmq_print_xml(doq, output_settings);
        return;
    }

    if (output_settings->output_format == XMQ_CONTENT_HTML)
    {
        xmq_print_html(doq, output_settings);
        return;
    }

    xmq_print_xmq(doq, output_settings);
}

void trim_text_node(xmlNode *node, XMQTrimType tt)
{
    const char *content = xml_element_content(node);
    // We remove any all whitespace node.
    // This ought to have been removed with XML_NOBLANKS alas that does not always happen.
    // Perhaps because libxml thinks that some of these are signficant whitespace.
    //
    // However we cannot really figure out exactly what is significant and what is not from
    // the default trimming. We will over-trim when going from html to htmq unless
    // --trim=none is used.
    if (is_all_xml_whitespace(content))
    {
        xmlUnlinkNode(node);
        xmlFreeNode(node);
        return;
    }
    // This is not entirely whitespace, now use the xmq_un_quote function to remove any incindental indentation.
    // Use indent==0 and space==0 to indicate to the unquote function to assume the the first line indent
    // is the same as the second line indent! This is necessary to gracefully handle all the possible xml indentations.
    const char *start = content;
    const char *stop = start+strlen(start);
    while (start < stop && *start == ' ') start++;
    while (stop > start && *(stop-1) == ' ') stop--;

    char *trimmed = xmq_un_quote(0, 0, start, stop, false);
    if (trimmed[0] == 0)
    {
        xmlUnlinkNode(node);
        xmlFreeNode(node);
        free(trimmed);
        return;
    }
    xmlNodeSetContent(node, (xmlChar*)trimmed);
    free(trimmed);
}

void trim_node(xmlNode *node, XMQTrimType tt)
{
    if (is_content_node(node))
    {
        trim_text_node(node, tt);
        return;
    }

    if (is_comment_node(node))
    {
        trim_text_node(node, tt);
        return;
    }

    xmlNodePtr i = xml_first_child(node);
    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.
        trim_node(i, tt);
        i = next;
    }
}

void xmqTrimWhitespace(XMQDoc *doq, XMQTrimType tt)
{
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    while (i)
    {
        trim_node(i, tt);
        i = xml_next_sibling(i);
    }
}

/*
const char *escape_comments(const char *comment)
{
    size_t len = 0;
    bool needs_escape = false;
    for (const char *i = comment; *i; ++i)
    {
        if (*i == '-' && *(i+1) == '-')
        {
            needs_escape = true;
            break;
        }

    }

    if (!needs_escape) return NULL;

    char *tmp = (char*)malloc(
    return
}
*/
void fixup_html(XMQDoc *doq, xmlNode *node, bool inside_cdata_declared)
{
    /*
    if (node->type == XML_COMMENT_NODE)
    {
        // When writing an xml comment we must replace -- with -␐-.
        // An already existing -␐- is replaced with -␐␐- etc.
        // A leading > (eg //>) is escaped <!--␐>-->
        // A <![CDATA[ ]]> is escaped as <␐![CDATA[ ]]␐>
        const char *buf = escape_xml_comments(node->content)
        xmlNodePtr new_node = xmlNewText(new_content);
        xmlReplaceNode(node, new_node);
        fix_xml_comment(node);
    }
    */
    if (node->type == XML_CDATA_SECTION_NODE)
    {
        // When the html is loaded by the libxml2 parser it creates a cdata
        // node instead of a text node for the style content.
        // If this is allowed to be printed as is, then this creates broken html.
        // I.e. <style><![CDATA[h1{color:red;}]]></style>
        // But we want: <style>h1{color:red;}</style>
        // Workaround until I understand the proper fix, just make it a text node.
        node->type = XML_TEXT_NODE;
    }

    if (is_entity_node(node) && inside_cdata_declared)
    {
        const char *new_content = (const char*)node->content;
        char buf[2];
        if (!node->content)
        {
            if (node->name[0] == '#')
            {
                int v = atoi(((const char*)node->name)+1);
                buf[0] = v;
                buf[1] = 0;
                new_content = buf;
            }
        }
        xmlNodePtr new_node = xmlNewDocText(doq->docptr_.xml, (const xmlChar*)new_content);
        xmlReplaceNode(node, new_node);
        xmlFreeNode(node);
        return;
    }

    xmlNode *i = xml_first_child(node);
    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.

        bool r = inside_cdata_declared;

        if (i->name &&
            (!strcasecmp((const char*)i->name, "style") ||
             !strcasecmp((const char*)i->name, "script")))
        {
            // The html style and script nodes are declared as cdata nodes.
            // The &#10; will not be decoded, instead remain as is a ruin the style.
            // Since htmq does not care about this distinction, we have to remove any
            // quoting specified in the htmq before entering the cdata declared node.
            r = true;
        }

        fixup_html(doq, i, r);
        i = next;
    }
}

void xmq_fixup_html_before_writeout(XMQDoc *doq)
{
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    while (i)
    {
        fixup_html(doq, i, false);
        i = xml_next_sibling(i);
    }
}

const char *xmqDocError(XMQDoc *doq)
{
    return doq->error_;
}

XMQParseError xmqDocErrno(XMQDoc *doq)
{
    return (XMQParseError)doq->errno_;
}

Stack *new_stack()
{
    Stack *s = (Stack*)malloc(sizeof(Stack));
    memset(s, 0, sizeof(Stack));
    return s;
}

void free_stack(Stack *stack)
{
    if (!stack) return;

    StackElement *e = stack->top;
    while (e)
    {
        StackElement *next = e->below;
        free(e);
        e = next;
    }

    free(stack);
}

void push_stack(Stack *stack, void *data)
{
    assert(stack);
    StackElement *element = (StackElement*)malloc(sizeof(StackElement));
    memset(element, 0, sizeof(StackElement));
    element->data = data;

    if (stack->top == NULL) {
        stack->top = stack->bottom = element;
    }
    else
    {
        element->below = stack->top;
        stack->top = element;
    }
    stack->size++;
}

void *pop_stack(Stack *stack)
{
    assert(stack);
    assert(stack->top);

    void *data = stack->top->data;
    StackElement *element = stack->top;
    stack->top = element->below;
    free(element);
    stack->size--;
    return data;
}

void xmqSetStateSourceName(XMQParseState *state, const char *source_name)
{
    if (source_name)
    {
        size_t l = strlen(source_name);
        state->source_name = (char*)malloc(l+1);
        strcpy(state->source_name, source_name);
    }
}

bool is_safe_char(const char *i, const char *stop)
{
    char c = *i;
    return !(count_whitespace(i, stop) > 0 ||
             c == '\n' ||
             c == '(' ||
             c == ')' ||
             c == '\'' ||
             c == '\"' ||
             c == '{' ||
             c == '}' ||
             c == '\t' ||
             c == '\r');
}

bool unsafe_start(char c, char cc)
{
    return c == '=' || c == '&' || (c == '/' && (cc == '/' || cc == '*'));
}

size_t calculate_buffer_size(const char *start, const char *stop, int indent, const char *pre_line, const char *post_line)
{
    size_t pre_n = strlen(pre_line);
    size_t post_n = strlen(post_line);
    const char *o = start;
    for (const char *i = start; i < stop; ++i)
    {
        char c = *i;
        if (c == '\n')
        {
            // Indent the line.
            for (int i=0; i<indent; ++i) o++;
            o--; // Remove the \n
            o += pre_n; // Add any pre line prefixes.
            o += post_n; // Add any post line suffixes (default is 1 for "\n")
        }
        o++;
    }
    return o-start;
}

void copy_and_insert(MemBuffer *mb,
                     const char *start,
                     const char *stop,
                     int num_prefix_spaces,
                     const char *implicit_indentation,
                     const char *explicit_space,
                     const char *newline,
                     const char *prefix_line,
                     const char *postfix_line)
{
    for (const char *i = start; i < stop; ++i)
    {
        char c = *i;
        if (c == '\n')
        {
            membuffer_append_region(mb, postfix_line, NULL);
            membuffer_append_region(mb, newline, NULL);
            membuffer_append_region(mb, prefix_line, NULL);

            // Indent the next line.
            for (int i=0; i<num_prefix_spaces; ++i) membuffer_append_region(mb, implicit_indentation, NULL);
        }
        else if (c == ' ')
        {
            membuffer_append_region(mb, explicit_space, NULL);
        }
        else
        {
            membuffer_append_char(mb, c);
        }
    }
}

char *copy_lines(int num_prefix_spaces,
                 const char *start,
                 const char *stop,
                 int num_quotes,
                 bool add_nls,
                 bool add_compound,
                 const char *implicit_indentation,
                 const char *explicit_space,
                 const char *newline,
                 const char *prefix_line,
                 const char *postfix_line)
{
    MemBuffer *mb = new_membuffer();

    const char *short_start = start;
    const char *short_stop = stop;

    if (add_compound)
    {
        membuffer_append(mb, "( ");

        short_start = has_leading_nl_whitespace(start, stop);
        if (!short_start) short_start = start;
        short_stop = has_ending_nl_whitespace(start, stop);
        if (!short_stop || short_stop == start) short_stop = stop;

        const char *i = start;

        while (i < short_start)
        {
            membuffer_append_entity(mb, *i);
            i++;
        }
    }

    for (int i = 0; i < num_quotes; ++i) membuffer_append_char(mb, '\'');
    membuffer_append_region(mb, prefix_line, NULL);
    if (add_nls)
    {
        membuffer_append_region(mb, postfix_line, NULL);
        membuffer_append_region(mb, newline, NULL);
        membuffer_append_region(mb, prefix_line, NULL);
        for (int i = 0; i < num_prefix_spaces; ++i) membuffer_append_region(mb, implicit_indentation, NULL);
    }
    // Copy content into quote.
    copy_and_insert(mb, short_start, short_stop, num_prefix_spaces, implicit_indentation, explicit_space, newline, prefix_line, postfix_line);
    // Done copying content.

    if (add_nls)
    {
        membuffer_append_region(mb, postfix_line, NULL);
        membuffer_append_region(mb, newline, NULL);
        membuffer_append_region(mb, prefix_line, NULL);
        for (int i = 0; i < num_prefix_spaces; ++i) membuffer_append_region(mb, implicit_indentation, NULL);
    }

    membuffer_append_region(mb, postfix_line, NULL);
    for (int i = 0; i < num_quotes; ++i) membuffer_append_char(mb, '\'');

    if (add_compound)
    {
        const char *i = short_stop;

        while (i < stop)
        {
            membuffer_append_entity(mb, *i);
            i++;
        }

        membuffer_append(mb, " )");
    }

    membuffer_append_null(mb);

    return free_membuffer_but_return_trimmed_content(mb);
}

size_t line_length(const char *start, const char *stop, int *numq, int *lq, int *eq)
{
    const char *i = start;
    int llq = 0, eeq = 0;
    int num = 0, max = 0;
    // Skip line leading quotes
    while (*i == '\'') { i++; llq++;  }
    const char *lstart = i; // Points to text after leading quotes.
    // Find end of line.
    while (i < stop && *i != '\n') i++;
    const char *eol = i;
    i--;
    while (i > lstart && *i == '\'') { i--; eeq++; }
    i++;
    const char *lstop = i;
    // Mark endof text inside ending quotes.
    for (i = lstart; i < lstop; ++i)
    {
        if (*i == '\'')
        {
            num++;
            if (num > max) max = num;
        }
        else
        {
            num = 0;
        }
    }
    *numq = max;
    *lq = llq;
    *eq = eeq;
    assert( (llq+eeq+(lstop-lstart)) == eol-start);
    return lstop-lstart;
}

/**
    count_necessary_quotes:
    @start: Points to first byte of memory buffer to scan for quotes.
    @stop:  Points to byte after memory buffer.
    @forbid_nl: Compact mode.
    @add_nls: Returns whether we need leading and ending newlines.
    @add_compound: Compounds ( ) is necessary.

    Scan the content to determine how it must be quoted, or if the content can
    remain as text without quotes. Return 0, nl_begin=nl_end=false for safe text.
    Return 1,3 or more for unsafe text with at most a single quote '.
    If forbid_nl is true, then we are generating xmq on a single line.
    Set add_nls to true if content starts or end with a quote and forbid_nl==false.
    Set add_compound to true if content starts or ends with spaces/newlines or if forbid_nl==true and
    content starts/ends with quotes.
*/
size_t count_necessary_quotes(const char *start, const char *stop, bool forbid_nl, bool *add_nls, bool *add_compound)
{
    size_t max = 0;
    size_t curr = 0;
    bool all_safe = true;

    assert(stop > start);

    if (unsafe_start(*start, start+1 < stop ? *(start+1):0))
    {
        // Content starts with = & // or /* so it must be quoted.
        all_safe = false;
    }

    if (*start == '\'' || *(stop-1) == '\'')
    {
        // If leading or ending quote, then add newlines both at the beginning and at the end.
        // Strictly speaking, if only a leading quote, then only newline at beginning is needed.
        // However to reduce visual confusion, we add newlines at beginning and end.
        if (!forbid_nl)
        {
            *add_nls = true;
        }
        else
        {
            *add_compound = true;
        }
    }

    if (begins_with_spaces_or_tabs_then_nl(start, stop) || ends_with_nl_then_sp_tb_cr(start, stop))
    {
        // Leading ending ws + nl, nl + ws will be trimmed, so we need a compound and entities.
        *add_compound = true;
    }

    for (const char *i = start; i < stop; ++i)
    {
        char c = *i;
        if (c == '\'')
        {
            curr++;
            if (curr > max) max = curr;
        }
        else
        {
            curr = 0;
            all_safe &= is_safe_char(i, stop);
        }
    }
    // We found 3 quotes, thus we need 4 quotes to quote them.
    if (max > 0) max++;
    // Content contains no quotes ', but has unsafe chars, a single quote is enough.
    if (max == 0 && !all_safe) max = 1;
    // Content contains two sequential '' quotes, must bump nof required quotes to 3.
    // Since two quotes means the empty string.
    if (max == 2) max = 3;
    return max;
}

/**
    count_necessary_slashes:
    @start: Start of buffer in which to count the slashes.
    @stop: Points to byte after buffer.

    Scan the comment to determine how it must be commented.
    If the comment contains asterisk plus slashes, then find the max num
    slashes after an asterisk. The returned value is 1 + this max.
*/
size_t count_necessary_slashes(const char *start, const char *stop)
{
    int max = 0;
    int curr = 0;
    bool counting = false;

    for (const char *i = start; i < stop; ++i)
    {
        char c = *i;
        if (counting)
        {
            if (c == '/')
            {
                curr++;
                if (curr > max) max = curr;
            }
            else
            {
                counting = false;
            }
        }

        if (!counting)
        {
            if (c == '*')
            {
                counting = true;
                curr = 0;
            }
        }
    }
    return max+1;
}

char *xmq_quote_with_entity_newlines(const char *start, const char *stop, XMQQuoteSettings *settings)
{
    // This code must only be invoked if there is at least one newline inside the to-be quoted text!
    InternalBuffer ib;
    size_t l = stop-start;
    new_buffer(&ib, l*2);

    const char *i = start;
    bool found_nl = false;
    while (i < stop)
    {
        int numq;
        int lq = 0;
        int eq = 0;
        size_t line_len = line_length(i, stop, &numq, &lq, &eq);
        i += lq;
        for (int j = 0; j < lq; ++j) append_buffer(&ib, "&#39;", NULL);
        if (line_len > 0)
        {
            if (numq == 0 && (settings->force)) numq = 1; else numq++;
            if (numq == 2) numq++;
            for (int i=0; i<numq; ++i) append_buffer(&ib, "'", NULL);
            append_buffer(&ib, i, i+line_len);
            for (int i=0; i<numq; ++i) append_buffer(&ib, "'", NULL);
        }
        for (int j = 0; j < eq; ++j) append_buffer(&ib, "&#39;", NULL);
        i += line_len+eq;
        if (i < stop && *i == '\n')
        {
            if (!found_nl) found_nl = true;
            append_buffer(&ib, "&#10;", NULL);
            i++;
        }
    }
    trim_buffer(&ib);
    return ib.buf;
}

char *xmq_quote_default(int indent,
                        const char *start,
                        const char *stop,
                        XMQQuoteSettings *settings)
{
    bool add_nls = false;
    bool add_compound = false;
    int numq = count_necessary_quotes(start, stop, false, &add_nls, &add_compound);

    if (numq > 0)
    {
        // If nl_begin is true and we have quotes, then we have to forced newline already due to quotes at
        // the beginning or end, therefore we use indent as is, however if
        if (add_nls == false) // then we might have to adjust the indent, or even introduce a nl_begin/nl_end.
        {
            if (indent == -1)
            {
                // Special case, -1 indentation requested this means the quote should be on its own line.
                // then we must insert newlines.
                // Otherwise the indentation will be 1.
                // e.g.
                // |'
                // |alfa beta
                // |gamma delta
                // |'
                add_nls = true;
                indent = 0;
            }
            else
            {
                // We have a nonzero indentation and number of quotes is 1 or 3.
                // Then the actual source indentation will be +1 or +3.
                if (numq < 4)
                {
                    // e.g. quote at 4 will have source at 5.
                    // |    'alfa beta
                    // |     gamma delta'
                    // e.g. quote at 4 will have source at 7.
                    // |    '''alfa beta
                    // |       gamma delta'
                    indent += numq;
                }
                else
                {
                    // More than 3 quotes, then we add newlines.
                    // e.g. quote at 4 will have source at 4.
                    // |    ''''
                    // |    alfa beta '''
                    // |    gamma delta
                    // |    ''''
                    add_nls = true;
                }
            }
        }
    }
    if (numq == 0 && settings->force) numq = 1;
    return copy_lines(indent,
                      start,
                      stop,
                      numq,
                      add_nls,
                      add_compound,
                      settings->indentation_space,
                      settings->explicit_space,
                      settings->explicit_nl,
                      settings->prefix_line,
                      settings->postfix_line);
}

void print_quote_lines_and_color_uwhitespace(XMQPrintState *ps,
                                             XMQColor c,
                                             const char *start,
                                             const char *stop)
{
    XMQWrite write = ps->output_settings->content.write;
    void *writer_state = ps->output_settings->content.writer_state;
    XMQColoring *coloring = &ps->output_settings->coloring;

    const char *pre, *post;
    get_color(coloring, c, &pre, &post);

    if (pre) write(writer_state, pre, NULL);

    const char *old_restart_line = ps->restart_line;
    if (!post) ps->restart_line = pre;
    else ps->restart_line = NULL;

    for (const char *i = start; i < stop;)
    {
        if (*i == '\n')
        {
            print_nl_and_indent(ps, pre, post);
            i++;
        }
        else
        {
            i += print_utf8_char(ps, i, stop);
        }
    }
    if (*(stop-1) != '\n' && post) write(writer_state, post, NULL);
    ps->restart_line = old_restart_line;
}

void print_quote(XMQPrintState *ps,
                 XMQColor c,
                 const char *start,
                 const char *stop)
{
    bool force = true;
    bool add_nls = false;
    bool add_compound = false;
    int numq = count_necessary_quotes(start, stop, false, &add_nls, &add_compound);
    size_t indent = ps->current_indent;

    if (numq > 0)
    {
        // If nl_begin is true and we have quotes, then we have to forced newline already due to quotes at
        // the beginning or end, therefore we use indent as is, however if
        if (add_nls == false) // then we might have to adjust the indent, or even introduce a nl_begin/nl_end.
        {
            if (indent == (size_t)-1)
            {
                // Special case, -1 indentation requested this means the quote should be on its own line.
                // then we must insert newlines.
                // Otherwise the indentation will be 1.
                // e.g.
                // |'
                // |alfa beta
                // |gamma delta
                // |'
                add_nls = true;
                indent = 0;
            }
            else
            {
                // We have a nonzero indentation and number of quotes is 1 or 3.
                // Then the actual source indentation will be +1 or +3.
                if (numq < 4)
                {
                    // e.g. quote at 4 will have source at 5.
                    // |    'alfa beta
                    // |     gamma delta'
                    // e.g. quote at 4 will have source at 7.
                    // |    '''alfa beta
                    // |       gamma delta'
                    indent += numq;
                }
                else
                {
                    // More than 3 quotes, then we add newlines.
                    // e.g. quote at 4 will have source at 4.
                    // |    ''''
                    // |    alfa beta '''
                    // |    gamma delta
                    // |    ''''
                    add_nls = true;
                }
            }
        }
    }
    if (numq == 0 && force) numq = 1;

    print_quotes(ps, numq, c);

    if (add_nls)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }

    size_t old_line_indent = ps->line_indent;
    ps->line_indent = ps->current_indent;

    print_quote_lines_and_color_uwhitespace(ps,
                                            c,
                                            start,
                                            stop);

    ps->line_indent = old_line_indent;

    if (add_nls)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }

    print_quotes(ps, numq, c);

}

const char *find_next_line_end(XMQPrintState *ps, const char *start, const char *stop)
{
    const char *i = start;

    while (i < stop)
    {
        int c = (int)((unsigned char)*i);
        if (c == '\n') break;
        i++;
    }

    return i;
}

const char *find_next_char_that_needs_escape(XMQPrintState *ps, const char *start, const char *stop)
{
    bool newlines = ps->output_settings->escape_newlines;
    bool non7bit = ps->output_settings->escape_non_7bit;

    if (!newlines && !non7bit) return stop;

    const char *i = start;

    while (i < stop)
    {
        int c = (int)((unsigned char)*i);
        if (newlines && c == '\n') break;
        if (non7bit && c > 126) break;
        i++;
    }

    return i;
}

void print_value_internal_text(XMQPrintState *ps, const char *start, const char *stop, Level level)
{
    if (!stop) stop = start+strlen(start);
    if (!start || start >= stop || start[0] == 0)
    {
        // This is for empty attribute values.
        // Empty elements do not have print_vale invoked so there is no: = ''
        check_space_before_quote(ps, level);
        print_utf8(ps, level_to_quote_color(level), 1, "''", NULL);
        return;
    }

    if (has_all_quotes(start, stop))
    {
        // A text with all single quotes, print using &apos; only.
        // We could also be quoted using n+1 more quotes and newlines, but it seems a bit annoying:
        // ''''''
        // '''''
        // ''''''
        // compared to &apos;&apos;&apos;&apos;
        // The &apos; solution takes a little bit more space, but works for compact too,
        // lets use that for both normal and compact formatting.
        check_space_before_entity_node(ps);
        for (const char *i = start; i < stop; ++i)
        {
            print_utf8(ps, level_to_entity_color(level), 1, "&apos;", NULL);
        }
        return;
    }

    bool all_space = false;
    bool all_whitespace = has_all_whitespace(start, stop, &all_space);

    if (all_space)
    {
        // This are all normal ascii 32 spaces. Print like: '     '
        check_space_before_quote(ps, level);
        print_quoted_spaces(ps, level_to_quote_color(level), (size_t)(stop-start));
        return;
    }

    if (all_whitespace)
    {
        // All whitespace, but more than just normal spaces, ie newlines!
        // This is often the case with trimmed whitespace, lets print using
        // entities, which makes this content be easy to spot when --trim=none is used.
        // Also works both for normal and compact mode.
        print_all_whitespace(ps, start, stop, level);
        return;
    }

    if (is_xmq_text_value(start, stop) && (level == LEVEL_ELEMENT_VALUE || level == LEVEL_ATTR_VALUE))
    {
        // This is a key_node text value or an attribute text value, ie key = 123 or color=blue, ie no quoting needed.
        print_utf8(ps, level_to_quote_color(level), 1, start, stop);
        return;
    }

    const char *new_start = has_leading_nl_whitespace(start, stop);
    if (new_start)
    {
        print_all_whitespace(ps, start, new_start, level);
        start = new_start;
    }

    const char *new_stop = has_ending_nl_whitespace(start, stop);
    const char *old_stop = stop;
    if (new_stop)
    {
        stop = new_stop;
    }

    // Ok, normal content to be quoted. However we might need to split the content
    // at chars that need to be replaced withcharacter entities. Normally no
    // chars need to be replaced. But in compact mode, the \n newlines are replaced with &#10;
    // Also one can replace all non-ascii chars with their entities if so desired.
    bool compact = ps->output_settings->compact;
    for (const char *from = start; from < stop; )
    {
        const char *to = find_next_char_that_needs_escape(ps, from, stop);
        if (from == to)
        {
            char c = *from;
            check_space_before_entity_node(ps);
            to += print_char_entity(ps, level_to_entity_color(level), from, stop);
            if (c == '\n' && !compact)
            {
                print_nl_and_indent(ps, NULL, NULL);
            }
        }
        else
        {
            check_space_before_quote(ps, level);
            print_quote(ps, level_to_quote_color(level), from, to);
        }
        from = to;
    }
    if (new_stop)
    {
        print_all_whitespace(ps, new_stop, old_stop, level);
    }
}

/**
   print_value:
   @ps: Print state.
   @node: Text node to be printed.
   @level: Printing node, key_value, kv_compound, attr_value, av_compound

   Print content as:
   EMPTY_: ''
   ENTITY: &#10;
   QUOTES: ( &apos;&apos; )
   WHITSP: ( &#32;&#32;&#10;&#32;&#32; )
   SPACES: '      '
   TEXT  : /root/home/foo&123
   QUOTE : 'x y z'
   QUOTEL: 'xxx
            yyy'
*/
void print_value_internal(XMQPrintState *ps, xmlNode *node, Level level)
{
    if (node->type == XML_ENTITY_REF_NODE ||
        node->type == XML_ENTITY_NODE)
    {
        print_entity_node(ps, node);
        return;
    }

    print_value_internal_text(ps, xml_element_content(node), NULL, level);
}

/**
   quote_needs_compounded:
   @ps: The print state.
   @start: Content buffer start.
   @stop: Points to after last buffer byte.

   Used to determine early if the quote needs to be compounded.
*/
bool quote_needs_compounded(XMQPrintState *ps, const char *start, const char *stop)
{
    if (stop == start+1 && *start == '\'') return false;
    if (has_leading_ending_quote(start, stop)) return true;
    if (has_leading_nl_whitespace(start, stop)) return true;
    if (has_ending_nl_whitespace(start, stop)) return true;
    if (ps->output_settings->compact && has_newlines(start, stop)) return true;

    bool newlines = ps->output_settings->escape_newlines;
    bool non7bit = ps->output_settings->escape_non_7bit;

    for (const char *i = start; i < stop; ++i)
    {
        int c = (int)(unsigned char)(*i);
        if (c == '\t') return true;
        if (newlines && (c == '\n' || c == '\r')) return true;
        if (non7bit && c > 126) return true;
    }
    return false;
}

/**
    print_attribute_value:

    Take the input and create an indented xmq quote using the XMQQuoteSettings.

    Supply indent 0 to place the starting quote ' char first on the line.
    Supply indent is 1 to get a single space before the starting quote ' char.
    As a special case, if you want the content to have 0 indent, use indent = -1.
*/
void print_attribute_value(XMQPrintState *ps, xmlAttr *attr)
{
}

void print_value(XMQPrintState *ps, xmlNode *node, Level level)
{
    // Check if there are more than one part, if so the value has to be compounded.
    bool is_compound = level != LEVEL_XMQ && node != NULL && node->next != NULL;

    // Check if the single part will split into multiple parts and therefore needs to be compounded.
    if (!is_compound && node && !is_entity_node(node) && level != LEVEL_XMQ)
    {
        // Check if there are leading ending quotes/whitespace. But also
        // if compact output and there are newlines inside.
        const char *start = xml_element_content(node);
        const char *stop = start+strlen(start);
        is_compound = quote_needs_compounded(ps, start, stop);
    }

    size_t old_line_indent = ps->line_indent;

    if (is_compound)
    {
        level = enter_compound_level(level);
        print_utf8(ps, COLOR_cpar_left, 1, "(", NULL);
        if (!ps->output_settings->compact) print_white_spaces(ps, 1);
        ps->line_indent = ps->current_indent;
    }

    for (xmlNode *i = node; i; i = xml_next_sibling(i))
    {
        print_value_internal(ps, i, level);
        if (level == LEVEL_XMQ) break;
    }

    if (is_compound)
    {
        if (!ps->output_settings->compact) print_white_spaces(ps, 1);
        print_utf8(ps, COLOR_cpar_right, 1, ")", NULL);
    }

    ps->line_indent = old_line_indent;
}

/**
    xmq_comment:

    Make a single line or multi line comment. Support compact mode with multiple line comments.
*/
char *xmq_comment(int indent, const char *start, const char *stop, XMQQuoteSettings *settings)
{
    assert(indent >= 0);
    assert(start);

    if (stop == NULL) stop = start+strlen(start);

    if (settings->compact)
    {
        return xmq_quote_with_entity_newlines(start, stop, settings);
    }

    return xmq_quote_default(indent, start, stop, settings);
}

void new_buffer(InternalBuffer *ib, size_t l)
{
    ib->buf = (char*)malloc(l);
    ib->size = l;
    ib->used = 0;
}

void free_buffer(InternalBuffer *ib)
{
    if (ib->buf) free(ib->buf);
    ib->buf = NULL;
    ib->size = 0;
    ib->used = 0;
}

void append_buffer(InternalBuffer *ib, const char *start, const char *stop)
{
    assert(ib->buf);
    if (stop == NULL) stop = start+strlen(start);
    size_t l = stop-start;
    if ((ib->used + l) > ib->size)
    {
        // Oups! Does not fit. Add size is same as current size, ie double the size.
        size_t as;
        if (ib->size > 1024*1024) as = 1024*1024; // But we top out doubling at 1MiB.
        else as = ib->size;
        if (as < l) as = l*2; // Oups, not enough.
        ib->size += as;
        ib->buf = (char*)realloc(ib->buf, ib->size);
    }
    memcpy(ib->buf+ib->used, start, l);
    ib->used += l;
}

void trim_buffer(InternalBuffer *ib)
{
    if (ib->size > ib->used)
    {
        ib->size = ib->used+1;
        ib->buf = (char*)realloc(ib->buf, ib->size);
        ib->buf[ib->size-1] = 0;
    }
}

xmlNode *xml_first_child(xmlNode *node)
{
    return node->children;
}

xmlNode *xml_last_child(xmlNode *node)
{
    return node->last;
}

xmlNode *xml_next_sibling(xmlNode *node)
{
    return node->next;
}

xmlNode *xml_prev_sibling(xmlNode *node)
{
    return node->prev;
}

xmlAttr *xml_first_attribute(xmlNode *node)
{
    return node->properties;
}

xmlAttr *xml_next_attribute(xmlAttr *attr)
{
    return attr->next;
}

xmlNs *xml_first_namespace_def(xmlNode *node)
{
    return node->nsDef;
}

xmlNs *xml_next_namespace_def(xmlNs *ns)
{
    return ns->next;
}

const char*xml_element_name(xmlNode *node)
{
    return (const char*)node->name;
}

const char*xml_element_content(xmlNode *node)
{
    return (const char*)node->content;
}

const char *xml_element_ns_prefix(const xmlNode *node)
{
    if (!node->ns) return NULL;
    return (const char*)node->ns->prefix;
}

const char *xml_attr_key(xmlAttr *attr)
{
    return (const char*)attr->name;
}

const char* xml_namespace_href(xmlNs *ns)
{
    return (const char*)ns->href;
}

bool is_entity_node(const xmlNode *node)
{
    return node->type == XML_ENTITY_NODE ||
        node->type == XML_ENTITY_REF_NODE;
}

bool is_content_node(const xmlNode *node)
{
    return node->type == XML_TEXT_NODE ||
        node->type == XML_CDATA_SECTION_NODE;
}

bool is_comment_node(const xmlNode *node)
{
    return node->type == XML_COMMENT_NODE;
}

bool is_doctype_node(const xmlNode *node)
{
    return node->type == XML_DTD_NODE;
}

bool is_element_node(const xmlNode *node)
{
    return node->type == XML_ELEMENT_NODE;
}

bool is_key_value_node(xmlNodePtr node)
{
    void *from = xml_first_child(node);
    void *to = xml_last_child(node);

    return from && from == to && (is_content_node((xmlNode*)from) || is_entity_node((xmlNode*)from));
}

bool is_leaf_node(xmlNode *node)
{
    return xml_first_child(node) == NULL;
}

bool has_attributes(xmlNodePtr node)
{
    return NULL == xml_first_attribute(node);
}

int xmqForeach(XMQDoc *doq, XMQNode *xmq_node, const char *xpath, XMQNodeCallback cb, void *user_data)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(doq);
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) return 0;

    if (xmq_node && xmq_node->node)
    {
        xmlXPathSetContextNode(xmq_node->node, ctx);
    }

    xmlXPathObjectPtr objects = xmlXPathEvalExpression((const xmlChar*)xpath, ctx);

    if (objects == NULL)
    {
        xmlXPathFreeContext(ctx);
        return 0;
    }

    xmlNodeSetPtr nodes = objects->nodesetval;
    int size = (nodes) ? nodes->nodeNr : 0;

    if (cb)
    {
        for(int i = 0; i < size; i++)
        {
            xmlNodePtr node = nodes->nodeTab[i];
            XMQNode xn;
            xn.node = node;
            XMQProceed proceed = cb(doq, &xn, user_data);
            if (proceed == XMQ_STOP) break;
        }
    }

    xmlXPathFreeObject(objects);
    xmlXPathFreeContext(ctx);

    return size;
}

const char *xmqGetName(XMQNode *node)
{
    xmlNodePtr p = node->node;
    if (p)
    {
        return (const char*)p->name;
    }
    return NULL;
}

const char *xmqGetContent(XMQNode *node)
{
    xmlNodePtr p = node->node;
    if (p && p->children)
    {
        return (const char*)p->children->content;
    }
    return NULL;
}

XMQProceed catch_single_content(XMQDoc *doc, XMQNode *node, void *user_data)
{
    const char **out = (const char **)user_data;
    xmlNodePtr n = node->node;
    if (n && n->children)
    {
        *out = (const char*)n->children->content;
    }
    else
    {
        *out = NULL;
    }
    return XMQ_STOP;
}

int32_t xmqGetInt(XMQDoc *doq, XMQNode *node, const char *xpath)
{
    const char *content = NULL;

    xmqForeach(doq, node, xpath, catch_single_content, (void*)&content);

    if (!content) return 0;

    if (content[0] == '0' &&
        content[1] == 'x')
    {
        int64_t tmp = strtol(content, NULL, 16);
        return tmp;
    }

    if (content[0] == '0')
    {
        int64_t tmp = strtol(content, NULL, 8);
        return tmp;
    }

    return atoi(content);
}

int64_t xmqGetLong(XMQDoc *doq, XMQNode *node, const char *xpath)
{
    const char *content = NULL;

    xmqForeach(doq, node, xpath, catch_single_content, (void*)&content);

    if (!content) return 0;

    if (content[0] == '0' &&
        content[1] == 'x')
    {
        int64_t tmp = strtol(content, NULL, 16);
        return tmp;
    }

    if (content[0] == '0')
    {
        int64_t tmp = strtol(content, NULL, 8);
        return tmp;
    }

    return atol(content);
}

const char *xmqGetString(XMQDoc *doq, XMQNode *node, const char *xpath)
{
    const char *content = NULL;

    xmqForeach(doq, node, xpath, catch_single_content, (void*)&content);

    return content;
}

double xmqGetDouble(XMQDoc *doq, XMQNode *node, const char *xpath)
{
    const char *content = NULL;

    xmqForeach(doq, node, xpath, catch_single_content, (void*)&content);

    if (!content) return 0;

    return atof(content);
}

char equals[] = "=";
char underline[] = "_";
char leftpar[] = "(";
char rightpar[] = ")";
char array[] = "A";
char boolean[] = "B";
char number[] = "N";

bool is_json_quote_start(char c)
{
    return c == '"';
}

size_t eat_json_quote(XMQParseState *state, const char **content_start, const char **content_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    increment('"', 1, &i, &line, &col);
    *content_start = i;

    while (i < end)
    {
        char c = *i;
        if (c == '\\')
        {
            increment(c, 1, &i, &line, &col);
            c = *i;
            if (c == '"' || c == '\\' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't')
            {
                increment(c, 1, &i, &line, &col);
                continue;
            }
            else if (c == 'u')
            {
                increment(c, 1, &i, &line, &col);
                c = *i;
                if (i+3 < end)
                {
                    if (is_hex(*(i+0)) && is_hex(*(i+1)) && is_hex(*(i+2)) && is_hex(*(i+3)))
                    {
                        increment(c, 1, &i, &line, &col);
                        increment(c, 1, &i, &line, &col);
                        increment(c, 1, &i, &line, &col);
                        increment(c, 1, &i, &line, &col);
                        continue;
                    }
                }
            }
            state->error_nr = XMQ_ERROR_JSON_INVALID_ESCAPE;
            longjmp(state->error_handler, 1);
        }
        if (c == '"')
        {
            increment(c, 1, &i, &line, &col);
            break;
        }

        increment(c, 1, &i, &line, &col);
    }
    state->i = i;
    state->line = line;
    state->col = col;

    return 1;
}

void handle_json_whitespace(XMQParseState *state)
{
    size_t start_line = state->line;
    size_t start_col = state->col;
    const char *start;
    const char *stop;
    eat_whitespace(state, &start, &stop);
    DO_CALLBACK(whitespace, state, start_line, start_col, start, start_col, start, stop, stop);
}

void handle_json_quote(XMQParseState *state)
{
    /*
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *content_start;
    const char *content_stop;

    size_t depth = eat_json_quote(state, &content_start, &content_stop);
    const char *stop = state->i;
    size_t content_start_col = start_col+depth;
    //DO_CALLBACK(content_quote, state, start_line, start_col, start, content_start_col, content_start, content_stop, stop);
    */
}

bool is_json_boolean(XMQParseState *state)
{
    /*
    const char *i = state->i;
    const char *stop = state->buffer_stop;

    if (i+4 <= stop && !strncmp("true", i, 4)) return true;
    if (i+5 <= stop && !strncmp("false", i, 5)) return true;
    return false;
    */
    return false;
}

void eat_json_boolean(XMQParseState *state, const char **content_start, const char **content_stop)
{
    /*
    const char *i = state->i;
    const char *stop = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    if (*i == 't')
    {
        increment('t', 1, &i, &line, &col);
        increment('r', 1, &i, &line, &col);
        increment('u', 1, &i, &line, &col);
        increment('e', 1, &i, &line, &col);
    }
    else
    {
        increment('f', 1, &i, &line, &col);
        increment('a', 1, &i, &line, &col);
        increment('l', 1, &i, &line, &col);
        increment('s', 1, &i, &line, &col);
        increment('e', 1, &i, &line, &col);
    }

    state->i = i;
    state->line = line;
    state->col = col;
    */
}

void handle_json_boolean(XMQParseState *state)
{
    /*
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *content_start;
    const char *content_stop;

    eat_json_boolean(state, &content_start, &content_stop);
    const char *stop = state->i;
    //DO_CALLBACK(element_value_text, state, start_line, start_col, start, start_col, content_start, content_stop, stop);
    */
}

bool is_json_number(XMQParseState *state)
{
    char c = *state->i;

    return c >= '0' && c <='9';
}

void eat_json_number(XMQParseState *state, const char **content_start, const char **content_stop)
{
    const char *i = state->i;
    const char *stop = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    while (i < stop)
    {
        char c = *i;
        if (! (c >= '0' && c <= '9')) break;
        increment(c, 1, &i, &line, &col);
    }

    state->i = i;
    state->line = line;
    state->col = col;
}

void handle_json_number(XMQParseState *state)
{
    /*
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    const char *content_start;
    const char *content_stop;

    eat_json_number(state, &content_start, &content_stop);
    const char *stop = state->i;
    //DO_CALLBACK(element_value_text, state, start_line, start_col, start, start_col, content_start, content_stop, stop);
    */
}

bool xmq_tokenize_buffer_json(XMQParseState *state, const char *start, const char *stop)
{
    if (state->magic_cookie != MAGIC_COOKIE)
    {
        PRINT_ERROR("Parser state not initialized!\n");
        assert(0);
        exit(1);
    }

    state->buffer_start = start;
    state->buffer_stop = stop;
    state->i = start;
    state->line = 1;
    state->col = 1;
    state->error_nr = 0;

    if (state->parse->init) state->parse->init(state);

    if (!setjmp(state->error_handler))
    {
        //handle_json_nodes(state);
        if (state->i < state->buffer_stop)
        {
            state->error_nr = XMQ_ERROR_UNEXPECTED_CLOSING_BRACE;
            longjmp(state->error_handler, 1);
        }
    }
    else
    {
        // Error detected
        PRINT_ERROR("Error while parsing json (errno %d) %s %zu:%zu\n", state->error_nr, state->generated_error_msg, state->line, state->col);
    }

    if (state->parse->done) state->parse->done(state);
    return true;
}

bool xmq_parse_buffer_json(XMQDoc *doq, const char *start, const char *stop)
{
    XMQOutputSettings *output_settings = xmqNewOutputSettings();
    XMQParseCallbacks *parse = xmqNewParseCallbacks();
//    xmq_setup_parse_json_callbacks(parse);

    XMQParseState *state = xmqNewParseState(parse, output_settings);
    state->doq = doq;

    xmlNodePtr root = xmlNewDocNode(state->doq->docptr_.xml, NULL, (const xmlChar *)("_"), NULL);
    push_stack(state->element_stack, root);
    xmlDocSetRootElement(state->doq->docptr_.xml, root);
    state->element_last = state->element_stack->top->data;

    // Tokenize the buffer and invoke the parse callbacks.
    xmqTokenizeBuffer(state, start, stop);

    xmqFreeParseState(state);
    xmqFreeParseCallbacks(parse);
    xmqFreeOutputSettings(output_settings);

    return true;
}

void handle_json_array(XMQParseState *state)
{
    char c = *state->i;
    assert(c == '[');
    increment(c, 1, &state->i, &state->line, &state->col);

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, underline, state->col, underline, underline+1, underline+1);
    DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, state->col, leftpar, leftpar+1, leftpar+1);
    DO_CALLBACK_SIM(attr_key, state, state->line, state->col, array, state->col, array, array+1, array+1);
    DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, state->col, rightpar, rightpar+1, rightpar+1);

    //handle_json_nodes(state);

    c = *state->i;
    assert(c == ']');
    increment(c, 1, &state->i, &state->line, &state->col);
}

void handle_json_nodes(XMQParseState *state)
{
    const char *stop = state->buffer_stop;

    while (state->i < stop)
    {
        char c = *(state->i);

        if (is_xml_whitespace(c)) handle_json_whitespace(state);
        else if (is_json_quote_start(c)) handle_json_quote(state);
        else if (is_json_boolean(state)) handle_json_boolean(state);
        else if (is_json_number(state)) handle_json_number(state);
        else if (c == '[') handle_json_array(state);
        else if (c == ']') break;
        else
        {
            state->error_nr = XMQ_ERROR_JSON_INVALID_CHAR;
            longjmp(state->error_handler, 1);
        }
    }
}

void handle_json_node(XMQParseState *state)
{
    /*
    char c = 0;
    const char *name = "_";
    const char *name_start = name;
    const char *name_stop = name+1;

    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;
    eat_xmq_text_name(state, &name_start, &name_stop);
    const char *stop = state->i;
    */
}

bool xmq_parse_buffer_xml(XMQDoc *doq, const char *start, const char *stop, XMQTrimType tt)
{
    xmlDocPtr doc;

    /* Macro to check API for match with the DLL we are using */
    LIBXML_TEST_VERSION ;

    int parse_options = XML_PARSE_NOCDATA | XML_PARSE_NONET;
    if (tt != XMQ_TRIM_NONE) parse_options |= XML_PARSE_NOBLANKS;

    doc = xmlReadMemory(start, stop-start, "foof", NULL, parse_options);
    if (doc == NULL)
    {
        PRINT_ERROR("Document not parsed successfully.\n");
        return 0;
    }

    if (doq->docptr_.xml)
    {
        xmlFreeDoc(doq->docptr_.xml);
    }
    doq->docptr_.xml = doc;
    xmlCleanupParser();

    return true;
}

bool xmq_parse_buffer_html(XMQDoc *doq, const char *start, const char *stop, XMQTrimType tt)
{
    htmlDocPtr doc;
    xmlNode *roo_element = NULL;

    /* Macro to check API for match with the DLL we are using */
    LIBXML_TEST_VERSION

    int parse_options = HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
    if (tt != XMQ_TRIM_NONE) parse_options |= HTML_PARSE_NOBLANKS;

    doc = htmlReadMemory(start, stop-start, "foof", NULL, parse_options);

    if (doc == NULL)
    {
        PRINT_ERROR("Document not parsed successfully.\n");
        return 0;
    }

    roo_element = xmlDocGetRootElement(doc);

    if (roo_element == NULL)
    {
        PRINT_ERROR("empty document\n");
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return 0;
    }

    if (doq->docptr_.html)
    {
        xmlFreeDoc(doq->docptr_.html);
    }
    doq->docptr_.html = doc;
    xmlCleanupParser();

    return true;
}

bool xmqParseBufferWithType(XMQDoc *doq,
                            const char *start,
                            const char *stop,
                            const char *implicit_root,
                            XMQContentType ct,
                            XMQTrimType tt)
{
    bool rc = true;

    // Unicode files might lead with a byte ordering mark.
    start = skip_any_potential_bom(start, stop);
    if (!start) return false;

    XMQContentType detected_ct = xmqDetectContentType(start, stop);
    if (ct == XMQ_CONTENT_DETECT)
    {
        ct = detected_ct;
    }
    else
    {
        if (ct != detected_ct)
        {
            switch (ct) {
            case XMQ_CONTENT_XMQ: doq->errno_ = XMQ_ERROR_EXPECTED_XMQ; break;
            case XMQ_CONTENT_HTMQ: doq->errno_ = XMQ_ERROR_EXPECTED_HTMQ; break;
            case XMQ_CONTENT_XML: doq->errno_ = XMQ_ERROR_EXPECTED_XML; break;
            case XMQ_CONTENT_HTML: doq->errno_ = XMQ_ERROR_EXPECTED_HTML; break;
            case XMQ_CONTENT_JSON: doq->errno_ = XMQ_ERROR_EXPECTED_JSON; break;
            default: break;
            }
            rc = false;
            goto exit;
        }
    }

    switch (ct)
    {
    case XMQ_CONTENT_XMQ: rc = xmqParseBuffer(doq, start, stop, implicit_root); break;
    case XMQ_CONTENT_HTMQ: rc = xmqParseBuffer(doq, start, stop, implicit_root); break;
    case XMQ_CONTENT_XML: rc = xmq_parse_buffer_xml(doq, start, stop, tt); break;
    case XMQ_CONTENT_HTML: rc = xmq_parse_buffer_html(doq, start, stop, tt); break;
    case XMQ_CONTENT_JSON: rc = xmq_parse_buffer_json(doq, start, stop); break;
    default: break;
    }

exit:

    if (rc)
    {
        if (tt == XMQ_TRIM_NORMAL ||
            tt == XMQ_TRIM_EXTRA ||
            tt == XMQ_TRIM_RESHUFFLE ||
            (tt == XMQ_TRIM_DEFAULT && (
                ct == XMQ_CONTENT_XML ||
                ct == XMQ_CONTENT_HTML)))
        {
            xmqTrimWhitespace(doq, tt);
        }
    }

    return rc;
}

bool load_stdin(XMQDoc *doq, size_t *out_fsize, const char **out_buffer)
{
    bool rc = true;
    int blocksize = 1024;
    char block[blocksize];

    MemBuffer *mb = new_membuffer();

    int fd = 0;
    while (true) {
        ssize_t n = read(fd, block, sizeof(block));
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            PRINT_ERROR("Could not read stdin errno=%d\n", errno);
            close(fd);

            return false;
        }
        membuffer_append_region(mb, block, block + n);
        if (n < (ssize_t)sizeof(block)) {
            break;
        }
    }
    close(fd);

    membuffer_append_null(mb);

    *out_fsize = mb->used_-1;
    *out_buffer = free_membuffer_but_return_trimmed_content(mb);

    return rc;
}

bool load_file(XMQDoc *doq, const char *file, size_t *out_fsize, const char **out_buffer)
{
    bool rc = false;
    char *buffer = NULL;

    FILE *f = fopen(file, "r");
    if (!f) {
        doq->errno_ = XMQ_ERROR_CANNOT_READ_FILE;
        doq->error_ = build_error_message("xmq: %s: No such file or directory\n", file);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = (char*)malloc(fsize + 1);
    if (!buffer) return false;

    size_t n = fread(buffer, fsize, 1, f);

    if (n != 1) {
        rc = false;
        doq->errno_ = XMQ_ERROR_CANNOT_READ_FILE;
        doq->error_ = build_error_message("xmq: %s: Cannot read file\n", file);
        goto exit;
    }
    fclose(f);
    buffer[fsize] = 0;
    rc = true;

exit:

    *out_fsize = fsize;
    *out_buffer = buffer;
    return rc;
}

bool xmqParseFileWithType(XMQDoc *doq,
                          const char *file,
                          const char *implicit_root,
                          XMQContentType ct,
                          XMQTrimType tt)
{
    bool rc = true;
    size_t fsize;
    const char *buffer;

    if (file)
    {
        xmqSetDocSourceName(doq, file);
        rc = load_file(doq, file, &fsize, &buffer);
    }
    else
    {
        xmqSetDocSourceName(doq, "-");
        rc = load_stdin(doq, &fsize, &buffer);
    }
    if (!rc) return false;

    rc = xmqParseBufferWithType(doq, buffer, buffer+fsize, implicit_root, ct, tt);

    free((void*)buffer);

    return rc;
}

/**
    enter_compound_level:

    When parsing a value and the parser has entered a compound value,
    the level for the child quotes/entities are now handled as
    tokens inside the compound.

    LEVEL_ELEMENT_VALUE -> LEVEL_ELEMENT_VALUE_COMPOUND
    LEVEL_ATTR_VALUE -> LEVEL_ATTR_VALUE_COMPOUND
*/
Level enter_compound_level(Level l)
{
    assert(l != 0);
    return (Level)(((int)l)+1);
}

XMQColor level_to_quote_color(Level level)
{
    switch (level)
    {
    case LEVEL_XMQ: return COLOR_quote;
    case LEVEL_ELEMENT_VALUE: return COLOR_element_value_quote;
    case LEVEL_ELEMENT_VALUE_COMPOUND: return COLOR_element_value_compound_quote;
    case LEVEL_ATTR_VALUE: return COLOR_attr_value_quote;
    case LEVEL_ATTR_VALUE_COMPOUND: return COLOR_attr_value_compound_quote;
    }
    assert(false);
}

XMQColor level_to_entity_color(Level level)
{
    switch (level)
    {
    case LEVEL_XMQ: return COLOR_entity;
    case LEVEL_ELEMENT_VALUE: return COLOR_element_value_entity;
    case LEVEL_ELEMENT_VALUE_COMPOUND: return COLOR_element_value_compound_entity;
    case LEVEL_ATTR_VALUE: return COLOR_attr_value_entity;
    case LEVEL_ATTR_VALUE_COMPOUND: return COLOR_attr_value_compound_entity;
    }
    assert(false);
}

xmlDtdPtr parse_doctype_raw(XMQDoc *doq, const char *start, const char *stop)
{
    size_t n = stop-start;
    xmlParserCtxtPtr ctxt;
    xmlDocPtr doc;

    ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, NULL);
    if (ctxt == NULL) {
        return NULL;
    }

    xmlParseChunk(ctxt, start, n, 0);
    xmlParseChunk(ctxt, start, 0, 1);

    doc = ctxt->myDoc;
    int rc = ctxt->wellFormed;
    xmlFreeParserCtxt(ctxt);

    if (!rc) {
        return NULL;
    }

    xmlDtdPtr dtd = xmlCopyDtd(doc->intSubset);
    xmlFreeDoc(doc);

    return dtd;
}
