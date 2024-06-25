/* libxmq - Copyright (C) 2023-2024 Fredrik Öhrström (spdx: MIT)

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

#define BUILDING_XMQ

// PART HEADERS //////////////////////////////////////////////////

// PARTS ALWAYS ////////////////////////////////////////

#include<stdbool.h>
#include<stdlib.h>

extern bool xmq_debug_enabled_;
extern bool xmq_verbose_enabled_;

void verbose__(const char* fmt, ...);
void debug__(const char* fmt, ...);
void check_malloc(void *a);

#define verbose(...) if (xmq_verbose_enabled_) { verbose__(__VA_ARGS__); }
#define debug(...) if (xmq_debug_enabled_) {debug__(__VA_ARGS__); }

#define PRINT_ERROR(...) fprintf(stderr, __VA_ARGS__)

#ifdef PLATFORM_WINAPI
char *strndup(const char *s, size_t l);
#endif

#define ALWAYS_MODULE

// PARTS COLORS ////////////////////////////////////////

#ifndef BUILDING_XMQ
#include "xmq.h"
#endif

struct XMQOutputSettings;
typedef struct XMQOutputSettings XMQOutputSettings;

/**
    XMQColor:

    Map token type into color index.
*/
typedef enum XMQColor {
    COLOR_none,
    COLOR_whitespace,
    COLOR_unicode_whitespace,
    COLOR_indentation_whitespace,
    COLOR_equals,
    COLOR_brace_left,
    COLOR_brace_right,
    COLOR_apar_left,
    COLOR_apar_right,
    COLOR_cpar_left,
    COLOR_cpar_right,
    COLOR_quote,
    COLOR_entity,
    COLOR_comment,
    COLOR_comment_continuation,
    COLOR_ns_colon,
    COLOR_element_ns,
    COLOR_element_name,
    COLOR_element_key,
    COLOR_element_value_text,
    COLOR_element_value_quote,
    COLOR_element_value_entity,
    COLOR_element_value_compound_quote,
    COLOR_element_value_compound_entity,
    COLOR_attr_ns,
    COLOR_attr_key,
    COLOR_attr_value_text,
    COLOR_attr_value_quote,
    COLOR_attr_value_entity,
    COLOR_attr_value_compound_quote,
    COLOR_attr_value_compound_entity,
    COLOR_ns_declaration,
    COLOR_ns_override_xsl,
} XMQColor;

/**
   XMQColorName:

   The actual number of colors are fewer than the number of tokens
   since we reuse colors for several tokens, no need to have different
   colors for left and right compound parentheses.
*/
typedef enum XMQColorName {
    XMQ_COLOR_C, // Comment
    XMQ_COLOR_Q, // Quote
    XMQ_COLOR_E, // Entity
    XMQ_COLOR_NS, // Name Space (both for element and attribute)
    XMQ_COLOR_EN, // Element Name
    XMQ_COLOR_EK, // Element Key
    XMQ_COLOR_EKV, // Element Key Value
    XMQ_COLOR_AK, // Attribute Key
    XMQ_COLOR_AKV, // Attribute Key Value
    XMQ_COLOR_CP, // Compound Parentheses
    XMQ_COLOR_NSD, // Name Space Declaration xmlns
    XMQ_COLOR_UW, // Unicode whitespace
    XMQ_COLOR_XLS, // Override XLS element names with this color.
} XMQColorName;

#define XMQ_COLOR_NAMES \
    X(C) \
    X(Q) \
    X(E) \
    X(NS) \
    X(EN) \
    X(EK) \
    X(EKV) \
    X(AK) \
    X(AKV) \
    X(CP) \
    X(NSD) \
    X(UW) \
    X(XLS) \

#define NUM_XMQ_COLOR_NAMES 13

const char* colorName(int i);

typedef struct XMQColorDef {
    int r, g, b;
    bool bold;
    bool underline;
} XMQColorDef;

/**
    XMQThemeStrings:
    @pre: string to inserted before the token
    @post: string to inserted after the token

    A color string object is stored for each type of token.
    It can store the ANSI color prefix, the html span etc.
    If post is NULL then when the token ends, the pre of the containing color will be reprinted.
    This is used for ansi codes where there is no stack memory (pop impossible) to the previous colors.
    I.e. pre = "\033[0;1;32m" which means reset;bold;green but post = NULL.
    For html/tex coloring we use the stack memory (pop possible) of tags.
    I.e. pre = "<span class="red">" post = "</span>"
    I.e. pre = "{\color{red}" post = "}"
*/
struct XMQThemeStrings
{
    const char *pre;
    const char *post;
};
typedef struct XMQThemeStrings XMQThemeStrings;

/**
    XMQTheme

    The theme struct is used to prefix/postfix ANSI/HTML/TEX strings for
    XMQ tokens to colorize the printed xmq output.
*/
struct XMQTheme
{
    const char *name;
    const char *indentation_space;
    const char *explicit_space;
    const char *explicit_nl;
    const char *explicit_tab;
    const char *explicit_cr;

    XMQThemeStrings document; // <html></html>  \documentclass{...}... etc
    XMQThemeStrings header; // <head>..</head>
    XMQThemeStrings style;  // Stylesheet content inside header (html) or color(tex) definitions.
    XMQThemeStrings body; // <body></body> \begin{document}\end{document}
    XMQThemeStrings content; // Wrapper around rendered code. Like <pre></pre>, \textt{...}

    XMQThemeStrings whitespace; // The normal whitespaces: space=32. Normally not colored.
    XMQThemeStrings unicode_whitespace; // The remaining unicode whitespaces, like: nbsp=160 color as red underline.
    XMQThemeStrings indentation_whitespace; // The xmq generated indentation spaces. Normally not colored.
    XMQThemeStrings equals; // The key = value equal sign.
    XMQThemeStrings brace_left; // Left brace starting a list of childs.
    XMQThemeStrings brace_right; // Right brace ending a list of childs.
    XMQThemeStrings apar_left; // Left parentheses surrounding attributes. foo(x=1)
    XMQThemeStrings apar_right; // Right parentheses surrounding attributes.
    XMQThemeStrings cpar_left; // Left parentheses surrounding a compound value. foo = (&#10;' x '&#10;)
    XMQThemeStrings cpar_right; // Right parentheses surrounding a compound value.
    XMQThemeStrings quote; // A quote 'x y z' can be single or multiline.
    XMQThemeStrings entity; // A entity &#10;
    XMQThemeStrings comment; // A comment // foo or /* foo */
    XMQThemeStrings comment_continuation; // A comment containing newlines /* Hello */* there */
    XMQThemeStrings ns_colon; // The color of the colon separating a namespace from a name.
    XMQThemeStrings element_ns; // The namespace part of an element tag, i.e. the text before colon in foo:alfa.
    XMQThemeStrings element_name; // When an element tag has multiple children or attributes it is rendered using this color.
    XMQThemeStrings element_key; // When an element tag is suitable to be presented as a key value, this color is used.
    XMQThemeStrings element_value_text; // When an element is presented as a key and the value is presented as text, use this color.
    XMQThemeStrings element_value_quote; // When the value is a single quote, use this color.
    XMQThemeStrings element_value_entity; // When the value is a single entity, use this color.
    XMQThemeStrings element_value_compound_quote; // When the value is compounded and this is a quote in the compound.
    XMQThemeStrings element_value_compound_entity; // When the value is compounded and this is an entity in the compound.
    XMQThemeStrings attr_ns; // The namespace part of an attribute name, i.e. the text before colon in bar:speed.
    XMQThemeStrings attr_key; // The color of the attribute name, i.e. the key.
    XMQThemeStrings attr_value_text; // When the attribute value is text, use this color.
    XMQThemeStrings attr_value_quote; // When the attribute value is a quote, use this color.
    XMQThemeStrings attr_value_entity; // When the attribute value is an entity, use this color.
    XMQThemeStrings attr_value_compound_quote; // When the attribute value is a compound and this is a quote in the compound.
    XMQThemeStrings attr_value_compound_entity; // When the attribute value is a compound and this is an entity in the compound.
    XMQThemeStrings ns_declaration; // The xmlns part of an attribute namespace declaration.
    XMQThemeStrings ns_override_xsl; // Override key/name colors for elements with xsl namespace.

    // RGB Sources + bold + underline from which we can configure the strings.
    XMQColorDef colors_darkbg[NUM_XMQ_COLOR_NAMES];
    XMQColorDef colors_lightbg[NUM_XMQ_COLOR_NAMES];
};
typedef struct XMQTheme XMQTheme;

void getThemeStrings(XMQOutputSettings *os, XMQColor c, const char **pre, const char **post);

bool string_to_color_def(const char *s, XMQColorDef *def);

// Expect buffer to store 128 bytes.
bool generate_ansi_color(char *buf, size_t buf_size, XMQColorDef *def);
bool generate_html_color(char *buf, size_t buf_size, XMQColorDef *def, const char *name);
bool generate_tex_color(char *buf, size_t buf_size, XMQColorDef *def, const char *name);


#define COLORS_MODULE

// PARTS DEFAULT_THEMES ////////////////////////////////////////

#ifndef BUILDING_XMQ
#include "xmq.h"
#endif

struct XMQTheme;
typedef struct XMQTheme XMQTheme;

void installDefaultThemeColors(XMQTheme *theme);
const char *ansiWin(int i);

#define DEFAULT_THEMES_MODULE

// PARTS ENTITIES ////////////////////////////////////////

/**
    toHtmlEntity:
    @uc: Unicode code point.
*/
const char *toHtmlEntity(int uc);

#define ENTITIES_MODULE

// PARTS UTF8 ////////////////////////////////////////

#ifndef BUILDING_XMQ
#include"xmq.h"
#include"colors.h"
// PARTS XMQ_INTERNALS ////////////////////////////////////////

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

#include"xmq.h"
#include<libxml/tree.h>
#include<libxml/parser.h>
#include<libxml/HTMLparser.h>
#include<libxml/HTMLtree.h>
#include<libxml/xmlreader.h>
#include<libxml/xpath.h>
#include<libxml/xpathInternals.h>

#ifndef BUILDING_XMQ
#include"colors.h"
#endif

// STACK /////////////////////

struct Stack;
typedef struct Stack Stack;

struct HashMap;
typedef struct HashMap HashMap;

struct MemBuffer;
typedef struct MemBuffer MemBuffer;

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
    X(ns_declaration) \
    X(ns_colon)  \

#define MAGIC_COOKIE 7287528

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
    XMQContentType original_content_type_; // Remember if this document was created from xmq/xml etc.
    size_t original_size_; // Remember the original source size of the document it was loaded from.
};

#ifdef __cplusplus
enum Level : short {
#else
enum Level {
#endif
    LEVEL_XMQ = 0,
    LEVEL_ELEMENT_VALUE = 1,
    LEVEL_ELEMENT_VALUE_COMPOUND = 2,
    LEVEL_ATTR_VALUE = 3,
    LEVEL_ATTR_VALUE_COMPOUND = 4
};
typedef enum Level Level;

/**
    XMQOutputSettings:
    @add_indent: Default is 4. Indentation starts at 0 which means no spaces prepended.
    @compact: Print on a single line limiting whitespace to a minimum.
    @escape_newlines: Replace newlines with &#10; this is implied if compact is set.
    @escape_non_7bit: Replace all chars above 126 with char entities, ie &#10;
    @output_format: Print xmq/xml/html/json
    @render_to: Render to terminal, html, tex.
    @render_raw: If true do not write surrounding html and css colors, likewise for tex.
    @only_style: Print only style sheet header.
    @write_content: Write content to buffer.
    @buffer_content: Supplied as buffer above.
    @write_error: Write error to buffer.
    @buffer_error: Supplied as buffer above.
    @colorings: Map from namespace (default is the empty string) to  prefixes/postfixes to colorize the output for ANSI/HTML/TEX.
*/
struct XMQOutputSettings
{
    int  add_indent;
    bool compact;
    bool use_color;
    bool bg_dark_mode;
    bool escape_newlines;
    bool escape_non_7bit;

    XMQContentType output_format;
    XMQRenderFormat render_to;
    bool render_raw;
    bool only_style;
    const char *render_theme;

    XMQWriter content;
    XMQWriter error;

    // If printing to memory:
    MemBuffer *output_buffer;
    char **output_buffer_start;
    char **output_buffer_stop;

    const char *indentation_space; // If NULL use " " can be replaced with any other string.
    const char *explicit_space; // If NULL use " " can be replaced with any other string.
    const char *explicit_tab; // If NULL use "\t" can be replaced with any other string.
    const char *explicit_cr; // If NULL use "\t" can be replaced with any other string.
    const char *explicit_nl; // If NULL use "\n" can be replaced with any other string.
    const char *prefix_line; // If non-NULL print this as the leader before each line.
    const char *postfix_line; // If non-NULL print this as the ending after each line.

    const char *use_id; // If non-NULL inser this id in the pre tag.
    const char *use_class; // If non-NULL insert this class in the pre tag.

    XMQTheme *theme; // The theme used to print.
    void *free_me;
    void *free_and_me;
};
typedef struct XMQOutputSettings XMQOutputSettings;

struct XMQParseState
{
    char *source_name; // Only used for generating any error messages.
    void *out;
    const char *buffer_start; // Points to first byte in buffer.
    const char *buffer_stop;   // Points to byte >after< last byte in buffer.
    const char *i; // Current parsing position.
    size_t line; // Current line.
    size_t col; // Current col.
    XMQParseError error_nr;
    char *generated_error_msg;
    MemBuffer *generating_error_msg;
    jmp_buf error_handler;

    bool simulated; // When true, this is generated from JSON parser to simulate an xmq element name.
    XMQParseCallbacks *parse;
    XMQDoc *doq;
    const char *implicit_root; // Assume that this is the first element name
    Stack *element_stack; // Top is last created node
    void *element_last; // Last added sibling to stack top node.
    bool parsing_doctype; // True when parsing a doctype.
    void *add_pre_node_before; // Used when retrofitting pre-root comments and doctype found in json.
    bool root_found; // Used to decide if _// should be printed before or after root.
    void *add_post_node_after; // Used when retrofitting post-root comments found in json.
    bool doctype_found; // True after a doctype has been parsed.
    bool parsing_pi; // True when parsing a processing instruction, pi.
    bool merge_text; // Merge text nodes and character entities.
    bool no_trim_quotes; // No trimming if quotes, used when reading json strings.
    const char *pi_name; // Name of the pi node just started.
    XMQOutputSettings *output_settings; // Used when coloring existing text using the tokenizer.
    int magic_cookie; // Used to check that the state has been properly initialized.

    char *element_namespace; // The element namespace is found before the element name. Remember the namespace name here.
    char *attribute_namespace; // The attribute namespace is found before the attribute key. Remember the namespace name here.
    bool declaring_xmlns; // Set to true when the xmlns declaration is found, the next attr value will be a href
    void *declaring_xmlns_namespace; // The namespace to be updated with attribute value, eg. xmlns=uri or xmlns:prefix=uri

    void *default_namespace; // If xmlns=http... has been set, then a pointer to the namespace object is stored here.

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
    const char *last_suspicios_quote_end;
    size_t last_suspicios_quote_end_line;
    size_t last_suspicios_quote_end_col;
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
   @ns: the last namespace reference.
   @output_settings: the output settings.
   @doc: The xmq document that is being printed.
*/
struct XMQPrintState
{
    size_t current_indent;
    size_t line_indent;
    int last_char;
    const char *replay_active_color_pre;
    const char *restart_line;
    const char *last_namespace;
    Stack *pre_nodes; // Used to remember leading comments/doctype when printing json.
    size_t pre_post_num_comments_total; // Number of comments outside of the root element.
    size_t pre_post_num_comments_used; // Active number of comment outside of the root element.
    Stack *post_nodes; // Used to remember ending comments when printing json.
    XMQOutputSettings *output_settings;
    XMQDoc *doq;
};
typedef struct XMQPrintState XMQPrintState;

/**
    XMQQuoteSettings:
    @force:           Always add single quotes. More quotes if necessary.
    @compact:         Generate compact quote on a single line. Using &#10; and no superfluous whitespace.
    @value_after_key: If enties are introduced by the quoting, then use compound ( ) around the content.

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

void generate_state_error_message(XMQParseState *state, XMQParseError error_nr, const char *start, const char *stop);

// Text functions ////////////////

bool is_all_xml_whitespace(const char *start);
bool is_lowercase_hex(char c);
bool is_unicode_whitespace(const char *start, const char *stop);
size_t count_whitespace(const char *i, const char *stop);

// XMQ parser utility functions //////////////////////////////////

bool is_xml_whitespace(char c); // 0x9 0xa 0xd 0x20
bool is_xmq_token_whitespace(char c); // 0xa 0xd 0x20
bool is_xmq_attribute_key_start(char c);
bool is_xmq_comment_start(char c, char cc);
bool is_xmq_compound_start(char c);
bool is_xmq_doctype_start(const char *start, const char *stop);
bool is_xmq_pi_start(const char *start, const char *stop);
bool is_xmq_entity_start(char c);
bool is_xmq_quote_start(char c);
bool is_xmq_text_value(const char *i, const char *stop);
bool is_xmq_text_value_char(const char *i, const char *stop);
bool unsafe_value_start(char c, char cc);
bool is_safe_value_char(const char *i, const char *stop);

size_t count_xmq_slashes(const char *i, const char *stop, bool *found_asterisk);
size_t count_necessary_quotes(const char *start, const char *stop, bool compact, bool *add_nls, bool *add_compound);
size_t count_necessary_slashes(const char *start, const char *stop);


// Common parser functions ///////////////////////////////////////

void increment(char c, size_t num_bytes, const char **i, size_t *line, size_t *col);

static const char *build_error_message(const char *fmt, ...);

Level enter_compound_level(Level l);
XMQColor level_to_quote_color(Level l);
XMQColor level_to_entity_color(Level l);
void attr_strlen_name_prefix(xmlAttr *attr, const char **name, const char **prefix, size_t *total_u_len);
void element_strlen_name_prefix(xmlNode *attr, const char **name, const char **prefix, size_t *total_u_len);
void namespace_strlen_prefix(xmlNs *ns, const char **prefix, size_t *total_u_len);
size_t find_attr_key_max_u_width(xmlAttr *a);
size_t find_namespace_max_u_width(size_t max, xmlNs *ns);
size_t find_element_key_max_width(xmlNodePtr node, xmlNodePtr *restart_find_at_node);
bool is_hex(char c);
unsigned char hex_value(char c);
const char *find_word_ignore_case(const char *start, const char *stop, const char *word);

XMQParseState *xmqAllocateParseState(XMQParseCallbacks *callbacks);
void xmqFreeParseState(XMQParseState *state);
bool xmqTokenizeBuffer(XMQParseState *state, const char *start, const char *stop);
bool xmqTokenizeFile(XMQParseState *state, const char *file);

void setup_terminal_coloring(XMQOutputSettings *os, XMQTheme *c, bool dark_mode, bool use_color, bool render_raw);
void setup_html_coloring(XMQOutputSettings *os, XMQTheme *c, bool dark_mode, bool use_color, bool render_raw);
void setup_tex_coloring(XMQOutputSettings *os, XMQTheme *c, bool dark_mode, bool use_color, bool render_raw);

// XMQ tokenizer functions ///////////////////////////////////////////////////////////

void eat_xml_whitespace(XMQParseState *state, const char **start, const char **stop);
void eat_xmq_token_whitespace(XMQParseState *state, const char **start, const char **stop);
void eat_xmq_entity(XMQParseState *state);
void eat_xmq_comment_to_eol(XMQParseState *state, const char **content_start, const char **content_stop);
void eat_xmq_comment_to_close(XMQParseState *state, const char **content_start, const char **content_stop, size_t n, bool *found_asterisk);
void eat_xmq_text_value(XMQParseState *state);
bool peek_xmq_next_is_equal(XMQParseState *state);
size_t count_xmq_quotes(const char *i, const char *stop);
void eat_xmq_quote(XMQParseState *state, const char **start, const char **stop);
char *xmq_trim_quote(size_t indent, char space, const char *start, const char *stop);
char *escape_xml_comment(const char *comment);
char *unescape_xml_comment(const char *comment);
void xmq_fixup_html_before_writeout(XMQDoc *doq);
void xmq_fixup_comments_after_readin(XMQDoc *doq);

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
char *xml_collapse_text(xmlNode *node);
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
bool is_pi_node(const xmlNode *node);
bool is_leaf_node(xmlNode *node);
bool is_key_value_node(xmlNode *node);
void trim_node(xmlNode *node, int flags);
void trim_text_node(xmlNode *node, int flags);

// Output buffer functions ////////////////////////////////////////////////////////

void node_strlen_name_prefix(xmlNode *node, const char **name, size_t *name_len, const char **prefix, size_t *prefix_len, size_t *total_len);

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

typedef void (*XMQContentCallback)(XMQParseState *state,
                                   size_t start_line,
                                   size_t start_col,
                                   const char *start,
                                   const char *stop,
                                   const char *suffix);

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

#define DO_CALLBACK(TYPE, state, start_line, start_col, start, stop, suffix) \
    { if (state->parse->handle_##TYPE != NULL) state->parse->handle_##TYPE(state,start_line,start_col,start,stop,suffix); }

#define DO_CALLBACK_SIM(TYPE, state, start_line, start_col, start, stop, suffix) \
    { if (state->parse->handle_##TYPE != NULL) { state->simulated=true; state->parse->handle_##TYPE(state,start_line,start_col,start,stop,suffix); state->simulated=false; } }

bool debug_enabled();

void xmq_setup_parse_callbacks(XMQParseCallbacks *callbacks);

// Multicolor terminals like gnome-term etc.

#define NOCOLOR      "\033[0m"

#define XMQ_INTERNALS_MODULE

#endif

struct XMQPrintState;
typedef struct XMQPrintState XMQPrintState;

enum XMQColor;
typedef enum XMQColor XMQColor;

size_t print_utf8_char(XMQPrintState *ps, const char *start, const char *stop);
size_t print_utf8_internal(XMQPrintState *ps, const char *start, const char *stop);
size_t print_utf8(XMQPrintState *ps, XMQColor c, size_t num_pairs, ...);

#define UTF8_MODULE

// PARTS HASHMAP ////////////////////////////////////////

struct HashMap;
typedef struct HashMap HashMap;

struct HashMapIterator;
typedef struct HashMapIterator HashMapIterator;

HashMap *hashmap_create(size_t max_size);
void hashmap_free_and_values(HashMap *map);
// Returns NULL if no key is found.
void *hashmap_get(HashMap* map, const char* key);
// Putting a non-NULL value.
void hashmap_put(HashMap* map, const char* key, void *val);
// How many key-vals are there?
size_t hashmap_size();
// Free it.
void hashmap_free(HashMap* map);

HashMapIterator *hashmap_iterate(HashMap *map);
bool hashmap_next_key_value(HashMapIterator *i, const char **key, void **val);
void hashmap_free_iterator(HashMapIterator *i);

#define HASHMAP_MODULE

// PARTS MEMBUFFER ////////////////////////////////////////

#include<stdlib.h>

/**
    MemBuffer:
    @max_: Current size of malloced buffer.
    @used_: Number of bytes actually used of buffer.
    @buffer_: Start of buffer data.
*/
struct MemBuffer;
typedef struct MemBuffer
{
    size_t max_; // Current size of malloc for buffer.
    size_t used_; // How much is actually used.
    char *buffer_; // The malloced data.
} MemBuffer;

// Output buffer functions ////////////////////////////////////////////////////////

MemBuffer *new_membuffer();
char *free_membuffer_but_return_trimmed_content(MemBuffer *mb);
void free_membuffer_and_free_content(MemBuffer *mb);
size_t pick_buffer_new_size(size_t max, size_t used, size_t add);
size_t membuffer_used(MemBuffer *mb);
void membuffer_reuse(MemBuffer *mb, char *start, size_t len);
void membuffer_append_region(MemBuffer *mb, const char *start, const char *stop);
void membuffer_append(MemBuffer *mb, const char *start);
void membuffer_append_char(MemBuffer *mb, char c);
void membuffer_append_entity(MemBuffer *mb, char c);
void membuffer_append_null(MemBuffer *mb);
void membuffer_drop_last_null(MemBuffer *mb);
void membuffer_append_pointer(MemBuffer *mb, void *ptr);

#define MEMBUFFER_MODULE

// PARTS STACK ////////////////////////////////////////

#include<stdlib.h>

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

Stack *new_stack();
void free_stack(Stack *stack);
void push_stack(Stack *s, void *);
// Pop the top element.
void *pop_stack(Stack *s);
// Pull the bottom element.
void *rock_stack(Stack *s);

#define STACK_MODULE

// PARTS TEXT ////////////////////////////////////////

#include<stdbool.h>
#include<stdlib.h>

/**
    UTF8Char: storage for 1 to 4 utf8 bytes

    An utf8 char is at most 4 bytes since the max unicode nr is capped at U+10FFFF:
*/
#define MAX_NUM_UTF8_BYTES 4
typedef struct
{
    char bytes[MAX_NUM_UTF8_BYTES];
} UTF8Char;

bool decode_utf8(const char *start, const char *stop, int *out_char, size_t *out_len);
size_t encode_utf8(int uc, UTF8Char *utf8);
const char *has_ending_nl_space(const char *start, const char *stop, size_t *only_newlines);
const char *has_leading_space_nl(const char *start, const char *stop, size_t *only_newlines);
bool has_leading_ending_quote(const char *start, const char *stop);
bool has_newlines(const char *start, const char *stop);
bool has_must_escape_chars(const char *start, const char *stop);
bool has_all_quotes(const char *start, const char *stop);
bool has_all_whitespace(const char *start, const char *stop, bool *all_space, bool *only_newlines);
bool is_lowercase_hex(char c);
bool is_xmq_token_whitespace(char c);
bool is_xml_whitespace(char c);
bool is_all_xml_whitespace(const char *s);
bool is_xmq_element_name(const char *start, const char *stop);
bool is_xmq_element_start(char c);
bool is_xmq_text_name(char c);
size_t num_utf8_bytes(char c);
size_t peek_utf8_char(const char *start, const char *stop, UTF8Char *uc);
void str_b_u_len(const char *start, const char *stop, size_t *b_len, size_t *u_len);
char to_hex(int c);
bool utf8_char_to_codepoint_string(UTF8Char *uc, char *buf);
char *xmq_quote_as_c(const char *start, const char *stop);
char *xmq_unquote_as_c(const char *start, const char *stop);
char *potentially_add_leading_ending_space(const char *start, const char *stop);

#define TEXT_MODULE

// PARTS XML ////////////////////////////////////////

#include<stdbool.h>
#include<libxml/tree.h>

void free_xml(xmlNode * node);
xmlNode *xml_first_child(xmlNode *node);
xmlNode *xml_last_child(xmlNode *node);
xmlNode *xml_next_sibling(xmlNode *node);
xmlNode *xml_prev_sibling(xmlNode *node);
xmlAttr *xml_first_attribute(xmlNode *node);
xmlAttr *xml_next_attribute(xmlAttr *attr);
xmlAttr *xml_get_attribute(xmlNode *node, const char *name);
xmlNs *xml_first_namespace_def(xmlNode *node);
xmlNs *xml_next_namespace_def(xmlNs *ns);
bool xml_non_empty_namespace(xmlNs *ns);
bool xml_has_non_empty_namespace_defs(xmlNode *node);
const char*xml_element_name(xmlNode *node);
const char*xml_element_content(xmlNode *node);
const char *xml_element_ns_prefix(const xmlNode *node);
const char *xml_attr_key(xmlAttr *attr);
const char* xml_namespace_href(xmlNs *ns);
bool is_entity_node(const xmlNode *node);
bool is_content_node(const xmlNode *node);
bool is_comment_node(const xmlNode *node);
bool is_pi_node(const xmlNode *node);
bool is_doctype_node(const xmlNode *node);
bool is_element_node(const xmlNode *node);
bool is_text_node(const xmlNode *node);
bool is_attribute_node(const xmlNode *node);
bool is_key_value_node(xmlNodePtr node);
bool is_leaf_node(xmlNode *node);
bool has_attributes(xmlNodePtr node);
char *xml_collapse_text(xmlNode *node);
int decode_entity_ref(const char *name);
void xml_add_root_child(xmlDoc *doc, xmlNode *node);

#define XML_MODULE

// PARTS XMQ_PARSER ////////////////////////////////////////

struct XMQParseState;
typedef struct XMQParseState XMQParseState;

void parse_xmq(XMQParseState *state);

#define XMQ_PARSER_MODULE

// PARTS XMQ_PRINTER ////////////////////////////////////////

struct XMQPrintState;
typedef struct XMQPrintState XMQPrintState;

#ifdef __cplusplus
enum Level : short;
#else
enum Level;
#endif
typedef enum Level Level;

size_t count_necessary_quotes(const char *start, const char *stop, bool compact, bool *add_nls, bool *add_compound);
size_t count_necessary_slashes(const char *start, const char *stop);

void print_nodes(XMQPrintState *ps, xmlNode *from, xmlNode *to, size_t align);
void print_content_node(XMQPrintState *ps, xmlNode *node);
void print_entity_node(XMQPrintState *ps, xmlNode *node);
void print_comment_line(XMQPrintState *ps, const char *start, const char *stop, bool compact);
void print_comment_lines(XMQPrintState *ps, const char *start, const char *stop, bool compact);
void print_comment_node(XMQPrintState *ps, xmlNode *node);
size_t print_element_name_and_attributes(XMQPrintState *ps, xmlNode *node);
void print_leaf_node(XMQPrintState *ps,
                     xmlNode *node);
void print_key_node(XMQPrintState *ps,
                    xmlNode *node,
                    size_t align);
void print_element_with_children(XMQPrintState *ps,
                                 xmlNode *node,
                                 size_t align);
void print_doctype(XMQPrintState *ps, xmlNode *node);
void print_pi_node(XMQPrintState *ps, xmlNode *node);
void print_node(XMQPrintState *ps, xmlNode *node, size_t align);

void print_white_spaces(XMQPrintState *ps, int num);
void print_all_whitespace(XMQPrintState *ps, const char *start, const char *stop, Level level);
void print_explicit_spaces(XMQPrintState *ps, XMQColor c, int num);
void print_quoted_spaces(XMQPrintState *ps, XMQColor color, int num);
void print_quotes(XMQPrintState *ps, size_t num, XMQColor color);
void print_nl(XMQPrintState *ps, const char *prefix, const char *postfix);
void print_nl_and_indent(XMQPrintState *ps, const char *prefix, const char *postfix);
size_t print_char_entity(XMQPrintState *ps, XMQColor color, const char *start, const char *stop);
void print_slashes(XMQPrintState *ps, const char *pre, const char *post, size_t n);


bool need_separation_before_attribute_key(XMQPrintState *ps);
bool need_separation_before_entity(XMQPrintState *ps);
bool need_separation_before_element_name(XMQPrintState *ps);
bool need_separation_before_quote(XMQPrintState *ps);
bool need_separation_before_comment(XMQPrintState *ps);
void check_space_before_attribute(XMQPrintState *ps);
void check_space_before_entity_node(XMQPrintState *ps);
void check_space_before_quote(XMQPrintState *ps, Level level);
void check_space_before_key(XMQPrintState *ps);
void check_space_before_opening_brace(XMQPrintState *ps);
void check_space_before_closing_brace(XMQPrintState *ps);
void check_space_before_comment(XMQPrintState *ps);

void print_attribute(XMQPrintState *ps, xmlAttr *a, size_t align);
void print_namespace_declaration(XMQPrintState *ps, xmlNs *ns, size_t align);
void print_attributes(XMQPrintState *ps, xmlNodePtr node);

void print_quote_lines_and_color_uwhitespace(XMQPrintState *ps,
                                             XMQColor color,
                                             const char *start,
                                             const char *stop);
void print_safe_leaf_quote(XMQPrintState *ps,
                           XMQColor c,
                           const char *start,
                           const char *stop);
const char *find_next_line_end(XMQPrintState *ps, const char *start, const char *stop);
const char *find_next_char_that_needs_escape(XMQPrintState *ps, const char *start, const char *stop);
void print_value_internal_text(XMQPrintState *ps, const char *start, const char *stop, Level level);
void print_value_internal(XMQPrintState *ps, xmlNode *node, Level level);
bool quote_needs_compounded(XMQPrintState *ps, const char *start, const char *stop);
void print_value(XMQPrintState *ps, xmlNode *node, Level level);

#define XMQ_PRINTER_MODULE


// XMQ STRUCTURES ////////////////////////////////////////////////

// PARTS XMQ_INTERNALS ////////////////////////////////////////

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

#include"xmq.h"
#include<libxml/tree.h>
#include<libxml/parser.h>
#include<libxml/HTMLparser.h>
#include<libxml/HTMLtree.h>
#include<libxml/xmlreader.h>
#include<libxml/xpath.h>
#include<libxml/xpathInternals.h>

#ifndef BUILDING_XMQ
#include"colors.h"
#endif

// STACK /////////////////////

struct Stack;
typedef struct Stack Stack;

struct HashMap;
typedef struct HashMap HashMap;

struct MemBuffer;
typedef struct MemBuffer MemBuffer;

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
    X(ns_declaration) \
    X(ns_colon)  \

#define MAGIC_COOKIE 7287528

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
    XMQContentType original_content_type_; // Remember if this document was created from xmq/xml etc.
    size_t original_size_; // Remember the original source size of the document it was loaded from.
};

#ifdef __cplusplus
enum Level : short {
#else
enum Level {
#endif
    LEVEL_XMQ = 0,
    LEVEL_ELEMENT_VALUE = 1,
    LEVEL_ELEMENT_VALUE_COMPOUND = 2,
    LEVEL_ATTR_VALUE = 3,
    LEVEL_ATTR_VALUE_COMPOUND = 4
};
typedef enum Level Level;

/**
    XMQOutputSettings:
    @add_indent: Default is 4. Indentation starts at 0 which means no spaces prepended.
    @compact: Print on a single line limiting whitespace to a minimum.
    @escape_newlines: Replace newlines with &#10; this is implied if compact is set.
    @escape_non_7bit: Replace all chars above 126 with char entities, ie &#10;
    @output_format: Print xmq/xml/html/json
    @render_to: Render to terminal, html, tex.
    @render_raw: If true do not write surrounding html and css colors, likewise for tex.
    @only_style: Print only style sheet header.
    @write_content: Write content to buffer.
    @buffer_content: Supplied as buffer above.
    @write_error: Write error to buffer.
    @buffer_error: Supplied as buffer above.
    @colorings: Map from namespace (default is the empty string) to  prefixes/postfixes to colorize the output for ANSI/HTML/TEX.
*/
struct XMQOutputSettings
{
    int  add_indent;
    bool compact;
    bool use_color;
    bool bg_dark_mode;
    bool escape_newlines;
    bool escape_non_7bit;

