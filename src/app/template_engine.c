#include <ctype.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* clang-format off */
#include "./utils.h"
#include "./memory.h"
#include "./minifiers.h"
#include "./template_engine.h"
/* clang-format on */

typedef enum { TAG_OPENING, TAG_CLOSING, TAG_SELFCLOSING } TagType;

typedef enum {
    TOKEN_EOF = 0,

    TOKEN_COMPONENT_DEFINITION = 1, // x-component-def
    TOKEN_COMPONENT_IMPORT = 2,     // x-component
    TOKEN_SLOT = 3,                 // x-slot
    TOKEN_INSERT = 4,               // x-insert
    TOKEN_VAL = 5,                  // x-val
    TOKEN_FOR = 6,                  // x-for

    TOKEN_ATTRIBUTE_NAME,  // for example "name" in <x-component-def name"layout" />
    TOKEN_ATTRIBUTE_VALUE, // for example "layout" in <x-component-def name"layout" />

    TOKEN_PLACEHOLDER, // for example "%product_image%" <img src="%product_image%" /> or "%attributes%" in <button %attributes%>...</button>

    TOKEN_INVALID
} TokenType;

typedef struct {
    String string; // this is usefull in case the token is invalid to tell the user what is the invalid token
    TokenType token_type;
    TagType tag_type;
} Token;

typedef struct {
    String file_path;

    String content;
    u32 cursor;

    u32 line_number;
    u32 boli; // beginning of lile (index)
} Lexer;

char *peek(Lexer *lexer) { return &lexer->content.data[lexer->cursor]; }

TokenType get_token_type(String str) {
    char COMPONENT_DEFINITION[] = "component-def";
    char COMPONENT_IMPORT[] = "component";
    char SLOT[] = "slot";
    char INSERT[] = "insert";
    char VAL[] = "val";
    char FOR[] = "for";

    if (strncmp(COMPONENT_DEFINITION, str.data, str.length) == 0 && str.length == (array_count(COMPONENT_DEFINITION) - 1)) {
        return TOKEN_COMPONENT_DEFINITION;
    } else if (strncmp(COMPONENT_IMPORT, str.data, str.length) == 0 && str.length == (array_count(COMPONENT_IMPORT) - 1)) {
        return TOKEN_COMPONENT_IMPORT;
    } else if (strncmp(SLOT, str.data, str.length) == 0 && str.length == (array_count(SLOT) - 1)) {
        return TOKEN_SLOT;
    } else if (strncmp(INSERT, str.data, str.length) == 0 && str.length == (array_count(INSERT) - 1)) {
        return TOKEN_INSERT;
    } else if (strncmp(VAL, str.data, str.length) == 0 && str.length == (array_count(VAL) - 1)) {
        return TOKEN_VAL;
    } else if (strncmp(FOR, str.data, str.length) == 0 && str.length == (array_count(FOR) - 1)) {
        return TOKEN_FOR;
    } else {
        return TOKEN_INVALID;
    }
}

typedef struct {
    String name;
    String value;
} Attribute;

typedef struct {
    Attribute *attribute;
    u32 count;
} AttributeArray;

