/* libxmq - Copyright (C) 2023-2026 Fredrik Öhrström (spdx: MIT)

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

/**
 * @mainpage libxmq
 *
 * libxmq is a library for working with xml/json and xmq.
 *
 *
 * @section getting_started Getting started
 *
 * ...
 *
 * @section api API overview
 *
 * See @ref documents and @ref nodes.
 */

#ifndef XMQ_H
#define XMQ_H

// #define XMQ_NO_XMQ_PRINTING
// #define XMQ_NO_LIBXML
// #define XMQ_NO_JSON

#define _hideLBfromEditor {
#define _hideRBfromEditor }

#ifdef __cplusplus
// Needed for the XMQ_ATTRS macro.
#include <initializer_list>

extern "C" _hideLBfromEditor
#endif

#include<stdarg.h>
#include<stdbool.h>
#include<stdlib.h>
#include<stdint.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////// TYPES and STRUCTURES ///////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
   Opaque structure storing a document.
*/
typedef struct XMQDoc XMQDoc;

/**
   Opaque structure storing a node in the document.
   A node can be an element, text or entity.
   An element containing only text and entities is usually displayed as a key=value.
*/
typedef struct XMQNode XMQNode;

/**
   Opaque structure storing an attribute in the document.
*/
typedef struct XMQAttr XMQAttr;

/**
    An opaque structure to maintain the parse state and the list of callbacks
    to be invoked when parsing xmq.
*/
typedef struct XMQParseState XMQParseState;

/**
    Store your own parse callbacks into this structure and register it
    with the XMQParseState. Then you will get your own callbacks when parsing XMQ
    and using these callbacks you can implement your own document builder or token handler.
*/
typedef struct XMQParseCallbacks XMQParseCallbacks;

/**
   Opaque structure storing a output settings when printing xmq, xml or json.
*/
typedef struct XMQOutputSettings XMQOutputSettings;

/**
   @brief Specify the file/buffer content type both for input and for output.
*/
typedef enum
{
    /** Unknown buffer content. */
    XMQ_CONTENT_UNKNOWN = 0,
    /** Try to detect buffer content, is it xmq, xml or json? */
    XMQ_CONTENT_DETECT = 1,
    /** Content is xmq. */
    XMQ_CONTENT_XMQ = 2,
    /** Content is htmq. */
    XMQ_CONTENT_HTMQ = 3,
    /** Content is xml. */
    XMQ_CONTENT_XML = 4,
    /** Content is html. */
    XMQ_CONTENT_HTML = 5,
    /** Content is json. */
    XMQ_CONTENT_JSON = 6,
    /** Content is ixml. */
    XMQ_CONTENT_IXML = 7,
    /** Content is text. */
    XMQ_CONTENT_TEXT = 8,
    /** Cline content looks like: xpath="c-escaped string" */
    XMQ_CONTENT_CLINES = 9
} XMQContentType;

/**
   @brief XMQRenderFormat decides how to format the xmq output:
   PLAIN, or for human consumption in TERMINAL, HTML, HTMQ, TEX.
*/
typedef enum
{
    /** Normal output for data storage. */
    XMQ_RENDER_PLAIN = 0,
    /** Colorize output using ansi codes. */
    XMQ_RENDER_TERMINAL = 1,
    /** Colorize output using html tags. */
    XMQ_RENDER_HTML = 2,
    /** Colorize output using htmq tags. */
    XMQ_RENDER_HTMQ = 3,
    /** Colorize using latex. */
    XMQ_RENDER_TEX = 4
} XMQRenderFormat;