    XMQContentType output_format;
    XMQRenderFormat render_to;
    bool render_raw;
    bool only_style;
    const char *render_theme;

    XMQWriter content;
    XMQWriter error;

    // If printing to memory:
    MemBuffer *output_buffer;
    char **output_buffer_start;
    char **output_buffer_stop;

    const char *indentation_space; // If NULL use " " can be replaced with any other string.
    const char *explicit_space; // If NULL use " " can be replaced with any other string.
    const char *explicit_tab; // If NULL use "\t" can be replaced with any other string.
    const char *explicit_cr; // If NULL use "\t" can be replaced with any other string.
    const char *explicit_nl; // If NULL use "\n" can be replaced with any other string.
    const char *prefix_line; // If non-NULL print this as the leader before each line.
    const char *postfix_line; // If non-NULL print this as the ending after each line.

    const char *use_id; // If non-NULL inser this id in the pre tag.
    const char *use_class; // If non-NULL insert this class in the pre tag.

    XMQTheme *theme; // The theme used to print.
    void *free_me;
    void *free_and_me;
};
typedef struct XMQOutputSettings XMQOutputSettings;

struct XMQParseState
{
    char *source_name; // Only used for generating any error messages.
    void *out;
    const char *buffer_start; // Points to first byte in buffer.
    const char *buffer_stop;   // Points to byte >after< last byte in buffer.
    const char *i; // Current parsing position.
    size_t line; // Current line.
    size_t col; // Current col.
    XMQParseError error_nr;
    char *generated_error_msg;
    MemBuffer *generating_error_msg;
    jmp_buf error_handler;

    bool simulated; // When true, this is generated from JSON parser to simulate an xmq element name.
    XMQParseCallbacks *parse;
    XMQDoc *doq;
    const char *implicit_root; // Assume that this is the first element name
    Stack *element_stack; // Top is last created node
    void *element_last; // Last added sibling to stack top node.
    bool parsing_doctype; // True when parsing a doctype.
    void *add_pre_node_before; // Used when retrofitting pre-root comments and doctype found in json.
    bool root_found; // Used to decide if _// should be printed before or after root.
    void *add_post_node_after; // Used when retrofitting post-root comments found in json.
    bool doctype_found; // True after a doctype has been parsed.
    bool parsing_pi; // True when parsing a processing instruction, pi.
    bool merge_text; // Merge text nodes and character entities.
    bool no_trim_quotes; // No trimming if quotes, used when reading json strings.
    const char *pi_name; // Name of the pi node just started.
    XMQOutputSettings *output_settings; // Used when coloring existing text using the tokenizer.
    int magic_cookie; // Used to check that the state has been properly initialized.

    char *element_namespace; // The element namespace is found before the element name. Remember the namespace name here.
    char *attribute_namespace; // The attribute namespace is found before the attribute key. Remember the namespace name here.
    bool declaring_xmlns; // Set to true when the xmlns declaration is found, the next attr value will be a href
    void *declaring_xmlns_namespace; // The namespace to be updated with attribute value, eg. xmlns=uri or xmlns:prefix=uri

    void *default_namespace; // If xmlns=http... has been set, then a pointer to the namespace object is stored here.

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
    const char *last_suspicios_quote_end;
    size_t last_suspicios_quote_end_line;
    size_t last_suspicios_quote_end_col;
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
   @ns: the last namespace reference.
   @output_settings: the output settings.
   @doc: The xmq document that is being printed.
*/
struct XMQPrintState
{
    size_t current_indent;
    size_t line_indent;
    int last_char;
    const char *replay_active_color_pre;
    const char *restart_line;
    const char *last_namespace;
    Stack *pre_nodes; // Used to remember leading comments/doctype when printing json.
    size_t pre_post_num_comments_total; // Number of comments outside of the root element.
    size_t pre_post_num_comments_used; // Active number of comment outside of the root element.
    Stack *post_nodes; // Used to remember ending comments when printing json.
    XMQOutputSettings *output_settings;
    XMQDoc *doq;
};
typedef struct XMQPrintState XMQPrintState;

/**
    XMQQuoteSettings:
    @force:           Always add single quotes. More quotes if necessary.
    @compact:         Generate compact quote on a single line. Using &#10; and no superfluous whitespace.
    @value_after_key: If enties are introduced by the quoting, then use compound ( ) around the content.

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

void generate_state_error_message(XMQParseState *state, XMQParseError error_nr, const char *start, const char *stop);

// Text functions ////////////////

bool is_all_xml_whitespace(const char *start);
bool is_lowercase_hex(char c);
bool is_unicode_whitespace(const char *start, const char *stop);
size_t count_whitespace(const char *i, const char *stop);

// XMQ parser utility functions //////////////////////////////////

bool is_xml_whitespace(char c); // 0x9 0xa 0xd 0x20
bool is_xmq_token_whitespace(char c); // 0xa 0xd 0x20
bool is_xmq_attribute_key_start(char c);
bool is_xmq_comment_start(char c, char cc);
bool is_xmq_compound_start(char c);
bool is_xmq_doctype_start(const char *start, const char *stop);
bool is_xmq_pi_start(const char *start, const char *stop);
bool is_xmq_entity_start(char c);
bool is_xmq_quote_start(char c);
bool is_xmq_text_value(const char *i, const char *stop);
bool is_xmq_text_value_char(const char *i, const char *stop);
bool unsafe_value_start(char c, char cc);
bool is_safe_value_char(const char *i, const char *stop);

size_t count_xmq_slashes(const char *i, const char *stop, bool *found_asterisk);
size_t count_necessary_quotes(const char *start, const char *stop, bool compact, bool *add_nls, bool *add_compound);
size_t count_necessary_slashes(const char *start, const char *stop);


// Common parser functions ///////////////////////////////////////

void increment(char c, size_t num_bytes, const char **i, size_t *line, size_t *col);

static const char *build_error_message(const char *fmt, ...);

Level enter_compound_level(Level l);
XMQColor level_to_quote_color(Level l);
XMQColor level_to_entity_color(Level l);
void attr_strlen_name_prefix(xmlAttr *attr, const char **name, const char **prefix, size_t *total_u_len);
void element_strlen_name_prefix(xmlNode *attr, const char **name, const char **prefix, size_t *total_u_len);
void namespace_strlen_prefix(xmlNs *ns, const char **prefix, size_t *total_u_len);
size_t find_attr_key_max_u_width(xmlAttr *a);
size_t find_namespace_max_u_width(size_t max, xmlNs *ns);
size_t find_element_key_max_width(xmlNodePtr node, xmlNodePtr *restart_find_at_node);
bool is_hex(char c);
unsigned char hex_value(char c);
const char *find_word_ignore_case(const char *start, const char *stop, const char *word);

XMQParseState *xmqAllocateParseState(XMQParseCallbacks *callbacks);
void xmqFreeParseState(XMQParseState *state);
bool xmqTokenizeBuffer(XMQParseState *state, const char *start, const char *stop);
bool xmqTokenizeFile(XMQParseState *state, const char *file);

void setup_terminal_coloring(XMQOutputSettings *os, XMQTheme *c, bool dark_mode, bool use_color, bool render_raw);
void setup_html_coloring(XMQOutputSettings *os, XMQTheme *c, bool dark_mode, bool use_color, bool render_raw);
void setup_tex_coloring(XMQOutputSettings *os, XMQTheme *c, bool dark_mode, bool use_color, bool render_raw);

// XMQ tokenizer functions ///////////////////////////////////////////////////////////

void eat_xml_whitespace(XMQParseState *state, const char **start, const char **stop);
void eat_xmq_token_whitespace(XMQParseState *state, const char **start, const char **stop);
void eat_xmq_entity(XMQParseState *state);
void eat_xmq_comment_to_eol(XMQParseState *state, const char **content_start, const char **content_stop);
void eat_xmq_comment_to_close(XMQParseState *state, const char **content_start, const char **content_stop, size_t n, bool *found_asterisk);
void eat_xmq_text_value(XMQParseState *state);
bool peek_xmq_next_is_equal(XMQParseState *state);
size_t count_xmq_quotes(const char *i, const char *stop);
void eat_xmq_quote(XMQParseState *state, const char **start, const char **stop);
char *xmq_trim_quote(size_t indent, char space, const char *start, const char *stop);
char *escape_xml_comment(const char *comment);
char *unescape_xml_comment(const char *comment);
void xmq_fixup_html_before_writeout(XMQDoc *doq);
void xmq_fixup_comments_after_readin(XMQDoc *doq);

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
char *xml_collapse_text(xmlNode *node);
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
bool is_pi_node(const xmlNode *node);
bool is_leaf_node(xmlNode *node);
bool is_key_value_node(xmlNode *node);
void trim_node(xmlNode *node, int flags);
void trim_text_node(xmlNode *node, int flags);

// Output buffer functions ////////////////////////////////////////////////////////

void node_strlen_name_prefix(xmlNode *node, const char **name, size_t *name_len, const char **prefix, size_t *prefix_len, size_t *total_len);

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

typedef void (*XMQContentCallback)(XMQParseState *state,
                                   size_t start_line,
                                   size_t start_col,
                                   const char *start,
                                   const char *stop,
                                   const char *suffix);

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

#define DO_CALLBACK(TYPE, state, start_line, start_col, start, stop, suffix) \
    { if (state->parse->handle_##TYPE != NULL) state->parse->handle_##TYPE(state,start_line,start_col,start,stop,suffix); }

#define DO_CALLBACK_SIM(TYPE, state, start_line, start_col, start, stop, suffix) \
    { if (state->parse->handle_##TYPE != NULL) { state->simulated=true; state->parse->handle_##TYPE(state,start_line,start_col,start,stop,suffix); state->simulated=false; } }

bool debug_enabled();

void xmq_setup_parse_callbacks(XMQParseCallbacks *callbacks);

// Multicolor terminals like gnome-term etc.

#define NOCOLOR      "\033[0m"

#define XMQ_INTERNALS_MODULE


// FUNCTIONALITY /////////////////////////////////////////////////

// PARTS JSON ////////////////////////////////////////

#include"xmq.h"
#include<libxml/tree.h>

struct XMQPrintState;
typedef struct XMQPrintState XMQPrintState;

void xmq_fixup_json_before_writeout(XMQDoc *doq);
void json_print_object_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to);
void collect_leading_ending_comments_doctype(XMQPrintState *ps, xmlNodePtr *first, xmlNodePtr *last);
void json_print_array_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to);
bool xmq_tokenize_buffer_json(XMQParseState *state, const char *start, const char *stop);

#define JSON_MODULE


//////////////////////////////////////////////////////////////////////////////////

void add_nl(XMQParseState *state);
XMQProceed catch_single_content(XMQDoc *doc, XMQNode *node, void *user_data);
size_t calculate_buffer_size(const char *start, const char *stop, int indent, const char *pre_line, const char *post_line);
void copy_and_insert(MemBuffer *mb, const char *start, const char *stop, int num_prefix_spaces, const char *implicit_indentation, const char *explicit_space, const char *newline, const char *prefix_line, const char *postfix_line);
char *copy_lines(int num_prefix_spaces, const char *start, const char *stop, int num_quotes, bool add_nls, bool add_compound, const char *implicit_indentation, const char *explicit_space, const char *newline, const char *prefix_line, const char *postfix_line);
void copy_quote_settings_from_output_settings(XMQQuoteSettings *qs, XMQOutputSettings *os);
xmlNodePtr create_entity(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop, xmlNodePtr parent);
void create_node(XMQParseState *state, const char *start, const char *stop);
void update_namespace_href(XMQParseState *state, xmlNsPtr ns, const char *start, const char *stop);
xmlNodePtr create_quote(XMQParseState *state, size_t l, size_t col, const char *start, const char *stop, const char *suffix,  xmlNodePtr parent);
void debug_content_comment(XMQParseState *state, size_t line, size_t start_col, const char *start, const char *stop, const char *suffix);
void debug_content_value(XMQParseState *state, size_t line, size_t start_col, const char *start, const char *stop, const char *suffix);
void debug_content_quote(XMQParseState *state, size_t line, size_t start_col, const char *start, const char *stop, const char *suffix);
void do_attr_key(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_attr_ns(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_ns_declaration(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_attr_value_compound_entity(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop);
void do_attr_value_compound_quote(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop);
void do_attr_value_entity(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop);
void do_attr_value_text(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_attr_value_quote(XMQParseState*state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_comment(XMQParseState*state, size_t l, size_t c, const char *start, const char *stop, const char *suffix);
void do_comment_continuation(XMQParseState*state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_apar_left(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_apar_right(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_brace_left(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_brace_right(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_cpar_left(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_cpar_right(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_equals(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_key(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_name(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_ns(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_value_compound_entity(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_value_compound_quote(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_value_entity(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_value_text(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_element_value_quote(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_entity(XMQParseState *state, size_t l, size_t c, const char *cstart, const char *cstop, const char*stop);
void do_ns_colon(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
void do_quote(XMQParseState *state, size_t l, size_t col, const char *start, const char *stop, const char *suffix);
void do_whitespace(XMQParseState *state, size_t line, size_t col, const char *start, const char *stop, const char *suffix);
bool find_line(const char *start, const char *stop, size_t *indent, const char **after_last_non_space, const char **eol);
const char *find_next_line_end(XMQPrintState *ps, const char *start, const char *stop);
const char *find_next_char_that_needs_escape(XMQPrintState *ps, const char *start, const char *stop);
void fixup_html(XMQDoc *doq, xmlNode *node, bool inside_cdata_declared);
void fixup_comments(XMQDoc *doq, xmlNode *node, int depth);
bool has_leading_ending_quote(const char *start, const char *stop);
bool is_safe_char(const char *i, const char *stop);
size_t line_length(const char *start, const char *stop, int *numq, int *lq, int *eq);
bool load_file(XMQDoc *doq, const char *file, size_t *out_fsize, const char **out_buffer);
bool load_stdin(XMQDoc *doq, size_t *out_fsize, const char **out_buffer);
bool need_separation_before_entity(XMQPrintState *ps);
size_t num_utf8_bytes(char c);
void print_explicit_spaces(XMQPrintState *ps, XMQColor c, int num);
void reset_ansi(XMQParseState *state);
void reset_ansi_nl(XMQParseState *state);
const char *skip_any_potential_bom(const char *start, const char *stop);
void text_print_node(XMQPrintState *ps, xmlNode *node);
void text_print_nodes(XMQPrintState *ps, xmlNode *from);
bool write_print_stderr(void *writer_state_ignored, const char *start, const char *stop);
bool write_print_stdout(void *writer_state_ignored, const char *start, const char *stop);
void write_safe_html(XMQWrite write, void *writer_state, const char *start, const char *stop);
void write_safe_tex(XMQWrite write, void *writer_state, const char *start, const char *stop);
bool xmqVerbose();
void xmqSetupParseCallbacksNoop(XMQParseCallbacks *callbacks);
bool xmq_parse_buffer_html(XMQDoc *doq, const char *start, const char *stop, int flags);
bool xmq_parse_buffer_xml(XMQDoc *doq, const char *start, const char *stop, int flags);
bool xmq_parse_buffer_text(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root);
void xmq_print_html(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_xml(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_xmq(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_json(XMQDoc *doq, XMQOutputSettings *output_settings);
void xmq_print_text(XMQDoc *doq, XMQOutputSettings *output_settings);
char *xmq_quote_with_entity_newlines(const char *start, const char *stop, XMQQuoteSettings *settings);
char *xmq_quote_default(int indent, const char *start, const char *stop, XMQQuoteSettings *settings);
bool xmq_parse_buffer_json(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root);
const char *xml_element_type_to_string(xmlElementType type);
const char *indent_depth(int i);
void free_indent_depths();

xmlNode *merge_surrounding_text_nodes(xmlNode *node);
xmlNode *merge_hex_chars_node(xmlNode *node);

// Declare tokenize_whitespace tokenize_name functions etc...
#define X(TYPE) void tokenize_##TYPE(XMQParseState*state, size_t line, size_t col,const char *start, const char *stop, const char *suffix);
LIST_OF_XMQ_TOKENS
#undef X

// Declare debug_whitespace debug_name functions etc...
#define X(TYPE) void debug_token_##TYPE(XMQParseState*state,size_t line,size_t col,const char*start,const char*stop,const char*suffix);
LIST_OF_XMQ_TOKENS
#undef X

//////////////////////////////////////////////////////////////////////////////////

char ansi_reset_color[] = "\033[0m";

void xmqSetupDefaultColors(XMQOutputSettings *os)
{
    bool dark_mode = os->bg_dark_mode;
    XMQTheme *theme = os->theme;
    if (os->render_theme == NULL)
    {
        if (os->render_to == XMQ_RENDER_TEX) dark_mode = false;
        os->render_theme = dark_mode?"darkbg":"lightbg";
    }
    else
    {
        if (!strcmp(os->render_theme, "darkbg"))
        {
            dark_mode = true;
        }
        else if (!strcmp(os->render_theme, "lightbg"))
        {
            dark_mode = false;
        }
    }

    verbose("(xmq) use theme %s\n", os->render_theme);
    installDefaultThemeColors(theme);

    os->indentation_space = theme->indentation_space; // " ";
    os->explicit_space = theme->explicit_space; // " ";
    os->explicit_nl = theme->explicit_nl; // "\n";
    os->explicit_tab = theme->explicit_tab; // "\t";
    os->explicit_cr = theme->explicit_cr; // "\r";

    if (os->render_to == XMQ_RENDER_PLAIN)
    {
    }
    else
    if (os->render_to == XMQ_RENDER_TERMINAL)
    {
        setup_terminal_coloring(os, theme, dark_mode, os->use_color, os->render_raw);
    }
    else if (os->render_to == XMQ_RENDER_HTML)
    {
        setup_html_coloring(os, theme, dark_mode, os->use_color, os->render_raw);
    }
    else if (os->render_to == XMQ_RENDER_TEX)
    {
        setup_tex_coloring(os, theme, dark_mode, os->use_color, os->render_raw);
    }

    if (os->only_style)
    {
        printf("%s\n", theme->style.pre);
        exit(0);
    }

}

const char *add_color(XMQColorDef *colors, XMQColorName n, char **pp);
const char *add_color(XMQColorDef *colors, XMQColorName n, char **pp)
{
#ifdef PLATFORM_WINAPI
    const char *tmp = ansiWin((int)n);
    char *p = *pp;
    char *color = p;
    strcpy(p, tmp);
    p += strlen(tmp);
    *p++ = 0;
    *pp = p;
    return color;
#else
    XMQColorDef *def = &colors[n];
    char *p = *pp;
    // Remember where the color starts in the buffer.
    char *color = p;
    char tmp[128];
    generate_ansi_color(tmp, 128, def);
    // Append the new color to the buffer.
    strcpy(p, tmp);
    p += strlen(tmp);
    *p++ = 0;
    // Export the new position in the buffer
    *pp = p;
    // Return the color position;
    return color;
#endif
}
void setup_terminal_coloring(XMQOutputSettings *os, XMQTheme *theme, bool dark_mode, bool use_color, bool render_raw)
{
    if (!use_color) return;

    XMQColorDef *colors = theme->colors_darkbg;
    if (!dark_mode) colors = theme->colors_lightbg;

    char *commands = (char*)malloc(4096);
    os->free_me = commands;
    char *p = commands;

    const char *c = add_color(colors, XMQ_COLOR_C, &p);
    theme->comment.pre = c;
    theme->comment_continuation.pre = c;

    c = add_color(colors, XMQ_COLOR_Q, &p);
    theme->quote.pre = c;

    c = add_color(colors, XMQ_COLOR_E, &p);
    theme->entity.pre = c;
    theme->element_value_entity.pre = c;
    theme->element_value_compound_entity.pre = c;
    theme->attr_value_entity.pre = c;
    theme->attr_value_compound_entity.pre = c;

    c = add_color(colors, XMQ_COLOR_NS, &p);
    theme->element_ns.pre = c;
    theme->attr_ns.pre = c;

    c = add_color(colors, XMQ_COLOR_EN, &p);
    theme->element_name.pre = c;

    c = add_color(colors,XMQ_COLOR_EK, &p);
    theme->element_key.pre = c;

    c = add_color(colors, XMQ_COLOR_EKV, &p);
    theme->element_value_text.pre = c;
    theme->element_value_quote.pre = c;
    theme->element_value_compound_quote.pre = c;

    c = add_color(colors, XMQ_COLOR_AK, &p);
    theme->attr_key.pre = c;

    c = add_color(colors, XMQ_COLOR_AKV, &p);
    theme->attr_value_text.pre = c;
    theme->attr_value_quote.pre = c;
    theme->attr_value_compound_quote.pre = c;

    c = add_color(colors, XMQ_COLOR_CP, &p);
    theme->cpar_left.pre  = c;
    theme->cpar_right.pre = c;

    c = add_color(colors, XMQ_COLOR_NSD, &p);
    theme->ns_declaration.pre = c;

    c = add_color(colors, XMQ_COLOR_UW, &p);
    theme->unicode_whitespace.pre = c;

    c = add_color(colors, XMQ_COLOR_XLS, &p);
    theme->ns_override_xsl.pre = c;

    theme->whitespace.pre  = NOCOLOR;
    theme->equals.pre      = NOCOLOR;
    theme->brace_left.pre  = NOCOLOR;
    theme->brace_right.pre = NOCOLOR;
    theme->apar_left.pre    = NOCOLOR;
    theme->apar_right.pre   = NOCOLOR;
    theme->ns_colon.pre = NOCOLOR;

}

void setup_html_coloring(XMQOutputSettings *os, XMQTheme *theme, bool dark_mode, bool use_color, bool render_raw)
{
    os->indentation_space = " ";
    os->explicit_nl = "\n";
    if (!render_raw)
    {
        theme->document.pre =
            "<!DOCTYPE html>\n<html>\n";
        theme->document.post =
            "</html>";
        theme->header.pre =
            "<head><meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, maximum-scale=5\"><style>";
        theme->header.post =
            "</style></head>";

        MemBuffer *style_pre = new_membuffer();

        membuffer_append(style_pre,
                         "@media screen and (orientation: portrait) { pre { font-size: 2vw; } }"
                         "@media screen and (orientation: landscape) { pre { max-width: 98%; } }"
                         "pre.xmq_dark {white-space:pre-wrap;word-break:break-all;border-radius:2px;background-color:#263338;border:solid 1px #555555;display:inline-block;padding:1em;color:white;}\n"
                         "pre.xmq_light{white-space:pre-wrap;word-break:break-all;border-radius:2px;background-color:#ffffcc;border:solid 1px #888888;display:inline-block;padding:1em;color:black;}\n"
                         "body.xmq_dark {background-color:black;}\n"
                         "body.xmq_light {}\n");

        for (int i=0; i<NUM_XMQ_COLOR_NAMES; ++i)
        {
            char buf[128];
            generate_html_color(buf, 128, &theme->colors_darkbg[i], colorName(i));
            membuffer_append(style_pre, buf);
        }
        membuffer_append(style_pre, "pre.xmq_light {\n");

        for (int i=0; i<NUM_XMQ_COLOR_NAMES; ++i)
        {
            char buf[128];
            generate_html_color(buf, 128, &theme->colors_lightbg[i], colorName(i));
            membuffer_append(style_pre, buf);
        }

        membuffer_append(style_pre, "pre.xmq_dark {}\n}\n");
        membuffer_append_null(style_pre);

        theme->style.pre = free_membuffer_but_return_trimmed_content(style_pre);
        os->free_me = (void*)theme->style.pre;
        if (dark_mode)
        {
            theme->body.pre = "<body class=\"xmq_dark\">";
        }
        else
        {
            theme->body.pre = "<body class=\"xmq_light\">";
        }

        theme->body.post =
            "</body>";
    }

    theme->content.pre = "<pre>";
    theme->content.post = "</pre>";

    const char *mode = "xmq_light";
    if (dark_mode) mode = "xmq_dark";

    char *buf = (char*)malloc(1024);
    os->free_and_me = buf;
    const char *id = os->use_id;
    const char *idb = "id=\"";
    const char *ide = "\" ";
    if (!id)
    {
        id = "";
        idb = "";
        ide = "";
    }
    const char *clazz = os->use_class;
    const char *space = " ";
    if (!clazz)
    {
        clazz = "";
        space = "";
    }
    snprintf(buf, 1023, "<pre %s%s%sclass=\"xmq %s%s%s\">", idb, id, ide, mode, space, clazz);
    theme->content.pre = buf;

    theme->whitespace.pre  = NULL;
    theme->indentation_whitespace.pre = NULL;
    theme->unicode_whitespace.pre  = "<xmqUW>";
    theme->unicode_whitespace.post  = "</xmqUW>";
    theme->equals.pre      = NULL;
    theme->brace_left.pre  = NULL;
    theme->brace_right.pre = NULL;
    theme->apar_left.pre    = NULL;
    theme->apar_right.pre   = NULL;
    theme->cpar_left.pre = "<xmqCP>";
    theme->cpar_left.post = "</xmqCP>";
    theme->cpar_right.pre = "<xmqCP>";
    theme->cpar_right.post = "</xmqCP>";
    theme->quote.pre = "<xmqQ>";
    theme->quote.post = "</xmqQ>";
    theme->entity.pre = "<xmqE>";
    theme->entity.post = "</xmqE>";
    theme->comment.pre = "<xmqC>";
    theme->comment.post = "</xmqC>";
    theme->comment_continuation.pre = "<xmqC>";
    theme->comment_continuation.post = "</xmqC>";
    theme->element_ns.pre = "<xmqNS>";
    theme->element_ns.post = "</xmqNS>";
    theme->element_name.pre = "<xmqEN>";
    theme->element_name.post = "</xmqEN>";
    theme->element_key.pre = "<xmqEK>";
    theme->element_key.post = "</xmqEK>";
    theme->element_value_text.pre = "<xmqEKV>";
    theme->element_value_text.post = "</xmqEKV>";
    theme->element_value_quote.pre = "<xmqEKV>";
    theme->element_value_quote.post = "</xmqEKV>";
    theme->element_value_entity.pre = "<xmqE>";
    theme->element_value_entity.post = "</xmqE>";
    theme->element_value_compound_quote.pre = "<xmqEKV>";
    theme->element_value_compound_quote.post = "</xmqEKV>";
    theme->element_value_compound_entity.pre = "<xmqE>";
    theme->element_value_compound_entity.post = "</xmqE>";
    theme->attr_ns.pre = "<xmqNS>";
    theme->attr_ns.post = "</xmqNS>";
    theme->attr_key.pre = "<xmqAK>";
    theme->attr_key.post = "</xmqAK>";
    theme->attr_value_text.pre = "<xmqAKV>";
    theme->attr_value_text.post = "</xmqAKV>";
    theme->attr_value_quote.pre = "<xmqAKV>";
    theme->attr_value_quote.post = "</xmqAKV>";
    theme->attr_value_entity.pre = "<xmqE>";
    theme->attr_value_entity.post = "</xmqE>";
    theme->attr_value_compound_quote.pre = "<xmqAKV>";
    theme->attr_value_compound_quote.post = "</xmqAKV>";
    theme->attr_value_compound_entity.pre = "<xmqE>";
    theme->attr_value_compound_entity.post = "</xmqE>";
    theme->ns_declaration.pre = "<xmqNSD>";
    theme->ns_declaration.post = "</xmqNSD>";
    theme->ns_override_xsl.pre = "<xmqXSL>";
    theme->ns_override_xsl.post = "</xmqXSL>";
    theme->ns_colon.pre = NULL;
}

void setup_tex_coloring(XMQOutputSettings *os, XMQTheme *theme, bool dark_mode, bool use_color, bool render_raw)
{
    XMQColorDef *colors = theme->colors_darkbg;
    if (!dark_mode) colors = theme->colors_lightbg;

    os->indentation_space = "\\xmqI ";
    os->explicit_space = " ";
    os->explicit_nl = "\\linebreak\n";

    if (!render_raw)
    {
        theme->document.pre =
            "\\documentclass[10pt,a4paper]{article}\n"
            "\\usepackage{color}\n"
            "\\usepackage{bold-extra}\n";

        char *style_pre = (char*)malloc(4096);
        char *p = style_pre;

        for (int i=0; i<NUM_XMQ_COLOR_NAMES; ++i)
        {
            char buf[128];
            generate_tex_color(buf, 128, &theme->colors_lightbg[i], colorName(i));
            strcpy(p, buf);
            p += strlen(p);
            *p++ = '\n';
        }

        for (int i=0; i<NUM_XMQ_COLOR_NAMES; ++i)
        {
            char buf[128];
            const char *bold_pre = "";
            const char *bold_post = "";
            const char *underline_pre = "";
            const char *underline_post = "";

            if (colors[i].bold)
            {
                bold_pre = "\\textbf{";
                bold_post = "}";
            }
            if (colors[i].underline)
            {
                underline_pre = "\\underline{";
                underline_post = "}";
            }

            snprintf(buf, 128, "\\newcommand{\\%s}[1]{{\\color{%s}%s%s#1%s%s}}\n",
                     colorName(i), colorName(i), bold_pre, underline_pre, bold_post, underline_post);

            strcpy(p, buf);
            p += strlen(p);
        }

        const char *cmds =
            "\\newcommand{\\xmqI}[0]{{\\mbox{\\ }}}\n";

        strcpy(p, cmds);
        p += strlen(p);
        *p = 0;

        theme->style.pre = style_pre;

        theme->body.pre = "\n\\begin{document}\n";
        theme->body.post = "\n\\end{document}\n";
    }

    theme->content.pre = "\\texttt{\\flushleft\\noindent ";
    theme->content.post = "\n}\n";
    theme->whitespace.pre  = NULL;
    theme->indentation_whitespace.pre = NULL;
    theme->unicode_whitespace.pre  = "\\xmqUW{";
    theme->unicode_whitespace.post  = "}";
    theme->equals.pre      = NULL;
    theme->brace_left.pre  = NULL;
    theme->brace_right.pre = NULL;
    theme->apar_left.pre    = NULL;
    theme->apar_right.pre   = NULL;
    theme->cpar_left.pre = "\\xmqCP{";
    theme->cpar_left.post = "}";
    theme->cpar_right.pre = "\\xmqCP{";
    theme->cpar_right.post = "}";
    theme->quote.pre = "\\xmqQ{";
    theme->quote.post = "}";
    theme->entity.pre = "\\xmqE{";
    theme->entity.post = "}";
    theme->comment.pre = "\\xmqC{";
    theme->comment.post = "}";
    theme->comment_continuation.pre = "\\xmqC{";
    theme->comment_continuation.post = "}";
    theme->element_ns.pre = "\\xmqNS{";
    theme->element_ns.post = "}";
    theme->element_name.pre = "\\xmqEN{";
    theme->element_name.post = "}";
    theme->element_key.pre = "\\xmqEK{";
    theme->element_key.post = "}";
    theme->element_value_text.pre = "\\xmqEKV{";
    theme->element_value_text.post = "}";
    theme->element_value_quote.pre = "\\xmqEKV{";
    theme->element_value_quote.post = "}";
    theme->element_value_entity.pre = "\\xmqE{";
    theme->element_value_entity.post = "}";
    theme->element_value_compound_quote.pre = "\\xmqEKV{";
    theme->element_value_compound_quote.post = "}";
    theme->element_value_compound_entity.pre = "\\xmqE{";
    theme->element_value_compound_entity.post = "}";
    theme->attr_ns.pre = "\\xmqNS{";
    theme->attr_ns.post = "}";
    theme->attr_key.pre = "\\xmqAK{";
    theme->attr_key.post = "}";
    theme->attr_value_text.pre = "\\xmqAKV{";
    theme->attr_value_text.post = "}";
    theme->attr_value_quote.pre = "\\xmqAKV{";
    theme->attr_value_quote.post = "}";
    theme->attr_value_entity.pre = "\\xmqE{";
    theme->attr_value_entity.post = "}";
    theme->attr_value_compound_quote.pre = "\\xmqAKV{";
    theme->attr_value_compound_quote.post = "}";
    theme->attr_value_compound_entity.pre = "\\xmqE{";
    theme->attr_value_compound_entity.post = "}";
    theme->ns_declaration.pre = "\\xmqNSD{";
    theme->ns_declaration.post = "}";
    theme->ns_override_xsl.pre = "\\xmqXSL{";
    theme->ns_override_xsl.post = "}";
    theme->ns_colon.pre = NULL;
}

void xmqOverrideSettings(XMQOutputSettings *settings,
                         const char *indentation_space,
                         const char *explicit_space,
                         const char *explicit_tab,
                         const char *explicit_cr,
                         const char *explicit_nl)
{
    if (indentation_space) settings->indentation_space = indentation_space;
    if (explicit_space) settings->explicit_space = explicit_space;
    if (explicit_tab) settings->explicit_tab = explicit_tab;
    if (explicit_cr) settings->explicit_cr = explicit_cr;
    if (explicit_nl) settings->explicit_nl = explicit_nl;
}

void xmqRenderHtmlSettings(XMQOutputSettings *settings,
                           const char *use_id,
                           const char *use_class)
{
    if (use_id) settings->use_id = use_id;
    if (use_class) settings->use_class = use_class;
}

void xmqOverrideColor(XMQOutputSettings *os, const char *render_style, XMQSyntax sy, const char *pre, const char *post, const char *ns)
{
    //
}

int xmqStateErrno(XMQParseState *state)
{
    return (int)state->error_nr;
}

#define X(TYPE) \
    void tokenize_##TYPE(XMQParseState*state, size_t line, size_t col,const char *start,const char *stop,const char *suffix) { \
        if (!state->simulated) { \
            const char *pre, *post;  \
            getThemeStrings(state->output_settings, COLOR_##TYPE, &pre, &post); \
            if (pre) state->output_settings->content.write(state->output_settings->content.writer_state, pre, NULL); \
            if (state->output_settings->render_to == XMQ_RENDER_TERMINAL) { \
                state->output_settings->content.write(state->output_settings->content.writer_state, start, stop); \
            } else if (state->output_settings->render_to == XMQ_RENDER_HTML) { \
                write_safe_html(state->output_settings->content.write, state->output_settings->content.writer_state, start, stop); \
            } else if (state->output_settings->render_to == XMQ_RENDER_TEX) { \
                write_safe_tex(state->output_settings->content.write, state->output_settings->content.writer_state, start, stop); \
            } \
            if (post) state->output_settings->content.write(state->output_settings->content.writer_state, post, NULL); \
        } \
    }
LIST_OF_XMQ_TOKENS
#undef X


const char *xmqStateErrorMsg(XMQParseState *state)
{
    if (!state->generated_error_msg && state->generating_error_msg)
    {
        state->generated_error_msg = free_membuffer_but_return_trimmed_content(state->generating_error_msg);
        state->generating_error_msg = NULL;
    }
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
    XMQOutputSettings *os = (XMQOutputSettings*)malloc(sizeof(XMQOutputSettings));
    memset(os, 0, sizeof(XMQOutputSettings));
    XMQTheme *theme = (XMQTheme*)malloc(sizeof(XMQTheme));
    memset(theme, 0, sizeof(XMQTheme));
    os->theme = theme;

    os->indentation_space = theme->indentation_space = " ";
    os->explicit_space = theme->explicit_space = " ";
    os->explicit_nl = theme->explicit_nl = "\n";
    os->explicit_tab = theme->explicit_tab = "\t";
    os->explicit_cr = theme->explicit_cr = "\r";
    os->add_indent = 4;
    os->use_color = false;

    return os;
}

void xmqFreeOutputSettings(XMQOutputSettings *os)
{
    if (os->theme)
    {
        free(os->theme);
        os->theme = NULL;
    }
    if (os->free_me)
    {
        free(os->free_me);
        os->free_me = NULL;
    }
    if (os->free_and_me)
    {
        free(os->free_and_me);
        os->free_and_me = NULL;
    }
    free(os);
}

void xmqSetAddIndent(XMQOutputSettings *os, int add_indent)
{
    os->add_indent = add_indent;
}

void xmqSetCompact(XMQOutputSettings *os, bool compact)
{
    os->compact = compact;
}

void xmqSetUseColor(XMQOutputSettings *os, bool use_color)
{
    os->use_color = use_color;
}

void xmqSetBackgroundMode(XMQOutputSettings *os, bool bg_dark_mode)
{
    os->bg_dark_mode = bg_dark_mode;
}

void xmqSetEscapeNewlines(XMQOutputSettings *os, bool escape_newlines)
{
    os->escape_newlines = escape_newlines;
}

void xmqSetEscapeNon7bit(XMQOutputSettings *os, bool escape_non_7bit)
{
    os->escape_non_7bit = escape_non_7bit;
}

void xmqSetOutputFormat(XMQOutputSettings *os, XMQContentType output_format)
{
    os->output_format = output_format;
}

void xmqSetRenderFormat(XMQOutputSettings *os, XMQRenderFormat render_to)
{
    os->render_to = render_to;
}

void xmqSetRenderRaw(XMQOutputSettings *os, bool render_raw)
{
    os->render_raw = render_raw;
}

void xmqSetRenderTheme(XMQOutputSettings *os, const char *theme_name)
{
    os->render_theme = theme_name;
}

void xmqSetRenderOnlyStyle(XMQOutputSettings *os, bool only_style)
{
    os->only_style = only_style;
}

void xmqSetWriterContent(XMQOutputSettings *os, XMQWriter content)
{
    os->content = content;
}

void xmqSetWriterError(XMQOutputSettings *os, XMQWriter error)
{
    os->error = error;
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

void write_safe_html(XMQWrite write, void *writer_state, const char *start, const char *stop)
{
    for (const char *i = start; i < stop; ++i)
    {
        const char *amp = "&amp;";
        const char *lt = "&lt;";
        const char *gt = "&gt;";
        const char *quot = "&quot;";
        if (*i == '&') write(writer_state, amp, amp+5);
        else if (*i == '<') write(writer_state, lt, lt+4);
        else if (*i == '>') write(writer_state, gt, gt+4);
        else if (*i == '"') write(writer_state, quot, quot+6); //"
        else write(writer_state, i, i+1);
    }
}

void write_safe_tex(XMQWrite write, void *writer_state, const char *start, const char *stop)
{
    for (const char *i = start; i < stop; ++i)
    {
        const char *amp = "\\&";
        const char *bs = "\\\\";
        const char *us = "\\_";
        if (*i == '&') write(writer_state, amp, amp+2);
        else if (*i == '\\') write(writer_state, bs, bs+2);
        else if (*i == '_') write(writer_state, us, us+2);
        else write(writer_state, i, i+1);
    }
}

void xmqSetupPrintStdOutStdErr(XMQOutputSettings *ps)
{
    ps->content.writer_state = NULL; // Not needed
    ps->content.write = write_print_stdout;
    ps->error.writer_state = NULL; // Not needed
    ps->error.write = write_print_stderr;
}

void xmqSetupPrintMemory(XMQOutputSettings *os, char **start, char **stop)
{
    os->output_buffer_start = start;
    os->output_buffer_stop = stop;
    os->output_buffer = new_membuffer();
    os->content.writer_state = os->output_buffer;
    os->content.write = (XMQWrite)(void*)membuffer_append_region;
    os->error.writer_state = os->output_buffer;
    os->error.write = (XMQWrite)(void*)membuffer_append_region;
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
        state->generated_error_msg = strdup("xmq: you can only tokenize the xmq format");
        state->error_nr = XMQ_ERROR_NOT_XMQ;
        return false;
    }

    state->buffer_start = start;
    state->buffer_stop = stop;
    state->i = start;
    state->line = 1;
    state->col = 1;
    state->error_nr = XMQ_ERROR_NONE;

    if (state->parse->init) state->parse->init(state);

    XMQOutputSettings *output_settings = state->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;

    const char *pre = output_settings->theme->content.pre;
    const char *post = output_settings->theme->content.post;
    if (pre) write(writer_state, pre, NULL);

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
        XMQParseError error_nr = state->error_nr;
        if (error_nr == XMQ_ERROR_INVALID_CHAR && state->last_suspicios_quote_end)
        {
            // Add warning about suspicious quote before the error.
            generate_state_error_message(state, XMQ_WARNING_QUOTES_NEEDED, start, stop);
        }
        generate_state_error_message(state, error_nr, start, stop);

        return false;
    }

    if (post) write(writer_state, post, NULL);

    if (state->parse->done) state->parse->done(state);

    if (output_settings->output_buffer &&
        output_settings->output_buffer_start &&
        output_settings->output_buffer_stop)
    {
        size_t size = membuffer_used(output_settings->output_buffer);
        char *buffer = free_membuffer_but_return_trimmed_content(output_settings->output_buffer);
        *output_settings->output_buffer_start = buffer;
        *output_settings->output_buffer_stop = buffer+size;
    }

    return true;
}

bool xmqTokenizeFile(XMQParseState *state, const char *file)
{
    bool rc = false;
    const char *buffer = NULL;
    size_t fsize = 0;
    XMQContentType content = XMQ_CONTENT_XMQ;

    XMQDoc *doq = xmqNewDoc();

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

    free((void*)buffer);
    xmqFreeDoc(doq);

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
                if (i+4 < stop &&
                    *(i+1) == '?' &&
                    *(i+2) == 'x' &&
                    *(i+3) == 'm' &&
                    *(i+4) == 'l')
                {
                    debug("[XMQ] content detected as xml since <?xml found\n");
                    return XMQ_CONTENT_XML;
                }

                if (i+3 < stop &&
                    *(i+1) == '!' &&
                    *(i+2) == '-' &&
                    *(i+3) == '-')
                {
                    // This is a comment, zip past it.
                    while (i+2 < stop &&
                           !(*(i+0) == '-' &&
                             *(i+1) == '-' &&
                             *(i+2) == '>'))
                    {
                        i++;
                    }
                    i += 3;
                    // No closing comment, return as xml.
                    if (i >= stop)
                    {
                        debug("[XMQ] content detected as xml since comment start found\n");
                        return XMQ_CONTENT_XML;
                    }
                    // Pick up after the comment.
                    c = *i;
                }

                // Starts with <html or < html
                const char *is_html = find_word_ignore_case(i+1, stop, "html");
                if (is_html)
                {
                    debug("[XMQ] content detected as html since html found\n");
                    return XMQ_CONTENT_HTML;
                }

                // Starts with <!doctype  html
                const char *is_doctype = find_word_ignore_case(i, stop, "<!doctype");
                if (is_doctype)
                {
                    i = is_doctype;
                    is_html = find_word_ignore_case(is_doctype+1, stop, "html");
                    if (is_html)
                    {
                        debug("[XMQ] content detected as html since doctype html found\n");
                        return XMQ_CONTENT_HTML;
                    }
                }
                // Otherwise we assume it is xml. If you are working with html content inside
                // the html, then use --html
                debug("[XMQ] content assumed to be xml\n");
                return XMQ_CONTENT_XML; // Or HTML...
            }
            if (c == '{' || c == '"' || c == '[' || (c >= '0' && c <= '9')) // "
            {
                debug("[XMQ] content detected as json\n");
                return XMQ_CONTENT_JSON;
            }
            // Strictly speaking true,false and null are valid xmq files. But we assume
            // it is json since it must be very rare with a single <true> <false> <null> tag in xml/xmq/html/htmq etc.
            // Force xmq with --xmq for the cli command.
            size_t l = 0;
            if (c == 't' || c == 'n') l = 4;
            else if (c == 'f') l = 5;

            if (l != 0)
            {
                if (i+l-1 < stop)
                {
                    if (i+l == stop || (*(i+l) == '\n' && i+l+1 == stop))
                    {
                        if (!strncmp(i, "true", 4) ||
                            !strncmp(i, "false", 5) ||
                            !strncmp(i, "null", 4))
                        {
                            debug("[XMQ] content detected as json since true/false/null found\n");
                            return XMQ_CONTENT_JSON;
                        }
                    }
                }
            }
            debug("[XMQ] content assumed to be xmq\n");
            return XMQ_CONTENT_XMQ;
        }
        i++;
    }

    debug("[XMQ] empty content assumed to be xmq\n");
    return XMQ_CONTENT_XMQ;
}


/** Scan a line, ie until \n pr \r\n or \r or NULL.
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
        if (*i == '\n' || *i == '\r')
        {
            if (*i == '\r' && *(i+1) == '\n') i++;
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

void xmqSetDebug(bool e)
{
    xmq_debug_enabled_ = e;
}

bool xmqDebugging()
{
    return xmq_debug_enabled_;
}

void xmqSetVerbose(bool e)
{
    xmq_verbose_enabled_ = e;
}

bool xmqVerbose() {
    return xmq_verbose_enabled_;
}

static const char *build_error_message(const char* fmt, ...)
{
    char *buf = (char*)malloc(4096);
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 4096, fmt, args);
    va_end(args);
    buf[4095] = 0;
    buf = (char*)realloc(buf, strlen(buf)+1);
    return buf;
}

/**
    xmq_un_quote:
    @indent: The number of chars before the quote starts on the first line.
    @space: Use this space to prepend the first line if needed due to indent.
    @start: Start of buffer to unquote.
    @stop: Points to byte after buffer.

    Do the reverse of xmq_quote, take a quote (with or without the surrounding single quotes)
    and removes any incidental indentation. Returns a newly malloced buffer
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

    Do the reverse of xmq_comment, Takes a comment (including /✻ ✻/ ///✻ ✻///) and removes any incidental
    indentation and trailing spaces. Returns a newly malloced buffer
    that must be free:ed later.

    The indent is 0 if the / first on the line.
    The indent is 1 if there is a single space before the starting / etc.
*/
char *xmq_un_comment(size_t indent, char space, const char *start, const char *stop)
{
    assert(start < stop);

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
        if (*i == ' ') {
            i++;
        }
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

    // Skip a single space immediately after the asterisk or before the ending asterisk.
    // I.e. /* Comment */ results in the comment text "Comment"
    if (*start == ' ')
    {
        start++;
        indent++;
    }
    if (*(stop-1) == ' ')
    {
        if (stop-1 >= start)
        {
            stop--;
        }
    }

    assert(start <= stop);
    char *foo = xmq_trim_quote(indent, space, start, stop);
    return foo;
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
        char *buf = strndup(start, stop-start);
        return buf;
    }

