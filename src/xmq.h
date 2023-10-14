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
#ifndef XMQ_H
#define XMQ_H

#define _hideLBfromEditor {
#define _hideRBfromEditor }

#ifdef __cplusplus
extern "C" _hideLBfromEditor
#endif

#include<stdbool.h>
#include<stdlib.h>
#include<stdint.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////// TYPES and STRUCTURES ///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

/** Opaque structure storing a loaded xmq/xml/json document.

    XMQDoc:

    Structure storing a loaded xmq/xml/json document.
*/
typedef struct XMQDoc XMQDoc;

/** Opaque structure referencing a node/attr in the xmq/xml/json document.

    XMQNode:

    Structure referencing a node/attr in the xmq/xml/json document.
*/
typedef struct XMQNode XMQNode;

/**
    XMQParseState:

    An opaque structure to maintain the parse state and the list of callbacks
    to be invoked when parsing xmq.
*/
typedef struct XMQParseState XMQParseState;

/**
    XMQParseCallbacks:

    Store your own parse callbacks into this structure and register
    your own callback structure with the XMQParseState. Then you will get your own
    callbacks when parsing xmq and using these callbacks you can implement
    your own document builder or token handler.
*/
typedef struct XMQParseCallbacks XMQParseCallbacks;

/** Specify the file/buffer content type.

    XMQContentType:
    @XMQ_CONTENT_UNKNOWN: a failed content detect will mark the content type as unknown
    @XMQ_CONTENT_DETECT: auto detect the content type
    @XMQ_CONTENT_XMQ: xmq detected
    @XMQ_CONTENT_HTMQ: htmq detected
    @XMQ_CONTENT_XML: xml detected
    @XMQ_CONTENT_HTML: html detected
    @XMQ_CONTENT_JSON: json detected

    Specify the file/buffer content type.
*/
typedef enum
{
    XMQ_CONTENT_UNKNOWN = 0,
    XMQ_CONTENT_DETECT = 1,
    XMQ_CONTENT_XMQ = 2,
    XMQ_CONTENT_HTMQ = 3,
    XMQ_CONTENT_XML = 4,
    XMQ_CONTENT_HTML = 5,
    XMQ_CONTENT_JSON = 6
} XMQContentType;

/**
    XMQRenderFormat:
    @XMQ_RENDER_PLAIN: normal output for data storage
    @XMQ_RENDER_TERMINAL: colorize using ansi codes
    @XMQ_RENDER_HTML: colorize using html tags
    @XMQ_RENDER_HTMQ: colorize using htmq tags
    @XMQ_RENDER_TEX: colorize using tex

    The xmq output can be rendered as PLAIN, or for human consumption in TERMINAL, HTML, HTMQ or TEX.
*/
typedef enum
{
   XMQ_RENDER_PLAIN,
   XMQ_RENDER_TERMINAL,
   XMQ_RENDER_HTML,
   XMQ_RENDER_HTMQ,
   XMQ_RENDER_TEX
} XMQRenderFormat;

/**
    XMQTrimType:
    @XMQ_TRIM_DEFAULT: Use the default, ie no-trim for xmq/json, normal-trim for xml/html.
    @XMQ_TRIM_NONE: Do not trim at all. Keep unnecessary xml/html indentation and newlines.
    @XMQ_TRIM_NORMAL: Normal trim heuristic. Remove leading/ending whitespace, remove incindental indentation.
    @XMQ_TRIM_EXTRA: Like normal but remove all indentation (not just incindental) and collapse whitespace.
    @XMQ_TRIM_RESHUFFLE: Like extra but also reflow all content at word boundaries to limit line lengths.

    When loading xml/html trim the whitespace from the input to generate the most likely desired xmq output.
    When loading xmq/htmq, the whitespace is never trimmed since xmq explicitly encodes all important whitespace.
    If you load xml with XMQ_TRIM_NONE (--trim=none) there will be a lot of unnecessary whitespace stored in
    the xmq, like &#32;&#9;&#10; etc.
    You can then view the xmq with XMQ_TRIM_NORMAL (--trim=normal) to drop the whitespace.
*/
typedef enum
{
    XMQ_TRIM_DEFAULT = 0,
    XMQ_TRIM_NONE = 1,
    XMQ_TRIM_NORMAL = 2,
    XMQ_TRIM_EXTRA = 3,
    XMQ_TRIM_RESHUFFLE = 4,
} XMQTrimType;

