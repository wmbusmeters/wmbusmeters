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
#ifndef XMQ_H
#define XMQ_H

// #define XMQ_NO_XMQ_PRINTING
// #define XMQ_NO_LIBXML
// #define XMQ_NO_JSON

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
    @XMQ_CONTENT_TEXT: valid utf8 text input/output is selected

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
    XMQ_CONTENT_JSON = 6,
    XMQ_CONTENT_TEXT = 7
} XMQContentType;

/**
    XMQRenderFormat:
    @XMQ_RENDER_PLAIN: normal output for data storage
    @XMQ_RENDER_TERMINAL: colorize using ansi codes
    @XMQ_RENDER_HTML: colorize using html tags
    @XMQ_RENDER_HTMQ: colorize using htmq tags
    @XMQ_RENDER_TEX: colorize using tex
    @XMQ_RENDER_RAW: write the text content using UTF8 with no escapes

    The xmq output can be rendered as PLAIN, or for human consumption in TERMINAL, HTML, HTMQ, TEX or RAW.
*/
typedef enum
{
   XMQ_RENDER_PLAIN,
   XMQ_RENDER_TERMINAL,
   XMQ_RENDER_HTML,
   XMQ_RENDER_HTMQ,
   XMQ_RENDER_TEX,
   XMQ_RENDER_RAW
} XMQRenderFormat;

/**
    XMQFlagBits:
    @XMQ_FLAG_TRIM_NONE: Do not trim any whitespace.
    @XMQ_FLAG_TRIM_HEURISTIC: Remove leading/ending whitespace, but try to keep significant, remove incidental indentation.
    @XMQ_FLAG_TRIM_EXACT: Trim exactly according to XML rules. Depends on your XSD,space:preserve and more and is COMPLICATED!
    @XMQ_FLAG_NOMERGE: Do not merge text and character entities.

    If a 0 is provided as the flags to the parse functions, then it will parse using the these default settings:
    When loading xml/html:
        trim the whitespace from the input to generate the most likely desired xmq output.
        merge character entities
    When loading xmq/htmq:
        no trimming but
        merge character entities such as &#10; and consecutive text quotes

    If you load xml with XMQ_TRIM_NONE (--trim=none) there will be a lot of unnecessary whitespace stored in
    the xmq, like &#32;&#9;&#10; etc.
    You can then view the xmq with XMQ_TRIM_HEURISTIC (--trim=heuristic) to drop the whitespace.

    If you load xmq with --nomerge then character entities and separate text blocks will be kept as is.
    The --nomerge currently does not work for XML/HTML since libxml2 does not have a setting for merge.
*/
typedef enum
{
    XMQ_FLAG_TRIM_NONE = 1,
    XMQ_FLAG_TRIM_HEURISTIC = 2,
    XMQ_FLAG_TRIM_EXACT = 4,
    XMQ_FLAG_NOMERGE = 8,
} XMQFlagBits;

/**
    XMQSyntax:
    @SYNTAX_C: Comments
    @SYNTAX_Q: Standalone quote.
    @SYNTAX_E: Entity
    @SYNTAX_ENS: Element Namespace
    @SYNTAX_EN: Element Name
    @SYNTAX_EK: Element Key
    @SYNTAX_EKV: Element Key Value
    @SYNTAX_ANS: Attribute NameSpace
    @SYNTAX_AK: Attribute Key
    @SYNTAX_AKV: Attribute Key Value
    @SYNTAX_CP: Compound Parentheses
    @SYNTAX_NDC: Namespace declaration
    @SYNTAX_UW: Unicode Whitespace
*/
typedef enum
{
    SYNTAX_C = 0, // Comments
    SYNTAX_Q = 1, // Standalone quote.
    SYNTAX_E = 2, // Entity
    SYNTAX_ENS = 3, // Element Namespace
    SYNTAX_EN = 4, // Element Name
    SYNTAX_EK = 5, // Element Key
    SYNTAX_EKV = 6, // Element Key Value
    SYNTAX_ANS = 7, // Attribute NameSpace
    SYNTAX_AK = 8, // Attribute Key
    SYNTAX_AKV = 9, // Attribute Key Value
    SYNTAX_CP = 10, // Compound Parentheses
    SYNTAX_NDC = 11, // Namespace declaration
    SYNTAX_UW = 12, // Unicode Whitespace
} XMQSyntax;

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
*/
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
    XMQNodeCallback: The function type which is called by foreach functions.
    @doc: The document being processed.
    @node: The node triggering the callback.
    @user_data: The user data supplied to for_each.