/**
    The flag bits specify by the parser builds the document.

    If a 0 is provided as the flag bits to the parse functions,
    then it will parse using the these default settings:

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
    The --nomerge currently does not work for XML/HTML since libxml2 does not have a setting for merge,
    it always merges.
*/
typedef enum
{
    /** Do not trim any whitespace. Only relevant when parsing xml. */
    XMQ_FLAG_TRIM_NONE = 1,
    /** Remove leading/ending whitespace, but try to keep significant, remove incidental indentation. */
    XMQ_FLAG_TRIM_HEURISTIC = 2,
    /** Not implemented. */
    XMQ_FLAG_TRIM_EXACT = 4,
    /** Do not merge adjacent text nodes and character entity nodes. */
    XMQ_FLAG_NOMERGE = 8,
    /** When ixml parse is ambiguous generate all parses. */
    XMQ_FLAG_IXML_ALL_PARSES = 16,
    /** When ixml parse fails, try to recover. */
    XMQ_FLAG_IXML_TRY_TO_RECOVER = 32,
    /** If the ixml parse fails generate an empty document and no errors. */
    XMQ_FLAG_IXML_FAIL_SILENT = 64,
} XMQFlagBits;

/**
   XMQColorName is used to color the output when pretty printing xmq.
*/
typedef enum XMQColorName {
    /** Comment. */
    XMQ_COLOR_C,
    /** Quote. */
    XMQ_COLOR_Q,
    /** Entity. */
    XMQ_COLOR_E,
    /** Name Space (both for element and attribute). */
    XMQ_COLOR_NS,
    /** Element name. */
    XMQ_COLOR_EN,
    /** Element key. */
    XMQ_COLOR_EK,
    /** Element key value. */
    XMQ_COLOR_EKV,
    /** Attribute key. */
    XMQ_COLOR_AK,
    /** Attribute key value. */
    XMQ_COLOR_AKV,
    /** Compound Parentheses. */
    XMQ_COLOR_CP,
    /** Name Space Declaration xmlns. */
    XMQ_COLOR_NSD,
    /** Unicode whitespace. */
    XMQ_COLOR_UW,
    /** Override xls prefixed element names with this color. */
    XMQ_COLOR_XLS,
} XMQColorName;

/**
    The xmq parser invokes the reader to fetch more data.
    The reader is resonsible for storing data in the buffer (start <= i < stop)
    and return the number of bytes stored.

    The reader_state is provided from the

    You can create your own reader with a function that takes a pointer to the reader state.
    Returns the number of bytes stored in buffer, maximum stored is stop-start.

    @param reader_state points to the reader state
    @param read invoked with the reader state and where to store input data.
*/
struct XMQReader
{
    /** The reader_state is passed to the read function. */
    void *reader_state;
    /** The function to be invoked from the parser to fetch more data to parse. */
    size_t (*read)(void *reader_state, char *start, char *stop);
};
typedef struct XMQReader XMQReader;

/**
    You can pass your own xmq_writer to the printer routines to do your own final output.
    Any function implementing XMQWrite must handle stop == NULL.

    @param writer_state Your own writer_state supplied to the printing function.
    @param start Start of buffer to write.
    @param stop Points to byte after buffer to write. If NULL then assume start is null terminated.
*/
typedef bool (*XMQWrite)(void *writer_state, const char *start, const char *stop);

/**
    The xmq printer uses the writer to write data supplied from a buffer (start <= i < stop).
    You can create your own writer with a function that takes a pointer to the writer state.
    The writer function must return true if the writing was successful.
*/
struct XMQWriter
{
    /** The writer_state is passed to the write function. */
    void *writer_state;
    /** The function to be invoked from the printer to write output data. */
    XMQWrite write;
};
typedef struct XMQWriter XMQWriter;