    size_t append_newlines = 0;

    // Check if the final line is all spaces.
    if (has_ending_nl_space(start, stop, NULL))
    {
        // So it is, now trim from the end.
        while (stop > start)
        {
            char c = *(stop-1);
            if (c == '\n') append_newlines++;
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
            stop--;
        }
    }
    if (append_newlines > 0) append_newlines--;

    if (stop == start)
    {
        // Oups! Quote was all space and newlines.
        char *buf = (char*)malloc(append_newlines+1);
        size_t i;
        for (i = 0; i < append_newlines; ++i) buf[i] = '\n';
        buf[i] = 0;
        return buf;
    }

    size_t prepend_newlines = 0;

    // Check if the first line is all spaces.
    if (has_leading_space_nl(start, stop, NULL))
    {
        // The first line is all spaces, trim leading spaces and newlines!
        ignore_first_indent = true;
        // Skip the already scanned first line.
        start = eol;
        const char *i = start;
        while (i < stop)
        {
            char c = *i;
            if (c == '\n')
            {
                start = i+1; // Restart find lines from here.
                prepend_newlines++;
            }
            else if (c != ' ' && c != '\t' && c != '\r') break;
            i++;
        }
    }
    size_t incidental = (size_t)-1;

    if (!ignore_first_indent)
    {
        incidental = indent;
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
            if (found_indent < incidental)  // Their indent is lesser than the so far found.
            {
                // Yep, remember it.
                if (!first_line || ignore_first_indent)
                {
                    incidental = found_indent;
                }
            }
            first_line = false;
        }
        i = eol; // Start at next line, or end at stop.
    }

    size_t prepend_spaces = 0;

    if (!ignore_first_indent &&
        indent >= incidental)
    {
        // The first indent is relevant and it is bigger than the incidental.
        // We need to prepend the output line with spaces that are not in the source!
        // But, only if there is more than one line with actual non spaces!
        prepend_spaces = indent - incidental;
    }

    // Allocate max size of output buffer, it usually becomes smaller
    // when incidental indentation and trailing whitespace is removed.
    size_t n = stop-start+prepend_spaces+prepend_newlines+append_newlines+1;
    char *output = (char*)malloc(n);
    char *o = output;

    // Insert any necessary prepended spaces due to source indentation of the line.
    if (space != 0)
    {
        while (prepend_spaces) { *o++ = space; prepend_spaces--; }
    }

    // Insert any necessary prepended newlines.
    while (prepend_newlines) { *o++ = '\n'; prepend_newlines--; }

    // Start scanning the lines from the beginning again.
    // Now use the found incidental to copy the right parts.
    i = start;

    first_line = true;
    while (i < stop)
    {
        bool has_nl = find_line(i, stop, &found_indent, &after_last_non_space, &eol);

        if (!first_line || ignore_first_indent)
        {
            // For all lines except the first. And for the first line if ignore_first_indent is true.
            // Skip the incidental indentation.
            size_t n = incidental;
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
        }
        // Copy content, but not line ending xml whitespace ie space, tab, cr.
        while (i < after_last_non_space) { *o++ = *i++; }

        if (has_nl)
        {
            *o++ = '\n';
        }
        else
        {
            // The final line has no nl, here we copy any ending spaces as well!
            while (i < eol) { *o++ = *i++; }
        }
        i = eol;
        first_line = false;
    }
    // Insert any necessary appended newlines.
    while (append_newlines) { *o++ = '\n'; append_newlines--; }
    *o++ = 0;
    size_t real_size = o-output;
    output = (char*)realloc(output, real_size);
    return output;
}

void xmqSetupParseCallbacksNoop(XMQParseCallbacks *callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));

#define X(TYPE) callbacks->handle_##TYPE = NULL;
LIST_OF_XMQ_TOKENS
#undef X

    callbacks->magic_cookie = MAGIC_COOKIE;
}

#define WRITE_ARGS(...) state->output_settings->content.write(state->output_settings->content.writer_state, __VA_ARGS__)

#define X(TYPE) void debug_token_##TYPE(XMQParseState*state,size_t line,size_t col,const char*start,const char*stop,const char*suffix) { \
    WRITE_ARGS("["#TYPE, NULL); \
    if (state->simulated) { WRITE_ARGS(" SIM", NULL); } \
    WRITE_ARGS(" \"", NULL); \
    char *tmp = xmq_quote_as_c(start, stop); \
    WRITE_ARGS(tmp, NULL); \
    free(tmp); \
    WRITE_ARGS("\"", NULL); \
    char buf[32]; \
    snprintf(buf, 32, " %zu:%zu]", line, col); \
    buf[31] = 0; \
    WRITE_ARGS(buf, NULL); \
};
LIST_OF_XMQ_TOKENS
#undef X

void xmqSetupParseCallbacksDebugTokens(XMQParseCallbacks *callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));
#define X(TYPE) callbacks->handle_##TYPE = debug_token_##TYPE ;
LIST_OF_XMQ_TOKENS
#undef X

    callbacks->done = add_nl;

    callbacks->magic_cookie = MAGIC_COOKIE;
}

void debug_content_value(XMQParseState *state,
                         size_t line,
                         size_t start_col,
                         const char *start,
                         const char *stop,
                         const char *suffix)
{
    char *tmp = xmq_quote_as_c(start, stop);
    WRITE_ARGS("{value \"", NULL);
    WRITE_ARGS(tmp, NULL);
    WRITE_ARGS("\"}", NULL);
    free(tmp);
}


void debug_content_quote(XMQParseState *state,
                         size_t line,
                         size_t start_col,
                         const char *start,
                         const char *stop,
                         const char *suffix)
{
    size_t indent = start_col-1;
    char *trimmed = xmq_un_quote(indent, ' ', start, stop, true);
    char *tmp = xmq_quote_as_c(trimmed, trimmed+strlen(trimmed));
    WRITE_ARGS("{quote \"", NULL);
    WRITE_ARGS(tmp, NULL);
    WRITE_ARGS("\"}", NULL);
    free(tmp);
    free(trimmed);
}