*/
typedef XMQProceed (*XMQNodeCallback)(XMQDoc *doc, XMQNode *node, void *user_data);

/**
    XMQParseError:
    @XMQ_ERROR_CANNOT_READ_FILE: file not found or cannot be opened for reading.
    @XMQ_ERROR_OOM: out of memory.
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
    @XMQ_ERROR_UNEXPECTED_TAB: tabs are not permitted as token separators.
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
    @XMQ_ERROR_PARSING_XML:
    @XMQ_ERROR_PARSING_HTML:

    Possible parse errors.
*/
typedef enum
{
    XMQ_ERROR_NONE = 0,
    XMQ_ERROR_CANNOT_READ_FILE = 1,
    XMQ_ERROR_OOM = 2,
    XMQ_ERROR_NOT_XMQ = 3,
    XMQ_ERROR_QUOTE_NOT_CLOSED = 4,
    XMQ_ERROR_ENTITY_NOT_CLOSED = 5,
    XMQ_ERROR_COMMENT_NOT_CLOSED = 6,
    XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES = 7,
    XMQ_ERROR_BODY_NOT_CLOSED = 8,
    XMQ_ERROR_ATTRIBUTES_NOT_CLOSED = 9,
    XMQ_ERROR_COMPOUND_NOT_CLOSED = 10,
    XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN = 11,
    XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES = 12,
    XMQ_ERROR_UNEXPECTED_CLOSING_BRACE = 13,
    XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS = 14,
    XMQ_ERROR_UNEXPECTED_TAB = 15,
    XMQ_ERROR_INVALID_CHAR = 16,
    XMQ_ERROR_BAD_DOCTYPE = 17,
    XMQ_ERROR_JSON_INVALID_ESCAPE = 18,
    XMQ_ERROR_JSON_INVALID_CHAR = 19,
    XMQ_ERROR_CANNOT_HANDLE_XML = 20,
    XMQ_ERROR_CANNOT_HANDLE_HTML = 21,
    XMQ_ERROR_CANNOT_HANDLE_JSON = 22,
    XMQ_ERROR_EXPECTED_XMQ = 23,
    XMQ_ERROR_EXPECTED_HTMQ = 24,
    XMQ_ERROR_EXPECTED_XML = 25,
    XMQ_ERROR_EXPECTED_HTML = 26,
    XMQ_ERROR_EXPECTED_JSON = 27,
    XMQ_ERROR_PARSING_XML = 28,
    XMQ_ERROR_PARSING_HTML = 29,
    XMQ_ERROR_VALUE_CANNOT_START_WITH = 30,
    XMQ_WARNING_QUOTES_NEEDED = 100
} XMQParseError;

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
   xmqParseErrorToString:
   @e: Translate this error to a human readable string.
*/
const char *xmqParseErrorToString(XMQParseError e);

/**
    xmqNewParseCallbacks:

    Allocate an empty XMQParseCallback structure. All callbacks are NULL and none will be called.
*/
XMQParseCallbacks *xmqNewParseCallbacks();

/**
    xmqFreeParseCallbacks:

    Free the XMQParseCallback structure.
*/
void xmqFreeParseCallbacks(XMQParseCallbacks *cb);

/**
    xmqSetupParseCallbacksColorizeTokens:

    Used to colorize xmq input, without building a parse tree.
*/
void xmqSetupParseCallbacksColorizeTokens(XMQParseCallbacks *state, XMQRenderFormat render_format);

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
    @doq: Parser which source file name should be set.
    @source_name: The document source location.

    Set the source name to make error message more readable when parsing fails.
    The source name is often the file name, but can be "-" for stdin or anything you like.
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
    xmqSetDocSourceName:
    @doq: Document which source file name should be set.
    @source_name: The document source location.

    Set the source name to make error message more readable when parsing fails.
    The source name is often the file name, but can be "-" for stdin or anything you like.
*/
void xmqSetDocSourceName(XMQDoc *doq, const char *source_name);

/**
    xmqGetOriginalContentType:

    If available, return the original content type (xmq/htmq/xml/html/json/text) of this document.
*/
XMQContentType xmqGetOriginalContentType(XMQDoc *doq);