Attribute get_next_attribute(Lexer *lexer) {
    // Typically, we encounter HTML attributes in a format like this:
    // <x-foo name = " value   " ...
    // However, attributes may also be formatted inconsistently, for example:
    // <x-foo  name =  " value   " ...
    //
    // If this is the first HTML attribute we are retrieveing we would be possition as follows:
    // <x-foo  name  = " value   " ...
    // ______^________________________
    // otherwise we would be possition after the double quotes (") of the last HTML
    // attribute we processed, like this:
    // <x-foo  name  = " value   " ...
    // ___________________________^___
    //
    // Let's assume we are about to process the first HTML attribute, the lexer is positioned as follows:
    // <x-foo  name  = " value   " ...
    // ______^________________________
    //
    // Our goal is to extract the first HTML attribute (name and value) we encounter,
    // which ending is indicated by double quote ("):
    // <x-foo  name  = " value   " ...
    // ______^^^^^^^^^^^^^^^^^^^^^____

    Attribute attribute = {0};

    char *c = peek(lexer);

    // We start by getting rid of possible initial whitespaces:
    // <x-foo  name  = " value   " ...
    // ______^^______________________
    while (isspace(*c)) {
        lexer->cursor += 1;
        c = peek(lexer);
    }

    // If this function is called after processing the last attribute,
    // we might be positioned here:
    // <x-foo  name  = " value   " name="value"    >
    // ________________________________________^____
    //
    // But as we already skip the witespaces in the previous step,
    // we would now be here:
    // <x-foo  name  = " value   " name="value"    >
    // ____________________________________________^
    //
    // (For now, we assume only opening tags, not self-closing ones.)
    if (*c == '>' || (*c == '/' && *(c + 1) == '>')) {
        return attribute;
    }

    // At this point we should be positioned at the start of the attribute name:
    // <x-foo  name  = " value   " ...
    // ________^______________________
    attribute.name.data = c;

    // Advance past the attribute name to here:
    // <x-foo  name  = " value   " ...
    // ____________^__________________
    while (*c != '>' && !isspace(*c) && *c != '=') {
        attribute.name.length += 1;

        lexer->cursor += 1;
        c = peek(lexer);
    }

    // Skip possible white spaces again:
    // <x-foo  name  = " value   " ...
    // ____________^^_________________
    while (*c != '>' && isspace(*c)) {
        lexer->cursor += 1;
        c = peek(lexer);
    }

    // At this point, we should be at the '=' character:
    // <x-foo  name  = " value   " ...
    // ______________^________________
    ASSERT(*c == '=');
    lexer->cursor += 1; // skip '='
    c = peek(lexer);

    // you get the point by now, same logic applies in the code bellow

    while (*c != '>' && isspace(*c)) {
        lexer->cursor += 1;
        c = peek(lexer);
    }

    ASSERT(*c == '"');
    lexer->cursor += 1; // skip double quotes (")
    c = peek(lexer);

    while (*c != '>' && isspace(*c)) {
        lexer->cursor += 1;
        c = peek(lexer);
    }

    attribute.value.data = c;

    while (*c != '>' && *c != '"') {
        attribute.value.length += 1;

        lexer->cursor += 1;
        c = peek(lexer);
    }

    while (*c != '>' && isspace(*c)) {
        lexer->cursor += 1;
        c = peek(lexer);
    }

    ASSERT(*c == '"');
    lexer->cursor += 1; // skip double quotes (")

    return attribute;
}

Token get_next_token(Lexer *lexer) {
    Token token = {0};
    while (lexer->cursor < lexer->content.length) {
        char *c = peek(lexer);

        if (*c == 'x' && *(c + 1) == '-') {
            if (*(c - 1) == '<') { // opening tag
                char *tag_type = c + 2;

                String html_tag_type = {0};
                html_tag_type.data = tag_type;
                while (*tag_type != '\0' && !isspace(*tag_type)) {
                    html_tag_type.length += 1;
                    tag_type += 1;
                }

                // check if tag is self closing
                char *p = html_tag_type.data;
                while (*p != '\0') {
                    if (*p == '>') {
                        token.tag_type = TAG_OPENING;
                        break;
                    }

                    if (*p == '/' && *(p + 1) == '>') {
                        token.tag_type = TAG_SELFCLOSING;
                        break;
                    }

                    p++;
                }

                token.token_type = get_token_type(html_tag_type);
                token.string = html_tag_type;

                lexer->cursor = (token.string.data + token.string.length) - lexer->content.data;

                return token;
            }

            if (*(c - 1) == '/' && *(c - 2) == '<') { // closing tag
                char *tag_type = c + 2;

                String html_tag_type = {0};
                html_tag_type.data = tag_type;
                while (*tag_type != '\0' && !isspace(*tag_type) && *tag_type != '>') {
                    html_tag_type.length += 1;
                    tag_type += 1;
                }

                token.tag_type = TAG_CLOSING;
                token.token_type = get_token_type(html_tag_type);
                token.string = html_tag_type;

                lexer->cursor = (token.string.data + token.string.length) - lexer->content.data;

                return token;
            }
        }

        if (*c == '\n') {
            lexer->line_number += 1;
            lexer->boli = lexer->cursor + 1;
        }

        lexer->cursor += 1;
    }

    return token;
}