/**
    XMQColorStrings:
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
struct XMQColorStrings
{
    const char *pre;
    const char *post;
};
typedef struct XMQColorStrings XMQColorStrings;

/**
    XMQColoring:

    The coloring struct is used to prefix/postfix ANSI/HTML/TEX strings for
    XMQ tokens to colorize the printed xmq output.
*/
struct XMQColoring
{
    XMQColorStrings document; // <html></html>  \documentclass{...}... etc
    XMQColorStrings header; // <head>..</head>
    XMQColorStrings style;  // Stylesheet content inside header (html) or color(tex) definitions.
    XMQColorStrings body; // <body></body> \begin{document}\end{document}
    XMQColorStrings content; // Wrapper around rendered code. Like <pre></pre>, \textt{...}

    XMQColorStrings whitespace; // The normal whitespaces: tab=9, nl=10, cr=13, space=32. Normally not colored.
    XMQColorStrings unicode_whitespace; // The remaining unicode whitespaces, like: nbsp=160 color as red underline.
    XMQColorStrings indentation_whitespace; // The xmq generated indentation spaces. Normally not colored.
    XMQColorStrings equals; // The key = value equal sign.
    XMQColorStrings brace_left; // Left brace starting a list of childs.
    XMQColorStrings brace_right; // Right brace ending a list of childs.
    XMQColorStrings apar_left; // Left parentheses surrounding attributes. foo(x=1)
    XMQColorStrings apar_right; // Right parentheses surrounding attributes.
    XMQColorStrings cpar_left; // Left parentheses surrounding a compound value. foo = (&#10;' x '&#10;)
    XMQColorStrings cpar_right; // Right parentheses surrounding a compound value.
    XMQColorStrings quote; // A quote 'x y z' can be single or multiline.
    XMQColorStrings entity; // A entity &#10;
    XMQColorStrings comment; // A comment // foo or /* foo */
    XMQColorStrings comment_continuation; // A comment containing newlines /* Hello */* there */
    XMQColorStrings ns_colon; // The color of the colon separating a namespace from a name.
    XMQColorStrings element_ns; // The namespace part of an element tag, i.e. the text before colon in foo:alfa.
    XMQColorStrings element_name; // When an element tag has multiple children or attributes it is rendered using this color.
    XMQColorStrings element_key; // When an element tag is suitable to be presented as a key value, this color is used.
    XMQColorStrings element_value_text; // When an element is presented as a key and the value is presented as text, use this color.
    XMQColorStrings element_value_quote; // When the value is a single quote, use this color.
    XMQColorStrings element_value_entity; // When the value is a single entity, use this color.
    XMQColorStrings element_value_compound_quote; // When the value is compounded and this is a quote in the compound.
    XMQColorStrings element_value_compound_entity; // When the value is compounded and this is an entity in the compound.
    XMQColorStrings attr_ns; // The namespace part of an attribute name, i.e. the text before colon in bar:speed.
    XMQColorStrings attr_key; // The color of the attribute name, i.e. the key.
    XMQColorStrings attr_value_text; // When the attribute value is text, use this color.
    XMQColorStrings attr_value_quote; // When the attribute value is a quote, use this color.
    XMQColorStrings attr_value_entity; // When the attribute value is an entity, use this color.
    XMQColorStrings attr_value_compound_quote; // When the attribute value is a compound and this is a quote in the compound.
    XMQColorStrings attr_value_compound_entity; // When the attribute value is a compound and this is an entity in the compound.

    const char *indentation_space; // If NULL use " " can be replaced with any other string.
    const char *explicit_space; // If NULL use " " can be replaced with any other string.
    const char *explicit_tab; // If NULL use "\t" can be replaced with any other string.
    const char *explicit_cr; // If NULL use "\t" can be replaced with any other string.
    const char *explicit_nl; // If NULL use "\n" can be replaced with any other string.
    const char *prefix_line; // If non-NULL print this as the leader before each line.
    const char *postfix_line; // If non-NULL print this as the ending after each line.
};
typedef struct XMQColoring XMQColoring;

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
} XMQColor;