/**
    xmqGetOriginalSize:

    If available, return the size of the original content, ie the loaded file size.
*/
size_t xmqGetOriginalSize(XMQDoc *doq);

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
    xmqSetImplementationDoc:

    Set the underlying implementation doc, could be an xmlDocPtr from libxml2 for example.
*/
void xmqSetImplementationDoc(XMQDoc *doq, void *doc);

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
bool xmqParseFile(XMQDoc *doc, const char *file, const char *implicit_root, int flags);

/**
    xmqParseBuffer:
    @doc: the xmq doc object
    @start: start of buffer to parse
    @stop: points to byte after last byte in buffer
    @implicit_root: the implicit root

    Parse a buffer or a file and create a document.
    The xmq format permits multiple root nodes if an implicit root is supplied.
*/
bool xmqParseBuffer(XMQDoc *doc, const char *start, const char *stop, const char *implicit_root, int flags);

/**
    xmqParseReader:
    @doc: the xmq doc object
    @reader: use this reader to fetch input data
    @implicit_root: the implicit root

    Parse data fetched with a reader and create a document.
    The xmq format permits multiple root nodes if an implicit root is supplied.
*/
bool xmqParseReader(XMQDoc *doc, XMQReader *reader, const char *implicit_root, int flags);

/** Allocate the print settings structure and zero it. */
XMQOutputSettings *xmqNewOutputSettings();

/** Free the print settings structure. */
void xmqFreeOutputSettings(XMQOutputSettings *os);

void xmqSetAddIndent(XMQOutputSettings *os, int add_indent);
void xmqSetCompact(XMQOutputSettings *os, bool compact);
void xmqSetUseColor(XMQOutputSettings *os, bool use_color);
void xmqSetBackgroundMode(XMQOutputSettings *os, bool bg_dark_mode);
void xmqSetEscapeNewlines(XMQOutputSettings *os, bool escape_newlines);
void xmqSetEscapeNon7bit(XMQOutputSettings *os, bool escape_non_7bit);
void xmqSetOutputFormat(XMQOutputSettings *os, XMQContentType output_format);
void xmqSetRenderFormat(XMQOutputSettings *os, XMQRenderFormat render_to);
void xmqSetRenderTheme(XMQOutputSettings *os, const char *theme_name);
void xmqSetRenderRaw(XMQOutputSettings *os, bool render_raw);
void xmqSetRenderOnlyStyle(XMQOutputSettings *os, bool only_style);
void xmqSetWriterContent(XMQOutputSettings *os, XMQWriter content);
void xmqSetWriterError(XMQOutputSettings *os, XMQWriter error);

/** Setup the printer to print content to stdout and errors to sderr. */
void xmqSetupPrintStdOutStdErr(XMQOutputSettings *ps);

/** Setup the printer to print to a file. */
void xmqSetupPrintFile(XMQOutputSettings *ps, const char *file);

/** Setup the printer to print to a filedescriptor. */
void xmqSetupPrintFileDescriptor(XMQOutputSettings *ps, int fd);

/** Setup the printer to print to a dynamically memory buffer. */
void xmqSetupPrintMemory(XMQOutputSettings *ps, char **start, char **stop);

/** Pretty print the document according to the settings. */
void xmqPrint(XMQDoc *doc, XMQOutputSettings *settings);

/** Trim xml whitespace. */
void xmqTrimWhitespace(XMQDoc *doc, int flags);

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
int32_t xmqGetInt(XMQDoc *doc, const char *xpath);

/**
    xmqGetIntRel:
    @doc: the xmq doc object
    @xpath: the location of the content to be parsed as an 32 bit signed integer.
    @relative: the xpath is search using this node as the starting point.
*/
int32_t xmqGetIntRel(XMQDoc *doc, const char *xpath, XMQNode *relative);

/**
    xmqGetLong:
    @doc: the xmq doc object
    @xpath: the location of the content to be parsed as an 64 bit signed integer.
*/
int64_t xmqGetLong(XMQDoc *doc, const char *xpath);

/**
    xmqGetLongRel:
    @doc: the xmq doc object
    @xpath: the location of the content to be parsed as an 64 bit signed integer.
    @relative: the xpath is search using this node as the starting point.
*/
int64_t xmqGetLongRel(XMQDoc *doc, const char *xpath, XMQNode *relative);