/**
    The XMQProceed is used to proceed or stop when iterating over xmq nodes.
*/
typedef enum
{
    /** Return XMQ_CONTINUE to continue iterating over xmq nodes. */
    XMQ_CONTINUE,
    /** Return XMQ_STOP to stop iterating. */
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
    The xmq functions return OK or error values using the XMQStatus.
*/
typedef enum
{
    /** No error. */
    XMQ_OK = 0,
    /** File not found or cannot be opened for reading. */
    XMQ_ERROR_CANNOT_READ_FILE = 1,
    /** Out of memory. */
    XMQ_ERROR_OOM = 2,
    /** Expected xmq but auto detect sees early that it is not xmq. */
    XMQ_ERROR_NOT_XMQ = 3,
    /** An xmq quote is not closed, ie single quotes are missing. */
    XMQ_ERROR_QUOTE_NOT_CLOSED = 4,
    /** An entity is missing the semicolon. */
    XMQ_ERROR_ENTITY_NOT_CLOSED = 5,
    /** A comment has not been closed. */
    XMQ_ERROR_COMMENT_NOT_CLOSED = 6,
    /** A comment close is not balanced. */
    XMQ_ERROR_COMMENT_CLOSED_WITH_TOO_MANY_SLASHES = 7,
    /** A body is missing a closing brace. */
    XMQ_ERROR_BODY_NOT_CLOSED = 8,
    /** The attribute list is missing the closing parentheses. */
    XMQ_ERROR_ATTRIBUTES_NOT_CLOSED = 9,
    /** Compound content is missing the closing double parentheses. */
    XMQ_ERROR_COMPOUND_NOT_CLOSED = 10,
    /** Compound content may only contains quotes and entities. */
    XMQ_ERROR_COMPOUND_MAY_NOT_CONTAIN = 11,
    /** Too many closing single quotes. */
    XMQ_ERROR_QUOTE_CLOSED_WITH_TOO_MANY_QUOTES = 12,
    /** An unexpected closing brace. */
    XMQ_ERROR_UNEXPECTED_CLOSING_BRACE = 13,
    /** Expected a value after equals. */
    XMQ_ERROR_EXPECTED_CONTENT_AFTER_EQUALS = 14,
    /** Tabs are not permitted as token separators. */
    XMQ_ERROR_UNEXPECTED_TAB = 15,
    /** An invalid character found. */
    XMQ_ERROR_INVALID_CHAR = 16,
    /** The doctype could not be parsed. */
    XMQ_ERROR_BAD_DOCTYPE = 17,
    /** An invalid json escape sequence. */
    XMQ_ERROR_JSON_INVALID_ESCAPE = 18,
    /** An invalid json character. */
    XMQ_ERROR_JSON_INVALID_CHAR = 19,
    /** The XMl parser has been left out to shrink code size. */
    XMQ_ERROR_CANNOT_HANDLE_XML = 20,
    /** The HTML parser has been left out to shrink code size. */
    XMQ_ERROR_CANNOT_HANDLE_HTML = 21,
    /** The JSON parser has been left out to shrink code size. */
    XMQ_ERROR_CANNOT_HANDLE_JSON = 22,
    /** Expecte xmq but was given something else. */
    XMQ_ERROR_EXPECTED_XMQ = 23,
    /** Expecte htmq but was given something else. */
    XMQ_ERROR_EXPECTED_HTMQ = 24,
    /** Expecte xml but was given something else. */
    XMQ_ERROR_EXPECTED_XML = 25,
    /** Expecte html but was given something else. */
    XMQ_ERROR_EXPECTED_HTML = 26,
    /** Expecte json but was given something else. */
    XMQ_ERROR_EXPECTED_JSON = 27,
    /** Error while parsing xml. */
    XMQ_ERROR_PARSING_XML = 28,
    /** Error while parsing html. */
    XMQ_ERROR_PARSING_HTML = 29,
    /** A value after a key cannot start with comments or equals. */
    XMQ_ERROR_VALUE_CANNOT_START_WITH = 30,
    /** Not a proper uri for a namespace. */
    XMQ_ERROR_INVALID_NAMESPACE_URI = 31,
    /** Not a proper prefix for a namespace. */
    XMQ_ERROR_INVALID_NAMESPACE_PREFIX = 32,
    /** When adding a new namespace with a prefix, the prefix has already been used. */
    XMQ_ERROR_NAMESPACE_PREFIX_ALREADY_TAKEN = 33,
    /** Pointer errors to buffers are wrong. */
    XMQ_ERROR_BAD_RANGE = 35,
    /** Invalid enums provided or other bad input to functions. */
    XMQ_ERROR_BAD_VALUE = 36,
    /** The ixml grammar cannot be parsed. */
    XMQ_ERROR_IXML_SYNTAX_ERROR = 50,
    /** Warning, when parse succeeds, but it seems there is a mistake. */
    XMQ_WARNING_QUOTES_NEEDED = 1000
} XMQStatus;

struct XMQReturnDoc
{
    XMQStatus status;
    XMQDoc    *doc;
};
typedef struct XMQReturnDoc XMQReturnDoc;

struct XMQReturnNode
{
    XMQStatus status;
    XMQNode   *node;
};
typedef struct XMQReturnNode XMQReturnNode;

struct XMQReturnAttr
{
    XMQStatus status;
    XMQAttr   *attr;
};
typedef struct XMQReturnAttr XMQReturnAttr;

struct XMQReturnString
{
    XMQStatus status;
    char *string;
};
typedef struct XMQReturnString XMQReturnString;

struct XMQReturnConstString
{
    XMQStatus status;
    const char *string;
};
typedef struct XMQReturnConstString XMQReturnConstString;

/**
    When loading xmq/xml/json as a config file, the content is parsed and decoded
    according the the requested type. These are the available core types.
*/
typedef enum
{
    XMQ_CORE_BOOL,
    /** Signed 8 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_I8,
    /** Signed 16 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_I16,
    /** Signed 32 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_I32,
    /** Signed 64 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_I64,
    /** Signed 128 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_I128,
    /** Unsigned 8 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_U8,
    /** Unsigned 16 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_U16,
    /** Unsigned 32  bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_U32,
    /** Unsigned 64 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_U64,
    /** Unsigned 128 bit integer. Can be decimal, hex (0x) or octal (0). */
    XMQ_CORE_U128,

    /** Floating point 32bit. */
    XMQ_CORE_F32,
    /** Floating point 64bit. */
    XMQ_CORE_F64,

    /** Zero to infinite sized unicode string. No zero bytes. */
    XMQ_CORE_STRING,
    /** String formatted as a valid email address. */
    XMQ_CORE_EMAIL,
    /** String formatted as a valid uri/iri. */
    XMQ_CORE_URI,
    /** String formatted as a valid url. */
    XMQ_CORE_URL,

    /** Either a v4 or a v6. */
    XMQ_CORE_IP_ADDRESS,
    /** 128.0.0.1 */
    XMQ_CORE_IPV4_ADDRESS,
    /** ::0 */
    XMQ_CORE_IPV6_ADDRESS,

    /** Base64 encoded binary data. */
    XMQ_BINARY_BASE64

} XMQCoreType;

typedef struct XMQLineConfig XMQLineConfig;

typedef struct XMQAddAttr XMQAddAttr;
struct XMQAddAttr {
    const char *name;
    const char *value;
};

typedef struct XMQAttrList XMQAttrList;
struct XMQAttrList {
    const XMQAddAttr *array;
    size_t count;
};

#ifndef __cplusplus

#define XMQ_ATTRS(...) \
    ((XMQAttrList) { \
        (XMQAddAttr[]) { __VA_ARGS__ }, \
        sizeof((XMQAddAttr[]) { __VA_ARGS__ }) / sizeof(XMQAddAttr) \
    })

#else

static inline XMQAttrList
xmqAttrs(std::initializer_list<XMQAddAttr> attrs)
{
    return { attrs.begin(), attrs.size() };
}

#define XMQ_ATTRS(...) \
    xmqAttrs({ __VA_ARGS__ })

#endif

/* This is the old definition for XMQ_ATTRS which fails to compile using g++-13.
 #define XMQ_ATTRS(...) \
    (XMQAddAttr[]){ __VA_ARGS__ }, \
    sizeof((XMQAddAttr[]){ __VA_ARGS__ }) / sizeof(XMQAddAttr)
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////// FUNCTIONS  /////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
    Detect the content type xmq/xml/html/json by examining a few leading
    non-whitespace words/characters.

    @param start Points to first byte of buffer to scan for content type.
    @param stop Points to byte after buffer.
 */
XMQContentType xmqDetectContentType(const char *start, const char *stop);

/**
   xmqParseErrorToString:
   @e: Translate this error to a human readable string.
*/
const char *xmqParseErrorToString(XMQStatus e);

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

    If the parse fails then use this function to get the integer value of XMQStatus.
*/
int xmqStateErrno(XMQParseState *state);

/**
    xmqStateErrorMsg:
    @state: the parse state.

    If the parse fails then use this function to get a string explaining the error.
*/
const char *xmqStateErrorMsg(XMQParseState *state);

/**
   xmqSetPrintAllParsesIXML:
   @state: the parse state.
   @all_parses: if true then generate all possible trees.

   If the parse is ambiguous generate all possible trees.
*/
void xmqSetPrintAllParsesIXML(XMQParseState *state, bool all_parses);

/**
   xmqSetTryToRecoverIXML:
   @state: the parse state.
   @try_recover: if true then try to recover a failed parse.

   If the parse fails then try to recover. Default is to fail.
*/
void xmqSetTryToRecoverIXML(XMQParseState *state, bool try_recover);

/**
    Create an empty document object.

    @return A return doc structure with status and doc pointer.
    If status == XMQ_OK then doc pointer is valid.
    If status == XMQ_ERROR_OOM the doc pointer is NULL.
*/
XMQReturnDoc xmqNewDoc();

/**
    Set the source name to make error message more readable when parsing fails.
    The source name is often the file name, but can be "-" for stdin or anything you like.

    @param doq         Document for which the source should be named.
    @param source_name The document source file name.
*/
void xmqSetDocSourceName(XMQDoc *doq, const char *source_name);

/**
    xmqGetDocSourceName:
    @doq: Document which source file name should be gotten.

    Returns the source name used to make error message more readable when parsing fails.
    The source name is often the file name, but can be "-" for stdin or anything you like.
*/
const char *xmqGetDocSourceName(XMQDoc *doq);

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
    xmqSetOriginalSize:

    Override the original size of the document.
*/
void xmqSetOriginalSize(XMQDoc *doq, size_t size);

/**
    xmqGetRootNode:

    Get root node suitable for xmqForeach.
*/
XMQNode *xmqGetRootNode(XMQDoc *doq);

typedef enum
{
    XMQ_NS_NONE,
    XMQ_NS_PARENT,
    XMQ_NS_HERE,
    XMQ_NS_ANCESTOR
}
NamespaceAction;

typedef struct XMQNS XMQNS;
struct XMQNS {
    NamespaceAction action;
    // A prefix (xyz) can be encoded into the uri: "{xyz}urn:myapp"
    const char *uri;
};

/** Assign no namespace to this node. */
#define NS_NONE   ((XMQNS){XMQ_NS_NONE,NULL})
/** Reuse the parent namespace for this node. */
#define NS_PARENT ((XMQNS){XMQ_NS_PARENT,NULL})
/** Create a new namespace for this node. */
#define NS_HERE(uri) ((XMQNS){XMQ_NS_HERE,uri})
/** Search for the namespace in parent and parents parent etc.
    If not found, create the namespace in the root element. */
#define NS_ANCESTOR(uri) ((XMQNS){XMQ_NS_ANCESTOR,uri})

#define END_OF_ATTRS NULL

/**
    Set a doctype for the document.

    @doq The xmq document.
    @type The name of the new element.

    @code
    xmqSetDocType(doc, "html");
    @endcode
*/
XMQStatus xmqSetDocType(XMQDoc *doq, const char *name);

/**
    Create a new root element.

    @doq The xmq document.
    @name The name of the new element.
    @ns The namespace setting.

    @code
    xmqAddElement(doc, p, "el", NS_NONE); // No namespace.
    xmqAddElement(doc, p, "el", NS_HERE("urn:myapp:driver"));
    xmqAddElement(doc, p, "el", NS_HERE("{drv}urn:myapp:driver"));
    @endcode

    NS_ANCESTOR is not permitted for the root node.
*/
XMQReturnNode xmqAddRootElement(XMQDoc *doq, const char *name, XMQNS ns);

/**
    Create a new element node below an existing element.

    @doq The xmq document.
    @parent The exiting element.
    @name The name of the new element.
    @ns The namespace setting.

    @code
    xmqAddElement(doc, p, "el", NS_NONE); // No namespace.
    xmqAddElement(doc, p, "el", NS_PARENT); // Inherit parent namespace.
    xmqAddElement(doc, p, "el", NS_HERE("urn:myapp:driver"));
    xmqAddElement(doc, p, "el", NS_HERE("{drv}urn:myapp:driver"));
    xmqAddElement(doc, p, "el", NS_ANCESTOR("urn:myapp:driver"));
    xmqAddElement(doc, p, "el", NS_ANCESTOR("{drv}urn:myapp:driver"));
    @endcode
*/
XMQReturnNode xmqAddElement(XMQDoc *doq, XMQNode *parent, const char *name, XMQNS ns);

/**
    Create a new element node below an existing element with attributes.

    @doq The xmq document.
    @parent The exiting element.
    @name The name of the new element.
    @ns The namespace setting.
    @attrs An array of attribute objects
    @num_attrs How many attribute objects.

    @code
    xmqAddElementWithAttrs(doc, p, "div", NS_PARENT,
                           XMQ_ATTRS( { "id", "123" },
                                      { "class", "info" } ));
    xmqAddElementWithAttrs(doc, p, "a", NS_PARENT, XMQ_ATTRS( { "href", "https://libxmq.org" } ));
    @endcode
*/
XMQReturnNode xmqAddElementWithAttrs(XMQDoc *doq,
                                     XMQNode *parent,
                                     const char *name,
                                     XMQNS ns,
                                     XMQAttrList attrs);

/**
    Create a key value under an existing node.

    @doq The xmq document.
    @parent The parent in which the key value is created.
    @key The key.
    @value The value.
    @ns The namespace setting.

    @code
    xmqAddKeyValue(doc, p, "price", "123", NS_PARENT);
    @endcode
*/
XMQReturnNode xmqAddKeyValue(XMQDoc *doq, XMQNode *parent, const char *key, const char *value, XMQNS ns);

/**
    Create a key value with attributes under an existing node.

    @doq The xmq document.
    @parent The parent in which the key value is created.
    @key The key.
    @value The value.
    @ns The namespace setting.
    @attrs

    @code
    xmqAddKeyValueWithAttrs(doc, p, "p", "hello", NS_NONE, XMQ_ATTRS( { "id", "123" } ));
    // <p id="123">hello</p>
    @endcode
*/
XMQReturnNode xmqAddKeyValueWithAttrs(XMQDoc *doq,
                                      XMQNode *parent,
                                      const char *key,
                                      const char *value,
                                      XMQNS ns,
                                      XMQAttrList attrs);

/**
    Create/update an attribute in an existing node.

    @code
    xmqAddElement(doc, p, "el", NS_NONE); // No namespace.
    xmqAddElement(doc, p, "el", NS_PARENT); // Inherit parent namespace.
    xmqAddElement(doc, p, "el", NS_ANCESTOR("urn:myapp:driver"));
    xmqAddElement(doc, p, "el", NS_ANCESTOR("{drv}urn:myapp:driver"));
    @endcode

    NS_HERE is not permitted for an attribute.
*/
XMQReturnAttr xmqSetAttribute(XMQDoc *doq, XMQNode *node, const char *name, const char *value, XMQNS ns);

/**
    Add a prefixed namespace to a node. It must have a prefix since the default namespace
    can only be assigned when the node is created. If the prefix is already taken an error is returned.

    @param doq The xmq document.
    @param node The node into which the namespace declaration is put.
    @param ns_uri The new namespace uri.
    @param prefix The desired prefix.

    @return XMQ_OK if all ok.

    @code
    xmqAddNamespace(doc, p, "el", NS_HERE("{drv}urn:myapp:driver"));
    @endcode

    Only NS_HERE is allowed.
*/
XMQStatus xmqAddNamespace(XMQDoc *doq, XMQNode *node, XMQNS ns);

/**
    Change the preferred prefix that was chosen automatically with xmqAddNamespace.
    Pass XMQ_NO_PREFIX to change the default prefix.
    If there is a conflict, the XMQ_ERROR_PREFIX_EXISTS is return.
*/
XMQReturnString xmqChangePrefix(XMQDoc *doq,
                                XMQNode *node,
                                const char *ns_uri,
                                const char *old_prefix,
                                const char *new_prefix);

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
    xmqClearDoc:

    Clear out, make the document empty.
*/
void xmqClearDoc(XMQDoc *doc);

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
    Parse data fetched with a reader and create a document.
    The xmq format permits multiple root nodes if an implicit root is supplied.

    @param doc The xmq doc object to populate with the parsed data.
    @param reader Use this reader to fetch input data.
    @param reader_state Pass this reader_state to the reader.
    @param implicit_root The implicit root.
    @param flags Specify parser settings.
*/
bool xmqParseReader(XMQDoc *doc, XMQReader *reader, void *reader_state, const char *implicit_root, int flags);

/** Allocate the print settings structure and zero it. */
XMQOutputSettings *xmqNewOutputSettings();

/** Free the print settings structure. */
void xmqFreeOutputSettings(XMQOutputSettings *os);

void xmqSetAddIndent(XMQOutputSettings *os, int add_indent);
void xmqSetCompact(XMQOutputSettings *os, bool compact);
void xmqSetUseColor(XMQOutputSettings *os, bool use_color);
void xmqSetTrueColor(XMQOutputSettings *os, bool truecolor);
void xmqSetBackgroundMode(XMQOutputSettings *os, bool bg_dark_mode);
void xmqSetPreferDoubleQuotes(XMQOutputSettings *os, bool prefer_double_quotes);
void xmqSetFinalNewline(XMQOutputSettings *os, bool final_nl);
void xmqSetEscapeNewlines(XMQOutputSettings *os, bool escape_newlines);
void xmqSetEscapeNon7bit(XMQOutputSettings *os, bool escape_non_7bit);
void xmqSetEscapeTabs(XMQOutputSettings *os, bool escape_tabs);
void xmqSetOutputFormat(XMQOutputSettings *os, XMQContentType output_format);
void xmqSetOmitDecl(XMQOutputSettings *os, bool omit_decl);
void xmqSetRenderFormat(XMQOutputSettings *os, XMQRenderFormat render_to);
void xmqSetRenderTheme(XMQOutputSettings *os, const char *theme_spec);
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

/** Setup where to store any potential skip. This is not an ideal solution. Fix? */
void xmqSetupPrintSkip(XMQOutputSettings *ps, size_t *skip);

/** Pretty print the document according to the settings. */
void xmqPrint(XMQDoc *doc, XMQOutputSettings *settings);

/** Recurse through the document and add offsets.
    I.e. <root>ABC<a>DEF</a>GHIJ<b>xyz</b></root> wille become
         <root o="0">ABC<a o="3">DEF</a>GHIJ<b o="10">xyz</b></root>
    assuming attribute_name="o"
*/
void xmqAnnotateOffsets(XMQDoc *doc, const char *attribute_name, const char *ns);

/** Trim xml whitespace. */
void xmqTrimWhitespace(XMQDoc *doc, int flags);

/**
    Create a compact single line quote safely storing the content.
    Output can for example be: 123, John, 'John Doe', "There's a light!", (&#10;'a line'&10;)

    @param content The string to safely quote using xmq quoting and output on a single line.

    @return Return a new null terminated buffer which the caller needs to free.
*/
char *xmqCompactQuote(const char *content);

/**
   Extract the parsing error.

   @param doc The document which we tried to parse.

   @return An explanatory text of the error. Is freed when the document itself is freed.
*/
const char *xmqDocError(XMQDoc *doc);

/**
   The error as status enum.

   @param doc The document which we tried to parse.

   @return The error status code.
*/
XMQStatus xmqDocErrno(XMQDoc *doc);

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
    xmqSetContent: set the raw content of element node
    @node: Node
*/
void xmqSetContent(XMQNode *node, const char *raw_content);

/**
    xmqGetNode:
    @doc: the xmq doc object
    @xpath: the location of the content to be returned as a node ptr.
*/
XMQNode *xmqGetNode(XMQDoc *doc, const char *xpath);

/**
    xmqGetNodeRel:
    @doc: the xmq doc object
    @xpath: the location of the content to be returned as a node ptr.
    @relative: the xpath is search using this node as the starting point.
*/
XMQNode *xmqGetNodeRel(XMQDoc *doc, const char *xpath, XMQNode *relative);

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
    xmqSetTrace:

    Enable/Disable tracing.
*/
void xmqSetTrace(bool e);

/**
    xmqLogFilter

    Enable/Disable debug/trace logging for certain prefixes.
*/
void xmqLogFilter(const char *log_filter);

/**
   xmqDebugging:

   Return whether debugging is enabled or not.
*/
bool xmqDebugging();

/**
   xmqTracing:

   Return whether tracing is enabled or not.
*/
bool xmqTracing();

/**
    xmqSetLogHumanReadable:

    Enable/Disable verbose/debug/tracing output as human readable.
    The default is in xmq format.
*/
void xmqSetLogHumanReadable(bool e);

/**
   xmqLogXMQ:

   Return whether logging in pure xmq is enabled or not.
*/
bool xmqLoggingXMQ();

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
    xmqParseBufferWithIXML:

    Parse buffer using the supplied IXML grammar.
*/
bool xmqParseBufferWithIXML(XMQDoc *doc,
                            const char *start,
                            const char *stop,
                            XMQDoc *ixml_grammar,
                            int flags);

/**
    xmqParseFileWithIXML:

    Load a file and parse it using the supplied IXML grammar. If file is NULL read from stdin.
*/
bool xmqParseFileWithIXML(XMQDoc *doc,
                          const char *file,
                          XMQDoc *ixml_grammar,
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
                      XMQColorName cn,
                      const char *pre,
                      const char *post,
                      const char *ns);


/**
    xmqNewLineConfig:

    Allocate a default XMQLineConfig.
*/
XMQLineConfig *xmqNewLineConfig();

/**
    xmqFreeLineConfig:

    Free the XMQLineConfig structure.
*/
void xmqFreeLineConfig(XMQLineConfig *lc);

/**
    xmqSetLineHumanReadable(XMQLineConfig *lc, bool enable);

    If enable is true then generate a more human readable log line.
*/
void xmqSetLineHumanReadable(XMQLineConfig *lc, bool enable);

/**
   xmqLineDoc:

   Generate from the doc a compact single line xmq string containing no newlines at all, for logging.
*/
char *xmqLineDoc(XMQLineConfig *lc, XMQDoc *doc);

/**
   xmqLinePrintf:

   Generate a compact single line xmq string containing no newlines at all, for logging.
   The content is constructed from printf formatted strings:
   xmqLinePrintf(&lc,
                 "car{",
                 "model=", "%s", modelstring,
                 "id=", "%d", idnumber,
                 "description=", "desc: %s", multilinedescription,
                 "}");
   Generates:
   car{model=Volvo id=123 description=('desc: lines of'&#10;'next line')}
*/
char *xmqLinePrintf(XMQLineConfig *lc, const char *element_name, ...);
char *xmqLineVPrintf(XMQLineConfig *lc, const char *element_name, va_list ap);

#ifdef __cplusplus
_hideRBfromEditor
#endif

#endif