/**
    XMQReader:
    @reader_state: points to the reader state
    @read: invoked with the reader state and where to store input data.

    The xmq parser uses the reader to fetch data into a buffer (start <= i < stop).
    You can create your own reader with a function that takes a pointer to the reader state.
    Returns the number of bytes stored in buffer, maximum stored is stop-start.
*/
struct XMQReader
{
    void *reader_state;
    size_t (*read)(void *reader_state, char *start, char *stop);
};
typedef struct XMQReader XMQReader;

/**
    XMQWrite:
    @writer_state: necessary state for writing.
    @start: start of buffer to write
    @stop: points to byte after buffer to write. If NULL then assume start is null terminated.

    Any function implementing XMQWrite must handle stop == NULL.
*/
typedef bool (*XMQWrite)(void *writer_state, const char *start, const char *stop);

/**
    XMQWriter:
    @writer_state: points to the writer state
    @write: invoked with the writer state to store output data. Must accept stop == NULL which assumes start is null terminated.

    The xmq printer uses the writer to write data supplied from a buffer (start <= i < stop).
    You can create your own writer with a function that takes a pointer to the writer state.
    The writer function must return true if the writing was successful.
*/
struct XMQWriter
{
    void *writer_state;
    XMQWrite write;
};
typedef struct XMQWriter XMQWriter;

/**
    XMQOutputSettings:
    @add_indent: Default is 4. Indentation starts at 0 which means no spaces prepended.
    @compact: Print on a single line limiting whitespace to a minimum.
    @escape_newlines: Replace newlines with &#10; this is implied if compact is set.
    @escape_non_7bit: Replace all chars above 126 with char entities, ie &#10;
    @output_format: Print xmq/xml/html/json
    @coloring: Print prefixes/postfixes to colorize the output for ANSI/HTML/TEX.
    @render_to: Render to terminal, html, tex.
    @render_raw: If true do not write surrounding html and css colors, likewise for tex.
    @only_style: Print only style sheet header.
    @write_content: Write content to buffer.
    @buffer_content: Supplied as buffer above.
    @write_error: Write error to buffer.
    @buffer_error: Supplied as buffer above.
*/
struct XMQOutputSettings
{
    int  add_indent;
    bool compact;
    bool use_color;
    bool escape_newlines;
    bool escape_non_7bit;

    XMQContentType output_format;
    XMQColoring coloring;
    XMQRenderFormat render_to;
    bool render_raw;
    bool only_style;

    XMQWriter content;
    XMQWriter error;
};
typedef struct XMQOutputSettings XMQOutputSettings;


/**
    XMQProceed:
    @XMQ_CONTINUE: Return "continue" to continue iterating over xmq nodes.
    @XMQ_RETURN: Return "return" to stop and return the current node.
    @XMQ_ABORT: Return "abort" to stop iterating and give an error.
*/
typedef enum
{
    XMQ_CONTINUE,
    XMQ_STOP,
} XMQProceed;

/**
    NodeCallback: The function type which is called by foreach functions.
    @doc: The document being processed.
    @node: The node triggering the callback.
    @user_data: The user data supplied to for_each.
*/
typedef XMQProceed (*NodeCallback)(XMQDoc *doc, XMQNode *node, void *user_data);


///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////// FUNCTIONS  /////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
    xmqDetectContentType:
    @start: points to first byte of buffer to scan for content type
    @stop: points to byte after buffer

    Detect the content type xmq/xml/html/json by examining a few leading
    non-whitespace words/characters.
 */
XMQContentType xmqDetectContentType(const char *start, const char *stop);

