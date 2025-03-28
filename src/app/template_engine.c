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
    String tag; // do I REALLY need this ???
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
                token.tag.data = c - 1;

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
                        p += 1;
                        break;
                    }

                    if (*p == '/' && *(p + 1) == '>') {
                        token.tag_type = TAG_SELFCLOSING;
                        p += 2;
                        break;
                    }

                    p++;
                }

                token.tag.length = p - token.tag.data;

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
    printf("Invalid attribute at ");
    print_token_tag_type(token);
    printf(" tag x-%.*s:\n", (int)token.string.length, token.string.data);
    printf("    \033[90m(Hint) Only x-component tag can have attributes other than the name attribute.\033[0m\n"); // Gray text
    print_error_location(lexer, token);
    u32 i;
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

typedef struct ParentPointerNode ParentPointerNode;
struct ParentPointerNode {
    TokenType token_type;
    AttributeArray attributes;
    ParentPointerNode *parent;
};

typedef struct ChildSiblingNode ChildSiblingNode;
struct ChildSiblingNode {
    TokenType token_type;
    AttributeArray attributes;
    ChildSiblingNode *first_child;
    ChildSiblingNode *next_sibling;
};

ParentPointerNode *create_parent_pointer_node(Memory *memory, TokenType token_type, AttributeArray attributes, ParentPointerNode *parent) {
    ParentPointerNode *new_node = memory_alloc(memory, sizeof(ParentPointerNode));
    if (new_node == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }
    new_node->token_type = token_type;
    new_node->attributes = attributes;
    new_node->parent = parent;
    return new_node;
}

ChildSiblingNode *create_child_sibling_node(Memory *memory, TokenType token_type, AttributeArray attributes) {
    ChildSiblingNode *new_node = memory_alloc(memory, sizeof(ChildSiblingNode));
    if (new_node == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }
    new_node->token_type = token_type;
    new_node->attributes = attributes;
    new_node->first_child = NULL;
    new_node->next_sibling = NULL;
    return new_node;
}

ParentPointerNode *find_parent_tree_root(ParentPointerNode *node) {
    while (node->parent != NULL) {
        node = node->parent;
    }
    return node;
}

ChildSiblingNode *convert_to_child_sibling(Memory *memory, ParentPointerNode **nodes, u32 count) {
    ChildSiblingNode **cs_nodes = memory_alloc(memory, count * sizeof(ChildSiblingNode *));
    if (cs_nodes == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }

    u32 i;

    // Create corresponding ChildSiblingNodes
    for (i = 0; i < count; i++) {
        cs_nodes[i] = create_child_sibling_node(memory, nodes[i]->token_type, nodes[i]->attributes);
    }

    // Arrange first_child and next_sibling pointers
    for (i = 0; i < count; i++) {
        ParentPointerNode *parent = nodes[i]->parent;
        if (parent != NULL) {
            u32 parent_index;
            for (parent_index = 0; parent_index < count; parent_index++) {
                if (nodes[parent_index] == parent)
                    break;
            }

            ChildSiblingNode *parent_cs = cs_nodes[parent_index];

            // Insert node as a child of its parent
            if (parent_cs->first_child == NULL) {
                parent_cs->first_child = cs_nodes[i];
            } else {
                ChildSiblingNode *temp = parent_cs->first_child;
                while (temp->next_sibling != NULL) {
                    temp = temp->next_sibling;
                }
                temp->next_sibling = cs_nodes[i];
            }
        }
    }

    // Find the root of the Child-Sibling tree
    ParentPointerNode *root = find_parent_tree_root(nodes[0]); // Any node can be used to find the root
    ChildSiblingNode *cs_root = NULL;

    for (i = 0; i < count; i++) {
        if (nodes[i] == root) {
            cs_root = cs_nodes[i];
            break;
        }
    }

    return cs_root;
}