u32 next_line_index(u32 boli, String text) {
    while (boli < text.length) {
        if (text.data[boli] == '\n') {
            return boli + 1;
        }

        boli += 1;
    }

    return text.length;
}

u32 previous_line_index(u32 boli, char *text) {
    boli -= 1;
    while (boli > 0) {
        if (text[boli] == '\n') {
            return boli + 1;
        }

        boli -= 1;
    }

    return 0;
}

void print_previous_line(Lexer *lexer, Token token) {
    if (lexer->line_number > 1) {
        u32 prev_boli = previous_line_index(lexer->boli - 1, lexer->content.data);
        u32 line_length = (lexer->boli - 1) - prev_boli;
        printf("    %d|  %.*s\n", lexer->line_number - 1, line_length, &lexer->content.data[prev_boli]);
    }
}

void print_next_line(Lexer *lexer, Token token) {
    u32 next_boli = next_line_index(lexer->boli, lexer->content);
    if (next_boli < lexer->content.length) {
        u32 next_eoli = next_line_index(next_boli, lexer->content);
        u32 next_line_length = next_eoli - next_boli;
        printf("    %d|  %.*s\n", lexer->line_number + 1, next_line_length, &lexer->content.data[next_boli]);
    }
}

void print_error_location(Lexer *lexer, Token token) {
    printf("    file: %.*s\n", (int)lexer->file_path.length, lexer->file_path.data);
    printf("    line: %d\n", lexer->line_number);
    printf("\n");
}

void print_token_tag_type(Token token) {
    switch (token.tag_type) {
        case TAG_OPENING:
            printf("opening");
            break;
        case TAG_CLOSING:
            printf("closing");
            break;
        case TAG_SELFCLOSING:
            printf("self-closing");
            break;
        default:
            ASSERT(0);
            break;
    }
}

void print_invalid_token_error(Lexer *lexer, Token token) {
    u32 i;

    u32 highlight_start = token.string.data - lexer->content.data;
    u32 highlight_end = (token.string.data + token.string.length) - lexer->content.data;
    printf("Invalid ");
    print_token_tag_type(token);
    printf(" tag at:\n");
    print_error_location(lexer, token);
    print_previous_line(lexer, token);
    printf("    %d|  ", lexer->line_number);
    u32 next_boli = next_line_index(lexer->boli, lexer->content);
    for (i = lexer->boli; i < next_boli; i++) {
        if (i >= highlight_start && i < highlight_end) {
            // ANSI escape codes for highlighting (red background)
            printf("\033[41m%c\033[0m", lexer->content.data[i]);
        } else {
            printf("%c", lexer->content.data[i]);
        }
    }
    print_next_line(lexer, token);
}