/**
    XMQParseError:
    @XMQ_ERROR_CANNOT_READ_FILE: file not found or cannot be opened for reading.
    @XMQ_ERROR_NOT_XMQ: expected xmq but auto detect sees early that it is not xmq.
    @XMQ_ERROR_QUOTE_NOT_CLOSED: an xmq quote is not closed, ie single quotes are missing.
    @XMQ_ERROR_ENTITY_NOT_CLOSED: an entity is missing the semicolon.
    @XMQ_ERROR_COMMENT_NOT_CLOSED: a comment has not been closed.
    @XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES: a comment close is not balanced.
    @XMQ_ERROR_BODY_NOT_CLOSED: an body is missing a closing brace.
    @XMQ_ERROR_ATTRIBUTES_NOT_CLOSED: the attribute list is missing the closing parentheses.
    @XMQ_ERROR_CONTENT_NOT_CLOSED: compound content is missing the closing double parentheses.
    @XMQ_ERROR_CONTENT_MAY_NOT_CONTAIN: compound content may only contains quotes and entities.
    @XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES: too many closing single quotes.
    @XMQ_ERROR_UNEXPECTED_CLOSING_BRACE: an unexpected closing brace.
    @XMQ_ERROR_INVALID_CHAR: an invalid character found.
    @XMQ_ERROR_BAD_DOCTYPE: the doctype could not be parsed.
    @XMQ_ERROR_JSON_INVALID_ESCAPE: an invalid json escape sequence.
    @XMQ_ERROR_JSON_INVALID_CHAR: an invalid character.
    @XMQ_ERROR_CANNOT_HANDLE_XML: x
    @XMQ_ERROR_CANNOT_HANDLE_HTML: x
    @XMQ_ERROR_CANNOT_HANDLE_JSON: x
    @XMQ_ERROR_EXPECTED_XMQ: x
    @XMQ_ERROR_EXPECTED_HTMQ: x
    @XMQ_ERROR_EXPECTED_XML: x
    @XMQ_ERROR_EXPECTED_HTML: x
    @XMQ_ERROR_EXPECTED_JSON: x

    Possible parse errors.
*/
typedef enum
{
    XMQ_ERROR_CANNOT_READ_FILE = 1,
    XMQ_ERROR_NOT_XMQ = 2,
    XMQ_ERROR_QUOTE_NOT_CLOSED = 3,
    XMQ_ERROR_ENTITY_NOT_CLOSED = 4,
    XMQ_ERROR_COMMENT_NOT_CLOSED = 5,
    XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES = 6,
    XMQ_ERROR_BODY_NOT_CLOSED = 7,
    XMQ_ERROR_ATTRIBUTES_NOT_CLOSED = 8,
    XMQ_ERROR_COMPOUND_NOT_CLOSED = 9,
    XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN = 10,
    XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES = 11,
    XMQ_ERROR_UNEXPECTED_CLOSING_BRACE = 12,
    XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS = 13,
    XMQ_ERROR_INVALID_CHAR = 14,
    XMQ_ERROR_BAD_DOCTYPE = 15,
    XMQ_ERROR_JSON_INVALID_ESCAPE = 16,
    XMQ_ERROR_JSON_INVALID_CHAR = 17,
    XMQ_ERROR_CANNOT_HANDLE_XML = 18,
    XMQ_ERROR_CANNOT_HANDLE_HTML = 19,
    XMQ_ERROR_CANNOT_HANDLE_JSON = 20,
    XMQ_ERROR_EXPECTED_XMQ = 21,
    XMQ_ERROR_EXPECTED_HTMQ = 22,
    XMQ_ERROR_EXPECTED_XML = 23,
    XMQ_ERROR_EXPECTED_HTML = 24,
    XMQ_ERROR_EXPECTED_JSON = 25
} XMQParseError;


const char *xmqParseErrorToString(XMQParseError e);

/** Allocate an empty XMQParseCallback structure. All callbacks are NULL and none will be called. */
XMQParseCallbacks *xmqNewParseCallbacks();

/** Free the XMQParseCallback structure. */
void xmqFreeParseCallbacks(XMQParseCallbacks *cb);

/**
    xmqSetupParseCallbacksNoopTokens:

    When tokenizing only, for coloring or debugging, you can
    use the setup functions below for a few standardized handlers.
*/
void xmqSetupParseCallbacksNoopTokens(XMQParseCallbacks *state);