/**
    xmqGetDouble:
    @doc: the xmq doc object
    @xpath: the location of the content to be parsed as double float.
*/
double xmqGetDouble(XMQDoc *doc, const char *xpath);

/**
    xmqGetDoubleRel:
    @doc: the xmq doc object
    @xpath: the location of the content to be parsed as double float.
    @relative: the xpath is search using this node as the starting point.
*/
double xmqGetDoubleRel(XMQDoc *doc, const char *xpath, XMQNode *relative);

/**
    xmqGetString:
    @doc: the xmq doc object
    @xpath: the location of the content to be parsed as string.
*/
const char *xmqGetString(XMQDoc *doc, const char *xpath);

/**
    xmqGetStringRel:
    @doc: the xmq doc object
    @xpath: the location of the content to be parsed as string.
    @relative: the xpath is search using this node as the starting point.
*/
const char *xmqGetStringRel(XMQDoc *doc, const char *xpath, XMQNode *relative);

/**
   xmqForeach: Find all locations matching the xpath.
   @xpath: the xpath pattern.
   @cb: the function to call for each found node. Can be NULL.
   @user_data: the user_data supplied to the function.

   Returns the number of hits.
*/
int xmqForeach(XMQDoc *doq, const char *xpath, XMQNodeCallback cb, void *user_data);

/**
   xmqForeachRel: Find all locations matching the xpath.
   @xpath: the xpath pattern.
   @cb: the function to call for each found node. Can be NULL.
   @user_data: the user_data supplied to the function.
   @relative: find nodes relative to this node.

   Returns the number of hits.
*/
int xmqForeachRel(XMQDoc *doq, const char *xpath, XMQNodeCallback cb, void *user_data, XMQNode *relative);

/**
   xmqReplaceEntity: Replace the selected entity with the supplied content.
   @entity: the entity
   @content: the string content to be inserted

   Returns the number of replacements.
*/
int xmqReplaceEntity(XMQDoc *doq, const char *entity, const char *content);

/**
   xmqReplaceEntity: Replace the selected entity with the supplied content node.
   @entity: the entity
   @content: the string content to be inserted

   Returns the number of replacements.
*/
int xmqReplaceEntityWithNode(XMQDoc *doq, const char *entity, XMQDoc *idoq, XMQNode *inode);

/**
    xmqVersion:

    Return the current xmq version in this library.
*/
const char *xmqVersion();

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
                            int flags);

/**
    xmqParseFileWithType:

    Load and parse file. If file is NULL read from stdin.
*/
bool xmqParseFileWithType(XMQDoc *doc,
                          const char *file,
                          const char *implicit_root,
                          XMQContentType ct,
                          int flags);

/**
   xmqSetupDefaultColors:

   Set the default colors for settings based on the theme and background color.
*/
void xmqSetupDefaultColors(XMQOutputSettings *settings);

/**
   xmqOverrideSetting: Change the default strings for spaces etc.
   @settings: The output settings to modify.
   @indentation_space: If NULL use " ".
   @explicit_space: If NULL use " ".
   @explicit_tab: If NULL use "\t"
   @explicit_cr: If NULL use "\r".
   @explicit_nl: If NULL use "\n".
   @prefix_line: If NULL do not print any prefix.
   @postfix_line: If NULL do not print any postfix.
*/
void xmqOverrideSettings(XMQOutputSettings *settings,
                         const char *indentation_space,
                         const char *explicit_space,
                         const char *explicit_tab,
                         const char *explicit_cr,
                         const char *explicit_nl);

/**
   xmqRenderHtmlSettings: Change the id or clas for the rendered html.
   @settings: The output settings to modify.
   @use_id: Mark the pre tag with this id.
   @use_class: Mark the pre tag with this class.
*/
void xmqRenderHtmlSettings(XMQOutputSettings *settings,
                           const char *use_id,
                           const char *use_class);

/**
   xmqOverrideColor:
   @settings:
   @render_style: Use "" for the default render_style
   @sc: The syntax element you want to change the color for.

   Change the color strings for the given color type. You have to run xmqSetupDefaultColors first.
*/
void xmqOverrideColor(XMQOutputSettings *settings,
                      const char *render_style,
                      XMQSyntax sc,
                      const char *pre,
                      const char *post,
                      const char *ns);


#ifdef __cplusplus
_hideRBfromEditor
#endif

#endif