const char *token_to_string(TokenType token) {
    switch (token) {
        case TOKEN_COMPONENT_DEFINITION:
            return "COMPONENT_DEFINITION";
        case TOKEN_COMPONENT_IMPORT:
            return "COMPONENT_IMPORT";
        case TOKEN_SLOT:
            return "SLOT";
        case TOKEN_INSERT:
            return "INSERT";
        case TOKEN_VAL:
            return "VAL";
        case TOKEN_FOR:
            return "FOR";
        default:
            return "UNKNOWN";
    }
}

void print_child_sibling_tree(ChildSiblingNode *node, int depth) {
    if (node == NULL)
        return;

    // Print node with indentation
    for (int i = 0; i < depth; i++) {
        printf("    ");
    }

    printf("%s ", token_to_string(node->token_type));
    u32 i;
    for (i = 0; i < node->attributes.count; i++) {
        printf("%.*s ", (int)(node->attributes.attribute[i].name.length), node->attributes.attribute[i].name.data);
    }
    printf("\n");

    // Print first child and siblings
    print_child_sibling_tree(node->first_child, depth + 1);
    print_child_sibling_tree(node->next_sibling, depth);
}

#define STACK_CAPACITY 64

typedef struct {
    u32 count;
    ParentPointerNode *items[STACK_CAPACITY];
} ParentPointerNodes;

typedef struct {
    ParentPointerNode *node;
    TokenType token_type;
} StackElement;

typedef struct {
    StackElement data[STACK_CAPACITY];
    u8 top;
} Stack;