/**
    xmqSetupParseCallbacksColorizeTokens:

    Used to colorize xmq input, without building a parse tree.
*/
void xmqSetupParseCallbacksColorizeTokens(XMQParseCallbacks *state, XMQRenderFormat render_format, bool dark_mode);

/**
    xmqSetupParseCallbacksDebugTokens:

    Used to debug location, type of tokens.
*/
void xmqSetupParseCallbacksDebugTokens(XMQParseCallbacks *state);

/**
    xmqSetupParseCallbacksDebugContent:

    Used debug the decoded content.
*/
void xmqSetupParseCallbacksDebugContent(XMQParseCallbacks *state);

/** Parse a buffer with xmq content. */
bool xmqTokenizeBuffer(XMQParseState *state, const char *start, const char *stop);

/** Parse a file with xmq content. */
bool xmqTokenizeFile(XMQParseState *state, const char *file);

/** Parse a file descriptor with xmq content. */
bool xmqTokenizeFileDescriptor(XMQParseState *state, int fd);

/**
    xmqNewParseState:
    @callbacks: these callbacks will be invoked for each token.
    @settings: these settings are available to the callbacks.

    Now prepare a parse state that is used to actually parse an xmq file.
    The print settings can be referenced from the callbacks for example when tokenizing.
*/
XMQParseState *xmqNewParseState(XMQParseCallbacks *callbacks, XMQOutputSettings *settings);

/**
    xmqFreeParseState:

    Free the memory allocated for the state.
*/
void xmqFreeParseState(XMQParseState *state);

/**
    xmqSetStateSourceName:
    @state: the parse state to inform.
    @source_name: the name of the file being parsed.

    To improve the error message the name of the file being parsed can be supplied.
*/
void xmqSetStateSourceName(XMQParseState *state, const char *source_name);

/**
    xmqStateErrno:
    @state: the parse state.

    If the parse fails then use this function to get the integer value of XMQParseError.
*/
int xmqStateErrno(XMQParseState *state);

/**
    xmqStateErrorMsg:
    @state: the parse state.

    If the parse fails then use this function to get a string explaining the error.
*/
const char *xmqStateErrorMsg(XMQParseState *state);

/**
    xmqNewDoc:

    Create an empty document object.
*/
XMQDoc *xmqNewDoc();

/**
    xmqSetSourceName:

    Set the source name to make error message more readable when parsing fails.
*/
void xmqSetDocSourceName(XMQDoc *doq, const char *file_name);

/**
    xmqGetRootNode:

    Get root node suitable for xmqForeach.
*/
XMQNode *xmqGetRootNode(XMQDoc *doq);

/**
    xmqGetImplementationDoc:

    Get the underlying implementation doc, could be an xmlDocPtr from libxml2 for example.
*/
void *xmqGetImplementationDoc(XMQDoc *doq);

/**
    xmqFreeDoc:

    Free the document object and all associated memory.
*/
void xmqFreeDoc(XMQDoc *doc);

/**
    xmqParseFile:
    @doc: the xmq doc object
    @file: file to load from file syste, or stdin if file is NULL
    @implicit_root: the implicit root

    Parse a file, or if file is NULL, read from stdin.
*/
bool xmqParseFile(XMQDoc *doc, const char *file, const char *implicit_root);

/**
    xmqParseBuffer:
    @doc: the xmq doc object
    @start: start of buffer to parse
    @stop: points to byte after last byte in buffer
    @implicit_root: the implicit root

    Parse a buffer or a file and create a document.
    The xmq format permits multiple root nodes if an implicit root is supplied.
*/
bool xmqParseBuffer(XMQDoc *doc, const char *start, const char *stop, const char *implicit_root);

/**
    xmqParseReader:
    @doc: the xmq doc object
    @reader: use this reader to fetch input data
    @implicit_root: the implicit root

    Parse data fetched with a reader and create a document.
    The xmq format permits multiple root nodes if an implicit root is supplied.
*/
bool xmqParseReader(XMQDoc *doc, XMQReader *reader, const char *implicit_root);