void print_not_allowed_attrs(Lexer *lexer, Token token, AttributeArray attributes) {
    u32 i;

    printf("Invalid attribute at ");
    print_token_tag_type(token);
    printf(" tag x-%.*s:\n", (int)token.string.length, token.string.data);
    printf("    \033[90m(Hint) Only x-component tag can have attributes other than the name attribute.\033[0m\n"); // Gray text
    print_error_location(lexer, token);
    for (i = 0; i < attributes.count; i++) {
        Attribute *attribute = &attributes.attribute[i];
        if (strncmp("name", attribute->name.data, attribute->name.length) == 0 && strlen("name") == attributes.attribute[i].name.length) {
            continue;
        }

        print_previous_line(lexer, token);
        printf("    %d|  ", lexer->line_number);
        u32 next_boli = next_line_index(lexer->boli, lexer->content);
        u32 j;
        u32 highlight_start = attribute->name.data - lexer->content.data;
        u32 highlight_end = (attribute->value.data + attribute->value.length + 1 /** include '"' */) - lexer->content.data;
        for (j = lexer->boli; j < next_boli; j++) {
            if (j >= highlight_start && j < highlight_end) {
                // ANSI escape codes for highlighting (red background)
                printf("\033[41m%c\033[0m", lexer->content.data[j]);
            } else {
                printf("%c", lexer->content.data[j]);
            }
        }
        print_next_line(lexer, token);
    }
}

// typedef struct {
//     String *name;  // all attribute names
//     String *value; // all attribute names
//     u32 count;
// } _Attribute;

// typedef struct {
//     TagIdentifier *tag; // tags for all decendants of a tree
//     String *component_name;
//     _Attribute *attribute;
//     u32 count; // how many items in arrays above (number of tree descendants)
// } TreeData;

// typedef struct ComponentNode ComponentNode;

// struct ComponentNode {
//     ComponentNode *child;
//     ComponentNode *sibling;
//     String *component_name;
//     TagIdentifier tag;
//     u32 index_access;
// };

// typedef struct {
//     String *name;  // all attribute names
//     String *value; // all attribute names
//     u32 count;
// } _Attribute;

// typedef struct {
//     TagIdentifier *tag; // tags for all decendants of a tree
//     String *component_name;
//     _Attribute *attribute;
//     u32 count; // how many items in arrays above (number of tree descendants)
// } TreeData;

// typedef struct ComponentNode ComponentNode;

// struct ComponentNode {
//     ComponentNode *child;
//     ComponentNode *sibling;
//     u32 index_access;
// };

// struct AttributeNode {
//     String name;
//     String value;
//     AttributeNode *next;
// };

// typedef struct ComponentNode ComponentNode;

// struct ComponentNode {
//     TagIdentifier tag;
//     String name;
//     Attribute *attributes;
//     ComponentNode *child;
//     ComponentNode *sibling;
// };