void build_html_components(Memory *memory, Memory *scratch_memory, AssetList asset_list) {
    size_t i;
    for (i = 0; i < asset_list.count; i++) {
        Lexer lexer = {0};

        // init lexer
        lexer.file_path = asset_list.asset_list[i];
        lexer.content = asset_list.asset_list_content[i];
        lexer.line_number = 1;

        u32 errors_count = 0;

        ParentPointerNodes parent_pointer_nodes = {0};

        Stack tags = {0};

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

            if (token.token_type == TOKEN_INVALID) {
                errors_count += 1;
                print_invalid_token_error(&lexer, token);

                ASSERT(0);
            }

            AttributeArray attributes = {0};

            Attribute *attribute = memory_alloc(memory, sizeof(Attribute));
            *attribute = get_next_attribute(&lexer);
            attributes.attribute = attribute;
            while (attribute->name.data != NULL) {
                attributes.count += 1;

                attribute = memory_alloc(memory, sizeof(Attribute));
                *attribute = get_next_attribute(&lexer);
            }

            Attribute name_attribute = {0};
            if (attributes.count > 0) {
                u32 i;
                for (i = 0; i < attributes.count; i++) {
                    if (strncmp("name", attributes.attribute[i].name.data, attributes.attribute[i].name.length) == 0) {
                        name_attribute = attributes.attribute[i];
                        break;
                    }
                }
            }

            // LEFT OF HERE, IMPORTANT:
            // There is a lot of error checking bellow, but some error checks make other error check irrelevant
            // so intead of doing all these error checks like bellow, just construct the tree at it comes and it
            // might be completely wrong, maybe slots having component definition as children or closing tags having
            // attributes, all kinds of crazy error, just contruct the tree as it comes, and parse all the error after
            // the tree is nicely constructed. This way I can just traverse down the tree and print every where where
            // the is an error.

            // Only component import opening tags can have other attributes other than the 'name' attribute

            if (attributes.count > 1 && token.token_type != TOKEN_COMPONENT_IMPORT && (token.tag_type == TAG_OPENING || token.tag_type == TAG_SELFCLOSING)) {
                u32 invalid_attributes_count = attributes.count - /* minus the name attribute */ 1;
                errors_count += invalid_attributes_count;
                print_not_allowed_attrs(&lexer, token, attributes);
            }

            // All opening or self-closing tags must have a 'name' attribute

            if (token.tag_type == TAG_SELFCLOSING || token.tag_type == TAG_OPENING) {
                if (name_attribute.name.data == NULL) {
                    u32 i;

                    printf("Missing name attribute for");
                    printf(" x-%.*s tag:\n", (int)token.string.length, token.string.data);
                    printf("    \033[90m(Hint) All opening or self-closing tags must have a name attribute.\033[0m\n");
                    print_error_location(&lexer, token);
                    print_previous_line(&lexer, token);
                    printf("    %d|  ", lexer.line_number);

                    u32 highlight_index = lexer.cursor;
                    u32 next_boli = next_line_index(lexer.boli, lexer.content);
                    for (i = lexer.boli; i < next_boli; i++) {
                        if (i == highlight_index) {
                            printf("\033[41m \033[0m");
                            i -= 1;
                            highlight_index = 0;
                        } else {
                            printf("%c", lexer.content.data[i]);
                        }
                    }
                    print_next_line(&lexer, token);
                }
            }

            // Closing tags can NOT have any attributes

            if (token.tag_type == TAG_CLOSING) {
                if (attributes.count > 0) {
                    printf("Invalid attribute at ");
                    print_token_tag_type(token);
                    printf(" tag x-%.*s:\n", (int)token.string.length, token.string.data);
                    printf("    \033[90m(Hint) Closing tags are NOT allowed any attributes.\033[0m\n"); // Gray text
                    print_error_location(&lexer, token);
                    u32 i;
                    for (i = 0; i < attributes.count; i++) {
                        Attribute *attribute = &attributes.attribute[i];
                        print_previous_line(&lexer, token);
                        printf("    %d|  ", lexer.line_number);
                        u32 next_boli = next_line_index(lexer.boli, lexer.content);
                        u32 j;
                        u32 highlight_start = attribute->name.data - lexer.content.data;
                        u32 highlight_end = (attribute->value.data + attribute->value.length + 1 /** include '"' */) - lexer.content.data;
                        for (j = lexer.boli; j < next_boli; j++) {
                            if (j >= highlight_start && j < highlight_end) {
                                // ANSI escape codes for highlighting (red background)
                                printf("\033[41m%c\033[0m", lexer.content.data[j]);
                            } else {
                                printf("%c", lexer.content.data[j]);
                            }
                        }
                        print_next_line(&lexer, token);
                    }
                }
            }

            // Check for wrong tag types. For example, a TOKEN_COMPONENT_DEFINITION can not be
            // self-closed or a TOKEN_SLOT can't have an opening tag as it should be self-closed.

            if (token.tag_type == TAG_SELFCLOSING && (token.token_type == TOKEN_COMPONENT_DEFINITION || token.token_type == TOKEN_INSERT || token.token_type == TOKEN_FOR)) {
                u32 i;

                printf("Invalid self-closing tag for");
                printf(" x-%.*s:\n", (int)token.string.length, token.string.data);
                printf("    \033[90m(Hint) All opening or self-closing tags must have a name attribute.\033[0m\n");
                print_error_location(&lexer, token);
                print_previous_line(&lexer, token);
                printf("    %d|  ", lexer.line_number);

                u32 highlight_start = lexer.cursor;
                u32 highlight_end = highlight_start + 2;
                u32 next_boli = next_line_index(lexer.boli, lexer.content);
                for (i = lexer.boli; i < next_boli; i++) {
                    if (i >= highlight_start && i < highlight_end) {
                        // ANSI escape codes for highlighting (red background)
                        printf("\033[41m%c\033[0m", lexer.content.data[i]);
                    } else {
                        printf("%c", lexer.content.data[i]);
                    }
                }
                print_next_line(&lexer, token);
            }

            if (token.token_type == TOKEN_SLOT || token.token_type == TOKEN_VAL) {
                if (token.tag_type == TAG_OPENING) {
                    printf("Invalid ");
                    print_token_tag_type(token);
                    printf(" tag for");
                    printf(" x-%.*s:\n", (int)token.string.length, token.string.data);
                    printf("    \033[90m(Hint) x-%.*s tags must be self-closing.\033[0m\n", (int)token.string.length, token.string.data);
                    print_error_location(&lexer, token);
                    print_previous_line(&lexer, token);
                    printf("    %d|  ", lexer.line_number);

                    u32 highlight_start = lexer.cursor;
                    u32 highlight_end = highlight_start + 1;
                    u32 next_boli = next_line_index(lexer.boli, lexer.content);
                    for (i = lexer.boli; i < next_boli; i++) {
                        if (i >= highlight_start && i < highlight_end) {
                            // ANSI escape codes for highlighting (red background)
                            printf("\033[41m%c\033[0m", lexer.content.data[i]);
                        } else {
                            printf("%c", lexer.content.data[i]);
                        }
                    }
                    print_next_line(&lexer, token);
                }

                if (token.tag_type == TAG_CLOSING) {
                    printf("Invalid ");
                    print_token_tag_type(token);
                    printf(" tag for");
                    printf(" x-%.*s:\n", (int)token.string.length, token.string.data);
                    printf("    \033[90m(Hint) x-%.*s tags must be self-closing.\033[0m\n", (int)token.string.length, token.string.data);
                    print_error_location(&lexer, token);
                    print_previous_line(&lexer, token);
                    printf("    %d|  ", lexer.line_number);

                    u32 highlight_start = (token.string.data - lexer.content.data) - 4;
                    u32 highlight_end = highlight_start + 2;
                    u32 next_boli = next_line_index(lexer.boli, lexer.content);
                    for (i = lexer.boli; i < next_boli; i++) {
                        if (i >= highlight_start && i < highlight_end) {
                            // ANSI escape codes for highlighting (red background)
                            printf("\033[41m%c\033[0m", lexer.content.data[i]);
                        } else {
                            printf("%c", lexer.content.data[i]);
                        }
                    }
                    print_next_line(&lexer, token);
                }
            }

            // Check tag relationship is valid. For example, there can't be a INSERT inside a SLOT
            // as slots are self-closing tags and INSERT parent should only be TOKEN_COMPONENT_IMPORT.

            // NOTE: In the code bellow, before you go to any siblings you always pass by the deepest
            //       child, maybe do a parent child relationship only and after the three is completed
            //       arrange relationship to mark those who are siblings to each other?

            if (token.token_type == TOKEN_COMPONENT_DEFINITION && token.tag_type == TAG_OPENING) {
                ParentPointerNode *node = create_parent_pointer_node(memory, TOKEN_COMPONENT_DEFINITION, attributes, NULL);

                tags.data[tags.top].token_type = TOKEN_COMPONENT_DEFINITION;
                tags.data[tags.top].node = node;
                tags.top += 1;

                parent_pointer_nodes.items[parent_pointer_nodes.count] = node;
                parent_pointer_nodes.count += 1;

                goto advance;
            }

            if (token.token_type == TOKEN_COMPONENT_IMPORT && (token.tag_type == TAG_OPENING || token.tag_type == TAG_SELFCLOSING)) {
                StackElement parent = tags.data[tags.top - 1];
                if (parent.token_type != TOKEN_COMPONENT_DEFINITION && parent.token_type != TOKEN_INSERT && parent.token_type != TOKEN_FOR) {
                    printf("Invalid ");
                    print_token_tag_type(token);
                    printf(" tag for");
                    printf(" x-%.*s:\n", (int)token.string.length, token.string.data);
                    printf("    \033[90m(Hint) x-%.*s tags must be self-closing.\033[0m\n", (int)token.string.length, token.string.data);
                    print_error_location(&lexer, token);
                    print_previous_line(&lexer, token);
                    printf("    %d|  ", lexer.line_number);

                    u32 highlight_start = (token.string.data - lexer.content.data) - 4;
                    u32 highlight_end = lexer.cursor + 1;
                    u32 next_boli = next_line_index(lexer.boli, lexer.content);
                    for (i = lexer.boli; i < next_boli; i++) {
                        if (i >= highlight_start && i < highlight_end) {
                            // ANSI escape codes for highlighting (red background)
                            printf("\033[41m%c\033[0m", lexer.content.data[i]);
                        } else {
                            printf("%c", lexer.content.data[i]);
                        }
                    }
                    print_next_line(&lexer, token);

                    ASSERT(0);
                }

                ParentPointerNode *node = create_parent_pointer_node(memory, TOKEN_COMPONENT_IMPORT, attributes, parent.node);

                if (token.tag_type == TAG_OPENING) {
                    tags.data[tags.top].token_type = TOKEN_COMPONENT_IMPORT;
                    tags.data[tags.top].node = node;
                    tags.top += 1;
                }

                parent_pointer_nodes.items[parent_pointer_nodes.count] = node;
                parent_pointer_nodes.count += 1;

                goto advance;
            }

            if (token.token_type == TOKEN_SLOT && token.tag_type == TAG_SELFCLOSING) {
                StackElement parent = tags.data[tags.top - 1];
                if (parent.token_type != TOKEN_COMPONENT_DEFINITION && parent.token_type != TOKEN_COMPONENT_IMPORT && parent.token_type != TOKEN_INSERT && parent.token_type != TOKEN_FOR) {
                    ASSERT(0);
                }

                ParentPointerNode *node = create_parent_pointer_node(memory, TOKEN_SLOT, attributes, parent.node);
                parent_pointer_nodes.items[parent_pointer_nodes.count] = node;
                parent_pointer_nodes.count += 1;

                goto advance;
            }

            if (token.token_type == TOKEN_INSERT && token.tag_type == TAG_OPENING) {
                StackElement parent = tags.data[tags.top - 1];
                if (parent.token_type != TOKEN_COMPONENT_IMPORT) {
                    ASSERT(0);
                }

                ParentPointerNode *node = create_parent_pointer_node(memory, TOKEN_INSERT, attributes, parent.node);

                tags.data[tags.top].token_type = TOKEN_INSERT;
                tags.data[tags.top].node = node;
                tags.top += 1;

                parent_pointer_nodes.items[parent_pointer_nodes.count] = node;
                parent_pointer_nodes.count += 1;

                goto advance;
            }

            if (token.token_type == TOKEN_VAL && token.tag_type == TAG_SELFCLOSING) {
                StackElement parent = tags.data[tags.top - 1];
                if (parent.token_type != TOKEN_COMPONENT_DEFINITION && parent.token_type != TOKEN_COMPONENT_IMPORT && parent.token_type != TOKEN_INSERT && parent.token_type != TOKEN_FOR) {
                    ASSERT(0);
                }

                ParentPointerNode *node = create_parent_pointer_node(memory, TOKEN_VAL, attributes, parent.node);
                parent_pointer_nodes.items[parent_pointer_nodes.count] = node;
                parent_pointer_nodes.count += 1;

                goto advance;
            }

            if (token.token_type == TOKEN_FOR && token.tag_type == TAG_OPENING) {
                StackElement parent = tags.data[tags.top - 1];
                ParentPointerNode *node = create_parent_pointer_node(memory, TOKEN_FOR, attributes, parent.node);

                tags.data[tags.top].token_type = TOKEN_FOR;
                tags.data[tags.top].node = node;
                tags.top += 1;

                parent_pointer_nodes.items[parent_pointer_nodes.count] = node;
                parent_pointer_nodes.count += 1;

                goto advance;
            }

            if (token.tag_type == TAG_CLOSING) {
                StackElement last_open = tags.data[tags.top - 1];
                if (token.token_type != last_open.token_type) {
                    ASSERT(0);
                }

                tags.data[tags.top - 1].token_type = 0;
                tags.data[tags.top - 1].node = NULL;
                tags.top -= 1;

                goto advance;
            }

        advance:;
            token = get_next_token(&lexer);
        }

        if (errors_count > 0) {
            ASSERT(0);
        }

        // Convert to child-sibling tree
        ChildSiblingNode *cs_root = convert_to_child_sibling(memory, parent_pointer_nodes.items, parent_pointer_nodes.count);
        print_child_sibling_tree(cs_root, 0);

        // TO DO: enforce
        //  - component imports must refer to a component that actually exists
        //  - all component import inserts must exist inside the imported component as slots
        //  - all component import attributes must exist inside the imported component as %replasables%
        //  - warn user if it's using a component import with self-closing tag but component definition for the imported component does contain slots. Same for attribues.

        printf("\n");
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