void debug_content_comment(XMQParseState *state,
                           size_t line,
                           size_t start_col,
                           const char *start,
                           const char *stop,
                           const char *suffix)
{
    size_t indent = start_col-1;
    char *trimmed = xmq_un_comment(indent, ' ', start, stop);
    char *tmp = xmq_quote_as_c(trimmed, trimmed+strlen(trimmed));
    WRITE_ARGS("{comment \"", NULL);
    WRITE_ARGS(tmp, NULL);
    WRITE_ARGS("\"}", NULL);
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

void xmqSetupParseCallbacksColorizeTokens(XMQParseCallbacks *callbacks, XMQRenderFormat render_format)
{
    memset(callbacks, 0, sizeof(*callbacks));

#define X(TYPE) callbacks->handle_##TYPE = tokenize_##TYPE ;
LIST_OF_XMQ_TOKENS
#undef X

    callbacks->magic_cookie = MAGIC_COOKIE;
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

void xmqSetImplementationDoc(XMQDoc *doq, void *doc)
{
    doq->docptr_.xml = (xmlDocPtr)doc;
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

XMQContentType xmqGetOriginalContentType(XMQDoc *doq)
{
    return doq->original_content_type_;
}

size_t xmqGetOriginalSize(XMQDoc *doq)
{
    return doq->original_size_;
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
    if (state->generating_error_msg)
    {
        free_membuffer_and_free_content(state->generating_error_msg);
        state->generating_error_msg = NULL;
    }
    free_stack(state->element_stack);
//    free(state->settings);
    state->output_settings = NULL;
    free(state);
}

void xmqFreeDoc(XMQDoc *doq)
{
    if (!doq) return;
    if (doq->source_name_)
    {
        debug("[XMQ] freeing source name\n");
        free((void*)doq->source_name_);
        doq->source_name_ = NULL;
    }
    if (doq->error_)
    {
        debug("[XMQ] freeing error message\n");
        free((void*)doq->error_);
        doq->error_ = NULL;
    }
    if (doq->docptr_.xml)
    {
        debug("[XMQ] freeing xml doc\n");
        xmlFreeDoc(doq->docptr_.xml);
        doq->docptr_.xml = NULL;
    }
    debug("[XMQ] freeing xmq doc\n");
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

bool xmqParseBuffer(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root, int flags)
{
    bool rc = true;
    XMQOutputSettings *output_settings = xmqNewOutputSettings();
    XMQParseCallbacks *parse = xmqNewParseCallbacks();

    xmq_setup_parse_callbacks(parse);

    XMQParseState *state = xmqNewParseState(parse, output_settings);
    state->merge_text = !(flags & XMQ_FLAG_NOMERGE);
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

bool xmqParseFile(XMQDoc *doq, const char *file, const char *implicit_root, int flags)
{
    bool ok = true;
    char *buffer = NULL;
    size_t fsize = 0;
    XMQContentType content = XMQ_CONTENT_XMQ;
    size_t block_size = 0;
    size_t n = 0;

    xmqSetDocSourceName(doq, file);

    FILE *f = fopen(file, "rb");
    if (!f) {
        doq->errno_ = XMQ_ERROR_CANNOT_READ_FILE;
        doq->error_ = build_error_message("xmq: %s: No such file or directory\n", file);
        ok = false;
        goto exit;
    }

    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = (char*)malloc(fsize + 1);
    if (!buffer)
    {
        doq->errno_ = XMQ_ERROR_OOM;
        doq->error_ = build_error_message("xmq: %s: File too big, out of memory\n", file);
        ok = false;
        goto exit;
    }

    block_size = fsize;
    if (block_size > 10000) block_size = 10000;
    n = 0;
    do {
        // We need to read smaller blocks because of a bug in Windows C-library..... blech.
        if (n + block_size > fsize) block_size = fsize - n;
        size_t r = fread(buffer+n, 1, block_size, f);
        debug("[XMQ] read %zu bytes total %zu\n", r, n);
        if (!r) break;
        n += r;
    } while (n < fsize);

    debug("[XMQ] read total %zu bytes\n", n);

    if (n != fsize)
    {
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

    ok = xmqParseBuffer(doq, buffer, buffer+fsize, implicit_root, flags);

    exit:

    free(buffer);

    return ok;
}

const char *xmqVersion()
{
    return "2.1.0";
}

void do_whitespace(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   const char *stop,
                   const char *suffix)
{
}

xmlNodePtr create_quote(XMQParseState *state,
                       size_t l,
                       size_t col,
                       const char *start,
                       const char *stop,
                       const char *suffix,
                       xmlNodePtr parent)
{
    size_t indent = col - 1;
    char *trimmed = (state->no_trim_quotes)?strndup(start, stop-start):xmq_un_quote(indent, ' ', start, stop, true);
    xmlNodePtr n = xmlNewDocText(state->doq->docptr_.xml, (const xmlChar *)trimmed);
    if (state->merge_text)
    {
        n = xmlAddChild(parent, n);
    }
    else
    {
        // I want to prevent merging of this new text node with previous text nodes....
        // Alas there is no such setting in libxml2 so I perform the addition explicit here.
        // Check the source for xmlAddChild.
        n->parent = parent;
        if (parent->children == NULL)
        {
            parent->children = n;
            parent->last = n;
        }
        else
        {
            xmlNodePtr prev = parent->last;
	    prev->next = n;
            n->prev = prev;
            parent->last = n;
        }
    }
    free(trimmed);
    return n;
}

void do_quote(XMQParseState *state,
              size_t l,
              size_t col,
              const char *start,
              const char *stop,
              const char *suffix)
{
    state->element_last = create_quote(state, l, col, start, stop, suffix,
                                       (xmlNode*)state->element_stack->top->data);
}

xmlNodePtr create_entity(XMQParseState *state,
                         size_t l,
                         size_t c,
                         const char *start,
                         const char *stop,
                         const char *suffix,
                         xmlNodePtr parent)
{
    char *tmp = strndup(start, stop-start);
    xmlNodePtr n = NULL;
    if (tmp[1] == '#')
    {
        // Character entity.
        if (!state->merge_text)
        {
            // Do not merge with surrounding text.
            n = xmlNewCharRef(state->doq->docptr_.xml, (const xmlChar *)tmp);
        }
        else
        {
            // Make inte text that will be merged.
            UTF8Char uni;
            int uc = 0;
            if (tmp[2] == 'x') uc = strtol(tmp+3, NULL, 16);
            else uc = strtol(tmp+2, NULL, 10);
            size_t len = encode_utf8(uc, &uni);
            char buf[len+1];
            memcpy(buf, uni.bytes, len);
            buf[len] = 0;
            n = xmlNewDocText(state->doq->docptr_.xml, (xmlChar*)buf);
        }
    }
    else
    {
        // Named references are kept as is.
        n = xmlNewReference(state->doq->docptr_.xml, (const xmlChar *)tmp);
    }
    n = xmlAddChild(parent, n);
    free(tmp);

    return n;
}

void do_entity(XMQParseState *state,
               size_t l,
               size_t c,
               const char *start,
               const char *stop,
               const char *suffix)
{
    state->element_last = create_entity(state, l, c, start, stop, suffix, (xmlNode*)state->element_stack->top->data);
}

void do_comment(XMQParseState*state,
                size_t line,
                size_t col,
                const char *start,
                const char *stop,
                const char *suffix)
{
    xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
    size_t indent = col-1;
    char *trimmed = (state->no_trim_quotes)?strndup(start, stop-start):xmq_un_comment(indent, ' ', start, stop);
    xmlNodePtr n = xmlNewDocComment(state->doq->docptr_.xml, (const xmlChar *)trimmed);

    if (state->add_pre_node_before)
    {
        // Insert comment before this node.
        xmlAddPrevSibling((xmlNodePtr)state->add_pre_node_before, n);
    }
    else if (state->add_post_node_after)
    {
        // Insert comment after this node.
        xmlAddNextSibling((xmlNodePtr)state->add_post_node_after, n);
    }
    else
    {
        xmlAddChild(parent, n);
    }
    state->element_last = n;
    free(trimmed);
}

void do_comment_continuation(XMQParseState*state,
                             size_t line,
                             size_t col,
                             const char *start,
                             const char *stop,
                             const char *suffix)
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
    size_t indent = col-1;
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
                           const char *stop,
                           const char *suffix)
{
    if (state->parsing_pi)
    {
        char *content = potentially_add_leading_ending_space(start, stop);
        xmlNodePtr n = (xmlNodePtr)xmlNewPI((xmlChar*)state->pi_name, (xmlChar*)content);
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, n);
        free(content);

        state->parsing_pi = false;
        free((char*)state->pi_name);
        state->pi_name = NULL;
    }
    else if (state->parsing_doctype)
    {
        size_t l = stop-start;
        char *tmp = (char*)malloc(l+1);
        memcpy(tmp, start, l);
        tmp[l] = 0;
        state->doq->docptr_.xml->intSubset = xmlNewDtd(state->doq->docptr_.xml, (xmlChar*)tmp, NULL, NULL);
        xmlNodePtr n = (xmlNodePtr)state->doq->docptr_.xml->intSubset;
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, n);
        free(tmp);

        state->parsing_doctype = false;
        state->doctype_found = true;
    }
    else
    {
        xmlNodePtr n = xmlNewDocTextLen(state->doq->docptr_.xml, (const xmlChar *)start, stop-start);
        xmlAddChild((xmlNode*)state->element_last, n);
    }
}

void do_element_value_quote(XMQParseState *state,
                            size_t line,
                            size_t col,
                            const char *start,
                            const char *stop,
                            const char *suffix)
{
    char *trimmed = (state->no_trim_quotes)?strndup(start, stop-start):xmq_un_quote(col-1, ' ', start, stop, true);
    if (state->parsing_pi)
    {
        char *content = potentially_add_leading_ending_space(trimmed, trimmed+strlen(trimmed));
        xmlNodePtr n = (xmlNodePtr)xmlNewPI((xmlChar*)state->pi_name, (xmlChar*)content);
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, n);
        state->parsing_pi = false;
        free((char*)state->pi_name);
        state->pi_name = NULL;
        free(content);
    }
    else if (state->parsing_doctype)
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
        if (state->add_pre_node_before)
        {
            // Insert doctype before this node.
            xmlAddPrevSibling((xmlNodePtr)state->add_pre_node_before, (xmlNodePtr)dtd);
        }
        else
        {
            // Append doctype to document.
            xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
            xmlAddChild(parent, (xmlNodePtr)dtd);
        }
        state->parsing_doctype = false;
        state->doctype_found = true;
        free(buf);
    }
    else
    {
        xmlNodePtr n = xmlNewDocText(state->doq->docptr_.xml, (const xmlChar *)trimmed);
        xmlAddChild((xmlNode*)state->element_last, n);
    }
    free(trimmed);
}

void do_element_value_entity(XMQParseState *state,
                             size_t line,
                             size_t col,
                             const char *start,
                             const char *stop,
                             const char *suffix)
{
    create_entity(state, line, col, start, stop, suffix, (xmlNode*)state->element_last);
}

void do_element_value_compound_quote(XMQParseState *state,
                                     size_t line,
                                     size_t col,
                                     const char *start,
                                     const char *stop,
                                     const char *suffix)
{
    do_quote(state, line, col, start, stop, suffix);
}

void do_element_value_compound_entity(XMQParseState *state,
                                      size_t line,
                                      size_t col,
                                      const char *start,
                                      const char *stop,
                                      const char *suffix)
{
    do_entity(state, line, col, start, stop, suffix);
}

void do_attr_ns(XMQParseState *state,
                size_t line,
                size_t col,
                const char *start,
                const char *stop,
                const char *suffix)
{
    if (!state->declaring_xmlns)
    {
        // Normal attribute namespace found before the attribute key, eg x:alfa=123 xlink:href=http...
        char *ns = strndup(start, stop-start);
        state->attribute_namespace = ns;
    }
    else
    {
        // This is the first namespace after the xmlns declaration, eg. xmlns:xsl = http....
        // The xsl has already been handled in do_ns_declaration that used suffix
        // to peek ahead to the xsl name.
    }
}

void do_ns_declaration(XMQParseState *state,
                       size_t line,
                       size_t col,
                       const char *start,
                       const char *stop,
                       const char *suffix)
{
    // We found a namespace. It is either a default declaration xmlns=... or xmlns:prefix=...
    //
    // We can see the difference here since the parser will invoke with suffix
    // either pointing to stop (xmlns=) or after stop (xmlns:foo=)
    xmlNodePtr element = (xmlNode*)state->element_stack->top->data;
    xmlNsPtr ns = NULL;
    if (stop == suffix)
    {
        // Stop is the same as suffix, so no prefix has been added.
        // I.e. this is a default namespace, eg: xmlns=uri
        ns = xmlNewNs(element,
                      NULL,
                      NULL);
        debug("[XMQ] create default namespace in element %s\n", element->name);
        if (!ns)
        {
            // Oups, this element already has a default namespace.
            // This is probably due to multiple xmlns=uri declarations. Is this an error or not?
            xmlNsPtr *list = xmlGetNsList(state->doq->docptr_.xml,
                                          element);
            for (int i = 0; list[i]; ++i)
            {
                if (!list[i]->prefix)
                {
                    ns = list[i];
                    break;
                }
            }
            free(list);
        }
        if (element->ns == NULL)
        {
            debug("[XMQ] set default namespace in element %s prefix=%s href=%s\n", element->name, ns->prefix, ns->href);
            xmlSetNs(element, ns);
        }
        state->default_namespace = ns;
    }
    else
    {
        // This a new namespace with a prefix. xmlns:prefix=uri
        // Stop points to the colon, suffix points to =.
        // The prefix starts at stop+1.
        size_t len = suffix-(stop+1);
        char *name = strndup(stop+1, len);
        ns = xmlNewNs(element,
                      NULL,
                      (const xmlChar *)name);

        if (!ns)
        {
            // Oups, this namespace has already been created, for example due to the namespace prefix
            // of the element itself, eg: abc:element(xmlns:abc = uri)
            // Lets pick this ns up and reuse it.
            xmlNsPtr *list = xmlGetNsList(state->doq->docptr_.xml,
                                          element);
            for (int i = 0; list[i]; ++i)
            {
                if (list[i]->prefix && !strcmp((char*)list[i]->prefix, name))
                {
                    ns = list[i];
                    break;
                }
            }
            free(list);
        }
        free(name);
    }

    if (!ns)
    {
        fprintf(stderr, "Internal error: expected namespace to be created/found.\n");
        assert(ns);
    }
    state->declaring_xmlns = true;
    state->declaring_xmlns_namespace = ns;
}

void do_attr_key(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 const char *stop,
                 const char *suffix)
{
    size_t n = stop - start;
    char *key = (char*)malloc(n+1);
    memcpy(key, start, n);
    key[n] = 0;

    xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
    xmlAttrPtr attr = NULL;

    if (!state->attribute_namespace)
    {
        // A normal attribute with no namespace.
        attr =  xmlNewProp(parent, (xmlChar*)key, NULL);
    }
    else
    {
        xmlNsPtr ns = xmlSearchNs(state->doq->docptr_.xml,
                                  parent,
                                  (const xmlChar *)state->attribute_namespace);
        if (!ns)
        {
            // The namespaces does not yet exist. Lets create it.. Lets hope it will be declared
            // inside the attributes of this node. Use a temporary href for now.
            ns = xmlNewNs(parent,
                          NULL,
                          (const xmlChar *)state->attribute_namespace);
        }
        attr = xmlNewNsProp(parent, ns, (xmlChar*)key, NULL);
        free(state->attribute_namespace);
        state->attribute_namespace = NULL;
    }

    // The new attribute attr should now be added to the parent elements: properties list.
    // Remember this attr as the last element so that we can set the value.
    state->element_last = attr;

    free(key);
}

void update_namespace_href(XMQParseState *state,
                           xmlNsPtr ns,
                           const char *start,
                           const char *stop)
{
    if (!stop) stop = start+strlen(start);

    char *href = strndup(start, stop-start);
    ns->href = (const xmlChar*)href;
    debug("[XMQ] update namespace prefix=%s with href=%s\n", ns->prefix, href);

    if (start[0] == 0 && ns == state->default_namespace)
    {
        xmlNodePtr element = (xmlNode*)state->element_stack->top->data;
        debug("[XMQ] remove default namespace in element %s\n", element->name);
        xmlSetNs(element, NULL);
        state->default_namespace = NULL;
        return;
    }
}

void do_attr_value_text(XMQParseState *state,
                        size_t line,
                        size_t col,
                        const char *start,
                        const char *stop,
                        const char *suffix)
{
    if (state->declaring_xmlns)
    {
        assert(state->declaring_xmlns_namespace);

        update_namespace_href(state, (xmlNsPtr)state->declaring_xmlns_namespace, start, stop);
        state->declaring_xmlns = false;
        state->declaring_xmlns_namespace = NULL;
        return;
    }
    xmlNodePtr n = xmlNewDocTextLen(state->doq->docptr_.xml, (const xmlChar *)start, stop-start);
    xmlAddChild((xmlNode*)state->element_last, n);
}

void do_attr_value_quote(XMQParseState*state,
                         size_t line,
                         size_t col,
                         const char *start,
                         const char *stop,
                         const char *suffix)
{
    if (state->declaring_xmlns)
    {
        char *trimmed = (state->no_trim_quotes)?strndup(start, stop-start):xmq_un_quote(col-1, ' ', start, stop, true);
        update_namespace_href(state, (xmlNsPtr)state->declaring_xmlns_namespace, trimmed, NULL);
        state->declaring_xmlns = false;
        state->declaring_xmlns_namespace = NULL;
        free(trimmed);
        return;
    }
    create_quote(state, line, col, start, stop, suffix, (xmlNode*)state->element_last);
}

void do_attr_value_entity(XMQParseState *state,
                          size_t line,
                          size_t col,
                          const char *start,
                          const char *stop,
                          const char *suffix)
{
    create_entity(state, line, col, start, stop, suffix, (xmlNode*)state->element_last);
}

void do_attr_value_compound_quote(XMQParseState *state,
                                  size_t line,
                                  size_t col,
                                  const char *start,
                                  const char *stop,
                                  const char *suffix)
{
    do_quote(state, line, col, start, stop, suffix);
}

void do_attr_value_compound_entity(XMQParseState *state,
                                             size_t line,
                                             size_t col,
                                             const char *start,
                                             const char *stop,
                                             const char *suffix)
{
    do_entity(state, line, col, start, stop, suffix);
}

void create_node(XMQParseState *state, const char *start, const char *stop)
{
    size_t len = stop-start;
    char *name = strndup(start, len);

    if (!strcmp(name, "!DOCTYPE"))
    {
        state->parsing_doctype = true;
    }
    else if (name[0] == '?')
    {
        state->parsing_pi = true;
        state->pi_name = strdup(name+1); // Drop the ?
    }
    else
    {
        xmlNodePtr new_node = xmlNewDocNode(state->doq->docptr_.xml, NULL, (const xmlChar *)name, NULL);
        if (state->element_last == NULL)
        {
            if (!state->implicit_root || !strcmp(name, state->implicit_root))
            {
                // There is no implicit root, or name is the same as the implicit root.
                // Then create the root node with name.
                state->element_last = new_node;
                xmlDocSetRootElement(state->doq->docptr_.xml, new_node);
                state->doq->root_.node = new_node;
            }
            else
            {
                // We have an implicit root and it is different from name.
                xmlNodePtr root = xmlNewDocNode(state->doq->docptr_.xml, NULL, (const xmlChar *)state->implicit_root, NULL);
                state->element_last = root;
                xmlDocSetRootElement(state->doq->docptr_.xml, root);
                state->doq->root_.node = root;
                push_stack(state->element_stack, state->element_last);
            }
        }
        xmlNodePtr parent = (xmlNode*)state->element_stack->top->data;
        xmlAddChild(parent, new_node);

        if (state->element_namespace)
        {
            // Have a namespace before the element name, eg abc:work
            xmlNsPtr ns = xmlSearchNs(state->doq->docptr_.xml,
                                      new_node,
                                      (const xmlChar *)state->element_namespace);
            if (!ns)
            {
                // The namespaces does not yet exist. Lets hope it will be declared
                // inside the attributes of this node. Use a temporary href for now.
                ns = xmlNewNs(new_node,
                              NULL,
                              (const xmlChar *)state->element_namespace);
                debug("[XMQ] created namespace prefix=%s in element %s\n", state->element_namespace, name);
            }
            debug("[XMQ] setting namespace prefix=%s for element %s\n", state->element_namespace, name);
            xmlSetNs(new_node, ns);
            free(state->element_namespace);
            state->element_namespace = NULL;
        }
        else if (state->default_namespace)
        {
            // We have a default namespace.
            xmlNsPtr ns = (xmlNsPtr)state->default_namespace;
            assert(ns->prefix == NULL);
            debug("[XMQ] set default namespace with href=%s for element %s\n", ns->href, name);
            xmlSetNs(new_node, ns);
        }

        state->element_last = new_node;
    }

    free(name);
}

void do_element_ns(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   const char *stop,
                   const char *suffix)
{
    char *ns = strndup(start, stop-start);
    state->element_namespace = ns;
}

void do_ns_colon(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 const char *stop,
                 const char *suffix)
{
}

void do_element_name(XMQParseState *state,
                     size_t line,
                     size_t col,
                     const char *start,
                     const char *stop,
                     const char *suffix)
{
    create_node(state, start, stop);
}

void do_element_key(XMQParseState *state,
                    size_t line,
                    size_t col,
                    const char *start,
                    const char *stop,
                    const char *suffix)
{
    create_node(state, start, stop);
}

void do_equals(XMQParseState *state,
               size_t line,
               size_t col,
               const char *start,
               const char *stop,
               const char *suffix)
{
}

void do_brace_left(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   const char *stop,
                   const char *suffix)
{
    push_stack(state->element_stack, state->element_last);
}

void do_brace_right(XMQParseState *state,
                    size_t line,
                    size_t col,
                    const char *start,
                    const char *stop,
                    const char *suffix)
{
    state->element_last = pop_stack(state->element_stack);
}

void do_apar_left(XMQParseState *state,
                 size_t line,
                 size_t col,
                 const char *start,
                 const char *stop,
                 const char *suffix)
{
    push_stack(state->element_stack, state->element_last);
}

void do_apar_right(XMQParseState *state,
                  size_t line,
                  size_t col,
                  const char *start,
                  const char *stop,
                  const char *suffix)
{
    state->element_last = pop_stack(state->element_stack);
}

void do_cpar_left(XMQParseState *state,
                  size_t line,
                  size_t col,
                  const char *start,
                  const char *stop,
                  const char *suffix)
{
    push_stack(state->element_stack, state->element_last);
}

void do_cpar_right(XMQParseState *state,
                   size_t line,
                   size_t col,
                   const char *start,
                   const char *stop,
                   const char *suffix)
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

void copy_quote_settings_from_output_settings(XMQQuoteSettings *qs, XMQOutputSettings *os)
{
    qs->indentation_space = os->indentation_space;
    qs->explicit_space = os->explicit_space;
    qs->explicit_nl = os->explicit_nl;
    qs->prefix_line = os->prefix_line;
    qs->postfix_line = os->prefix_line;
    qs->compact = os->compact;
}


void xmq_print_xml(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    xmq_fixup_html_before_writeout(doq);

    xmlChar *buffer;
    int size;
    xmlDocDumpMemoryEnc(doq->docptr_.xml,
                        &buffer,
                        &size,
                        "utf8");

    membuffer_reuse(output_settings->output_buffer,
                    (char*)buffer,
                    size);

    debug("[XMQ] xmq_print_xml wrote %zu bytes\n", size);
}

void xmq_print_html(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    xmq_fixup_html_before_writeout(doq);
    xmlOutputBufferPtr out = xmlAllocOutputBuffer(NULL);
    if (out)
    {
        htmlDocContentDumpOutput(out, doq->docptr_.html, "utf8");
        const xmlChar *buffer = xmlBufferContent((xmlBuffer *)out->buffer);
        MemBuffer *membuf = output_settings->output_buffer;
        membuffer_append(membuf, (char*)buffer);
        debug("[XMQ] xmq_print_html wrote %zu bytes\n", membuf->used_);
        xmlOutputBufferClose(out);
    }
    /*
    xmlNodePtr child = doq->docptr_.xml->children;
    xmlBuffer *buffer = xmlBufferCreate();
    while (child != NULL)
    {
        xmlNodeDump(buffer, doq->docptr_.xml, child, 0, 0);
        child = child->next;
        }
    const char *c = (const char *)xmlBufferContent(out);
    fputs(c, stdout);
    fputs("\n", stdout);
    xmlBufferFree(buffer);
    */
}

void xmq_print_json(XMQDoc *doq, XMQOutputSettings *os)
{
    xmq_fixup_json_before_writeout(doq);

    void *first = doq->docptr_.xml->children;
    if (!doq || !first) return;
    void *last = doq->docptr_.xml->last;

    XMQPrintState ps = {};
    ps.pre_nodes = new_stack();
    ps.post_nodes = new_stack();
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;
    ps.doq = doq;
    if (os->compact) os->escape_newlines = true;
    ps.output_settings = os;
    assert(os->content.write);

    // Find any leading (doctype/comments) and ending (comments) nodes and store in pre_nodes and post_nodes inside ps.
    // Adjust the first and last pointer.
    collect_leading_ending_comments_doctype(&ps, (xmlNode**)&first, (xmlNode**)&last);
    json_print_object_nodes(&ps, NULL, (xmlNode*)first, (xmlNode*)last);
    write(writer_state, "\n", NULL);

    free_stack(ps.pre_nodes);
    free_stack(ps.post_nodes);
}

void text_print_node(XMQPrintState *ps, xmlNode *node)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;

    if (is_content_node(node))
    {
        const char *content = xml_element_content(node);
        write(writer_state, content, NULL);
    }
    else if (is_entity_node(node))
    {
        const char *name = xml_element_name(node);
        write(writer_state, "<ENTITY>", NULL);
        write(writer_state, name, NULL);
    }
    else if (is_element_node(node))
    {
        text_print_nodes(ps, node->children);
    }
}

void text_print_nodes(XMQPrintState *ps, xmlNode *from)
{
    xmlNode *i = from;

    while (i)
    {
        text_print_node(ps, i);
        i = xml_next_sibling(i);
    }
}

void xmq_print_text(XMQDoc *doq, XMQOutputSettings *os)
{
    void *first = doq->docptr_.xml->children;
    if (!doq || !first) return;

    XMQPrintState ps = {};
    ps.doq = doq;
    ps.output_settings = os;

    text_print_nodes(&ps, (xmlNode*)first);
}

void xmq_print_xmq(XMQDoc *doq, XMQOutputSettings *os)
{
    void *first = doq->docptr_.xml->children;
    if (!doq || !first) return;
    void *last = doq->docptr_.xml->last;

    XMQPrintState ps = {};
    ps.doq = doq;
    if (os->compact) os->escape_newlines = true;
    ps.output_settings = os;
    assert(os->content.write);

    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;
    XMQTheme *theme = os->theme;

    if (theme->document.pre) write(writer_state, theme->document.pre, NULL);
    if (theme->header.pre) write(writer_state, theme->header.pre, NULL);
    if (theme->style.pre) write(writer_state, theme->style.pre, NULL);
    if (theme->header.post) write(writer_state, theme->header.post, NULL);
    if (theme->body.pre) write(writer_state, theme->body.pre, NULL);

    if (theme->content.pre) write(writer_state, theme->content.pre, NULL);
    print_nodes(&ps, (xmlNode*)first, (xmlNode*)last, 0);
    if (theme->content.post) write(writer_state, theme->content.post, NULL);

    if (theme->body.post) write(writer_state, theme->body.post, NULL);
    if (theme->document.post) write(writer_state, theme->document.post, NULL);

    write(writer_state, "\n", NULL);
}

void xmqPrint(XMQDoc *doq, XMQOutputSettings *output_settings)
{
    if (output_settings->output_format == XMQ_CONTENT_XML)
    {
        xmq_print_xml(doq, output_settings);
    }
    else if (output_settings->output_format == XMQ_CONTENT_HTML)
    {
        xmq_print_html(doq, output_settings);
    }
    else if (output_settings->output_format == XMQ_CONTENT_JSON)
    {
        xmq_print_json(doq, output_settings);
    }
    else if (output_settings->output_format == XMQ_CONTENT_TEXT)
    {
        xmq_print_text(doq, output_settings);
    }
    else
    {
        xmq_print_xmq(doq, output_settings);
    }

    if (output_settings->output_buffer &&
        output_settings->output_buffer_start &&
        output_settings->output_buffer_stop)
    {
        size_t size = membuffer_used(output_settings->output_buffer);
        char *buffer = free_membuffer_but_return_trimmed_content(output_settings->output_buffer);
        *output_settings->output_buffer_start = buffer;
        *output_settings->output_buffer_stop = buffer+size;
    }
}

void trim_text_node(xmlNode *node, int flags)
{
    // If node has whitespace preserve set, then do not trim.
    // if (xmlNodeGetSpacePreserve (node)) return;

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
    // This is not entirely whitespace, now use the xmq_un_quote function to remove any incidental indentation.
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

void trim_node(xmlNode *node, int flags)
{
    debug("[XMQ] trim %s\n", xml_element_type_to_string(node->type));

    if (is_content_node(node))
    {
        trim_text_node(node, flags);
        return;
    }

    if (is_comment_node(node))
    {
        trim_text_node(node, flags);
        return;
    }

    // Do not recurse into these
    if (node->type == XML_ENTITY_DECL) return;

    xmlNodePtr i = xml_first_child(node);
    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.
        trim_node(i, flags);
        i = next;
    }
}

void xmqTrimWhitespace(XMQDoc *doq, int flags)
{
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.
        trim_node(i, flags);
        i = next;
    }
}

/*
xmlNode *merge_surrounding_text_nodes(xmlNode *node)
{
    const char *val = (const char *)node->name;
    // Not a hex entity.
    if (val[0] != '#' || val[1] != 'x') return node->next;

    debug("[XMQ] merge hex %s chars %s\n", val, xml_element_type_to_string(node->type));

    UTF8Char uni;
    int uc = strtol(val+2, NULL, 16);
    size_t len = encode_utf8(uc, &uni);
    char buf[len+1];
    memcpy(buf, uni.bytes, len);
    buf[len] = 0;

    xmlNodePtr prev = node->prev;
    xmlNodePtr next = node->next;
    if (prev && prev->type == XML_TEXT_NODE)
    {
        xmlNodeAddContentLen(prev, (xmlChar*)buf, len);
        xmlUnlinkNode(node);
        xmlFreeNode(node);
        debug("[XMQ] merge left\n");
    }
    if (next && next->type == XML_TEXT_NODE)
    {
        xmlNodeAddContent(prev, next->content);
        xmlNode *n = next->next;
        xmlUnlinkNode(next);
        xmlFreeNode(next);
        next = n;
        debug("[XMQ] merge right\n");
    }

    return next;
}

xmlNode *merge_hex_chars_node(xmlNode *node)
{
    if (node->type == XML_ENTITY_REF_NODE)
    {
        return merge_surrounding_text_nodes(node);
    }

    // Do not recurse into these
    if (node->type == XML_ENTITY_DECL) return node->next;

    xmlNodePtr i = xml_first_child(node);
    while (i)
    {
        i = merge_hex_chars_node(i);
    }
    return node->next;
}

void xmqMergeHexCharEntities(XMQDoc *doq)
{
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    while (i)
    {
        i = merge_hex_chars_node(i);
    }
}
*/

char *escape_xml_comment(const char *comment)
{
    // The escape char is ␐ which is utf8 0xe2 0x90 0x90
    size_t escapes = 0;
    const char *i = comment;
    for (; *i; ++i)
    {
        if (*i == '-' && ( *(i+1) == '-' ||
                           (*(const unsigned char*)(i+1) == 0xe2 &&
                            *(const unsigned char*)(i+2) == 0x90 &&
                            *(const unsigned char*)(i+3) == 0x90)))
        {
            escapes++;
        }
    }

    // If no escapes are needed, return NULL.
    if (!escapes) return NULL;

    size_t len = i-comment;
    size_t new_len = len+escapes*3+1;
    char *tmp = (char*)malloc(new_len);

    i = comment;
    char *j = tmp;
    for (; *i; ++i)
    {
        *j++ = *i;
        if (*i == '-' && ( *(i+1) == '-' ||
                           (*(const unsigned char*)(i+1) == 0xe2 &&
                            *(const unsigned char*)(i+2) == 0x90 &&
                            *(const unsigned char*)(i+3) == 0x90)))
        {
            *j++ = 0xe2;
            *j++ = 0x90;
            *j++ = 0x90;
        }
    }
    *j = 0;

    assert( (size_t)((j-tmp)+1) == new_len);
    return tmp;
}

char *unescape_xml_comment(const char *comment)
{
    // The escape char is ␐ which is utf8 0xe2 0x90 0x90
    size_t escapes = 0;
    const char *i = comment;

    for (; *i; ++i)
    {
        if (*i == '-' && (*(const unsigned char*)(i+1) == 0xe2 &&
                          *(const unsigned char*)(i+2) == 0x90 &&
                          *(const unsigned char*)(i+3) == 0x90))
        {
            escapes++;
        }
    }

    // If no escapes need to be removed, then return NULL.
    if (!escapes) return NULL;

    size_t len = i-comment;
    char *tmp = (char*)malloc(len+1);

    i = comment;
    char *j = tmp;
    for (; *i; ++i)
    {
        *j++ = *i;
        if (*i == '-' && (*(const unsigned char*)(i+1) == 0xe2 &&
                          *(const unsigned char*)(i+2) == 0x90 &&
                          *(const unsigned char*)(i+3) == 0x90))
        {
            // Skip the dle quote character.
            i += 3;
        }
    }
    *j++ = 0;

    size_t new_len = j-tmp;
    tmp = (char*)realloc(tmp, new_len);

    return tmp;
}

void fixup_html(XMQDoc *doq, xmlNode *node, bool inside_cdata_declared)
{
    if (node->type == XML_COMMENT_NODE)
    {
        // When writing an xml comment we must replace --- with -␐-␐-.
        // An already existing -␐- is replaced with -␐␐- etc.
        char *new_content = escape_xml_comment((const char*)node->content);
        if (new_content)
        {
            // Oups, the content contains -- which must be quoted as -␐-␐
            // Likewise, if the content contains -␐-␐ it will be quoted as -␐␐-␐␐
            xmlNodePtr new_node = xmlNewComment((const xmlChar*)new_content);
            xmlReplaceNode(node, new_node);
            xmlFreeNode(node);
            free(new_content);
        }
        return;
    }
    else if (node->type == XML_CDATA_SECTION_NODE)
    {
        // When the html is loaded by the libxml2 parser it creates a cdata
        // node instead of a text node for the style content.
        // If this is allowed to be printed as is, then this creates broken html.
        // I.e. <style><![CDATA[h1{color:red;}]]></style>
        // But we want: <style>h1{color:red;}</style>
        // Workaround until I understand the proper fix, just make it a text node.
        node->type = XML_TEXT_NODE;
    }
    else if (is_entity_node(node) && inside_cdata_declared)
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
        xmlNode *next = xml_next_sibling(i); // i might be freed in fixup_html.
        fixup_html(doq, i, false);
        i = next;
    }
}

char *depths_[64] = {};

const char *indent_depth(int i)
{
    if (i < 0 || i > 63) return "----";
    char *c = depths_[i];
    if (!c)
    {
        c = (char*)malloc(i*4+1);
        memset(c, ' ', i*4);
        c[i*4] = 0;
        depths_[i] = c;
    }
    return c;
}

void free_indent_depths()
{
    for (int i = 0; i < 64; ++i)
    {
        if (depths_[i])
        {
            free(depths_[i]);
            depths_[i] = NULL;
        }
    }
}

const char *xml_element_type_to_string(xmlElementType type)
{
    switch (type)
    {
	case XML_ELEMENT_NODE: return "element";
	case XML_ATTRIBUTE_NODE: return "attribute";
	case XML_TEXT_NODE: return "text";
	case XML_CDATA_SECTION_NODE: return "cdata";
	case XML_ENTITY_REF_NODE: return "entity_ref";
	case XML_ENTITY_NODE: return "entity";
	case XML_PI_NODE: return "pi";
	case XML_COMMENT_NODE: return "comment";
	case XML_DOCUMENT_NODE: return "document";
	case XML_DOCUMENT_TYPE_NODE: return "document_type";
	case XML_DOCUMENT_FRAG_NODE: return "document_frag";
	case XML_NOTATION_NODE: return "notation";
	case XML_HTML_DOCUMENT_NODE: return "html_document";
	case XML_DTD_NODE: return "dtd";
	case XML_ELEMENT_DECL: return "element_decl";
	case XML_ATTRIBUTE_DECL: return "attribute_decl";
	case XML_ENTITY_DECL: return "entity_decl";
	case XML_NAMESPACE_DECL: return "namespace_decl";
	case XML_XINCLUDE_START: return "xinclude_start";
	case XML_XINCLUDE_END: return "xinclude_end";
	case XML_DOCB_DOCUMENT_NODE: return "docb_document";
    }
    return "?";
}

void fixup_comments(XMQDoc *doq, xmlNode *node, int depth)
{
    debug("[XMQ] fixup comments %s|%s %s\n", indent_depth(depth), node->name, xml_element_type_to_string(node->type));
    if (node->type == XML_COMMENT_NODE)
    {
        // An xml comment containing dle escapes for example: -␐-␐- is replaceed with ---.
        // If multiple dle escapes exists, then for example: -␐␐- is replaced with -␐-.
        char *content_needed_escaping = unescape_xml_comment((const char*)node->content);
        if (content_needed_escaping)
        {
            if (xmq_debug_enabled_)
            {
                char *from = xmq_quote_as_c((const char*)node->content, NULL);
                char *to = xmq_quote_as_c(content_needed_escaping, NULL);
                debug("[XMQ] fix comment \"%s\" to \"%s\"\n", from, to);
            }

            xmlNodePtr new_node = xmlNewComment((const xmlChar*)content_needed_escaping);
            xmlReplaceNode(node, new_node);
            xmlFreeNode(node);
            free(content_needed_escaping);
        }
        return;
    }

    // Do not recurse into these
    if (node->type == XML_ENTITY_DECL) return;

    xmlNode *i = xml_first_child(node);
    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.
        fixup_comments(doq, i, depth+1);
        i = next;
    }
}