/** Allocate the print settings structure and zero it. */
XMQOutputSettings *xmqNewOutputSettings();

/** Free the print settings structure. */
void xmqFreeOutputSettings(XMQOutputSettings *ps);

/** Setup the printer to print content to stdout and errors to sderr. */
void xmqSetupPrintStdOutStdErr(XMQOutputSettings *ps);

/** Setup the printer to print to a file. */
void xmqSetupPrintFile(XMQOutputSettings *ps, const char *file);

/** Setup the printer to print to a filedescriptor. */
void xmqSetupPrintFileDescriptor(XMQOutputSettings *ps, int fd);

/** Setup the printer to print to a dynamically memory buffer. */
void xmqSetupPrintMemory(XMQOutputSettings *ps, const char **start, const char **stop);

/** Pretty print the document according to the settings. */
void xmqPrint(XMQDoc *doc, XMQOutputSettings *settings);

/** Trim xml whitespace. */
void xmqTrimWhitespace(XMQDoc *doc, XMQTrimType tt);

/** A parsing error will be described here! */
const char *xmqDocError(XMQDoc *doc);

/** The error as errno. */
XMQParseError xmqDocErrno(XMQDoc *doc);

/**
    xmqGetName: get name of node
    @node: Node
*/
const char *xmqGetName(XMQNode *node);

/**
    xmqGetContent: get content of element node
    @node: Node
*/
const char *xmqGetContent(XMQNode *node);

/**
    xmqGetInt:
    @doc: the xmq doc object
    @xpath: the location of the content to be parsed as an 32 bit signed integer.
*/
int32_t xmqGetInt(XMQDoc *doc, XMQNode *node, const char *xpath);

/**
    xmqGetLong:
    @doc: the xmq doc object
    @xpath: the location of the content to be parsed as an 64 bit signed integer.
*/
int64_t xmqGetLong(XMQDoc *doc, XMQNode *node, const char *xpath);

/**
    xmqGetDouble:
    @doc: the xmq doc object
    @xpath: the location of the content to be parsed as double float.
*/
double xmqGetDouble(XMQDoc *doc, XMQNode *node, const char *xpath);

/**
    xmqGetString:
    @doc: the xmq doc object
    @xpath: the location of the content to be parsed as string.
*/
const char *xmqGetString(XMQDoc *doc, XMQNode *node, const char *xpath);

/**
   xmqqForeach: Find all locations matching the xpath.
   @node: the starting node to search below.
   @xpath: the xpath pattern.
   @cb: the function to call for each found node. Can be NULL.
   @user_data: the user_data supplied to the function.

   Returns the number of hits.
*/
int xmqForeach(XMQDoc *doq, XMQNode *node, const char *xpath, NodeCallback cb, void *user_data);

/**

    xmqVersion:

    Return the current xmq version in this library.
*/
const char *xmqVersion();

/**
    xmqCommit:

    Return the git commit used to build this library.
*/
const char *xmqCommit();

/**
    xmqSetVerbose:

    Enable/Disable verbose logging.
*/
void xmqSetVerbose(bool e);

/**
    xmqSetDebug:

    Enable/Disable debugging.
*/
void xmqSetDebug(bool e);

/**
   xmqDebugging:

   Return whether debugging is enabled or not.
*/
bool xmqDebugging();

/**
    xmqParseBufferWithType:

    Parse buffer.
*/
bool xmqParseBufferWithType(XMQDoc *doc,
                            const char *start,
                            const char *stop,
                            const char *implicit_root,
                            XMQContentType ct,
                            XMQTrimType tt);

/**
    xmqParseFileWithType:

    Load and parse file. If file is NULL read from stdin.
*/
bool xmqParseFileWithType(XMQDoc *doc,
                          const char *file,
                          const char *implicit_root,
                          XMQContentType ct,
                          XMQTrimType tt);

/**
   xmqSetupDefaultColors:

   Set the default colors for settings based on the background color.
*/
void xmqSetupDefaultColors(XMQOutputSettings *settings, bool dark_mode);

#ifdef __cplusplus
_hideRBfromEditor
#endif

#endif