void build_html_components(Memory *memory, Memory *scratch_memory, AssetList asset_list) {
    size_t i;
    for (i = 0; i < asset_list.count; i++) {
        Lexer lexer = {0};

        // init lexer
        lexer.file_path = asset_list.asset_list[i];
        lexer.content = asset_list.asset_list_content[i];
        lexer.line_number = 1;

        u32 errors_count = 0;

        Token token = get_next_token(&lexer);
        while (token.token_type != TOKEN_EOF) {
            /*
            Enforce:
            +---------------------------+-------------------+----------------+------------+
            | Tag Type                  | Tag Identifier    | Self-Closing   | Attributes |
            +---------------------------+-------------------+----------------+------------+
            | Component definition      | x-component-def   | Can't be       | No         |
            | Component definition slot | x-slot            | Must be        | No         |
            | Component import          | x-component       | (*1)Either     | Yes        |
            | Component import insert   | x-insert          | Can't be       | No         |
            +---------------------------+-------------------+----------------+------------+
            *1 x-component tags can be either self-closing or use opening and closing tags.

            • All tags MUST have a name attribute.
            • x-component can exclusively have x-insert as direct children these must have a pair of opening/closing tags.
            • All x-insert direct children inside x-component must exist in its corresponding x-component-def component definition as x-slot(s).
            • All x-component HTML attributes (except the name attr) must exist in its corresponding x-component-def component definition
              as %attributes-name%, these attributes are not in the x-component-def though, rather anywhere inside the HTML enclosed by the
              opening/closing x-component-def tags.
            • When x-component is self-closed its corresponding x-component-def component definition can NOT have x-slot(s).

            The parser will generate a tree for each component. The trees will be used for checking rules layed out above:
                component def (+ name attr)
                    component import (+ name attr + other attrs)
                        component insert (+ name attr)
                            component import (+ name attr + other attrs)
                                component insert (+ name attr)
                                    slot (+ name attr)
                        component insert (+ name attr)
                        component insert (+ name attr)
                    component import (+ name attr + other attrs)
                        component insert (+ name attr)
                    component import (+ name attr + other attrs)
                    slot (+ name attr)
            */

            // NOTE: Parser, trees.
            // Create the component trees, then traverse them to verify their
            // relationships. For example, if the first child of the current
            // tree is a component import with 3 attributes, check the corresponding
            // component tree to ensure that the attributes are properly declared.

            AttributeArray attributes = {0};

            Attribute *attribute = memory_alloc(memory, sizeof(Attribute));
            *attribute = get_next_attribute(&lexer);
            attributes.attribute = attribute;
            while (attribute->name.data != NULL) {
                attributes.count += 1;
                // printf("%.*s: %.*s\n", (int)attribute->name.length, attribute->name.data, (int)attribute->value.length, attribute->value.data);

                attribute = memory_alloc(memory, sizeof(Attribute));
                *attribute = get_next_attribute(&lexer);
            }

            boolean has_name_attr = false;
            if (attributes.count > 0) {
                u32 i;
                for (i = 0; i < attributes.count; i++) {
                    if (strncmp("name", attributes.attribute[i].name.data, attributes.attribute[i].name.length) == 0) {
                        has_name_attr = true;
                        break;
                    }
                }
            }

            if (token.tag_type == TAG_OPENING) {
                if (token.token_type == TOKEN_INVALID) {
                    errors_count += 1;
                    print_invalid_token_error(&lexer, token);
                }

                ASSERT(token.token_type != TOKEN_SLOT); // Slots MUST be self-closing tags
                ASSERT(has_name_attr == true);          // All opening tags MUST have a name attribute

                if (token.token_type != TOKEN_COMPONENT_IMPORT && attributes.count > 1) {
                    // highlight the attribute that is not supposed to be there
                    errors_count += attributes.count - 1 /* minus the name attribute */;
                    print_not_allowed_attrs(&lexer, token, attributes);
                }

                // printf("%.*s(opening)\n", (int)token.string.length, token.string.data);
            }

            if (token.tag_type == TAG_CLOSING) {
                if (token.token_type == TOKEN_INVALID) {
                    errors_count += 1;
                    print_invalid_token_error(&lexer, token);
                }

                ASSERT(token.token_type != TOKEN_SLOT); // Slots MUST be self-closing tags
                ASSERT(attributes.count == 0);          // Closing tags can NOT have attributes

                // printf("%.*s(closing)\n", (int)token.string.length, token.string.data);
            }

            if (token.tag_type == TAG_SELFCLOSING) {
                if (token.token_type == TOKEN_INVALID) {
                    errors_count += 1;
                    print_invalid_token_error(&lexer, token);
                }

                ASSERT(token.token_type != TOKEN_COMPONENT_DEFINITION); // Component definitions can NOT be self-closing tags
                ASSERT(token.token_type != TOKEN_INSERT);               // Inserts can NOT be self-closing tags
                ASSERT(has_name_attr == true);                          // All opening tags MUST have a name attribute

                // printf("%.*s(selfclosing)\n", (int)token.string.length, token.string.data);
            }

            token = get_next_token(&lexer);
        }

        if (errors_count > 0) {
            ASSERT(0);
        }
    }

    return;
}

size_t render_val(char *template, char *val_name, char *value) {
    size_t r = 0;
    return r;
}
size_t replace_val(char *template, char *val_name, char *value) {
    size_t r = 0;
    return r;
}