void xmq_fixup_comments_after_readin(XMQDoc *doq)
{
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    debug("[XMQ] fixup comments after readin\n");

    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in fixup_comments.
        fixup_comments(doq, i, 0);
        i = next;
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

void xmqSetStateSourceName(XMQParseState *state, const char *source_name)
{
    if (source_name)
    {
        size_t l = strlen(source_name);
        state->source_name = (char*)malloc(l+1);
        strcpy(state->source_name, source_name);
    }
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

        short_start = has_leading_space_nl(start, stop, NULL);
        if (!short_start) short_start = start;
        short_stop = has_ending_nl_space(start, stop, NULL);
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

char *xmq_quote_with_entity_newlines(const char *start, const char *stop, XMQQuoteSettings *settings)
{
    // This code must only be invoked if there is at least one newline inside the to-be quoted text!
    MemBuffer *mb = new_membuffer();

    const char *i = start;
    bool found_nl = false;
    while (i < stop)
    {
        int numq;
        int lq = 0;
        int eq = 0;
        size_t line_len = line_length(i, stop, &numq, &lq, &eq);
        i += lq;
        for (int j = 0; j < lq; ++j) membuffer_append(mb, "&#39;");
        if (line_len > 0)
        {
            if (numq == 0 && (settings->force)) numq = 1; else numq++;
            if (numq == 2) numq++;
            for (int i=0; i<numq; ++i) membuffer_append(mb, "'");
            membuffer_append_region(mb, i, i+line_len);
            for (int i=0; i<numq; ++i) membuffer_append(mb, "'");
        }
        for (int j = 0; j < eq; ++j) membuffer_append(mb, "&#39;");
        i += line_len+eq;
        if (i < stop && *i == '\n')
        {
            if (!found_nl) found_nl = true;
            membuffer_append(mb, "&#10;");
            i++;
        }
    }
    return free_membuffer_but_return_trimmed_content(mb);
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

int xmqForeach(XMQDoc *doq, const char *xpath, XMQNodeCallback cb, void *user_data)
{
    return xmqForeachRel(doq, xpath, cb, user_data, NULL);
}

int xmqForeachRel(XMQDoc *doq, const char *xpath, XMQNodeCallback cb, void *user_data, XMQNode *relative)
{
    xmlDocPtr doc = (xmlDocPtr)xmqGetImplementationDoc(doq);
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    if (!ctx) return 0;

    if (relative && relative->node)
    {
        xmlXPathSetContextNode(relative->node, ctx);
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

int32_t xmqGetInt(XMQDoc *doq, const char *xpath)
{
    return xmqGetIntRel(doq, xpath, NULL);
}

int32_t xmqGetIntRel(XMQDoc *doq, const char *xpath, XMQNode *relative)
{
    const char *content = NULL;

    xmqForeachRel(doq, xpath, catch_single_content, (void*)&content, relative);

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

int64_t xmqGetLong(XMQDoc *doq, const char *xpath)
{
    return xmqGetLongRel(doq, xpath, NULL);
}

int64_t xmqGetLongRel(XMQDoc *doq, const char *xpath, XMQNode *relative)
{
    const char *content = NULL;

    xmqForeachRel(doq, xpath, catch_single_content, (void*)&content, relative);

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

const char *xmqGetString(XMQDoc *doq, const char *xpath)
{
    return xmqGetStringRel(doq, xpath, NULL);
}

const char *xmqGetStringRel(XMQDoc *doq, const char *xpath, XMQNode *relative)
{
    const char *content = NULL;

    xmqForeachRel(doq, xpath, catch_single_content, (void*)&content, relative);

    return content;
}

double xmqGetDouble(XMQDoc *doq, const char *xpath)
{
    return xmqGetDoubleRel(doq, xpath, NULL);
}

double xmqGetDoubleRel(XMQDoc *doq, const char *xpath, XMQNode *relative)
{
    const char *content = NULL;

    xmqForeachRel(doq, xpath, catch_single_content, (void*)&content, relative);

    if (!content) return 0;

    return atof(content);
}

bool xmq_parse_buffer_xml(XMQDoc *doq, const char *start, const char *stop, int flags)
{
    /* Macro to check API for match with the DLL we are using */
    LIBXML_TEST_VERSION ;

    int parse_options = XML_PARSE_NOCDATA | XML_PARSE_NONET;
    bool should_trim = false;
    if (flags & XMQ_FLAG_TRIM_HEURISTIC ||
        flags & XMQ_FLAG_TRIM_EXACT) should_trim = true;
    if (flags & XMQ_FLAG_TRIM_NONE) should_trim = false;

    if (should_trim) parse_options |= XML_PARSE_NOBLANKS;

    xmlDocPtr doc = xmlReadMemory(start, stop-start, doq->source_name_, NULL, parse_options);
    if (doc == NULL)
    {
        doq->errno_ = XMQ_ERROR_PARSING_XML;
        // Let libxml2 print the error message.
        doq->error_ = NULL;
        return false;
    }

    if (doq->docptr_.xml)
    {
        xmlFreeDoc(doq->docptr_.xml);
    }

    doq->docptr_.xml = doc;

    xmq_fixup_comments_after_readin(doq);

    return true;
}

bool xmq_parse_buffer_html(XMQDoc *doq, const char *start, const char *stop, int flags)
{
    htmlDocPtr doc;
    xmlNode *roo_element = NULL;

    /* Macro to check API for match with the DLL we are using */
    LIBXML_TEST_VERSION

    int parse_options = HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET;

    bool should_trim = false;
    if (flags & XMQ_FLAG_TRIM_HEURISTIC ||
        flags & XMQ_FLAG_TRIM_EXACT) should_trim = true;
    if (flags & XMQ_FLAG_TRIM_NONE) should_trim = false;

    if (should_trim) parse_options |= HTML_PARSE_NOBLANKS;

    doc = htmlReadMemory(start, stop-start, "foof", NULL, parse_options);

    if (doc == NULL)
    {
        doq->errno_ = XMQ_ERROR_PARSING_HTML;
        // Let libxml2 print the error message.
        doq->error_ = NULL;
        return false;
    }

    roo_element = xmlDocGetRootElement(doc);

    if (roo_element == NULL)
    {
        PRINT_ERROR("empty document\n");
        xmlFreeDoc(doc);
        return 0;
    }

    if (doq->docptr_.html)
    {
        xmlFreeDoc(doq->docptr_.html);
    }
    doq->docptr_.html = doc;

    xmq_fixup_comments_after_readin(doq);

    return true;
}

bool xmq_parse_buffer_text(XMQDoc *doq, const char *start, const char *stop, const char *implicit_root)
{
    char *buffer = strndup(start, stop-start);
    xmlNodePtr text = xmlNewDocText(doq->docptr_.xml, (xmlChar*)buffer);
    free(buffer);

    if (implicit_root && implicit_root[0])
    {
        // We have an implicit root must be created since input is text.
        xmlNodePtr root = xmlNewDocNode(doq->docptr_.xml, NULL, (const xmlChar *)implicit_root, NULL);
        xmlDocSetRootElement(doq->docptr_.xml, root);
        doq->root_.node = root;
        xmlAddChild(root, text);
    }
    else
    {
        // There is no implicit root. Text is the new root node.
        xmlDocSetRootElement(doq->docptr_.xml, text);
    }
    return true;
}

bool xmqParseBufferWithType(XMQDoc *doq,
                            const char *start,
                            const char *stop,
                            const char *implicit_root,
                            XMQContentType ct,
                            int flags)
{
    bool ok = true;

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
        if (ct != detected_ct && ct != XMQ_CONTENT_TEXT)
        {
            if (detected_ct == XMQ_CONTENT_XML && ct == XMQ_CONTENT_HTML)
            {
                // This is fine! We might be loading a fragment of html
                // that is detected as xml.
            }
            else
            {
                switch (ct) {
                case XMQ_CONTENT_XMQ: doq->errno_ = XMQ_ERROR_EXPECTED_XMQ; break;
                case XMQ_CONTENT_HTMQ: doq->errno_ = XMQ_ERROR_EXPECTED_HTMQ; break;
                case XMQ_CONTENT_XML: doq->errno_ = XMQ_ERROR_EXPECTED_XML; break;
                case XMQ_CONTENT_HTML: doq->errno_ = XMQ_ERROR_EXPECTED_HTML; break;
                case XMQ_CONTENT_JSON: doq->errno_ = XMQ_ERROR_EXPECTED_JSON; break;
                default: break;
                }
                ok = false;
                goto exit;
            }
        }
    }

    doq->original_content_type_ = detected_ct;
    doq->original_size_ = stop-start;

    switch (ct)
    {
    case XMQ_CONTENT_XMQ: ok = xmqParseBuffer(doq, start, stop, implicit_root, flags); break;
    case XMQ_CONTENT_HTMQ: ok = xmqParseBuffer(doq, start, stop, implicit_root, flags); break;
    case XMQ_CONTENT_XML: ok = xmq_parse_buffer_xml(doq, start, stop, flags); break;
    case XMQ_CONTENT_HTML: ok = xmq_parse_buffer_html(doq, start, stop, flags); break;
    case XMQ_CONTENT_JSON: ok = xmq_parse_buffer_json(doq, start, stop, implicit_root); break;
    case XMQ_CONTENT_TEXT: ok = xmq_parse_buffer_text(doq, start, stop, implicit_root); break;
    default: break;
    }

exit:

    if (ok)
    {
        bool should_trim = false;

        if (flags & XMQ_FLAG_TRIM_HEURISTIC ||
            flags & XMQ_FLAG_TRIM_EXACT) should_trim = true;

        if (!(flags & XMQ_FLAG_TRIM_NONE) &&
            (ct == XMQ_CONTENT_XML ||
             ct == XMQ_CONTENT_HTML))
        {
            should_trim = true;
        }

        if (should_trim) xmqTrimWhitespace(doq, flags);
    }

    return ok;
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
        if (n == 0) {
            break;
        }
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            PRINT_ERROR("Could not read stdin errno=%d\n", errno);
            close(fd);

            return false;
        }
        membuffer_append_region(mb, block, block + n);
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
    size_t block_size = 0;
    size_t n = 0;

    FILE *f = fopen(file, "rb");
    if (!f) {
        doq->errno_ = XMQ_ERROR_CANNOT_READ_FILE;
        doq->error_ = build_error_message("xmq: %s: No such file or directory\n", file);
        return false;
    }

    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    debug("[XMQ] file size %zu\n", fsize);

    buffer = (char*)malloc(fsize + 1);
    if (!buffer)
    {
        doq->errno_ = XMQ_ERROR_OOM;
        doq->error_ = build_error_message("xmq: %s: File too big, out of memory\n", file);
        goto exit;
    }

    block_size = fsize;
    if (block_size > 10000) block_size = 10000;
    n = 0;
    do {
        if (n + block_size > fsize) block_size = fsize - n;
        size_t r = fread(buffer+n, 1, block_size, f);
        debug("[XMQ] read %zu bytes total %zu\n", r, n);
        if (!r) break;
        n += r;
    } while (n < fsize);

    debug("[XMQ] read total %zu bytes fsize %zu bytes\n", n, fsize);

    if (n != fsize) {
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
                          int flags)
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

    rc = xmqParseBufferWithType(doq, buffer, buffer+fsize, implicit_root, ct, flags);

    free((void*)buffer);

    return rc;
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

bool xmq_parse_buffer_json(XMQDoc *doq,
                           const char *start,
                           const char *stop,
                           const char *implicit_root)
{
    bool rc = true;
    XMQOutputSettings *os = xmqNewOutputSettings();
    XMQParseCallbacks *parse = xmqNewParseCallbacks();

    xmq_setup_parse_callbacks(parse);

    XMQParseState *state = xmqNewParseState(parse, os);
    state->no_trim_quotes = true;
    state->doq = doq;
    xmqSetStateSourceName(state, doq->source_name_);

    if (implicit_root != NULL && implicit_root[0] == 0) implicit_root = NULL;

    state->implicit_root = implicit_root;

    push_stack(state->element_stack, doq->docptr_.xml);
    // Now state->element_stack->top->data points to doq->docptr_;
    state->element_last = NULL; // Will be set when the first node is created.
    // The doc root will be set when the first element node is created.

    // Time to tokenize the buffer and invoke the parse callbacks.
    xmq_tokenize_buffer_json(state, start, stop);

    if (xmqStateErrno(state))
    {
        rc = false;
        doq->errno_ = xmqStateErrno(state);
        doq->error_ = build_error_message("%s\n", xmqStateErrorMsg(state));
    }

    xmqFreeParseState(state);
    xmqFreeParseCallbacks(parse);
    xmqFreeOutputSettings(os);

    return rc;
}

// PARTS ALWAYS_C ////////////////////////////////////////

#ifdef ALWAYS_MODULE

void check_malloc(void *a)
{
    if (!a)
    {
        PRINT_ERROR("libxmq: Out of memory!\n");
        exit(1);
    }
}

bool xmq_verbose_enabled_ = false;

void verbose__(const char* fmt, ...)
{
    if (xmq_verbose_enabled_) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

bool xmq_debug_enabled_ = false;

void debug__(const char* fmt, ...)
{
    if (xmq_debug_enabled_) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

#ifdef PLATFORM_WINAPI
char *strndup(const char *s, size_t l)
{
    char *buf = (char*)malloc(l+1);
    size_t i = 0;
    for (; i < l; ++i)
    {
        buf[i] = s[i];
        if (!s[i]) break;
    }
    if (i < l)
    {
        // The string s was shorter than l. We have already copied the null terminator.
        // Just resize.
        buf = (char*)realloc(buf, i+1);
    }
    else
    {
        // We copied l bytes. Store a null-terminator in the position i (== l).
        buf[i] = 0;
    }
    return buf;
}
#endif


#endif // ALWAYS_MODULE

// PARTS COLORS_C ////////////////////////////////////////

#ifdef COLORS_MODULE

bool hex_to_number(char c, char cc, int *v);

/**
   get_color: Lookup the color strings
   coloring: The table of colors.
   c: The color to use from the table.
   pre: Store a pointer to the start color string here.
   post: Store a pointer to the end color string here.
*/
void getThemeStrings(XMQOutputSettings *os, XMQColor color, const char **pre, const char **post)
{
    XMQTheme *theme = os->theme;
    switch(color)
    {

#define X(TYPE) case COLOR_##TYPE: *pre = theme->TYPE.pre; *post = theme->TYPE.post; return;
LIST_OF_XMQ_TOKENS
#undef X

    case COLOR_unicode_whitespace: *pre = theme->unicode_whitespace.pre; *post = theme->unicode_whitespace.post; return;
    case COLOR_indentation_whitespace: *pre = theme->indentation_whitespace.pre; *post = theme->indentation_whitespace.post; return;
    case COLOR_ns_override_xsl: *pre = theme->ns_override_xsl.pre; *post = theme->ns_override_xsl.post; return;
    default:
        *pre = NULL;
        *post = NULL;
        return;
    }
    assert(false);
    *pre = "";
    *post = "";
}

// Set background color: echo -ne "\033]11;#53186f\007"

//#define DEFAULT_COLOR "\033]11;#aa186f\007"
//#echo -ne '\e]10;#123456\e\\'  # set default foreground to #123456
//#echo -ne '\e]11;#abcdef\e\\'  # set default background to #abcdef
//printf "\x1b[38;2;40;177;249mTRUECOLOR\x1b[0m\n"

bool string_to_color_def(const char *s, XMQColorDef *def)
{
    // #aabbcc
    // #aabbcc_B
    // #aabbcc_U
    // #aabbcc_B_U

    def->r = -1;
    def->g = -1;
    def->b = -1;
    def->bold = false;
    def->underline = false;

    int r, g, b;
    bool bold, underline;

    r = g = b = 0;
    bold = false;
    underline = false;

    if (strlen(s) < 7) return false;
    if (*s != '#') return false;
    s++;

    bool ok = hex_to_number(*(s+0), *(s+1), &r);
    ok &= hex_to_number(*(s+2), *(s+3), &g);
    ok &= hex_to_number(*(s+4), *(s+5), &b);

    if (!ok) return false;

    s+=6;
    if (*s == '_')
    {
        if (*(s+1) == 'B') bold = true;
        if (*(s+1) == 'U') underline = true;
        s += 2;
    }
    if (*s == '_')
    {
        if (*(s+1) == 'B') bold = true;
        if (*(s+1) == 'U') underline = true;
        s += 2;
    }
    if (*s != 0) return false;

    def->r = r;
    def->g = g;
    def->b = b;
    def->bold = bold;
    def->underline = underline;

    return true;
}

bool hex_to_number(char c, char cc, int *v)
{
    int hi = 0;
    if (c >= '0' && c <= '9') hi = (int)c-'0';
    else if (c >= 'a' && c <= 'f') hi = 10+(int)c-'a';
    else if (c >= 'A' && c <= 'F') hi = 10+(int)c-'A';
    else return false;

    int lo = 0;
    if (cc >= '0' && cc <= '9') lo = (int)cc-'0';
    else if (cc >= 'a' && cc <= 'f') lo = 10+(int)cc-'a';
    else if (cc >= 'A' && cc <= 'F') lo = 10+(int)cc-'A';
    else return false;

    *v = hi*16+lo;
    return true;
}

bool generate_ansi_color(char *buf, size_t buf_size, XMQColorDef *def)
{
    // Example: \x1b[38;2;40;177;249mTRUECOLOR\x1b[0m
    if (buf_size < 32) return false;

    char *i = buf;

    *i++ = 27;
    *i++ = '[';
    *i++ = '0';
    *i++ = ';';
    if (def->bold)
    {
        *i++ = '1';
        *i++ = ';';
    }
    if (def->underline) {
        *i++ = '4';
        *i++ = ';';
    }
    *i++ = '3';
    *i++ = '8';
    *i++ = ';';
    *i++ = '2';
    *i++ = ';';

    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", def->r);
    strcpy(i, tmp);
    i += strlen(tmp);
    *i++ = ';';

    snprintf(tmp, sizeof(tmp), "%d", def->g);
    strcpy(i, tmp);
    i += strlen(tmp);
    *i++ = ';';

    snprintf(tmp, sizeof(tmp), "%d", def->b);
    strcpy(i, tmp);
    i += strlen(tmp);
    *i++ = 'm';
    *i++ = 0;

    return true;
}

bool generate_html_color(char *buf, size_t buf_size, XMQColorDef *def, const char *name)
{
    // xmqQ{color:#26a269;font-weight:600;}

    if (buf_size < 64) return false;

    char *i = buf;
    i += snprintf(i, buf_size, "%s{color:#%02x%02x%02x", name, def->r, def->g, def->b);
    *i++ = ';';

    if (def->bold)
    {
        const char *tmp = "font-weight:600;";
        strcpy(i, tmp);
        i += strlen(tmp);
    }

    if (def->underline)
    {
        const char *tmp = "text-decoration:underline;";
        strcpy(i, tmp);
        i += strlen(tmp);
    }

    *i++ = '}';
    *i++ = 0;
    return false;
}

bool generate_tex_color(char *buf, size_t buf_size, XMQColorDef *def, const char *name)
{
    // \definecolor{mypink2}{RGB}{219, 48, 122}

    if (buf_size < 128) return false;

    snprintf(buf, buf_size, "\\definecolor{%s}{RGB}{%d,%d,%d}", name, def->r, def->g, def->b);
    return true;
}

const char *color_names[13] = {
    "xmqC", // Comment
    "xmqQ", // Quote
    "xmqE", // Entity
    "xmqNS", // Name Space (both for element and attribute)
    "xmqEN", // Element Name
    "xmqEK", // Element Key
    "xmqEKV", // Element Key Value
    "xmqAK", // Attribute Key
    "xmqAKV", // Attribute Key Value
    "xmqCP", // Compound Parentheses
    "xmqNSD", // Name Space declaration xmlns
    "xmqUW", // Unicode whitespace
    "xmqXSL", // Element color for xsl transform elements.
};

const char* colorName(int i)
{
    return color_names[i];
}

void setColorDef(XMQColorDef *cd, int r, int g, int b, bool bold, bool underline);

void setColorDef(XMQColorDef *cd, int r, int g, int b, bool bold, bool underline)
{
    cd->r = r;
    cd->g = g;
    cd->b = b;
    cd->bold = bold;
    cd->underline = underline;
}

#endif // COLORS_MODULE

// PARTS DEFAULT_THEMES_C ////////////////////////////////////////

#ifdef DEFAULT_THEMES_MODULE

const char *defaultColor(int i, const char *theme_name);

const char *default_darkbg_colors[NUM_XMQ_COLOR_NAMES] = {
    "#2aa1b3", // XMQ_COLOR_C
    "#26a269_B", // XMQ_COLOR_Q
    "#c061cb", // XMQ_COLOR_E
    "#a9a9a9", // XMQ_COLOR_NS
    "#ff8c00", // XMQ_COLOR_EN
    "#88b4f7", // XMQ_COLOR_EK
    "#26a269_B", // XMQ_COLOR_EKV
    "#88b4f7", // XMQ_COLOR_AK
    "#6196ec", // XMQ_COLOR_AKV
    "#c061cb", // XMQ_COLOR_CP
    "#2aa1b3", // XMQ_COLOR_NSD
    "#880000_U", // XMQ_COLOR_UW
    "#c061cb" // XMQ_COLOR_XSL
};

const char *win_darkbg_ansi[NUM_XMQ_COLOR_NAMES] = {
    "\033[96m\033[24m", // XMQ_COLOR_C --- CYAN
    "\033[92m\033[24m", // XMQ_COLOR_Q --- GREEN
    "\033[95m\033[24m", // XMQ_COLOR_E --- MAGENTA
    "\033[37m\033[24m", // XMQ_COLOR_NS --- GRAY
    "\033[93m\033[24m", // XMQ_COLOR_EN --- ORANGE
    "\033[36m\033[24m", // XMQ_COLOR_EK --- LIGHT BLUE
    "\033[92m\033[24m", // XMQ_COLOR_EKV --- GREEN
    "\033[36m\033[24m", // XMQ_COLOR_AK --- LIGHT BLUE
    "\033[94m\033[24m", // XMQ_COLOR_AKV --- BLUE
    "\033[95m\033[24m", // XMQ_COLOR_CP --- MAGENTA
    "\033[36m\033[24m", // XMQ_COLOR_NSD --- LIGHT BLUE
    "\033[91m\033[4m", // XMQ_COLOR_UW --- RED UNDERLINE
    "\033[95m\033[24m", // XMQ_COLOR_XSL -- MAGENTA
};

const char *default_lightbg_colors[NUM_XMQ_COLOR_NAMES] = {
    "#2aa1b3", // XMQ_COLOR_C
    "#26a269_B", // XMQ_COLOR_Q
    "#c061cb", // XMQ_COLOR_E
    "#696969", // XMQ_COLOR_NS
    "#a86c00", // XMQ_COLOR_EN
    "#0060fd", // XMQ_COLOR_EK
    "#26a269_B", // XMQ_COLOR_EKV
    "#0060fd", // XMQ_COLOR_AK
    "#12488c", // XMQ_COLOR_AKV
    "#c061cb", // XMQ_COLOR_CP
    "#1a91a3", // XMQ_COLOR_NSD
    "#880000_U", // XMQ_COLOR_UW
    "#c061cb" // XMQ_COLOR_XSL
};

const char *ansiWin(int i)
{
    return win_darkbg_ansi[i];
}

const char *defaultColor(int i, const char *theme_name)
{
    if (!strcmp(theme_name, "lightbg")) return default_lightbg_colors[i];
    return default_darkbg_colors[i];
}

void installDefaultThemeColors(XMQTheme *theme)
{
    XMQColorDef *colors = theme->colors_darkbg;
    for (int i = 0; i < NUM_XMQ_COLOR_NAMES; ++i)
    {
        const char *color = defaultColor(i, "darkbg");
        string_to_color_def(color, &colors[i]);
    }

    colors = theme->colors_lightbg;
    for (int i = 0; i < NUM_XMQ_COLOR_NAMES; ++i)
    {
        const char *color = defaultColor(i, "lightbg");
        string_to_color_def(color, &colors[i]);
    }
}

#endif // DEFAULT_THEMES_MODULE

// PARTS ENTITIES_C ////////////////////////////////////////

#ifdef ENTITIES_MODULE

// This is not complete yet...

#define HTML_GREEK \
X(913,Alpha,"Α","Alpha") \
X(914,Beta,"Β","Beta") \
X(915,Gamma,"Γ","Gamma") \
X(916,Delta,"Δ","Delta") \
X(917,Epsilon,"Ε","Epsilon") \
X(918,Zeta,"Ζ","Zeta") \
X(919,Eta,"Η","Eta") \
X(920,Theta,"Θ","Theta") \
X(921,Iota,"Ι","Iota") \
X(922,Kappa,"Κ","Kappa") \
X(923,Lambda,"Λ","Lambda") \
X(924,Mu,"Μ","Mu") \
X(925,Nu,"Ν","Nu") \
X(926,Xi,"Ξ","Xi") \
X(927,Omicron,"Ο","Omicron") \
X(928,Pi,"Π","Pi") \
X(929,Rho,"Ρ","Rho") \
X(931,Sigma,"Σ","Sigma") \
X(932,Tau,"Τ","Tau") \
X(933,Upsilon,"Υ","Upsilon") \
X(934,Phi,"Φ","Phi") \
X(935,Chi,"Χ","Chi") \
X(936,Psi,"Ψ","Psi") \
X(937,Omega,"Ω","Omega") \
X(945,alpha,"α","alpha") \
X(946,beta,"β","beta") \
X(947,gamma,"γ","gamma") \
X(948,delta,"δ","delta") \
X(949,epsilon,"ε","epsilon") \
X(950,zeta,"ζ","zeta") \
X(951,eta,"η","eta") \
X(952,theta,"θ","theta") \
X(953,iota,"ι","iota") \
X(954,kappa,"κ","kappa") \
X(955,lambda,"λ","lambda") \
X(956,mu,"μ","mu") \
X(957,nu,"ν","nu") \
X(958,xi,"ξ","xi") \
X(959,omicron,"ο","omicron") \
X(960,pi,"π","pi") \
X(961,rho,"ρ","rho") \
X(962,sigmaf,"ς","sigmaf") \
X(963,sigma,"σ","sigma") \
X(964,tau,"τ","tau") \
X(965,upsilon,"υ","upsilon") \
X(966,phi,"φ","phi") \
X(967,chi,"χ","chi") \
X(968,psi,"ψ","psi") \
X(969,omega,"ω","omega") \
X(977,thetasym,"ϑ","Theta") \
X(978,upsih,"ϒ","Upsilon") \
X(982,piv,"ϖ","Pi") \

#define HTML_MATH        \
X(8704,forall,"∀","For") \
X(8706,part,"∂","Part") \
X(8707,exist,"∃","Exist") \
X(8709,empty,"∅","Empty") \
X(8711,nabla,"∇","Nabla") \
X(8712,isin,"∈","Is") \
X(8713,notin,"∉","Not") \
X(8715,ni,"∋","Ni") \
X(8719,prod,"∏","Product") \
X(8721,sum,"∑","Sum") \
X(8722,minus,"−","Minus") \
X(8727,lowast,"∗","Asterisk") \
X(8730,radic,"√","Square") \
X(8733,prop,"∝","Proportional") \
X(8734,infin,"∞","Infinity") \
X(8736,ang,"∠","Angle") \
X(8743,and,"∧","And") \
X(8744,or,"∨","Or") \
X(8745,cap,"∩","Cap") \
X(8746,cup,"∪","Cup") \
X(8747,int,"∫","Integral") \
X(8756,there4,"∴","Therefore") \
X(8764,sim,"∼","Similar") \
X(8773,cong,"≅","Congurent") \
X(8776,asymp,"≈","Almost") \
X(8800,ne,"≠","Not") \
X(8801,equiv,"≡","Equivalent") \
X(8804,le,"≤","Less") \
X(8805,ge,"≥","Greater") \
X(8834,sub,"⊂","Subset") \
X(8835,sup,"⊃","Superset") \
X(8836,nsub,"⊄","Not") \
X(8838,sube,"⊆","Subset") \
X(8839,supe,"⊇","Superset") \
X(8853,oplus,"⊕","Circled") \
X(8855,otimes,"⊗","Circled") \
X(8869,perp,"⊥","Perpendicular") \
X(8901,sdot,"⋅","Dot") \

#define HTML_MISC \
X(338,OElig,"Œ","Uppercase") \
X(339,oelig,"œ","Lowercase") \
X(352,Scaron,"Š","Uppercase") \
X(353,scaron,"š","Lowercase") \
X(376,Yuml,"Ÿ","Capital") \
X(402,fnof,"ƒ","Lowercase") \
X(710,circ,"ˆ","Circumflex") \
X(732,tilde,"˜","Tilde") \
X(8194,ensp," ","En") \
X(8195,emsp," ","Em") \
X(8201,thinsp," ","Thin") \
X(8204,zwnj,"‌","Zero") \
X(8205,zwj,"‍","Zero") \
X(8206,lrm,"‎","Left-to-right") \
X(8207,rlm,"‏","Right-to-left") \
X(8211,ndash,"–","En") \
X(8212,mdash,"—","Em") \
X(8216,lsquo,"‘","Left") \
X(8217,rsquo,"’","Right") \
X(8218,sbquo,"‚","Single") \
X(8220,ldquo,"“","Left") \
X(8221,rdquo,"”","Right") \
X(8222,bdquo,"„","Double") \
X(8224,dagger,"†","Dagger") \
X(8225,Dagger,"‡","Double") \
X(8226,bull,"•","Bullet") \
X(8230,hellip,"…","Horizontal") \
X(8240,permil,"‰","Per") \
X(8242,prime,"′","Minutes") \
X(8243,Prime,"″","Seconds") \
X(8249,lsaquo,"‹","Single") \
X(8250,rsaquo,"›","Single") \
X(8254,oline,"‾","Overline") \
X(8364,euro,"€","Euro") \
X(8482,trade,"™","Trademark") \
X(8592,larr,"←","Left") \
X(8593,uarr,"↑","Up") \
X(8594,rarr,"→","Right") \
X(8595,darr,"↓","Down") \
X(8596,harr,"↔","Left") \
X(8629,crarr,"↵","Carriage") \
X(8968,lceil,"⌈","Left") \
X(8969,rceil,"⌉","Right") \
X(8970,lfloor,"⌊","Left") \
X(8971,rfloor,"⌋","Right") \
X(9674,loz,"◊","Lozenge") \
X(9824,spades,"♠","Spade") \
X(9827,clubs,"♣","Club") \
X(9829,hearts,"♥","Heart") \
X(9830,diams,"♦","Diamond") \

const char *toHtmlEntity(int uc)
{
    switch(uc)
    {
#define X(uc,name,is,about) case uc: return #name;
HTML_GREEK
HTML_MATH
HTML_MISC
#undef X
    }
    return NULL;
}

#endif // ENTITIES_MODULE

// PARTS HASHMAP_C ////////////////////////////////////////

#ifdef HASHMAP_MODULE

struct HashMapNode;
typedef struct HashMapNode HashMapNode;

struct HashMapNode
{
    char *key_;
    void *val_;
    struct HashMapNode *next_;
};

struct HashMap
{
    int size_;
    int max_size_;
    HashMapNode** nodes_;
};

struct HashMapIterator
{
    HashMap *hm_;
    int offset_;
    struct HashMapNode *node_;
};

// FUNCTION DECLARATIONS //////////////////////////////////////////////////

size_t hash_code(const char *str);
HashMapNode* hashmap_create_node(const char *key);

///////////////////////////////////////////////////////////////////////////

size_t hash_code(const char *str)
{
    size_t hash = 0;
    size_t c;

    while (true)
    {
        c = *str++;
        if (!c) break;
        hash = c + (hash << 6) + (hash << 16) - hash; // sdbm
        // hash = hash * 33 ^ c; // djb2a
    }

    return hash;
}

HashMapNode* hashmap_create_node(const char *key)
{
    HashMapNode* new_node = (HashMapNode*) malloc(sizeof(HashMapNode));
    memset(new_node, 0, sizeof(*new_node));
    new_node->key_ = strdup(key);

    return new_node;
}

HashMap* hashmap_create(size_t max_size)
{
    HashMap* new_table = (HashMap*) malloc(sizeof(HashMap));
    memset(new_table, 0, sizeof(*new_table));

    new_table->nodes_ = (HashMapNode**)calloc(max_size, sizeof(HashMapNode*));
    new_table->max_size_ = max_size;
    new_table->size_ = 0;

    return new_table;
}

void hashmap_free_and_values(HashMap *map)
{
    for (int i=0; i < map->max_size_; ++i)
    {
        HashMapNode *node = map->nodes_[i];
        map->nodes_[i] = NULL;
        while (node)
        {
            if (node->key_) free((char*)node->key_);
            node->key_ = NULL;
            if (node->val_) free(node->val_);
            node->val_ = NULL;
            HashMapNode *next = node->next_;
            free(node);
            node = next;
        }
    }
    map->size_ = 0;
    map->max_size_ = 0;
    free(map->nodes_);
    free(map);
}


void hashmap_put(HashMap* table, const char* key, void *val)
{
    assert(val != NULL);

    size_t index = hash_code(key) % table->max_size_;
    HashMapNode* slot = table->nodes_[index];
    HashMapNode* prev_slot = NULL;

    if (!slot)
    {
        // No hash in this position, create new node and fill slot.
        HashMapNode* new_node = hashmap_create_node(key);
        new_node->val_ = val;
        table->nodes_[index] = new_node;
        table->size_++;
        return;
    }

    // Look for a match...
    while (slot)
    {
        if (strcmp(slot->key_, key) == 0)
        {
            slot->val_ = val;
            return;
        }
        prev_slot = slot;
        slot = slot->next_;
    }

    // No matching node found, append to prev_slot
    HashMapNode* new_node = hashmap_create_node(key);
    new_node->val_ = val;
    prev_slot->next_ = new_node;
    table->size_++;
    return;
}

void *hashmap_get(HashMap* table, const char* key)
{
    int index = hash_code(key) % table->max_size_;
    HashMapNode* slot = table->nodes_[index];

    while (slot)
    {
        if (!strcmp(slot->key_, key))
        {
            return slot->val_;
        }
        slot = slot->next_;
    }

    return NULL;
}

size_t hashmap_size(HashMap* table)
{
    return table->size_;
}

void hashmap_free(HashMap* table)
{
    for (int i=0; i < table->max_size_; i++)
    {
        HashMapNode* slot = table->nodes_[i];
        while (slot != NULL)
        {
            HashMapNode* tmp = slot;
            slot = slot ->next_;
            free(tmp);
        }
    }

    free(table->nodes_);
    free(table);
}

HashMapIterator *hashmap_iterate(HashMap *map)
{
    HashMapIterator* new_iterator = (HashMapIterator*) malloc(sizeof(HashMapIterator));
    memset(new_iterator, 0, sizeof(*new_iterator));
    new_iterator->hm_ = map;
    new_iterator->offset_ = -1;

    return new_iterator;
}

bool hashmap_next_key_value(HashMapIterator *i, const char **key, void **val)
{
    if (i->node_)
    {
        i->node_ = i->node_->next_;
    }
    if (i->node_)
    {
        *key = i->node_->key_;
        *val = i->node_->val_;
        return true;
    }
    do
    {
        i->offset_++;
    }
    while (i->offset_ < i->hm_->max_size_ && i->hm_->nodes_[i->offset_] == NULL);

    if (i->offset_ >= i->hm_->max_size_) return false;

    i->node_ = i->hm_->nodes_[i->offset_];
    *key = i->node_->key_;
    *val = i->node_->val_;
    return true;
}

void hashmap_free_iterator(HashMapIterator *i)
{
    free(i);
}

#endif // HASHMAP_MODULE

// PARTS STACK_C ////////////////////////////////////////

#ifdef STACK_MODULE

StackElement *find_element_above(Stack *s, StackElement *b);

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

StackElement *find_element_above(Stack *s, StackElement *b)
{
    StackElement *e = s->top;

    for (;;)
    {
        if (!e) return NULL;
        if (e->below == b) return e;
        e = e->below;
    }

    return NULL;
}

void *rock_stack(Stack *stack)
{
    assert(stack);
    assert(stack->bottom);

    assert(stack->size > 0);

    if (stack->size == 1) return pop_stack(stack);

    StackElement *element = stack->bottom;
    void *data = element->data;
    StackElement *above = find_element_above(stack, element);
    assert(above);
    stack->bottom = above;
    above->below = NULL;
    free(element);
    stack->size--;

    return data;
}

#endif // STACK_MODULE

// PARTS MEMBUFFER_C ////////////////////////////////////////

#ifdef MEMBUFFER_MODULE

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

void membuffer_reuse(MemBuffer *mb, char *start, size_t len)
{
    if (mb->buffer_) free(mb->buffer_);
    mb->buffer_ = start;
    mb->used_ = mb->max_ = len;
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

void membuffer_drop_last_null(MemBuffer *mb)
{
    char *buf = mb->buffer_;
    size_t used = mb->used_;
    if (used > 0 && buf[used-1] == 0)
    {
        mb->used_--;
    }
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

void membuffer_append_pointer(MemBuffer *mb, void *ptr)
{
    size_t add = sizeof(ptr);
    size_t max = pick_buffer_new_size(mb->max_, mb->used_, add);
    if (max > mb->max_)
    {
        mb->buffer_ = (char*)realloc(mb->buffer_, max);
        mb->max_ = max;
    }
    memcpy(mb->buffer_+mb->used_, &ptr, sizeof(ptr));
    mb->used_ += add;
}

size_t membuffer_used(MemBuffer *mb)
{
    return mb->used_;
}

#endif // MEMBUFFER_MODULE

// PARTS JSON_C ////////////////////////////////////////

#ifdef JSON_MODULE

void eat_json_boolean(XMQParseState *state);
void eat_json_null(XMQParseState *state);
void eat_json_number(XMQParseState *state);
void eat_json_quote(XMQParseState *state, char **content_start, char **content_stop);

void fixup_json(XMQDoc *doq, xmlNode *node);

void parse_json(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_array(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_boolean(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_null(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_number(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_object(XMQParseState *state, const char *key_start, const char *key_stop);
void parse_json_quote(XMQParseState *state, const char *key_start, const char *key_stop);

bool has_number_ended(char c);
bool has_attr_other_than_AS_(xmlNode *node);
const char *is_jnumber(const char *start, const char *stop);
bool is_json_boolean(XMQParseState *state);
bool is_json_null(XMQParseState *state);
bool is_json_number(XMQParseState *state);
bool is_json_quote_start(char c);
bool is_json_whitespace(char c);
void json_print_attribute(XMQPrintState *ps, xmlAttrPtr a);
void json_print_attributes(XMQPrintState *ps, xmlNodePtr node);
void json_print_array_with_children(XMQPrintState *ps,
                                    xmlNode *container,
                                    xmlNode *node);
void json_print_comment_node(XMQPrintState *ps, xmlNodePtr node, bool prefix_ul, size_t total, size_t used);
void json_print_doctype_node(XMQPrintState *ps, xmlNodePtr node);
void json_print_entity_node(XMQPrintState *ps, xmlNodePtr node);
void json_print_standalone_quote(XMQPrintState *ps, xmlNode *container, xmlNodePtr node, size_t total, size_t used);
void json_print_object_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to);
void json_print_array_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to);
void json_print_node(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used);
void json_print_value(XMQPrintState *ps, xmlNode *from, xmlNode *to, Level level, bool force_string);
void json_print_element_name(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used);
void json_print_element_with_children(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used);
void json_print_key_node(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used, bool force_string);

void json_check_comma(XMQPrintState *ps);
void json_print_comma(XMQPrintState *ps);
bool json_is_number(const char *start);
bool json_is_keyword(const char *start);
void json_print_leaf_node(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used);

void trim_index_suffix(const char *key_start, const char **stop);

bool xmq_tokenize_buffer_json(XMQParseState *state, const char *start, const char *stop);

char equals[] = "=";
char underline[] = "_";
char leftpar[] = "(";
char rightpar[] = ")";
char leftbrace[] = "{";
char rightbrace[] = "}";
char array[] = "A";
char string[] = "S";

bool is_json_whitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_json_quote_start(char c)
{
    return c == '"';
}

void eat_json_quote(XMQParseState *state, char **content_start, char **content_stop)
{
    const char *start = state->i;
    const char *stop = state->buffer_stop;

    MemBuffer *buf = new_membuffer();

    const char *i = start;
    size_t line = state->line;
    size_t col = state->col;

    increment('"', 1, &i, &line, &col);

    while (i < stop)
    {
        char c = *i;
        if (c == '"')
        {
            increment(c, 1, &i, &line, &col);
            break;
        }
        if (c == '\\')
        {
            increment(c, 1, &i, &line, &col);
            c = *i;
            if (c == '"' || c == '\\' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't')
            {
                increment(c, 1, &i, &line, &col);
                switch(c)
                {
                case 'b': c = 8; break;
                case 'f': c = 12; break;
                case 'n': c = 10; break;
                case 'r': c = 13; break;
                case 't': c = 9; break;
                }
                membuffer_append_char(buf, c);
                continue;
            }
            else if (c == 'u')
            {
                increment(c, 1, &i, &line, &col);
                c = *i;
                if (i+3 < stop)
                {
                    // Woot? Json can only escape unicode up to 0xffff ? What about 10000 up to 10ffff?
                    if (is_hex(*(i+0)) && is_hex(*(i+1)) && is_hex(*(i+2)) && is_hex(*(i+3)))
                    {
                        unsigned char c1 = hex_value(*(i+0));
                        unsigned char c2 = hex_value(*(i+1));
                        unsigned char c3 = hex_value(*(i+2));
                        unsigned char c4 = hex_value(*(i+3));
                        increment(c, 1, &i, &line, &col);
                        increment(c, 1, &i, &line, &col);
                        increment(c, 1, &i, &line, &col);
                        increment(c, 1, &i, &line, &col);

                        int uc = (c1<<12)|(c2<<8)|(c3<<4)|c4;
                        UTF8Char utf8;
                        size_t n = encode_utf8(uc, &utf8);

                        for (size_t j = 0; j < n; ++j)
                        {
                            membuffer_append_char(buf, utf8.bytes[j]);
                        }
                        continue;
                    }
                }
            }
            state->error_nr = XMQ_ERROR_JSON_INVALID_ESCAPE;
            longjmp(state->error_handler, 1);
        }
        membuffer_append_char(buf, c);
        increment(c, 1, &i, &line, &col);
    }
    // Add a zero termination to the string which is not used except for
    // guaranteeing that there is at least one allocated byte for empty strings.
    membuffer_append_null(buf);
    state->i = i;
    state->line = line;
    state->col = col;

    // Calculate the real length which might be less than the original
    // since escapes have disappeared. Add 1 to have at least something to allocate.
    size_t len = membuffer_used(buf);
    char *quote = free_membuffer_but_return_trimmed_content(buf);
    *content_start = quote;
    *content_stop = quote+len-1; // Drop the zero byte.
}

void trim_index_suffix(const char *key_start, const char **stop)
{
    const char *key_stop = *stop;

    if (key_start && key_stop && *(key_stop-1) == ']')
    {
        // This is an indexed element name "path[32]":"123" ie the 32:nd xml element
        // which has been indexed because json objects must have unique keys.
        // Well strictly speaking the json permits multiple keys, but no implementation does....
        //
        // Lets drop the suffix [32].
        const char *i = key_stop-2;
        // Skip the digits stop at [ or key_start
        while (i > key_start && *i >= '0' && *i <= '9' && *i != '[') i--;
        if (i > key_start && *i == '[')
        {
            // We found a [ which is not at key_start. Trim off suffix!
            *stop = i;
        }
    }
}

void parse_json_quote(XMQParseState *state, const char *key_start, const char *key_stop)
{
    int start_line = state->line;
    int start_col = state->col;

    char *content_start = NULL;
    char *content_stop = NULL;

    // Decode and content_start points to newly allocated buffer where escapes have been removed.
    eat_json_quote(state, &content_start, &content_stop);
    size_t content_len = content_stop-content_start;

    trim_index_suffix(key_start, &key_stop);

    if (key_start && *key_start == '|' && key_stop == key_start+1)
    {
        // This is "|":"symbol" which means a pure text node in xml.
        DO_CALLBACK_SIM(quote, state, start_line, 1, content_start, content_stop, content_stop);
        free(content_start);
        return;
    }

    if (key_start && key_stop == key_start+2 && *key_start == '/' && *(key_start+1) == '/')
    {
        // This is "//":"symbol" which means a comment node in xml.
        DO_CALLBACK_SIM(comment, state, start_line, start_col, content_start, content_stop, content_stop);
        free(content_start);
        return;
    }

    if (key_start && key_stop == key_start+3 && *key_start == '_' && *(key_start+1) == '/' && *(key_start+2) == '/')
    {
        // This is "_//":"symbol" which means a comment node in xml prefixing the root xml node.
        if (!state->root_found) state->add_pre_node_before = (xmlNode*)state->element_stack->top->data;
        else                    state->add_post_node_after = (xmlNode*)state->element_stack->top->data;
        DO_CALLBACK_SIM(comment, state, start_line, start_col, content_start, content_stop, content_stop);
        if (!state->root_found) state->add_pre_node_before = NULL;
        else                    state->add_post_node_after = NULL;
        free(content_start);
        return;
    }

    if (key_start && key_stop == key_start+1 && *key_start == '_' )
    {
        // This is the element name "_":"symbol" stored inside the json object,
        // in situations where the name is not visible as a key. For example
        // the root json object and any object in arrays.
        xmlNodePtr container = (xmlNodePtr)state->element_stack->top->data;
        size_t len = content_stop - content_start;
        char *name = (char*)malloc(len+1);
        memcpy(name, content_start, len);
        name[len] = 0;
        xmlNodeSetName(container, (xmlChar*)name);
        free(name);
        free(content_start);
        // This will be set many times.
        state->root_found = true;
        return;
    }

    if (key_start && *key_start == '!' && !state->doctype_found)
    {
        size_t len = key_stop-key_start;
        if (len == 8 && !strncmp("!DOCTYPE", key_start, 8))
        {
            // This is the one and only !DOCTYPE element.
            DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);
            state->parsing_doctype = true;
            state->add_pre_node_before = (xmlNode*)state->element_stack->top->data;
            DO_CALLBACK_SIM(element_value_quote, state, state->line, state->col, content_start, content_stop, content_stop);
            state->add_pre_node_before = NULL;
            free(content_start);
            return;
        }
    }

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;

    if (!key_start)
    {
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    if (*key_start == '_' && key_stop > key_start+1)
    {
        // This is an attribute that was stored as "_attr":"value"
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, key_start+1, key_stop, key_stop);
        DO_CALLBACK_SIM(attr_value_quote, state, start_line, start_col, content_start, content_stop, content_stop);
        free(content_start);
        return;
    }

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);

    bool need_string_type =
        content_len > 0 && (
            (content_len == 4 && !strncmp(content_start, "true", 4)) ||
            (content_len == 5 && !strncmp(content_start, "false", 5)) ||
            (content_len == 4 && !strncmp(content_start, "null", 4)) ||
            content_stop == is_jnumber(content_start, content_stop));

    if (need_string_type || unsafe_key_start)
    {
        // Ah, this is the string "false" not the boolean false. Mark this with the attribute S to show that it is a string.
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, leftpar+1, leftpar+1);
        if (unsafe_key_start)
        {
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, underline+1, underline+1);
            DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
        }
        if (need_string_type)
        {
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, string, string+1, string+1);
        }
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, rightpar+1, rightpar+1);
    }

    DO_CALLBACK_SIM(element_value_text, state, start_line, start_col, content_start, content_stop, content_stop);
    free(content_start);
}

bool is_json_null(XMQParseState *state)
{
    const char *i = state->i;
    const char *stop = state->buffer_stop;

    if (i+4 <= stop && !strncmp("null", i, 4)) return true;
    return false;
}

void eat_json_null(XMQParseState *state)
{
    const char *i = state->i;
    size_t line = state->line;
    size_t col = state->col;

    increment('n', 1, &i, &line, &col);
    increment('u', 1, &i, &line, &col);
    increment('l', 1, &i, &line, &col);
    increment('l', 1, &i, &line, &col);

    state->i = i;
    state->line = line;
    state->col = col;
}

void parse_json_null(XMQParseState *state, const char *key_start, const char *key_stop)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;

    eat_json_null(state);
    const char *stop = state->i;

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;

    trim_index_suffix(key_start, &key_stop);

    if (key_start && *key_start == '_' && key_stop > key_start+1)
    {
        // script:{"_async":null "_href":"abc"}
        // translates into scripts(async href=abc)
        // detect attribute before this null. Make into attribute without value.
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, key_start+1, key_stop, key_stop);
        return;
    }

    if (!key_start)
    {
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);
    if (unsafe_key_start)
    {
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, leftpar+1, leftpar+1);
        if (unsafe_key_start)
        {
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, underline+1, underline+1);
            DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
        }
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, rightpar+1, rightpar+1);
    }

    DO_CALLBACK(element_value_text, state, start_line, start_col, start, stop, stop);
}

bool has_number_ended(char c)
{
    return c == ' ' || c == '\n' || c == ',' || c == '}' || c == ']';
}

const char *is_jnumber(const char *start, const char *stop)
{
    if (stop == NULL) stop = start+strlen(start);
    if (start == stop) return NULL;

    bool found_e = false;
    bool found_e_sign = false;
    bool leading_zero = false;
    bool last_is_digit = false;
    bool found_dot = false;

    const char *i;
    for (i = start; i < stop; ++i)
    {
        char c = *i;

        last_is_digit = false;
        bool current_is_not_digit = (c < '0' || c > '9');

        if (i == start)
        {
            if (current_is_not_digit && c != '-' ) return NULL;
            if (c == '0') leading_zero = true;
            if (c != '-') last_is_digit = true;
            continue;
        }

        if (leading_zero)
        {
            leading_zero = false;
            if (has_number_ended(c)) return i;
            if (c != '.') return NULL;
            found_dot = true;
        }
        else if (c == '.')
        {
            if (found_dot) return NULL;
            found_dot = true;
        }
        else if (c == 'e' || c == 'E')
        {
            if (found_e) return NULL;
            found_e = true;
        }
        else if (found_e && !found_e_sign)
        {
            if (has_number_ended(c)) return i;
            if (current_is_not_digit && c != '-' && c != '+') return NULL;
            if (c == '+' || c == '-')
            {
                found_e_sign = true;
            }
            else
            {
                last_is_digit = true;
            }
        }
        else
        {
            found_e_sign = false;
            if (has_number_ended(c)) return i;
            if (current_is_not_digit) return NULL;
            last_is_digit = true;
        }
    }

    if (last_is_digit == false) return NULL;

    return i;
}

bool is_json_boolean(XMQParseState *state)
{
    const char *i = state->i;
    const char *stop = state->buffer_stop;

    if (i+4 <= stop && !strncmp("true", i, 4)) return true;
    if (i+5 <= stop && !strncmp("false", i, 5)) return true;
    return false;
}

void eat_json_boolean(XMQParseState *state)
{
    const char *i = state->i;
    //const char *stop = state->buffer_stop;
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
}

void parse_json_boolean(XMQParseState *state, const char *key_start, const char *key_stop)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;

    eat_json_boolean(state);
    const char *stop = state->i;

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;

    trim_index_suffix(key_start, &key_stop);

    if (!key_start)
    {
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);
    if (unsafe_key_start)
    {
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, leftpar+1, leftpar+1);
        if (unsafe_key_start)
        {
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, underline+1, underline+1);
            DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
        }
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, rightpar+1, rightpar+1);
    }

    DO_CALLBACK(element_value_text, state, start_line, start_col, start, stop, stop);
}

bool is_json_number(XMQParseState *state)
{
    return NULL != is_jnumber(state->i, state->buffer_stop);
}

void eat_json_number(XMQParseState *state)
{
    const char *start = state->i;
    const char *stop = state->buffer_stop;
    const char *i = start;
    size_t line = state->line;
    size_t col = state->col;

    const char *end = is_jnumber(i, stop);
    assert(end); // Must not call eat_json_number without check for a number before...
    increment('?', end-start, &i, &line, &col);

    state->i = i;
    state->line = line;
    state->col = col;
}

void parse_json_number(XMQParseState *state, const char *key_start, const char *key_stop)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;

    eat_json_number(state);
    const char *stop = state->i;

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;

    trim_index_suffix(key_start, &key_stop);

    if (!key_start)
    {
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);
    if (unsafe_key_start)
    {
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, leftpar+1, leftpar+1);
        if (unsafe_key_start)
        {
            DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, underline+1, underline+1);
            DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
        }
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, rightpar+1, rightpar+1);
    }

    DO_CALLBACK(element_value_text, state, start_line, start_col, start, stop, stop);
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
    state->error_nr = XMQ_ERROR_NONE;

    if (state->parse->init) state->parse->init(state);

    if (!setjmp(state->error_handler))
    {
        parse_json(state, NULL, NULL);
        if (state->i < state->buffer_stop)
        {
            state->error_nr = XMQ_ERROR_UNEXPECTED_CLOSING_BRACE;
            longjmp(state->error_handler, 1);
        }
    }
    else
    {
        XMQParseError error_nr = state->error_nr;
        generate_state_error_message(state, error_nr, start, stop);
        return false;
    }

    if (state->parse->done) state->parse->done(state);
    return true;
}

void parse_json_array(XMQParseState *state, const char *key_start, const char *key_stop)
{
    char c = *state->i;
    assert(c == '[');
    increment(c, 1, &state->i, &state->line, &state->col);

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;

    trim_index_suffix(key_start, &key_stop);

    if (!key_start)
    {
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);
    DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, leftpar+1, leftpar+1);
    if (unsafe_key_start)
    {
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, underline+1, underline+1);
        DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
    }
    DO_CALLBACK_SIM(attr_key, state, state->line, state->col, array, array+1, array+1);
    DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, rightpar+1, rightpar+1);

    DO_CALLBACK_SIM(brace_left, state, state->line, state->col, leftbrace, leftbrace+1, leftbrace+1);

    const char *stop = state->buffer_stop;

    c = ',';

    while (state->i < stop && c == ',')
    {
        eat_xml_whitespace(state, NULL, NULL);
        c = *(state->i);
        if (c == ']') break;

        parse_json(state, NULL, NULL);
        c = *state->i;
        if (c == ',') increment(c, 1, &state->i, &state->line, &state->col);
    }

    assert(c == ']');
    increment(c, 1, &state->i, &state->line, &state->col);

    DO_CALLBACK_SIM(brace_right, state, state->line, state->col, rightbrace, rightbrace+1, rightbrace+1);
}

void parse_json(XMQParseState *state, const char *key_start, const char *key_stop)
{
    eat_xml_whitespace(state, NULL, NULL);

    char c = *(state->i);

    if (is_json_quote_start(c)) parse_json_quote(state, key_start, key_stop);
    else if (is_json_boolean(state)) parse_json_boolean(state, key_start, key_stop);
    else if (is_json_null(state)) parse_json_null(state, key_start, key_stop);
    else if (is_json_number(state)) parse_json_number(state, key_start, key_stop);
    else if (c == '{') parse_json_object(state, key_start, key_stop);
    else if (c == '[') parse_json_array(state, key_start, key_stop);
    else
    {
        state->error_nr = XMQ_ERROR_JSON_INVALID_CHAR;
        longjmp(state->error_handler, 1);
    }
    eat_xml_whitespace(state, NULL, NULL);
}

typedef struct
{
    size_t total;
    size_t used;
} Counter;

void json_print_object_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to)
{
    xmlNode *i = from;

    HashMap* map = hashmap_create(100);

    while (i)
    {
        const char *name = (const char*)i->name;
        if (name && strcmp(name, "_")) // We have a name and it is NOT a single _
        {
            Counter *c = (Counter*)hashmap_get(map, name);
            if (!c)
            {
                c = (Counter*)malloc(sizeof(Counter));
                memset(c, 0, sizeof(Counter));
                hashmap_put(map, name, c);
            }
            c->total++;
        }
        if (i == to) break;
        i = xml_next_sibling(i);
    }

    i = from;
    while (i)
    {
        const char *name = (const char*)i->name;
        if (name && strcmp(name, "_"))
        {
            Counter *c = (Counter*)hashmap_get(map, (const char*)i->name);
            json_print_node(ps, container, i, c->total, c->used);
            c->used++;
        }
        else
        {
            json_print_node(ps, container, i, 1, 0);
        }
        if (i == to) break;
        i = xml_next_sibling(i);
    }

    hashmap_free_and_values(map);
}

void json_print_array_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to)
{
    xmlNode *i = from;
    while (i)
    {
        json_check_comma(ps);
        bool force_string = xml_get_attribute(i, "S");
        bool is_number = xml_element_content(i) && json_is_number(xml_element_content(i));
        bool is_keyword = xml_element_content(i) && json_is_keyword(xml_element_content(i));

        if (force_string || is_number || is_keyword)
        {
            json_print_value(ps, xml_first_child(i), xml_last_child(i), LEVEL_ELEMENT_VALUE, force_string);
        }
        else
        {
            json_print_node(ps, NULL, i, 1, 0);
        }
        i = xml_next_sibling(i);
    }
}

bool has_attr_other_than_AS_(xmlNode *node)
{
    xmlAttr *a = xml_first_attribute(node);

    while (a)
    {
        if (strcmp((const char*)a->name, "A") &&
            strcmp((const char*)a->name, "S") &&
            strcmp((const char*)a->name, "_")) return true;
        a = a->next;
    }

    return false;
}

void json_print_node(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used)
{
    // This is a comment translated into "//":"Comment text"
    if (is_comment_node(node))
    {
        json_print_comment_node(ps, node, false, total, used);
        return;
    }

    // Standalone quote must be quoted: 'word' 'some words'
    if (is_content_node(node))
    {
        json_print_standalone_quote(ps, container, node, total, used);
        return;
    }

    // This is an entity reference node. &something;
    if (is_entity_node(node))
    {
        json_print_entity_node(ps, node);
        return;
    }

    // This is a node with no children, but the only such valid json nodes are
    // the empty object _ ---> {} or _(A) ---> [].
    if (is_leaf_node(node) && container)
    {
        return json_print_leaf_node(ps, container, node, total, used);
    }

    // This is a key = value or key = 'value value' or key = ( 'value' &#10; )
    // Also! If the node has attributes, then we cannot print as key value in json.
    // It has to be an object.
    if (is_key_value_node(node) &&
        (!has_attributes(node) ||
         !has_attr_other_than_AS_(node)))
    {
        bool force_string = xml_get_attribute(node, "S");
        return json_print_key_node(ps, container, node, total, used, force_string);
    }

    // The node is marked foo(A) { } translate this into: "foo":[ ]
    if (xml_get_attribute(node, "A"))
    {
        return json_print_array_with_children(ps, container, node);
    }

    // All other nodes are printed
    json_print_element_with_children(ps, container, node, total, used);
}

void parse_json_object(XMQParseState *state, const char *key_start, const char *key_stop)
{
    char c = *state->i;
    assert(c == '{');
    increment(c, 1, &state->i, &state->line, &state->col);

    const char *unsafe_key_start = NULL;
    const char *unsafe_key_stop = NULL;

    trim_index_suffix(key_start, &key_stop);

    if (!key_start)
    {
        key_start = underline;
        key_stop = underline+1;
    }
    else if (!is_xmq_element_name(key_start, key_stop))
    {
        unsafe_key_start = key_start;
        unsafe_key_stop = key_stop;
        key_start = underline;
        key_stop = underline+1;
    }

    DO_CALLBACK_SIM(element_key, state, state->line, state->col, key_start, key_stop, key_stop);
    if (unsafe_key_start)
    {
        DO_CALLBACK_SIM(apar_left, state, state->line, state->col, leftpar, leftpar+1, leftpar+1);
        DO_CALLBACK_SIM(attr_key, state, state->line, state->col, underline, underline+1, underline+1);
        DO_CALLBACK_SIM(attr_value_quote, state, state->line, state->col, unsafe_key_start, unsafe_key_stop, unsafe_key_stop);
        DO_CALLBACK_SIM(apar_right, state, state->line, state->col, rightpar, rightpar+1, rightpar+1);
    }

    DO_CALLBACK_SIM(brace_left, state, state->line, state->col, leftbrace, leftbrace+1, leftbrace+1);

    const char *stop = state->buffer_stop;

    c = ',';

    while (state->i < stop && c == ',')
    {
        eat_xml_whitespace(state, NULL, NULL);
        c = *(state->i);
        if (c == '}') break;

        if (!is_json_quote_start(c))
        {
            state->error_nr = XMQ_ERROR_JSON_INVALID_CHAR;
            longjmp(state->error_handler, 1);
        }

        // Find the key string, ie speed in { "speed":123 }
        char *new_key_start = NULL;
        char *new_key_stop = NULL;
        eat_json_quote(state, &new_key_start, &new_key_stop);

        eat_xml_whitespace(state, NULL, NULL);
        c = *(state->i);

        if (c == ':')
        {
            increment(c, 1, &state->i, &state->line, &state->col);
        }
        else
        {
            state->error_nr = XMQ_ERROR_JSON_INVALID_CHAR;
            longjmp(state->error_handler, 1);
        }

        parse_json(state, new_key_start, new_key_stop);
        free(new_key_start);

        c = *state->i;
        if (c == ',') increment(c, 1, &state->i, &state->line, &state->col);
    }
    while (c == ',');

    assert(c == '}');
    increment(c, 1, &state->i, &state->line, &state->col);

    DO_CALLBACK_SIM(brace_right, state, state->line, state->col, rightbrace, rightbrace+1, rightbrace+1);
}

void json_print_value(XMQPrintState *ps, xmlNode *from, xmlNode *to, Level level, bool force_string)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;

    xmlNode *node = from;
    const char *content = xml_element_content(node);

    if (!xml_next_sibling(node) &&
        !force_string &&
        (json_is_number(xml_element_content(node))
         || json_is_keyword(xml_element_content(node))))
    {
        // This is a number or a keyword. E.g. 123 true false null
        write(writer_state, content, NULL);
        ps->last_char = content[strlen(content)-1];
    }
    else if (!xml_next_sibling(node) && content[0] == 0)
    {
        write(writer_state, "\"\"", NULL);
        ps->last_char = '"';
    }
    else
    {
        print_utf8(ps, COLOR_none, 1, "\"", NULL);

        if (is_entity_node(node))
        {
            write(writer_state, "&", NULL);
            write(writer_state, (const char*)node->name, NULL);
            write(writer_state, ";", NULL);
        }
        else
        {
            do
            {
                if (is_entity_node(node))
                {
                    const char *name = xml_element_name(node);
                    print_utf8(ps, COLOR_none, 3, "&", NULL, name, NULL, ";", NULL);
                }
                else
                {
                    const char *value = xml_element_content(node);
                    if (value)
                    {
                        char *quoted_value = xmq_quote_as_c(value, value+strlen(value));
                        print_utf8(ps, COLOR_none, 1, quoted_value, NULL);
                        free(quoted_value);
                    }
                }
                if (node == to) break;
                node = xml_next_sibling(node);
            } while (node);
        }

        print_utf8(ps, COLOR_none, 1, "\"", NULL);
        ps->last_char = '"';
    }
}

void json_print_array_with_children(XMQPrintState *ps,
                                    xmlNode *container,
                                    xmlNode *node)
{
    json_check_comma(ps);

    if (container)
    {
        // We have a containing node, then we can print this using "name" : [ ... ]
        json_print_element_name(ps, container, node, 1, 0);
        print_utf8(ps, COLOR_none, 1, ":", NULL);
    }

    void *from = xml_first_child(node);
    void *to = xml_last_child(node);

    print_utf8(ps, COLOR_brace_left, 1, "[", NULL);
    ps->last_char = '[';

    ps->line_indent += ps->output_settings->add_indent;

    if (!container)
    {
        // Top level object or object inside array. [ {} {} ]
        // Dump the element name! It cannot be represented!
    }

    if (from)
    {
        while (xml_prev_sibling((xmlNode*)from)) from = xml_prev_sibling((xmlNode*)from);
        assert(from != NULL);
    }

    json_print_array_nodes(ps, NULL, (xmlNode*)from, (xmlNode*)to);

    ps->line_indent -= ps->output_settings->add_indent;

    print_utf8(ps, COLOR_brace_right, 1, "]", NULL);
    ps->last_char = ']';
}

void json_print_attribute(XMQPrintState *ps, xmlAttr *a)
{
    const char *key;
    const char *prefix;
    size_t total_u_len;
    attr_strlen_name_prefix(a, &key, &prefix, &total_u_len);

    // Do not print "_" attributes since they are the name of the element
    // when the element name is not valid xml.
    if (!strcmp(key, "_")) return;

    json_check_comma(ps);

    if (prefix)
    {
        print_utf8(ps, COLOR_none, 1, prefix, NULL);
        print_utf8(ps, COLOR_none, 1, ":", NULL);
    }
    char *quoted_key = xmq_quote_as_c(key, key+strlen(key));
    print_utf8(ps, COLOR_none, 3, "\"_", NULL, quoted_key, NULL, "\":", NULL);
    free(quoted_key);

    if (a->children != NULL)
    {
        char *value = (char*)xmlNodeListGetString(a->doc, a->children, 1);
        char *quoted_value = xmq_quote_as_c(value, value+strlen(value));
        print_utf8(ps, COLOR_none, 3, "\"", NULL, quoted_value, NULL, "\"", NULL);
        free(quoted_value);
        xmlFree(value);
    }
    else
    {
        print_utf8(ps, COLOR_none, 1, "null", NULL);
    }
}

void json_print_attributes(XMQPrintState *ps,
                           xmlNodePtr node)
{
    xmlAttr *a = xml_first_attribute(node);
    //xmlNs *ns = xml_first_namespace_def(node);

    while (a)
    {
        json_print_attribute(ps, a);
        a = xml_next_attribute(a);
    }
    /*
    while (ns)
    {
        print_namespace(ps, ns, max);
        ns = xml_next_namespace_def(ns);
        }*/
}

void json_print_element_with_children(XMQPrintState *ps,
                                      xmlNode *container,
                                      xmlNode *node,
                                      size_t total,
                                      size_t used)
{
    json_check_comma(ps);

    if (container)
    {
        // We have a containing node, then we can print this using "name" : { ... }
        json_print_element_name(ps, container, node, total, used);
        print_utf8(ps, COLOR_none, 1, ":", NULL);
    }

    void *from = xml_first_child(node);
    void *to = xml_last_child(node);

    print_utf8(ps, COLOR_brace_left, 1, "{", NULL);
    ps->last_char = '{';

    ps->line_indent += ps->output_settings->add_indent;

    while (!container && ps->pre_nodes && ps->pre_nodes->size > 0)
    {
        xmlNodePtr node = (xmlNodePtr)rock_stack(ps->pre_nodes);

        if (is_doctype_node(node))
        {
            json_print_doctype_node(ps, node);
        }
        else if (is_comment_node(node))
        {
            json_print_comment_node(ps, node, true, ps->pre_post_num_comments_total, ps->pre_post_num_comments_used++);
        }
        else
        {
            assert(false);
        }
    }

    const char *name = xml_element_name(node);
    bool is_underline = (name[0] == '_' && name[1] == 0);
    if (!container && name && !is_underline)
    {
        // Top level object or object inside array.
        // Hide the name of the object inside the json object with the key "_".
        // I.e. x { a=1 } -> { "_":"x", "a":1 }
        json_check_comma(ps);
        print_utf8(ps, COLOR_none, 1, "\"_\":", NULL);
        ps->last_char = ':';
        json_print_element_name(ps, container, node, total, used);
    }

    json_print_attributes(ps, node);

    if (from)
    {
        while (xml_prev_sibling((xmlNode*)from)) from = xml_prev_sibling((xmlNode*)from);
        assert(from != NULL);
    }

    json_print_object_nodes(ps, node, (xmlNode*)from, (xmlNode*)to);

    while (!container && ps->post_nodes && ps->post_nodes->size > 0)
    {
        xmlNodePtr node = (xmlNodePtr)rock_stack(ps->post_nodes);

        if (is_comment_node(node))
        {
            json_print_comment_node(ps, node, true, ps->pre_post_num_comments_total, ps->pre_post_num_comments_used++);
        }
        else
        {
            assert(false);
        }
    }

    ps->line_indent -= ps->output_settings->add_indent;

    print_utf8(ps, COLOR_brace_right, 1, "}", NULL);
    ps->last_char = '}';
}

void json_print_element_name(XMQPrintState *ps, xmlNode *container, xmlNode *node, size_t total, size_t used)
{
    const char *name = (const char*)node->name;
    const char *prefix = NULL;

    if (node->ns && node->ns->prefix)
    {
        prefix = (const char*)node->ns->prefix;
    }

    print_utf8(ps, COLOR_none, 1, "\"", NULL);

    if (prefix)
    {
        print_utf8(ps, COLOR_none, 1, prefix, NULL);
        print_utf8(ps, COLOR_none, 1, ":", NULL);
    }

    if (name[0] != '_' || name[1] != 0)
    {
        print_utf8(ps, COLOR_none, 1, name, NULL);
    }
    else
    {
        xmlAttr *a = xml_get_attribute(node, "_");
        if (a)
        {
            // The key was stored inside the attribute because it could not
            // be used as the element name.
            char *value = (char*)xmlNodeListGetString(node->doc, a->children, 1);
            char *quoted_value = xmq_quote_as_c(value, value+strlen(value));
            print_utf8(ps, COLOR_none, 1, quoted_value, NULL);
            free(quoted_value);
            xmlFree(value);
            ps->last_char = '"';
        }
    }

    if (total > 1)
    {
        char buf[32];
        buf[31] = 0;
        snprintf(buf, 32, "[%zu]", used);
        print_utf8(ps, COLOR_none, 1, buf, NULL);
    }
    print_utf8(ps, COLOR_none, 1, "\"", NULL);

    ps->last_char = '"';
}

void json_print_key_node(XMQPrintState *ps,
                         xmlNode *container,
                         xmlNode *node,
                         size_t total,
                         size_t used,
                         bool force_string)
{
    json_check_comma(ps);

    if (container)
    {
        json_print_element_name(ps, container, node, total, used);
        print_utf8(ps, COLOR_equals, 1, ":", NULL);
        ps->last_char = ':';
    }

    json_print_value(ps, xml_first_child(node), xml_last_child(node), LEVEL_ELEMENT_VALUE, force_string);
}

void json_check_comma(XMQPrintState *ps)
{
    char c = ps->last_char;
    if (c == 0) return;

    if (c != '{' && c != '[' && c != ',')
    {
        json_print_comma(ps);
    }
}

void json_print_comma(XMQPrintState *ps)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;
    write(writer_state, ",", NULL);
    ps->last_char = ',';
    ps->current_indent ++;
}

void json_print_comment_node(XMQPrintState *ps,
                             xmlNode *node,
                             bool prefix_ul,
                             size_t total,
                             size_t used)
{
    json_check_comma(ps);

    if (prefix_ul) print_utf8(ps, COLOR_equals, 1, "\"_//", NULL);
    else print_utf8(ps, COLOR_equals, 1, "\"//", NULL);

    if (total > 1)
    {
        char buf[32];
        buf[31] = 0;
        snprintf(buf, 32, "[%zu]\":", used);
        print_utf8(ps, COLOR_equals, 1, buf, NULL);
    }
    else
    {
        print_utf8(ps, COLOR_equals, 1, "\":", NULL);
    }
    ps->last_char = ':';
    json_print_value(ps, node, node, LEVEL_XMQ, true);
    ps->last_char = '"';
}

void json_print_doctype_node(XMQPrintState *ps, xmlNodePtr node)
{
    json_check_comma(ps);

    // Print !DOCTYPE inside top level object.
    // I.e. !DOCTYPE=html html { body = a } -> { "!DOCTYPE":"html", "html":{ "body":"a"}}
    print_utf8(ps, COLOR_none, 1, "\"!DOCTYPE\":", NULL);
    ps->last_char = ':';
    xmlBuffer *buffer = xmlBufferCreate();
    xmlNodeDump(buffer, (xmlDocPtr)ps->doq->docptr_.xml, node, 0, 0);
    char *c = (char*)xmlBufferContent(buffer);
    char *quoted_value = xmq_quote_as_c(c+10, c+strlen(c)-1);
    print_utf8(ps, COLOR_none, 3, "\"", NULL, quoted_value, NULL, "\"", NULL);
    free(quoted_value);
    xmlBufferFree(buffer);
    ps->last_char = '"';
}

void json_print_entity_node(XMQPrintState *ps, xmlNodePtr node)
{
    json_check_comma(ps);

    const char *name = xml_element_name(node);

    print_utf8(ps, COLOR_none, 3, "\"&\":\"&", NULL, name, NULL, ";\"", NULL);
    ps->last_char = '"';
}

void json_print_standalone_quote(XMQPrintState *ps, xmlNodePtr container, xmlNodePtr node, size_t total, size_t used)
{
    json_check_comma(ps);
    const char *value = xml_element_content(node);
    char *quoted_value = xmq_quote_as_c(value, value+strlen(value));
    if (total == 1)
    {
        print_utf8(ps, COLOR_none, 3, "\"|\":\"", NULL, quoted_value, NULL, "\"", NULL);
    }
    else
    {
        char buf[32];
        buf[31] = 0;
        snprintf(buf, 32, "\"|[%zu]\":\"", used);
        print_utf8(ps, COLOR_none, 3, buf, NULL, quoted_value, NULL, "\"", NULL);
    }
    free(quoted_value);
    ps->last_char = '"';
}

bool json_is_number(const char *start)
{
    const char *stop = start+strlen(start);
    return stop == is_jnumber(start, stop);
}

bool json_is_keyword(const char *start)
{
    if (!strcmp(start, "true")) return true;
    if (!strcmp(start, "false")) return true;
    if (!strcmp(start, "null")) return true;
    return false;
}

void json_print_leaf_node(XMQPrintState *ps,
                          xmlNode *container,
                          xmlNode *node,
                          size_t total,
                          size_t used)
{
    XMQOutputSettings *output_settings = ps->output_settings;
    XMQWrite write = output_settings->content.write;
    void *writer_state = output_settings->content.writer_state;
    const char *name = xml_element_name(node);

    json_check_comma(ps);

    if (name)
    {
        if (!(name[0] == '_' && name[1] == 0))
        {
            json_print_element_name(ps, container, node, total, used);
            write(writer_state, ":", NULL);
        }
    }

    if (xml_get_attribute(node, "A"))
    {
        write(writer_state, "[]", NULL);
        ps->last_char = ']';
    }
    else
    {
        if (xml_first_attribute(node))
        {
            write(writer_state, "{", NULL);
            ps->last_char = '{';
            json_print_attributes(ps, node);
            write(writer_state, "}", NULL);
            ps->last_char = '}';
        }
        else
        {
            write(writer_state, "{}", NULL);
            ps->last_char = '}';
        }
    }
}

void fixup_json(XMQDoc *doq, xmlNode *node)
{
    if (is_element_node(node))
    {
        char *new_content = xml_collapse_text(node);
        if (new_content)
        {
            xmlNodePtr new_child = xmlNewDocText(doq->docptr_.xml, (const xmlChar*)new_content);
            xmlNode *i = node->children;
            while (i) {
                xmlNode *next = i->next;
                xmlUnlinkNode(i);
                xmlFreeNode(i);
                i = next;
            }
            assert(node);
            assert(new_child);
            xmlAddChild(node, new_child);
            free(new_content);
            return;
        }
    }

    xmlNode *i = xml_first_child(node);
    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in trim.
        fixup_json(doq, i);
        i = next;
    }
}

void xmq_fixup_json_before_writeout(XMQDoc *doq)
{
    xmlNodePtr i = doq->docptr_.xml->children;
    if (!doq || !i) return;

    while (i)
    {
        xmlNode *next = xml_next_sibling(i); // i might be freed in fixup_json.
        fixup_json(doq, i);
        i = next;
    }
}

void collect_leading_ending_comments_doctype(XMQPrintState *ps, xmlNodePtr *first, xmlNodePtr *last)
{
    xmlNodePtr f = *first;
    xmlNodePtr l = *last;
    xmlNodePtr node;

    for (node = f; node && node != l; node = node->next)
    {
        if (is_doctype_node(node) || is_comment_node(node))
        {
            push_stack(ps->pre_nodes, node);
            if (is_comment_node(node)) ps->pre_post_num_comments_total++;
            continue;
        }
        break;
    }

    if (*first != node)
    {
        *first = node;
        f = node;
    }

    for (node = l; node && node != f; node = node->prev)
    {
        if (is_comment_node(node))
        {
            push_stack(ps->post_nodes, node);
            ps->pre_post_num_comments_total++;
            continue;
        }
        break;
    }

    if (*last != node)
    {
        *last = node;
    }
}

#else

// Empty function when XMQ_NO_JSON is defined.
void xmq_fixup_json_before_writeout(XMQDoc *doq)
{
}

// Empty function when XMQ_NO_JSON is defined.
bool xmq_parse_buffer_json(XMQDoc *doq, const char *start, const char *stop)
{
    return false;
}

// Empty function when XMQ_NO_JSON is defined.
void json_print_object_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to)
{
}

void collect_leading_ending_comments_doctype(XMQPrintState *ps, xmlNodePtr *first, xmlNodePtr *last)
{
}

void json_print_array_nodes(XMQPrintState *ps, xmlNode *container, xmlNode *from, xmlNode *to)
{
}

#endif // JSON_MODULE

// PARTS TEXT_C ////////////////////////////////////////

#ifdef TEXT_MODULE

const char *has_leading_space_nl(const char *start, const char *stop, size_t *only_newlines)
{
    const char *i = start;
    bool found_nl = false;
    size_t only_nls = 0;

    if (only_newlines != NULL) *only_newlines = 0;

    // Look for leading newlines.
    while (i < stop && *i == '\n')
    {
        i++;
        only_nls++;
    }
    const char *middle = NULL;

    // Yep, we found some leading pure newlines.
    if (only_nls > 0)
    {
        found_nl = true;
        middle = i;
    }

    // Scan other leading whitespace, perhaps none.
    while (i < stop)
    {
        if (*i == '\n') found_nl = true;
        if (!is_xml_whitespace(*i)) break;
        i++;
    }

    // No newline found before content, so leading spaces/tabs will not be trimmed.
    if (!found_nl) return 0;

    if (middle == i)
    {
        // We have for example "\ncontent" this we can represent in xmq with a visible empty line, eg:
        // '
        //
        // content'
        if (only_newlines != NULL) *only_newlines = only_nls;
    }
    return i;
}

const char *has_ending_nl_space(const char *start, const char *stop, size_t *only_newlines)
{
    const char *i = stop;
    bool found_nl = false;
    size_t only_nls = 0;

    if (only_newlines != NULL) *only_newlines = 0;

    // Look for ending newlines.
    i--;
    while (i >= start && *i == '\n')
    {
        i--;
        only_nls++;
        found_nl = true;
    }
    const char *middle = i;

    while (i >= start)
    {
        if (*i == '\n') found_nl = true;
        if (!is_xml_whitespace(*i)) break;
        i--;
    }
    // No newline found after content, so ending spaces/tabs will not be trimmed.
    if (!found_nl) return 0;

    if (middle == i)
    {
        // We have for example "content\n" this we can represent in xmq with a visible empty line, eg:
        // 'content
        //
        // '
        if (only_newlines != NULL) *only_newlines = only_nls;
    }

    i++;
    return i;
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

bool has_must_escape_chars(const char *start, const char *stop)
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

bool has_all_whitespace(const char *start, const char *stop, bool *all_space, bool *only_newlines)
{
    *all_space = true;
    *only_newlines = true;
    for (const char *i = start; i < stop; ++i)
    {
        if (!is_xml_whitespace(*i))
        {
            *all_space = false;
            *only_newlines = false;
            return false;
        }
        if (*i != ' ' && *all_space == true)
        {
            *all_space = false;
        }
        if (*i != '\n' && *only_newlines == true)
        {
            *only_newlines = false;
        }
    }
    return true;
}

bool is_lowercase_hex(char c)
{
    return
        (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f');
}

size_t num_utf8_bytes(char c)
{
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xe0) == 0xc0) return 2;
    if ((c & 0xf0) == 0xe0) return 3;
    if ((c & 0xf8) == 0xf0) return 4;
    return 0; // Error
}

/**
   peek_utf8_char: Peek 1 to 4 chars from s belonging to the next utf8 code point and store them in uc.
   @start: Read utf8 from this string.
   @stop: Points to byte after last byte in string. If NULL assume start is null terminated.
   @uc: Store the UTF8 char here.

   Return the number of bytes peek UTF8 char, use this number to skip ahead to next char in s.
*/
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

/**
   utf8_char_to_codepoint_string: Decode an utf8 char and store as a string "U+123"
   @uc: The utf8 char to decode.
   @buf: Store the codepoint string here must have space for 9 bytes, i.e. U+10FFFF and a NULL byte.
*/
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

/**
   encode_utf8: Convert an integer unicode code point into utf8 bytes.
   @uc: The unicode code point to encode as utf8
   @out_char: Store the unicode code point here.
   @out_len: How many bytes the utf8 char used.

   Return true if valid utf8 char.
*/
size_t encode_utf8(int uc, UTF8Char *utf8)
{
    utf8->bytes[0] = 0;
    utf8->bytes[1] = 0;
    utf8->bytes[2] = 0;
    utf8->bytes[3] = 0;

    if (uc <= 0x7f)
    {
        utf8->bytes[0] = uc;
        return 1;
    }
    else if (uc <= 0x7ff)
    {
        utf8->bytes[0] = (0xc0 | ((uc >> 6) & 0x1f));
        utf8->bytes[1] = (0x80 | (uc & 0x3f));
        return 2;
    }
    else if (uc <= 0xffff)
    {
        utf8->bytes[0] = (0xe0 | ((uc >> 12) & 0x0f));
        utf8->bytes[1] = (0x80 | ((uc >> 6) & 0x3f));
        utf8->bytes[2] = (0x80 | (uc & 0x3f));
        return 3;
    }
    assert (uc <= 0x10ffff);
    utf8->bytes[0] = (0xf0 | ((uc >> 18) & 0x07));
    utf8->bytes[1] = (0x80 | ((uc >> 12) & 0x3f));
    utf8->bytes[2] = (0x80 | ((uc >> 6) & 0x3f));
    utf8->bytes[3] = (0x80 | (uc & 0x3f));
    return 4;
}

/**
   decode_utf8: Peek 1 to 4 chars from start and calculate unicode codepoint.
   @start: Read utf8 from this string.
   @stop: Points to byte after last byte in string. If NULL assume start is null terminated.
   @out_char: Store the unicode code point here.
   @out_len: How many bytes the utf8 char used.

   Return true if valid utf8 char.
*/
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

bool is_xmq_element_name(const char *start, const char *stop)
{
    const char *i = start;
    if (!is_xmq_element_start(*i)) return false;
    i++;

    for (; i < stop; ++i)
    {
        char c = *i;
        if (!is_xmq_text_name(c)) return false;
    }

    return true;
}

bool is_xmq_token_whitespace(char c)
{
    if (c == ' ' || c == '\n' || c == '\r')
    {
        return true;
    }
    return false;
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
        UTF8Char uc;
        size_t n = peek_utf8_char(i, stop, &uc);
        if (n > 1)
        {
            while (n) {
                *o++ = *i;
                real++;
                n--;
                i++;
            }
            i--;
            continue;
        }
        char c = *i;
        if (c >= ' ' && c <= 126 && c != '"' && c != '\\') { *o++ = *i; real++;}
        else if (c == '\\') { *o++ = '\\'; *o++ = '\\'; real+=2; }
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

char *potentially_add_leading_ending_space(const char *start, const char *stop)
{
    char *content = NULL;
    int prefix = *start == '\'' ? 1 : 0;
    int postfix = *(stop-1) == '\'' ? 1 : 0;
    if (prefix || postfix)
    {
        size_t len = stop-start;
        len += prefix;
        len += postfix;
        content = (char*)malloc(len+1);
        if (prefix)
        {
            content[0] = ' ';
        }
        if (postfix)
        {
            content[len-1] = ' ';
        }
        memcpy(content+prefix, start, stop-start);
        content[len] = 0;
    }
    else
    {
        content = strndup(start, stop-start);
    }
    return content;
}

#endif // TEXT_MODULE

// PARTS UTF8_C ////////////////////////////////////////

#ifdef UTF8_MODULE

size_t print_utf8_char(XMQPrintState *ps, const char *start, const char *stop)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    const char *i = start;

    // Find next utf8 char....
    const char *j = i+1;
    while (j < stop && (*j & 0xc0) == 0x80) j++;

    // Is the utf8 char a unicode whitespace and not space,tab,cr,nl?
    bool uw = is_unicode_whitespace(i, j);
    //bool tw = *i == '\t';

    // If so, then color it. This will typically red underline the non-breakable space.
    if (uw) print_color_pre(ps, COLOR_unicode_whitespace);

    if (*i == ' ')
    {
        write(writer_state, os->explicit_space, NULL);
    }
    else
    if (*i == '\t')
    {
        write(writer_state, os->explicit_tab, NULL);
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
   start: Points to bytes to be printed.
   stop: Points to byte after last byte to be printed. If NULL then assume start is null-terminated.

   Returns number of bytes printed.
*/
size_t print_utf8_internal(XMQPrintState *ps, const char *start, const char *stop)
{
    if (stop == start || *start == 0) return 0;

    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    size_t u_len = 0;

    const char *i = start;
    while (*i && (!stop || i < stop))
    {
        // Find next utf8 char....
        const char *j = i+1;
        while (j < stop && (*j & 0xc0) == 0x80) j++;

        // Is the utf8 char a unicode whitespace and not space,tab,cr,nl?
        bool uw = is_unicode_whitespace(i, j);
        //bool tw = *i == '\t';

        // If so, then color it. This will typically red underline the non-breakable space.
        if (uw) print_color_pre(ps, COLOR_unicode_whitespace);

        if (*i == ' ')
        {
            write(writer_state, os->explicit_space, NULL);
        }
        else if (*i == '\t')
        {
            write(writer_state, os->explicit_tab, NULL);
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
   @num_pairs:  Number of start, stop pairs.
   @start: First utf8 byte to print.
   @stop: Points to byte after the last utf8 content.

   Returns the number of bytes used after start.
*/
size_t print_utf8(XMQPrintState *ps, XMQColor color, size_t num_pairs, ...)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    const char *pre, *post;
    getThemeStrings(os, color, &pre, &post);
    const char *previous_color = NULL;

    if (pre)
    {
        write(writer_state, pre, NULL);
        previous_color = ps->replay_active_color_pre;
        ps->replay_active_color_pre = pre;
    }

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

    if (post)
    {
        write(writer_state, post, NULL);
    }
    if (previous_color)
    {
        ps->replay_active_color_pre = previous_color;
    }

    return b_len;
}

#endif // UTF8_MODULE

// PARTS XML_C ////////////////////////////////////////

#ifdef XML_MODULE

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

xmlAttr *xml_get_attribute(xmlNode *node, const char *name)
{
    return xmlHasProp(node, (const xmlChar*)name);
}

xmlNs *xml_first_namespace_def(xmlNode *node)
{
    return node->nsDef;
}

bool xml_non_empty_namespace(xmlNs *ns)
{
    const char *prefix = (const char*)ns->prefix;
    const char *href = (const char*)ns->href;

    // These three are non_empty.
    //   xmlns = "something"
    //   xmlns:something = ""
    //   xmlns:something = "something"
    return (href && href[0]) || (prefix && prefix[0]);
}

bool xml_has_non_empty_namespace_defs(xmlNode *node)
{
    xmlNs *ns = node->nsDef;
    if (ns) return true;
    /*!= NULL)
    while (ns)
    {
        //if (xml_non_empty_namespace(ns)) return true;
        ns = xml_next_namespace_def(ns);
    }
    */
    return false;
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

bool is_pi_node(const xmlNode *node)
{
    return node->type == XML_PI_NODE;
}

bool is_doctype_node(const xmlNode *node)
{
    return node->type == XML_DTD_NODE;
}

bool is_element_node(const xmlNode *node)
{
    return node->type == XML_ELEMENT_NODE;
}

bool is_attribute_node(const xmlNode *node)
{
    return node->type == XML_ATTRIBUTE_NODE;
}

bool is_text_node(const xmlNode *node)
{
    return node->type == XML_TEXT_NODE;
}

bool is_key_value_node(xmlNodePtr node)
{
    void *from = xml_first_child(node);
    void *to = xml_last_child(node);

    // Single content or entity node.
    bool yes = from && from == to && (is_content_node((xmlNode*)from) || is_entity_node((xmlNode*)from));
    if (yes) return true;

    if (!from) return false;

    // Multiple text or entity nodes.
    xmlNode *i = node->children;
    while (i)
    {
        xmlNode *next = i->next;
        if (i->type != XML_TEXT_NODE &&
            i->type != XML_ENTITY_REF_NODE)
        {
            // Found something other than text or character entities.
            return false;
        }
        i = next;
    }
    return true;
}

bool is_leaf_node(xmlNode *node)
{
    return xml_first_child(node) == NULL;
}

bool has_attributes(xmlNodePtr node)
{
    return NULL != xml_first_attribute(node);
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

char *xml_collapse_text(xmlNode *node)
{
    xmlNode *i = node->children;
    size_t len = 0;
    size_t num_text = 0;
    size_t num_entities = 0;

    while (i)
    {
        xmlNode *next = i->next;
        if (i->type != XML_TEXT_NODE &&
            i->type != XML_ENTITY_REF_NODE)
        {
            // Found something other than text or character entities.
            // Cannot collapse.
            return NULL;
        }
        if (i->type == XML_TEXT_NODE)
        {
            len += strlen((const char*)i->content);
            num_text++;
        }
        else
        {
            // &apos;
            len += 2 + strlen((const char*)i->name);
            num_entities++;
        }
        i = next;
    }

    // It is already collapsed.
    if (num_text <= 1 && num_entities == 0) return NULL;

    char *buf = (char*)malloc(len+1);
    char *out = buf;
    i = node->children;
    while (i)
    {
        xmlNode *next = i->next;
        if (i->type == XML_TEXT_NODE)
        {
            size_t l = strlen((const char *)i->content);
            memcpy(out, i->content, l);
            out += l;
        }
        else
        {
            int uc = decode_entity_ref((const char *)i->name);
            UTF8Char utf8;
            size_t n = encode_utf8(uc, &utf8);
            for (size_t j = 0; j < n; ++j)
            {
                *out++ = utf8.bytes[j];
            }
        }
        i = next;
    }
    *out = 0;
    out++;
    buf = (char*)realloc(buf, out-buf);
    return buf;
}

int decode_entity_ref(const char *name)
{
    if (!strcmp(name, "apos")) return '\'';
    if (!strcmp(name, "gt")) return '>';
    if (!strcmp(name, "lt")) return '<';
    if (!strcmp(name, "quot")) return '"';
    if (!strcmp(name, "nbsp")) return 160;
    if (name[0] != '#') return 0;
    if (name[1] == 'x') {
        long v = strtol((const char*)name, NULL, 16);
        return (int)v;
    }
    return atoi(name+1);
}

void xml_add_root_child(xmlDoc *doc, xmlNode *node)
{
    if (doc->children == NULL)
    {
        doc->children = node;
        doc->last = node;
    }
    else
    {
        xmlNode *prev = doc->last;
        prev->next = node;
        node->prev = prev;
        doc->last = node;
    }
}

#endif // XML_MODULE

// PARTS XMQ_INTERNALS_C ////////////////////////////////////////

#ifdef XMQ_INTERNALS_MODULE

void generate_state_error_message(XMQParseState *state, XMQParseError error_nr, const char *start, const char *stop)
{
    // Error detected during parsing and this is where the longjmp will end up!
    if (!state->generating_error_msg)
    {
        state->generating_error_msg = new_membuffer();
    }
    else
    {
        membuffer_drop_last_null(state->generating_error_msg);
        membuffer_append(state->generating_error_msg, "\n");
    }

    const char *error = xmqParseErrorToString(error_nr);

    const char *statei = state->i;
    size_t line = state->line;
    size_t col = state->col;

    // For certain errors we have a better point where the error triggered.
    if (error_nr == XMQ_ERROR_BODY_NOT_CLOSED)
    {
        statei = state->last_body_start;
        line = state->last_body_start_line;
        col = state->last_body_start_col;
    }
    else if (error_nr == XMQ_ERROR_ATTRIBUTES_NOT_CLOSED)
    {
        statei = state->last_attr_start;
        line = state->last_attr_start_line;
        col = state->last_attr_start_col;
    }
    else if (error_nr == XMQ_ERROR_QUOTE_NOT_CLOSED)
    {
        statei = state->last_quote_start;
        line = state->last_quote_start_line;
        col = state->last_quote_start_col;
    }
    else if (error_nr == XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS)
    {
        statei = state->last_equals_start;
        line = state->last_equals_start_line;
        col = state->last_equals_start_col;
    }
    else if (error_nr == XMQ_WARNING_QUOTES_NEEDED)
    {
        statei = state->last_suspicios_quote_end;
        line = state->last_suspicios_quote_end_line;
        col = state->last_suspicios_quote_end_col;
    }

    // Move pointer to the beginning of the line and
    // calculate the indent for the line so that we
    // can position the ^ marker under the problematic character.
    // Give up on lines longer than 1024 characters.
    size_t line_length = 0;
    size_t indent = 0;

    const char *line_start = statei;
    while (line_start > start && *(line_start-1) != '\n' && line_length < 1024)
    {
        line_length++;
        line_start--;
        indent++;
    }

    // Now find the end of the line.
    const char *line_stop = statei;
    while (line_stop < stop && *line_stop && *line_stop != '\n' && line_length < 1024)
    {
        line_length++;
        line_stop++;
    }

    const char *char_error = "";
    char buf[32];

    if (error_nr == XMQ_ERROR_INVALID_CHAR ||
        error_nr == XMQ_ERROR_JSON_INVALID_CHAR)
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
                 (int)line_length,
                 line_start,
                 (int)indent,
                 "^");
    }

    const char *e_or_w = "error";
    if (error_nr >= XMQ_WARNING_QUOTES_NEEDED)
    {
        e_or_w = "warning";
    }
    char error_msg[2048];
    memset(error_msg, 0, sizeof(error_msg));
    snprintf(error_msg, 2048,
             "%s:%zu:%zu: %s: %s%s%s",
             state->source_name,
             line, col,
             e_or_w,
             error,
             char_error,
             line_error
        );
    error_msg[2047] = 0;
    membuffer_append(state->generating_error_msg, error_msg);
    membuffer_append_null(state->generating_error_msg);
}

size_t count_whitespace(const char *i, const char *stop)
{
    unsigned char c = *i;
    if (c == ' ' || c == '\n' || c == '\t' || c == '\r')
    {
        return 1;
    }

    if (i+1 >= stop) return 0;

    // If not a unbreakable space U+00A0 (utf8 0xc2a0)
    // or the other unicode whitespaces (utf8 starts with 0xe2)
    // then we have not whitespaces here.
    if (c != 0xc2 && c != 0xe2) return 0;

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

void eat_xml_whitespace(XMQParseState *state, const char **start, const char **stop)
{
    const char *i = state->i;
    const char *buffer_stop = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;
    if (start) *start = i;

    size_t nw = count_whitespace(i, buffer_stop);
    if (!nw) return;

    while (i < buffer_stop)
    {
        size_t nw = count_whitespace(i, buffer_stop);
        if (!nw) break;
        // Pass the first char, needed to detect '\n' which increments line and set cols to 1.
        increment(*i, nw, &i, &line, &col);
    }

    if (stop) *stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
}

void eat_xmq_token_whitespace(XMQParseState *state, const char **start, const char **stop)
{
    const char *i = state->i;
    const char *buffer_stop = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;
    if (start) *start = i;

    size_t nw = count_whitespace(i, buffer_stop);
    if (!nw) return;

    while (i < buffer_stop)
    {
        size_t nw = count_whitespace(i, buffer_stop);
        if (!nw) break;
        // Tabs are not permitted as xmq token whitespace.
        if (nw == 1 && *i == '\t') break;
        // Pass the first char, needed to detect '\n' which increments line and set cols to 1.
        increment(*i, nw, &i, &line, &col);
    }

    if (stop) *stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
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

bool is_hex(char c)
{
    return
        (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F');
}

unsigned char hex_value(char c)
{
    if (c >= '0' && c <= '9') return c-'0';
    if (c >= 'a' && c <= 'f') return 10+c-'a';
    if (c >= 'A' && c <= 'F') return 10+c-'A';
    assert(false);
    return 0;
}

bool is_unicode_whitespace(const char *start, const char *stop)
{
    size_t n = count_whitespace(start, stop);

    // Single char whitespace is ' ' '\t' '\n' '\r'
    // First unicode whitespace is 160 nbsp require two or more chars.
    return n > 1;
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

void print_color_pre(XMQPrintState *ps, XMQColor color)
{
    XMQOutputSettings *os = ps->output_settings;
    const char *pre = NULL;
    const char *post = NULL;
    getThemeStrings(os, color, &pre, &post);

    if (pre)
    {
        XMQWrite write = os->content.write;
        void *writer_state = os->content.writer_state;
        write(writer_state, pre, NULL);
    }
}

void print_color_post(XMQPrintState *ps, XMQColor color)
{
    XMQOutputSettings *os = ps->output_settings;
    const char *pre = NULL;
    const char *post = NULL;
    getThemeStrings(os, color, &pre, &post);

    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    if (post)
    {
        write(writer_state, post, NULL);
    }
    else
    {
        write(writer_state, ps->replay_active_color_pre, NULL);
    }
}

const char *xmqParseErrorToString(XMQParseError e)
{
    switch (e)
    {
    case XMQ_ERROR_NONE: return "no warning, no error";
    case XMQ_ERROR_CANNOT_READ_FILE: return "cannot read file";
    case XMQ_ERROR_OOM: return "out of memory";
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
    case XMQ_ERROR_UNEXPECTED_TAB: return "invalid tab character found, remember that tab is not allowed as a field separator, to store tab as content it must be quoted";
    case XMQ_ERROR_INVALID_CHAR: return "unexpected character";
    case XMQ_ERROR_BAD_DOCTYPE: return "doctype could not be parsed";
    case XMQ_ERROR_CANNOT_HANDLE_XML: return "cannot handle xml use libxmq-all for this!";
    case XMQ_ERROR_CANNOT_HANDLE_HTML: return "cannot handle html use libxmq-all for this!";
    case XMQ_ERROR_CANNOT_HANDLE_JSON: return "cannot handle json use libxmq-all for this!";
    case XMQ_ERROR_JSON_INVALID_ESCAPE: return "invalid json escape";
    case XMQ_ERROR_JSON_INVALID_CHAR: return "unexpected json character";
    case XMQ_ERROR_EXPECTED_XMQ: return "expected xmq source";
    case XMQ_ERROR_EXPECTED_HTMQ: return "expected htmlq source";
    case XMQ_ERROR_EXPECTED_XML: return "expected xml source";
    case XMQ_ERROR_EXPECTED_HTML: return "expected html source";
    case XMQ_ERROR_EXPECTED_JSON: return "expected json source";
    case XMQ_ERROR_PARSING_XML: return "error parsing xml";
    case XMQ_ERROR_PARSING_HTML: return "error parsing html";
    case XMQ_ERROR_VALUE_CANNOT_START_WITH: return "value cannot start with = /* or //";
    case XMQ_WARNING_QUOTES_NEEDED: return "perhaps you need more quotes to quote this quote";
    }
    assert(false);
    return "unknown error";
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
    return COLOR_none;
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
    return COLOR_none;
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

/** Check if a value can start with these two characters. */
bool unsafe_value_start(char c, char cc)
{
    return c == '=' || c == '&' || (c == '/' && (cc == '/' || cc == '*'));
}

bool is_safe_value_char(const char *i, const char *stop)
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


#endif // XMQ_INTERNALS_MODULE

// PARTS XMQ_PARSER_C ////////////////////////////////////////

#ifdef XMQ_PARSER_MODULE

void eat_xmq_doctype(XMQParseState *state, const char **text_start, const char **text_stop);
void eat_xmq_pi(XMQParseState *state, const char **text_start, const char **text_stop);
void eat_xmq_text_name(XMQParseState *state, const char **content_start, const char **content_stop,
                       const char **namespace_start, const char **namespace_stop);
bool possibly_lost_content_after_equals(XMQParseState *state);
bool possibly_need_more_quotes(XMQParseState *state);

size_t count_xmq_quotes(const char *i, const char *stop)
{
    const char *start = i;

    while (i < stop && *i == '\'') i++;

    return i-start;
}

void eat_xmq_quote(XMQParseState *state, const char **start, const char **stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    size_t depth = count_xmq_quotes(i, end);
    size_t count = depth;

    state->last_quote_start = state->i;
    state->last_quote_start_line = state->line;
    state->last_quote_start_col = state->col;

    *start = i;

    while (count > 0)
    {
        increment('\'', 1, &i, &line, &col);
        count--;
    }

    if (depth == 2)
    {
        // The empty quote ''
        state->i = i;
        state->line = line;
        state->col = col;
        *stop = i;
        return;
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
            while (count > 0)
            {
                increment('\'', 1, &i, &line, &col);
                count--;
            }
            depth = 0;
            *stop = i;
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

    if (possibly_need_more_quotes(state))
    {
        state->last_suspicios_quote_end = state->i-1;
        state->last_suspicios_quote_end_line = state->line;
        state->last_suspicios_quote_end_col = state->col-1;
    }
}

void eat_xmq_entity(XMQParseState *state)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;

    size_t line = state->line;
    size_t col = state->col;
    increment('&', 1, &i, &line, &col);

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

void eat_xmq_text_name(XMQParseState *state,
                       const char **text_start,
                       const char **text_stop,
                       const char **namespace_start,
                       const char **namespace_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    const char *colon = NULL;
    size_t line = state->line;
    size_t col = state->col;

    *text_start = i;

    while (i < end)
    {
        char c = *i;
        if (!is_xmq_text_name(c)) break;
        if (c == ':') colon = i;
        increment(c, 1, &i, &line, &col);
    }

    if (colon)
    {
        *namespace_start = *text_start;
        *namespace_stop = colon;
        *text_start = colon+1;
    }
    else
    {
        *namespace_start = NULL;
        *namespace_stop = NULL;
    }
    *text_stop = i;
    state->i = i;
    state->line = line;
    state->col = col;
}

void eat_xmq_text_value(XMQParseState *state)
{
    const char *i = state->i;
    const char *stop = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;

    while (i < stop)
    {
        char c = *i;
        if (!is_safe_value_char(i, stop)) break;
        increment(c, 1, &i, &line, &col);
    }

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

void eat_xmq_pi(XMQParseState *state, const char **text_start, const char **text_stop)
{
    const char *i = state->i;
    const char *end = state->buffer_stop;
    size_t line = state->line;
    size_t col = state->col;
    *text_start = i;

    assert(*i == '?');
    increment('?', 1, &i, &line, &col);
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

bool is_xmq_pi_start(const char *start, const char *stop)
{
    if (*start != '?') return false;
    // We need at least one character, eg. ?x
    if (start+2 > stop) return false;
    return true;
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

bool is_xmq_text_value(const char *start, const char *stop)
{
    char c = *start;
    char cc = *(start+1);

    if (unsafe_value_start(c, cc)) return false;

    for (const char *i = start; i < stop; ++i)
    {
        if (!is_safe_value_char(i, stop))
        {
            return false;
        }
    }
    return true;
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

void parse_xmq(XMQParseState *state)
{
    const char *end = state->buffer_stop;

    while (state->i < end)
    {
        char c = *(state->i);
        char cc = 0;
        if ((c == '/' || c == '(') && state->i+1 < end) cc = *(state->i+1);

        if (is_xmq_token_whitespace(c)) parse_xmq_whitespace(state);
        else if (is_xmq_quote_start(c)) parse_xmq_quote(state, LEVEL_XMQ);
        else if (is_xmq_entity_start(c)) parse_xmq_entity(state, LEVEL_XMQ);
        else if (is_xmq_comment_start(c, cc)) parse_xmq_comment(state, cc);
        else if (is_xmq_element_start(c)) parse_xmq_element(state);
        else if (is_xmq_doctype_start(state->i, end)) parse_xmq_doctype(state);
        else if (is_xmq_pi_start(state->i, end)) parse_xmq_pi(state);
        else if (c == '}') return;
        else
        {
            if (possibly_lost_content_after_equals(state))
            {
                state->error_nr = XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS;
            }
            else if (c == '\t')
            {
                state->error_nr = XMQ_ERROR_UNEXPECTED_TAB;
            }
            else
            {
                state->error_nr = XMQ_ERROR_INVALID_CHAR;
            }
            longjmp(state->error_handler, 1);
        }
    }
}

void parse_xmq_quote(XMQParseState *state, Level level)
{
    size_t start_line = state->line;
    size_t start_col = state->col;
    const char *start;
    const char *stop;

    eat_xmq_quote(state, &start, &stop);

    switch(level)
    {
    case LEVEL_XMQ:
       DO_CALLBACK(quote, state, start_line, start_col, start, stop, stop);
       break;
    case LEVEL_ELEMENT_VALUE:
        DO_CALLBACK(element_value_quote, state, start_line, start_col, start, stop, stop);
        break;
    case LEVEL_ELEMENT_VALUE_COMPOUND:
        DO_CALLBACK(element_value_compound_quote, state, start_line, start_col, start, stop, stop);
        break;
    case LEVEL_ATTR_VALUE:
        DO_CALLBACK(attr_value_quote, state, start_line, start_col, start, stop, stop);
        break;
    case LEVEL_ATTR_VALUE_COMPOUND:
        DO_CALLBACK(attr_value_compound_quote, state, start_line, start_col, start, stop, stop);
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

    eat_xmq_entity(state);
    const char *stop = state->i;

    switch (level) {
    case LEVEL_XMQ:
        DO_CALLBACK(entity, state, start_line, start_col, start,  stop, stop);
        break;
    case LEVEL_ELEMENT_VALUE:
        DO_CALLBACK(element_value_entity, state, start_line, start_col, start, stop, stop);
        break;
    case LEVEL_ELEMENT_VALUE_COMPOUND:
        DO_CALLBACK(element_value_compound_entity, state, start_line, start_col, start,  stop, stop);
        break;
    case LEVEL_ATTR_VALUE:
        DO_CALLBACK(attr_value_entity, state, start_line, start_col, start, stop, stop);
        break;
    case LEVEL_ATTR_VALUE_COMPOUND:
        DO_CALLBACK(attr_value_compound_entity, state, start_line, start_col, start, stop, stop);
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
        DO_CALLBACK(comment, state, start_line, start_col, start, stop, stop);
    }
    else
    {
        // This is a /* ... */ or ////*  ... *//// comment.
        eat_xmq_comment_to_close(state, &comment_start, &comment_stop, n, &found_asterisk);
        const char *stop = state->i;
        DO_CALLBACK(comment, state, start_line, start_col, start, stop, stop);

        while (found_asterisk)
        {
            // Aha, this is a comment continuation /* ... */* ...
            start = state->i;
            start_line = state->line;
            start_col = state->col;
            eat_xmq_comment_to_close(state, &comment_start, &comment_stop, n, &found_asterisk);
            stop = state->i;
            DO_CALLBACK(comment_continuation, state, start_line, start_col, start, stop, stop);
        }
    }
}

void parse_xmq_text_value(XMQParseState *state, Level level)
{
    const char *start = state->i;
    int start_line = state->line;
    int start_col = state->col;

    eat_xmq_text_value(state);
    const char *stop = state->i;

    assert(level != LEVEL_XMQ);
    if (level == LEVEL_ATTR_VALUE)
    {
        DO_CALLBACK(attr_value_text, state, start_line, start_col, start, stop, stop);
    }
    else
    {
        DO_CALLBACK(element_value_text, state, start_line, start_col, start, stop, stop);
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
        char cc = *(state->i+1);
        if (unsafe_value_start(c, cc))
        {
            state->error_nr = XMQ_ERROR_VALUE_CANNOT_START_WITH;
            longjmp(state->error_handler, 1);
        }
        parse_xmq_text_value(state, level);
    }
}

void parse_xmq_element_internal(XMQParseState *state, bool doctype, bool pi)
{
    char c = 0;
    // Name
    const char *name_start = NULL;
    const char *name_stop = NULL;
    // Namespace
    const char *ns_start = NULL;
    const char *ns_stop = NULL;

    size_t start_line = state->line;
    size_t start_col = state->col;

    if (doctype)
    {
        eat_xmq_doctype(state, &name_start, &name_stop);
    }
    else if (pi)
    {
        eat_xmq_pi(state, &name_start, &name_stop);
    }
    else
    {
        eat_xmq_text_name(state, &name_start, &name_stop, &ns_start, &ns_stop);
    }
    const char *stop = state->i;

    // The only peek ahead in the whole grammar! And its only for syntax coloring. :-)
    // key = 123   vs    name { '123' }
    bool is_key = peek_xmq_next_is_equal(state);

    if (!ns_start)
    {
        // Normal key/name element.
        if (is_key)
        {
            DO_CALLBACK(element_key, state, start_line, start_col, name_start, name_stop, stop);
        }
        else
        {
            DO_CALLBACK(element_name, state, start_line, start_col, name_start, name_stop, stop);
        }
    }
    else
    {
        // We have a namespace prefixed to the element, eg: abc:working
        size_t ns_len = ns_stop - ns_start;
        DO_CALLBACK(element_ns, state, start_line, start_col, ns_start, ns_stop, ns_stop);
        DO_CALLBACK(ns_colon, state, start_line, start_col+ns_len, ns_stop, ns_stop+1, ns_stop+1);

        if (is_key)
        {
            DO_CALLBACK(element_key, state, start_line, start_col+ns_len+1, name_start, name_stop, stop);
        }
        else
        {
            DO_CALLBACK(element_name, state, start_line, start_col+ns_len+1, name_start, name_stop, stop);
        }
    }


    c = *state->i;
    if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }

    if (c == '(')
    {
        const char *start = state->i;
        state->last_attr_start = state->i;
        state->last_attr_start_line = state->line;
        state->last_attr_start_col = state->col;
        start_line = state->line;
        start_col = state->col;
        increment('(', 1, &state->i, &state->line, &state->col);
        const char *stop = state->i;
        DO_CALLBACK(apar_left, state, start_line, start_col, start, stop, stop);

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

        start_line = state->line;
        start_col = state->col;
        increment(')', 1, &state->i, &state->line, &state->col);
        stop = state->i;
        DO_CALLBACK(apar_right, state, start_line, start_col, parentheses_right_start, parentheses_right_stop, stop);
    }

    c = *state->i;
    if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }

    if (c == '=')
    {
        state->last_equals_start = state->i;
        state->last_equals_start_line = state->line;
        state->last_equals_start_col = state->col;
        const char *start = state->i;
        start_line = state->line;
        start_col = state->col;
        increment('=', 1, &state->i, &state->line, &state->col);
        const char *stop = state->i;

        DO_CALLBACK(equals, state, start_line, start_col, start, stop, stop);

        parse_xmq_value(state, LEVEL_ELEMENT_VALUE);
        return;
    }

    if (c == '{')
    {
        const char *start = state->i;
        state->last_body_start = state->i;
        state->last_body_start_line = state->line;
        state->last_body_start_col = state->col;
        start_line = state->line;
        start_col = state->col;
        increment('{', 1, &state->i, &state->line, &state->col);
        const char *stop = state->i;
        DO_CALLBACK(brace_left, state, start_line, start_col, start, stop, stop);

        parse_xmq(state);
        c = *state->i;
        if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }
        if (c != '}')
        {
            state->error_nr = XMQ_ERROR_BODY_NOT_CLOSED;
            longjmp(state->error_handler, 1);
        }

        start = state->i;
        start_line = state->line;
        start_col = state->col;
        increment('}', 1, &state->i, &state->line, &state->col);
        stop = state->i;
        DO_CALLBACK(brace_right, state, start_line, start_col, start, stop, stop);
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
    const char *ns_start = NULL;
    const char *ns_stop = NULL;

    int start_line = state->line;
    int start_col = state->col;

    eat_xmq_text_name(state, &name_start, &name_stop, &ns_start, &ns_stop);
    const char *stop = state->i;

    if (!ns_start)
    {
        // No colon found, we have either a normal: key=123
        // or a default namespace declaration xmlns=...
        size_t len = name_stop - name_start;
        if (len == 5 && !strncmp(name_start, "xmlns", 5))
        {
            // A default namespace declaration, eg: xmlns=uri
            DO_CALLBACK(ns_declaration, state, start_line, start_col, name_start, name_stop, name_stop);
        }
        else
        {
            // A normal attribute key, eg: width=123
            DO_CALLBACK(attr_key, state, start_line, start_col, name_start, name_stop, stop);
        }
    }
    else
    {
        // We have a colon in the attribute key.
        // E.g. alfa:beta where alfa is attr_ns and beta is attr_key
        // However we can also have xmlns:xsl then it gets tokenized as ns_declaration and attr_ns.
        size_t ns_len = ns_stop - ns_start;
        if (ns_len == 5 && !strncmp(ns_start, "xmlns", 5))
        {
            // The xmlns signals a declaration of a namespace.
            DO_CALLBACK(ns_declaration, state, start_line, start_col, ns_start, ns_stop, name_stop);
            DO_CALLBACK(ns_colon, state, start_line, start_col+ns_len, ns_stop, ns_stop+1, ns_stop+1);
            DO_CALLBACK(attr_ns, state, start_line, start_col+ns_len+1, name_start, name_stop, stop);
        }
        else
        {
            // Normal namespaced attribute. Please try to avoid namespaced attributes because you only need to attach the
            // namespace to the element itself, from that follows automatically the unique namespaced attributes.
            // The exception being special use cases as: xlink:href.
            DO_CALLBACK(attr_ns, state, start_line, start_col, ns_start, ns_stop, ns_stop);
            DO_CALLBACK(ns_colon, state, start_line, start_col+ns_len, ns_stop, ns_stop+1, ns_stop+1);
            DO_CALLBACK(attr_key, state, start_line, start_col+ns_len+1, name_start, name_stop, stop);
        }
    }

    c = *state->i;
    if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }

    if (c == '=')
    {
        const char *start = state->i;
        start_line = state->line;
        start_col = state->col;
        increment('=', 1, &state->i, &state->line, &state->col);
        const char *stop = state->i;
        DO_CALLBACK(equals, state, start_line, start_col, start, stop, stop);
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
    increment('(', 1, &state->i, &state->line, &state->col);
    const char *stop = state->i;
    DO_CALLBACK(cpar_left, state, start_line, start_col, start, stop, stop);

    parse_xmq_compound_children(state, enter_compound_level(level));

    char c = *state->i;
    if (is_xml_whitespace(c)) { parse_xmq_whitespace(state); c = *state->i; }

    if (c != ')')
    {
        state->error_nr = XMQ_ERROR_COMPOUND_NOT_CLOSED;
        longjmp(state->error_handler, 1);
    }

    start = state->i;
    start_line = state->line;
    start_col = state->col;
    increment(')', 1, &state->i, &state->line, &state->col);
    stop = state->i;
    DO_CALLBACK(cpar_right, state, start_line, start_col, start, stop, stop);
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

bool possibly_need_more_quotes(XMQParseState *state)
{
    if (state->i - 2 < state->buffer_start ||
        state->i >= state->buffer_stop)
    {
        return false;
    }
    // Should have triple quotes: 'There's a man.'
    // c0 = e
    // c1 = '
    // c2 = s
    char c0 = *(state->i-2);
    char c1 = *(state->i-1); // This is the apostrophe
    char c2 = *(state->i);

    // We have just parsed a quote. Check if this is a false ending and
    // there is a syntax error since we need more quotes. For example:
    // greeting = 'There's a man, a wolf and a boat.'
    // We get this error:
    // ../forgot.xmq:1:26: error: unexpected character "," U+2C
    // greeting = 'There's a man, a wolf and a boat.'
    //                          ^
    // The quote terminated too early, we need three quotes.
    //
    // This function detects a suspicious quote ending and remembers it,
    // but does not flag an error until the parser fails.

    // Any non-quote quote non-quote, is suspicios: for example: g's t's
    // or e'l or y'v etc....
    // But do not trigger on [space]'x since that is probably a valid quote start.
    if (c0 != '\'' &&
        c0 != ' ' &&
        c1 == '\'' &&
        c2 != '\'') return true;

    return false;

    // isn't doesn't shouldn't can't aren't won't
    // dog's it's
    // we'll
    // they've
    // he'd
    // she'd've
    // 'clock
    // Hallowe'en
    // fo'c's'le = forecastle
    // cat-o'-nine-tails = cat-of-nine-tails
    // ne'er-do-well = never-do-well
    // will-o'-the-wisp
    // 'tis = it is
    // o'er = over
    // 'twas = it was
    // e'en = even
    // 'Fraid so.'Nother drink?
    // I s'pose so.'S not funny.
    // O'Leary (Irish), d'Abbadie (French), D'Angelo (Italian), M'Tavish (Scots Gaelic)
    // Robert Burns poetry: gi' for give and a' for all
    // the generation of '98
    // James's shop (or James' shop)
    // a month's pay
    // For God's sake! (= exclamation of exasperation)
    // a stone's throw away (= very near)
    // at death's door (= very ill)
    // in my mind's eye (= in my imagination)
}

void parse_xmq_whitespace(XMQParseState *state)
{
    size_t start_line = state->line;
    size_t start_col = state->col;
    const char *start;
    const char *stop;
    eat_xmq_token_whitespace(state, &start, &stop);
    DO_CALLBACK(whitespace, state, start_line, start_col, start, stop, stop);
}

#endif // XMQ_PARSER_MODULE

// PARTS XMQ_PRINTER_C ////////////////////////////////////////

#ifdef XMQ_PRINTER_MODULE

bool xml_has_non_empty_namespace_defs(xmlNode *node);
bool xml_non_empty_namespace(xmlNs *ns);
const char *toHtmlEntity(int uc);

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
size_t count_necessary_quotes(const char *start, const char *stop, bool compact, bool *add_nls, bool *add_compound)
{
    size_t max = 0;
    size_t curr = 0;
    bool all_safe = true;

    assert(stop > start);

    if (unsafe_value_start(*start, start+1 < stop ? *(start+1):0))
    {
        // Content starts with = & // or /* so it must be quoted.
        all_safe = false;
    }

    if (*start == '\'' || *(stop-1) == '\'')
    {
        // If leading or ending quote, then add newlines both at the beginning and at the end.
        // Strictly speaking, if only a leading quote, then only newline at beginning is needed.
        // However to reduce visual confusion, we add newlines at beginning and end.
        if (!compact)
        {
            // We quote this using:
            // '''
            // 'howdy'
            // '''
            *add_nls = true;
        }
        else
        {
            // We quote this using:
            // ( &#39; 'howdy' &#39; )
            *add_compound = true;
        }
    }

    size_t only_prepended_newlines = 0;
    size_t only_appended_newlines = 0;
    const char *ls = has_leading_space_nl(start, stop, &only_prepended_newlines);
    const char *es = has_ending_nl_space(start, stop, &only_appended_newlines);

    // We do not need to add a compound, if there is no leading nl+space or if there is pure newlines.
    // Likewise for the ending. Test this.
    if ((ls != NULL && only_prepended_newlines == 0) || // We have leading nl and some non-newlines.
        (es != NULL && only_appended_newlines == 0))    // We have ending nl and some non-newlines.
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
            all_safe &= is_safe_value_char(i, stop);
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

void print_entity_node(XMQPrintState *ps, xmlNode *node)
{
    check_space_before_entity_node(ps);

    print_utf8(ps, COLOR_entity, 1, "&", NULL);
    print_utf8(ps, COLOR_entity, 1, (const char*)node->name, NULL);
    print_utf8(ps, COLOR_entity, 1, ";", NULL);
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
        if (*i != '\n') print_white_spaces(ps, 1);
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
                    if (*(i-1) == 10 && *(i+1) != 0)
                    {
                        // This is an empty line. Do not indent.
                        // Except the last line which must be indented.
                        print_nl(ps, NULL, NULL);
                    }
                    else
                    {
                        print_nl_and_indent(ps, NULL, NULL);
                    }
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

    check_space_before_comment(ps);

    bool has_newline = has_newlines(start, stop);
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

    XMQColor ns_color = COLOR_element_ns;
    XMQColor key_color = COLOR_element_key;
    XMQColor name_color = COLOR_element_name;

    check_space_before_key(ps);

    node_strlen_name_prefix(node, &name, &name_len, &prefix, &prefix_len, &total_u_len);

    if (prefix)
    {
        if (!strcmp(prefix, "xsl"))
        {
            //ns_color = COLOR_ns_override_xsl;
            key_color = COLOR_ns_override_xsl;
            name_color = COLOR_ns_override_xsl;
        }
        print_utf8(ps, ns_color, 1, prefix, NULL);
        print_utf8(ps, COLOR_ns_colon, 1, ":", NULL);
    }

    if (is_key_value_node(node) && !xml_first_attribute(node))
    {
        // Only print using key color if = and no attributes.
        // I.e. alfa=1
        print_utf8(ps, key_color, 1, name, NULL);
    }
    else
    {
        // All other cases print with node color.
        // I.e. alfa{a b} alfa(x=1)=1
        print_utf8(ps, name_color, 1, name, NULL);
    }

    bool has_non_empty_ns = xml_has_non_empty_namespace_defs(node);

    if (xml_first_attribute(node) || has_non_empty_ns)
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

void print_pi_node(XMQPrintState *ps, xmlNode *node)
{
    if (!node) return;

    check_space_before_key(ps);
    size_t name_len = strlen((const char*)node->name);
    print_utf8(ps, COLOR_element_key, 2, "?", NULL, node->name, NULL);
    if (!ps->output_settings->compact) print_white_spaces(ps, 1);
    print_utf8(ps, COLOR_equals, 1, "=", NULL);
    if (!ps->output_settings->compact) print_white_spaces(ps, 1);

    xmlBuffer *buffer = xmlBufferCreate();
    xmlNodeDump(buffer, (xmlDocPtr)ps->doq->docptr_.xml, (xmlNodePtr)node, 0, 0);
    char *c = (char*)xmlBufferContent(buffer);
    size_t n = strlen(c);
    // now detect if we need to add a leading/ending space.
    if (c[n-1] == '>' && c[n-2] == '?')
    {
        n-=2;
    }
    char *content = potentially_add_leading_ending_space(c+name_len+3, c+n);
    n = strlen(content);
    char *end = content+n;

    if (ps->output_settings->compact)
    {
        for (char *i = content; i < end; ++i)
        {
            if (*i == '\n') *i = ' ';
        }
    }

    print_value_internal_text(ps, content, end, LEVEL_ELEMENT_VALUE);

    free(content);
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

    // This is a pi node ?something
    if (is_pi_node(node))
    {
        return print_pi_node(ps, node);
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

void print_white_spaces(XMQPrintState *ps, int num)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQTheme *c = os->theme;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;
    if (c->whitespace.pre) write(writer_state, c->whitespace.pre, NULL);
    for (int i=0; i<num; ++i)
    {
        write(writer_state, os->indentation_space, NULL);
    }
    ps->current_indent += num;
    if (c->whitespace.post) write(writer_state, c->whitespace.post, NULL);
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
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    const char *pre = NULL;
    const char *post = NULL;
    getThemeStrings(os, c, &pre, &post);

    write(writer_state, pre, NULL);
    for (int i=0; i<num; ++i)
    {
        write(writer_state, os->explicit_space, NULL);
    }
    ps->current_indent += num;
    write(writer_state, post, NULL);
}

void print_quoted_spaces(XMQPrintState *ps, XMQColor color, int num)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQTheme *c = os->theme;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    if (c->quote.pre) write(writer_state, c->quote.pre, NULL);
    write(writer_state, "'", NULL);
    for (int i=0; i<num; ++i)
    {
        write(writer_state, os->explicit_space, NULL);
    }
    ps->current_indent += num;
    ps->last_char = '\'';
    write(writer_state, "'", NULL);
    if (c->quote.post) write(writer_state, c->quote.post, NULL);
}

void print_quotes(XMQPrintState *ps, size_t num, XMQColor color)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    const char *pre = NULL;
    const char *post = NULL;
    getThemeStrings(os, color, &pre, &post);

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
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    if (postfix) write(writer_state, postfix, NULL);
    write(writer_state, os->explicit_nl, NULL);
    ps->current_indent = 0;
    ps->last_char = 0;
    print_white_spaces(ps, ps->line_indent);
    if (ps->restart_line) write(writer_state, ps->restart_line, NULL);
    if (prefix) write(writer_state, prefix, NULL);
}

void print_nl(XMQPrintState *ps, const char *prefix, const char *postfix)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    if (postfix) write(writer_state, postfix, NULL);
    write(writer_state, os->explicit_nl, NULL);
    ps->current_indent = 0;
    ps->last_char = 0;
    if (ps->restart_line) write(writer_state, ps->restart_line, NULL);
    if (prefix) write(writer_state, prefix, NULL);
}

size_t print_char_entity(XMQPrintState *ps, XMQColor color, const char *start, const char *stop)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;
    const char *pre, *post;
    getThemeStrings(os, color, &pre, &post);

    int uc = 0;
    size_t bytes = 0;
    if (decode_utf8(start, stop, &uc, &bytes))
    {
        // Max entity &#1114112; max buf is 11 bytes including terminating zero byte.
        char buf[32] = {};
        memset(buf, 0, sizeof(buf));

        const char *replacement = NULL;
        if (ps->output_settings->escape_non_7bit &&
            ps->output_settings->output_format == XMQ_CONTENT_HTMQ)
        {
            replacement = toHtmlEntity(uc);
        }

        if (replacement)
        {
            snprintf(buf, 32, "&%s;", replacement);
        }
        else
        {
            snprintf(buf, 32, "&#%d;", uc);
        }

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
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;
    const char *cpre = NULL;
    const char *cpost = NULL;
    getThemeStrings(os, COLOR_comment, &cpre, &cpost);

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

void print_namespace_declaration(XMQPrintState *ps, xmlNs *ns, size_t align)
{
    //if (!xml_non_empty_namespace(ns)) return;

    check_space_before_attribute(ps);

    const char *prefix;
    size_t total_u_len;

    namespace_strlen_prefix(ns, &prefix, &total_u_len);

    print_utf8(ps, COLOR_ns_declaration, 1, "xmlns", NULL);

    if (prefix)
    {
        print_utf8(ps, COLOR_ns_colon, 1, ":", NULL);
        XMQColor ns_color = COLOR_attr_ns;
        if (!strcmp(prefix, "xsl")) ns_color = COLOR_ns_override_xsl;
        print_utf8(ps, ns_color, 1, prefix, NULL);
    }

    const char *v = xml_namespace_href(ns);

    if (v != NULL)
    {
        if (!ps->output_settings->compact) print_white_spaces(ps, 1+align-total_u_len);

        print_utf8(ps, COLOR_equals, 1, "=", NULL);

        if (!ps->output_settings->compact) print_white_spaces(ps, 1);

        print_value_internal_text(ps, v, NULL, LEVEL_ATTR_VALUE);
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
        print_namespace_declaration(ps, ns, max);
        ns = xml_next_namespace_def(ns);
    }

    ps->line_indent = line_indent;
}

void print_quote_lines_and_color_uwhitespace(XMQPrintState *ps,
                                             XMQColor color,
                                             const char *start,
                                             const char *stop)
{
    XMQOutputSettings *os = ps->output_settings;
    XMQWrite write = os->content.write;
    void *writer_state = os->content.writer_state;

    const char *pre, *post;
    getThemeStrings(os, color, &pre, &post);

    if (pre) write(writer_state, pre, NULL);

    const char *old_restart_line = ps->restart_line;
    if (!post) ps->restart_line = pre;
    else ps->restart_line = NULL;

    // We are leading with a newline, print an extra into the quote, which will be trimmed away during parse.
    if (*start == '\n')
    {
        print_nl(ps, pre, post);
    }

    bool all_newlines = true;
    for (const char *i = start; i < stop;)
    {
        if (*i == '\n')
        {
            if (i+1 < stop && *(i+1) != '\n')
            {
                print_nl_and_indent(ps, pre, post);
            }
            else
            {
                print_nl(ps, pre, post);
            }
            i++;
        }
        else
        {
            i += print_utf8_char(ps, i, stop);
            all_newlines = false;
        }
    }
    // We are ending with a newline, print an extra into the quote, which will be trimmed away during parse.
    if (*(stop-1) == '\n')
    {
        ps->line_indent--;
        if (!all_newlines)
        {
            print_nl_and_indent(ps, NULL, post);
        }
        else
        {
            ps->current_indent = 0;
            ps->last_char = 0;
            print_white_spaces(ps, ps->line_indent);
        }
        ps->line_indent++;
    }
    if (*(stop-1) != '\n' && post) write(writer_state, post, NULL);
    ps->restart_line = old_restart_line;
}

void print_safe_leaf_quote(XMQPrintState *ps,
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

    size_t old_line_indent = 0;

    if (add_nls)
    {
        old_line_indent = ps->line_indent;
        ps->line_indent = ps->current_indent;
    }

    print_quotes(ps, numq, c);

    if (!add_nls)
    {
        old_line_indent = ps->line_indent;
        ps->line_indent = ps->current_indent;
    }

    if (add_nls)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }

    print_quote_lines_and_color_uwhitespace(ps,
                                            c,
                                            start,
                                            stop);

    if (!add_nls)
    {
        ps->line_indent = old_line_indent;
    }

    if (add_nls)
    {
        print_nl_and_indent(ps, NULL, NULL);
    }

    print_quotes(ps, numq, c);

    if (add_nls)
    {
        ps->line_indent = old_line_indent;
    }
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
    bool compact = ps->output_settings->compact;
    bool newlines = ps->output_settings->escape_newlines;
    bool non7bit = ps->output_settings->escape_non_7bit;

    const char *i = start;

    if (*i == '\'' && compact)
    {
        return i;
    }
    const char *pre_stop = stop-1;
    if (compact && *pre_stop == '\'')
    {
        while (pre_stop > start && *pre_stop == '\'') pre_stop--;
        pre_stop++;
    }

    while (i < stop)
    {
        int c = (int)((unsigned char)*i);
        if (compact && c == '\'' && i == pre_stop) break;
        if (newlines && c == '\n') break;
        if (non7bit && c > 126) break;
        if (c < 32 && c != '\n') break;
        i++;
    }

    // Now move backwards, perhaps there was newlines before this problematic character...
    // Then we have to escape those as well since they are ending the previous quote.
    /*
    const char *j = i-1;
    while (j > start)
    {
        int c = (int)((unsigned char)*j);
        if (c != '\n') break;
        j--;
        }*/
    return i; // j+1;
}

void print_value_internal_text(XMQPrintState *ps, const char *start, const char *stop, Level level)
{
    if (!stop) stop = start+strlen(start);
    if (!start || start >= stop || start[0] == 0)
    {
        // This is for empty attribute values.
        // Empty elements do not have print_value invoked so there is no equal char printed here (eg = '')
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
    bool only_newlines = false;
    bool all_whitespace = has_all_whitespace(start, stop, &all_space, &only_newlines);

    if (all_space)
    {
        // This are all normal ascii 32 spaces. Print like: '     '
        check_space_before_quote(ps, level);
        print_quoted_spaces(ps, level_to_quote_color(level), (size_t)(stop-start));
        return;
    }

    if (all_whitespace)
    {
        if (only_newlines && !ps->output_settings->compact && ((size_t)(stop-start)) > 1)
        {
            // All newlines and more than 1 newline. This is printed further on.
        }
        else
        {
            // All whitespace, but more than just normal spaces, ie newlines!
            // This is often the case with trimmed whitespace, lets print using
            // entities, which makes this content be easy to spot when --trim=none is used.
            // Also works both for normal and compact mode.
            print_all_whitespace(ps, start, stop, level);
            return;
        }
    }

    if (is_xmq_text_value(start, stop) && (level == LEVEL_ELEMENT_VALUE || level == LEVEL_ATTR_VALUE))
    {
        // This is a key_node text value or an attribute text value, ie key = 123 or color=blue, ie no quoting needed.
        print_utf8(ps, level_to_quote_color(level), 1, start, stop);
        return;
    }

    size_t only_prepended_newlines = 0;
    const char *new_start = has_leading_space_nl(start, stop, &only_prepended_newlines);
    if (new_start && only_prepended_newlines == 0)
    {
        // We have a leading mix of newlines and whitespace.
        print_all_whitespace(ps, start, new_start, level);
        start = new_start;
    }

    size_t only_appended_newlines = 0;
    const char *new_stop = has_ending_nl_space(start, stop, &only_appended_newlines);
    const char *old_stop = stop;
    if (new_stop && only_appended_newlines == 0)
    {
        // We have an ending mix of newlines and whitespace.
        stop = new_stop;
        // Move forward over normal spaces.
        while (stop < old_stop && *stop == ' ') stop++;
    }

    // Ok, normal content to be quoted. However we might need to split the content
    // at chars that need to be replaced with character entities. Normally no
    // chars need to be replaced. But in compact mode, the \n newlines are replaced with &#10;
    // If content contains CR LF its replaced with &#13;&#10;
    // Also one can replace all non-ascii chars with their entities if so desired.
    for (const char *from = start; from < stop; )
    {
        const char *to = find_next_char_that_needs_escape(ps, from, stop);
        if (from == to)
        {
            check_space_before_entity_node(ps);
            to += print_char_entity(ps, level_to_entity_color(level), from, stop);
            while (from+1 < stop && *(from+1) == '\n')
            {
                // Special case, we have escaped something right before newline(s).
                // Escape the newline(s) as well. This is important for CR LF.
                // If not then we have to loop around detecting that the newline
                // is leading a quote and needs to be escaped. Escape it here already.
                from++;
                check_space_before_entity_node(ps);
                to += print_char_entity(ps, level_to_entity_color(level), from, stop);
            }
        }
        else
        {
            bool add_nls = false;
            bool add_compound = false;
            bool compact = ps->output_settings->compact;
            count_necessary_quotes(from, to, false, &add_nls, &add_compound);
            if (!add_compound && (!add_nls || !compact))
            {
                check_space_before_quote(ps, level);
                print_safe_leaf_quote(ps, level_to_quote_color(level), from, to);
            }
            else
            {
                print_value_internal_text(ps, from, to, level);
            }
        }
        from = to;
    }

    if (new_stop && only_appended_newlines == 0)
    {
        // This trailing whitespace could not be printed inside the quote.
        print_all_whitespace(ps, stop, old_stop, level);
    }
}

/**
   print_value_internal:
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
    bool compact = ps->output_settings->compact;
    if (stop == start+1)
    {
        // A single quote becomes &apos;
        // A single newline becomes &#10;
        // A single cr becomes &#13;
        // A single tab becomes &#9;
        if (*start == '\'') return false;
        if (*start == '\n') return false;
        if (*start == '\r') return false;
        if (*start == '\t') return false;
    }

    size_t only_leading_newlines = 0;
    const char *ls = has_leading_space_nl(start, stop, &only_leading_newlines);
    if (ls != NULL && only_leading_newlines == 0) return true;
    size_t only_ending_newlines = 0;
    const char *es = has_ending_nl_space(start, stop, &only_ending_newlines);
    if (es != NULL && only_ending_newlines == 0) return true;

    if (compact)
    {
        // In compact form newlines must be escaped: &#10;
        if (has_newlines(start, stop)) return true;
        // In compact form leading or ending single quotes triggers &#39; escapes
        // since we cannot use the multiline quote trick:
        // '''
        // 'alfa'
        // '''
        if (has_leading_ending_quote(start, stop)) return true;
    }

    bool newlines = ps->output_settings->escape_newlines;
    bool non7bit = ps->output_settings->escape_non_7bit;

    for (const char *i = start; i < stop; ++i)
    {
        int c = (int)(unsigned char)(*i);
        if (newlines && c == '\n') return true;
        if (non7bit && c > 126) return true;
        if (c < 32 && c != '\n') return true;
    }
    return false;
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

#endif // XMQ_PRINTER_MODULE

