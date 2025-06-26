#include <ctype.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* clang-format off */
#include "../utils/utils.h"
#include "../memory/memory.h"
#include "../minifiers/minifiers.h"
#include "../markup/markup_parser.h"
/* clang-format on */

#define COMPONENT_DEFINITION_IDENTIFIER "x-component-def"
#define COMPONENT_IMPORT_IDENTIFIER "x-component"
#define SLOT_IDENTIFIER "x-slot"
#define INSERT_IDENTIFIER "x-insert"
#define VAL_IDENTIFIER "x-val"
#define FOR_IDENTIFIER "x-for"

typedef enum {
    TOKEN_EOF = 0,

    TOKEN_COMPONENT_DEFINITION = 1,
    TOKEN_COMPONENT_IMPORT = 2,
    TOKEN_SLOT = 3,
    TOKEN_INSERT = 4,
    TOKEN_VAL = 5,
    TOKEN_FOR = 6,

    TOKEN_PLACEHOLDER,

    TOKEN_INVALID
} TokenKind;

typedef struct {
    enum { TAG_OPENING, TAG_CLOSING, TAG_SELFCLOSING } type;
    String markup;
} Tag;

typedef struct {
    TokenKind kind;
    Tag tag; // when is TokenKind is TOKEN_PLACEHOLDER this can be just left empty
    String identifier;
} Token;

typedef struct {
    String file_path;
    String content;
    u32 cursor;
    u32 line_number;
} Lexer;

typedef struct {
    String name;
    String value;
} Attribute;

typedef struct {
    Attribute *attributes;
    u32 name_attr_index;
    u32 count;
} AttributeArray;

typedef struct Node Node;
struct Node {
    Token token;
    u32 line_number;
    AttributeArray attribute_array;
    Node *parent;
};

typedef struct ASTNode ASTNode;
struct ASTNode {
    Token token;
    u32 line_number;
    AttributeArray attribute_array;
    ASTNode *first_child;
    ASTNode *next_sibling;
};

typedef struct {
    Node **data;
    u8 count;
} NodeArray;

typedef struct {
    Node *node;
    TokenKind kind;
} TagStackElement;

typedef struct {
    TagStackElement *data;
    u8 count;
} TagStack;

#define MAX_NAME_LENGTH 32
typedef char Name[MAX_NAME_LENGTH]; // 2 of these fit nicely in a 64 bytes cache line

typedef struct {
    Name *names;
    ASTNode **ast_nodes;
    u32 count;
} SlotSOA;

typedef struct {
    Name *names;
    u32 count;
} PlaceholderSOA;

typedef struct {
    Name *names;
    ASTNode **ast_nodes;
    u32 count;
} InsertSOA;

typedef struct {
    Name *names;
    ASTNode **ast_nodes;
    InsertSOA *inserts_soa;
    u32 count;
} ImportSOA;

typedef struct {
    Name *names;
    String *components;
    PlaceholderSOA **placeholders_soa;
    ImportSOA **imports_soa;
    SlotSOA **slots_soa;
    String *file_paths;
    String *contents;
    u32 count;
} ComponentSOA;

char *peek(Lexer *lexer) { return &lexer->content.data[lexer->cursor]; }
void advance_cursor(Lexer *lexer) { lexer->cursor += 1; }

TokenKind get_token_kind(String identifier) {
    if (strncmp(COMPONENT_DEFINITION_IDENTIFIER, identifier.data, identifier.length) == 0 && identifier.length == (array_count(COMPONENT_DEFINITION_IDENTIFIER) - 1)) {
        return TOKEN_COMPONENT_DEFINITION;
    } else if (strncmp(COMPONENT_IMPORT_IDENTIFIER, identifier.data, identifier.length) == 0 && identifier.length == (array_count(COMPONENT_IMPORT_IDENTIFIER) - 1)) {
        return TOKEN_COMPONENT_IMPORT;
    } else if (strncmp(SLOT_IDENTIFIER, identifier.data, identifier.length) == 0 && identifier.length == (array_count(SLOT_IDENTIFIER) - 1)) {
        return TOKEN_SLOT;
    } else if (strncmp(INSERT_IDENTIFIER, identifier.data, identifier.length) == 0 && identifier.length == (array_count(INSERT_IDENTIFIER) - 1)) {
        return TOKEN_INSERT;
    } else if (strncmp(VAL_IDENTIFIER, identifier.data, identifier.length) == 0 && identifier.length == (array_count(VAL_IDENTIFIER) - 1)) {
        return TOKEN_VAL;
    } else if (strncmp(FOR_IDENTIFIER, identifier.data, identifier.length) == 0 && identifier.length == (array_count(FOR_IDENTIFIER) - 1)) {
        return TOKEN_FOR;
    } else {
        return TOKEN_INVALID;
    }
}

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
        advance_cursor(lexer);
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

        advance_cursor(lexer);
        c = peek(lexer);
    }

    // Skip possible white spaces again:
    // <x-foo  name  = " value   " ...
    // ____________^^_________________
    while (*c != '>' && isspace(*c)) {
        advance_cursor(lexer);
        c = peek(lexer);
    }

    // At this point, we should be at the '=' character:
    // <x-foo  name  = " value   " ...
    // ______________^________________
    ASSERT(*c == '=');
    advance_cursor(lexer); // skip '='
    c = peek(lexer);

    // you get the point by now, same logic applies in the code bellow

    while (*c != '>' && isspace(*c)) {
        advance_cursor(lexer);
        c = peek(lexer);
    }

    ASSERT(*c == '"');
    advance_cursor(lexer); // skip double quotes (")
    c = peek(lexer);

    while (*c != '>' && isspace(*c)) {
        advance_cursor(lexer);
        c = peek(lexer);
    }

    attribute.value.data = c;

    while (*c != '>' && *c != '"') {
        attribute.value.length += 1;

        advance_cursor(lexer);
        c = peek(lexer);
    }

    while (*c != '>' && isspace(*c)) {
        advance_cursor(lexer);
        c = peek(lexer);
    }

    ASSERT(*c == '"');
    advance_cursor(lexer); // skip double quotes (")

    return attribute;
}

Token get_next_token(Lexer *lexer) {
    Token token = {0};
    while (lexer->cursor < lexer->content.length) {
        char *c = peek(lexer);

        if (*c == 'x' && *(c + 1) == '-') {
            if (*(c - 1) == '<') { // opening tag
                char *p = NULL;

                token.tag.markup.data = c - 1;

                p = c;
                String identifier = {0};
                identifier.data = p;
                while (*p != '\0' && !isspace(*p) && *p != '>') {
                    identifier.length += 1;
                    p += 1;
                }

                // check if tag is self closing
                p = identifier.data;
                while (*p != '\0') {
                    if (*p == '>') {
                        token.tag.type = TAG_OPENING;
                        token.tag.markup.length = p + 1 - token.tag.markup.data;
                        break;
                    }

                    if (*p == '/' && *(p + 1) == '>') {
                        token.tag.type = TAG_SELFCLOSING;
                        token.tag.markup.length = p + 2 - token.tag.markup.data;
                        break;
                    }

                    p++;
                }

                token.identifier = identifier;
                token.kind = get_token_kind(token.identifier);

                lexer->cursor = (identifier.data + identifier.length) - lexer->content.data;

                return token;
            }

            if (*(c - 1) == '/' && *(c - 2) == '<') { // closing tag
                char *p = NULL;

                token.tag.markup.data = c - 2;

                p = c;
                String identifier = {0};
                identifier.data = p;

                while (*p != '\0') {
                    if (*p == '>') {
                        token.tag.markup.length = p + 1 - token.tag.markup.data;
                        break;
                    }

                    // NOTE: Think of a better way to do this
                    if (!isspace(*p)) {
                        identifier.length += 1;
                    }

                    p += 1;
                }

                token.tag.type = TAG_CLOSING;
                token.kind = get_token_kind(identifier);

                lexer->cursor = (identifier.data + identifier.length) - lexer->content.data;

                return token;
            }
        }

        if (*c == '%') {
            char *p = NULL;

            p = c + 1;
            if (*p != '%') {
                while (*p != '\0') {
                    if (isspace(*p)) {
                        break;
                    }

                    if (*p == '%') {
                        String identifier = {0};
                        identifier.data = c + 1;
                        identifier.length = p - identifier.data;

                        token.identifier = identifier;
                        token.kind = TOKEN_PLACEHOLDER;

                        lexer->cursor = (identifier.data + identifier.length + 1) - lexer->content.data;

                        return token;
                    }

                    p += 1;
                }
            }
        }

        if (*c == '\n') {
            lexer->line_number += 1;
        }

        advance_cursor(lexer);
    }

    return token;
}

char *find_eol(char *p, char *text_end) {
    char *eol = p;
    while (eol < text_end) {
        if (*eol == '\n') {
            return eol;
        }
        eol += 1;
    }

    return text_end;
}

char *find_bol(char *p, char *text_start) {
    char *bol = p;
    while (bol > text_start) {
        if (*bol == '\n') {
            bol += 1;
            return bol;
        }
        bol -= 1;
    }

    return text_start;
}

Node *create_node(Memory *memory, Token token, String file_path, u32 line_number, AttributeArray attribute_array, Node *parent) {
    Node *new_node = memory_alloc(memory, sizeof(Node));
    if (new_node == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }
    new_node->token = token;
    new_node->line_number = line_number;
    new_node->attribute_array = attribute_array;
    new_node->parent = parent;
    return new_node;
}

ASTNode *create_ast_node(Memory *memory, Token token, u32 line_number, AttributeArray attribute_array) {
    ASTNode *new_node = memory_alloc(memory, sizeof(ASTNode));
    if (new_node == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }
    new_node->token = token;
    new_node->line_number = line_number;
    new_node->attribute_array = attribute_array;
    new_node->first_child = NULL;
    new_node->next_sibling = NULL;
    return new_node;
}

Node *find_parent_tree_root(Node *node) {
    while (node->parent != NULL) {
        node = node->parent;
    }
    return node;
}

ASTNode *convert_to_ast(Memory *memory, Node **nodes, u32 count) {
    ASTNode **ast_nodes = memory_alloc(memory, count * sizeof(ASTNode *));

    u32 i;

    // Create corresponding ChildSiblingNodes
    for (i = 0; i < count; i++) {
        ast_nodes[i] = create_ast_node(memory, nodes[i]->token, nodes[i]->line_number, nodes[i]->attribute_array);
    }

    // Arrange first_child and next_sibling pointers
    for (i = 0; i < count; i++) {
        Node *_parent = nodes[i]->parent;
        if (_parent != NULL) {
            u32 parent_index;
            for (parent_index = 0; parent_index < count; parent_index++) {
                if (nodes[parent_index] == _parent)
                    break;
            }

            ASTNode *parent = ast_nodes[parent_index];

            // Insert node as a child of its parent
            if (parent->first_child == NULL) {
                parent->first_child = ast_nodes[i];
            } else {
                ASTNode *temp = parent->first_child;
                while (temp->next_sibling != NULL) {
                    temp = temp->next_sibling;
                }
                temp->next_sibling = ast_nodes[i];
            }
        }
    }

    // Find the root of the Child-Sibling tree
    Node *root = find_parent_tree_root(nodes[0]); // Any node can be used to find the root
    ASTNode *ast_root = NULL;

    for (i = 0; i < count; i++) {
        if (nodes[i] == root) {
            ast_root = ast_nodes[i];
            break;
        }
    }

    return ast_root;
}

const char *token_kind_to_string(TokenKind kind) {
    switch (kind) {
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
            return "INVALID";
    }
}

void print_ast_tree(ASTNode *node, int depth) {
    if (node == NULL) {
        return;
    }

    // Print node with indentation
    for (int i = 0; i < depth; i++) {
        printf("    ");
    }

    printf("%s ", token_kind_to_string(node->token.kind));
    u32 i;
    for (i = 0; i < node->attribute_array.count; i++) {
        printf("%.*s=\"%.*s\" ", (int)(node->attribute_array.attributes[i].name.length), node->attribute_array.attributes[i].name.data, (int)(node->attribute_array.attributes[i].value.length), node->attribute_array.attributes[i].value.data);
    }
    printf("\n");

    // Print first child and siblings
    print_ast_tree(node->first_child, depth + 1);
    print_ast_tree(node->next_sibling, depth);
}

void print_tag_error(String content, char error[], String file_path, String identifier, u32 line_number) {
    char *content_end = content.data + content.length;

    printf("%s:\n", error);
    printf("    file: %.*s\n", (int)file_path.length, file_path.data);
    printf("    line: %d\n", line_number);
    printf("\n");
    char *bol = find_bol(identifier.data, content.data);
    if (line_number > 1) {
        char *prev_bol = find_bol(bol - 2, content.data);
        u32 prev_line_length = (bol - 1) - prev_bol;
        printf("    %d|  %.*s\n", line_number - 1, prev_line_length, prev_bol);
    }
    char *eol = find_eol(bol, content_end);

    printf("    %d|  ", line_number);
    char *p = bol;
    while (p < eol) {
        if (p >= identifier.data && p < identifier.data + identifier.length) {
            // ANSI escape codes for highlighting (red background)
            printf("\033[41m%c\033[0m", *p);
        } else {
            printf("%c", *p);
        }
        p += 1;
    }

    printf("\n");

    if (eol < content_end) {
        char *next_bol = eol + 1;
        char *next_eol = find_eol(next_bol, content.data + content.length);
        u32 next_line_length = next_eol - next_bol;
        printf("    %d|  %.*s\n", line_number + 1, next_line_length, next_bol);
    }

    printf("\n");
}

void print_tag_attr_error(String content, ASTNode *node, Attribute attribute) {
    char *content_end = content.data + content.length;

    char *bol = find_bol(node->token.identifier.data, content.data);
    if (node->line_number > 1) {
        char *prev_bol = find_bol(bol - 2, content.data);
        u32 prev_line_length = (bol - 1) - prev_bol;
        printf("    %d|  %.*s\n", node->line_number - 1, prev_line_length, prev_bol);
    }
    char *eol = find_eol(bol, content_end);

    printf("    %d|  ", node->line_number);
    char *p = bol;
    while (p < eol) {
        if (p >= attribute.name.data && p < attribute.value.data + attribute.value.length + 1) {
            printf("\033[41m%c\033[0m", *p);
        } else {
            printf("%c", *p);
        }
        p += 1;
    }

    printf("\n");

    if (eol < content_end) {
        char *next_bol = eol + 1;
        char *next_eol = find_eol(next_bol, content.data + content.length);
        u32 next_line_length = next_eol - next_bol;
        printf("    %d|  %.*s\n", node->line_number + 1, next_line_length, next_bol);
    }

    printf("\n");
}

void validate_ast_tag_hierarchy(String content, String file_path, ASTNode *node, u32 *import_count, u32 *slot_count, u32 *error_count) {
    switch (node->token.kind) {
        case TOKEN_COMPONENT_DEFINITION: {
            ASTNode *child = node->first_child;
            while (child) {
                if (child->token.kind == TOKEN_INSERT) {
                    char error[] = "x-insert can not be direct child of x-component-def, it can only be direct child of x-component";

                    print_tag_error(content, error, file_path, child->token.identifier, child->line_number);
                    *error_count += 1;
                }

                child = child->next_sibling;
            }

            break;
        }
        case TOKEN_COMPONENT_IMPORT: {
            ASTNode *child = node->first_child;
            while (child) {
                if (child->token.kind != TOKEN_INSERT) {
                    char error[] = "x-component can only have direct child x-insert";

                    print_tag_error(content, error, file_path, child->token.identifier, child->line_number);
                    *error_count += 1;
                }

                child = child->next_sibling;
            }

            *import_count += 1;

            break;
        }
        case TOKEN_SLOT: {
            ASTNode *child = node->first_child;
            if (child) {
                char error[] = "x-slot can't have any children";

                print_tag_error(content, error, file_path, node->token.identifier, node->line_number);
                *error_count += 1;
            }

            *slot_count += 1;

            break;
        }
        case TOKEN_INSERT: {
            ASTNode *child = node->first_child;
            while (child) {
                if (child->token.kind == TOKEN_INSERT) {
                    char error[] = "x-insert can not be direct child of x-insert, it can only be direct child of x-component";

                    print_tag_error(content, error, file_path, child->token.identifier, child->line_number);
                    *error_count += 1;
                }

                child = child->next_sibling;
            }

            break;
        }
        case TOKEN_VAL: {
            ASTNode *child = node->first_child;
            if (child) {
                char error[] = "x-val can't have any children";

                print_tag_error(content, error, file_path, node->token.identifier, node->line_number);
                *error_count += 1;
            }

            break;
        }
        case TOKEN_FOR: {
            ASTNode *child = node->first_child;
            while (child) {
                if (child->token.kind == TOKEN_INSERT) {
                    char error[] = "x-insert can not be direct child of x-for, it can only be direct child of x-component";

                    print_tag_error(content, error, file_path, child->token.identifier, child->line_number);
                    *error_count += 1;
                }

                child = child->next_sibling;
            }

            break;
        }
        default: {
            ASSERT(0);
            break;
        }
    }

    if (node->next_sibling) {
        validate_ast_tag_hierarchy(content, file_path, node->next_sibling, import_count, slot_count, error_count);
    }

    if (node->first_child) {
        validate_ast_tag_hierarchy(content, file_path, node->first_child, import_count, slot_count, error_count);
    }
}

void ast_to_soa(Memory *memory, ASTNode *node, ComponentSOA *components_soa) {
    u32 current_component_index = components_soa->count;

    if (node->token.kind == TOKEN_SLOT) {
        SlotSOA *slots_soa = components_soa->slots_soa[current_component_index];

        memcpy(slots_soa->names[slots_soa->count], node->attribute_array.attributes[node->attribute_array.name_attr_index].value.data, node->attribute_array.attributes[node->attribute_array.name_attr_index].value.length);
        slots_soa->ast_nodes[slots_soa->count] = node;

        slots_soa->count += 1;
    }

    if (node->token.kind == TOKEN_COMPONENT_IMPORT) {
        ImportSOA *imports_soa = components_soa->imports_soa[current_component_index];

        memcpy(imports_soa->names[imports_soa->count], node->attribute_array.attributes[node->attribute_array.name_attr_index].value.data, node->attribute_array.attributes[node->attribute_array.name_attr_index].value.length);
        imports_soa->ast_nodes[imports_soa->count] = node;

        u32 child_count = 0;
        ASTNode *child = node->first_child;
        while (child) {
            child_count += 1;
            child = child->next_sibling;
        }

        InsertSOA *inserts_soa = &(imports_soa->inserts_soa[imports_soa->count]);

        if (child_count) {
            inserts_soa->names = memory_alloc(memory, sizeof(Name) * child_count);
            inserts_soa->ast_nodes = memory_alloc(memory, sizeof(ASTNode *) * child_count);

            child = node->first_child;
            while (child) {

                memcpy(inserts_soa->names[inserts_soa->count], child->attribute_array.attributes[child->attribute_array.name_attr_index].value.data, child->attribute_array.attributes[child->attribute_array.name_attr_index].value.length);
                inserts_soa->ast_nodes[inserts_soa->count] = child;
                inserts_soa->count += 1;

                child = child->next_sibling;
            }
        }

        imports_soa->count += 1;
    }

    if (node->next_sibling) {
        ast_to_soa(memory, node->next_sibling, components_soa);
    }

    if (node->first_child) {
        ast_to_soa(memory, node->first_child, components_soa);
    }
}

void print_components_soa(ComponentSOA *components_soa, u32 i) {
    printf("Component %s\n", components_soa->names[i]);

    if (components_soa->slots_soa[i]) {
        SlotSOA *slots_soa = components_soa->slots_soa[i];
        printf("    has %d slots: ", slots_soa->count);
        u32 j;
        for (j = 0; j < slots_soa->count; j++) {
            printf("%s", slots_soa->names[j]);

            if ((j + 1) != slots_soa->count) {
                printf(", ");
            }
        }
        printf("\n");
    }

    if (components_soa->imports_soa[i]) {
        ImportSOA *imports_soa = components_soa->imports_soa[i];
        printf("    has %d imports: ", imports_soa->count);
        u32 j;
        for (j = 0; j < imports_soa->count; j++) {
            printf("%s", imports_soa->names[j]);
            if (imports_soa->inserts_soa[j].count) {
                InsertSOA *inserts_soa = &(imports_soa->inserts_soa[j]);
                printf("(");
                u32 k;
                for (k = 0; k < inserts_soa->count; k++) {
                    printf("%s", inserts_soa->names[k]);

                    if ((k + 1) != inserts_soa->count) {
                        printf(", ");
                    }
                }
                printf(")");
            }

            if ((j + 1) != imports_soa->count) {
                printf(", ");
            }
        }
        printf("\n");
    }

    printf("\n");
}

AttributeArray get_attributes(Memory *memory, Lexer *lexer) {
    AttributeArray attribute_array = {0};

    u32 cursor = lexer->cursor;

    Attribute *attributes = memory_alloc(memory, sizeof(Attribute));
    *attributes = get_next_attribute(lexer);
    attribute_array.attributes = attributes;
    while (attributes->name.data != NULL) {
        attribute_array.count += 1;

        attributes = memory_alloc(memory, sizeof(Attribute));
        *attributes = get_next_attribute(lexer);
    }

    lexer->cursor = cursor; // restore cursor because main parser loop needs to check for tokens

    return attribute_array;
}

void print_no_attribute_allowed_error(String content, char *error, String file_path, String identifier, u32 line_number, Attribute *attribute) {
    char *content_end = content.data + content.length;
    printf("Invalid attribute for %.*s tag, only x-component tag can have attributes other than the name attribute:\n", (int)identifier.length, identifier.data);
    printf("    file: %.*s\n", (int)file_path.length, file_path.data);
    printf("    line: %d\n", line_number);
    printf("\n");

    // print here

    // u32 i;
    // for (i = 0; i < attributes.count; i++) {
    //     Attribute attribute = attributes.attribute[i];
    //     if (strncmp("name", attribute.name.data, attribute.name.length) == 0 && strlen("name") == attributes.attribute[i].name.length) {
    //         continue;
    //     }

    //     // TODO
    //     // print_tag_attr_error(content, node, attribute);

    //     // *error_count += 1;
    // }
}

Lexer lexer_init(String file_path, String content) {
    Lexer lexer = {0};
    lexer.file_path = file_path;
    lexer.content = content;
    lexer.line_number = 1;

    return lexer;
}

int build_html_components(Memory *memory, Memory *scratch_memory, AssetSOA assets_soa) {
    u32 i;

    ComponentSOA components_soa = {0};

    u32 components_count = 0;

    // Count amount of component across all html files: needed for allocations
    for (i = 0; i < assets_soa.count; i++) {
        if (!is_html_path(assets_soa.locations[i])) {
            continue;
        }

        Lexer lexer = lexer_init(assets_soa.locations[i], assets_soa.contents[i]);

        String opening_tag = {0};
        u32 opening_tag_line_number = 0;

        while (lexer.cursor < lexer.content.length) {
            char *c = peek(&lexer);
            if (strncmp("<x-component-def ", c, strlen("<x-component-def ")) == 0) {
                if (opening_tag.data) {
                    char error[] = "x-component-def can't ever be a child tag, it must always be placed at the top level. "
                                   "If you think you already placed x-component-def at the top level, check that you did't "
                                   "leave open a x-component-def earlier in the file";
                    print_tag_error(lexer.content, error, lexer.file_path, opening_tag, opening_tag_line_number);

                    ASSERT(0); // TODO: remove
                    return -1;
                }

                opening_tag.data = peek(&lexer) + 1;
                opening_tag.length = strlen("x-component-def");
                opening_tag_line_number = lexer.line_number;

                components_count += 1;
            }

            if (strncmp("</x-component-def>", c, strlen("</x-component-def>")) == 0) {
                memset(&opening_tag, 0, sizeof(opening_tag));
            }

            if (*c == '\n') {
                lexer.line_number += 1;
            }

            advance_cursor(&lexer);
        }
    }

    components_soa.names = memory_alloc(memory, sizeof(Name) * components_count);
    components_soa.components = memory_alloc(memory, sizeof(String) * components_count);
    components_soa.placeholders_soa = memory_alloc(memory, sizeof(PlaceholderSOA *) * components_count);
    components_soa.imports_soa = memory_alloc(memory, sizeof(ImportSOA *) * components_count);
    components_soa.slots_soa = memory_alloc(memory, sizeof(SlotSOA *) * components_count);
    components_soa.file_paths = memory_alloc(memory, sizeof(String) * components_count);
    components_soa.contents = memory_alloc(memory, sizeof(String) * components_count);

    u32 *placeholder_amounts = memory_alloc(memory, sizeof(u32) * components_count);

    u32 component_index = 0;
    boolean inside_component = false;

    u32 tag_stack_max = 0;
    u32 tag_stack_count = 0;

    // Count amount of placeholders per component: needed for allocations
    for (i = 0; i < assets_soa.count; i++) {
        if (!is_html_path(assets_soa.locations[i])) {
            continue;
        }

        Lexer lexer = lexer_init(assets_soa.locations[i], assets_soa.contents[i]);

        while (lexer.cursor < lexer.content.length) {
            char *c = peek(&lexer);
            if (strncmp("<x-component-def ", c, strlen("<x-component-def ")) == 0) {
                inside_component = true;
            }

            if (strncmp("</x-component-def>", c, strlen("</x-component-def>")) == 0) {
                component_index += 1;
                inside_component = false;

                if (tag_stack_count > tag_stack_max) {
                    tag_stack_max = tag_stack_count;
                }

                tag_stack_count = 0;
            }

            if (inside_component) {
                if (*c == '%') {
                    char *p = NULL;
                    u32 count = 0;
                    u32 placeholder_name_length = 0;

                    p = c + 1;
                    if (*p != '%') {
                        while (*p != '\0') {
                            if (isspace(*p)) {
                                break;
                            }

                            if (*p == '%') {
                                placeholder_name_length = count;

                                ASSERT(placeholder_name_length < MAX_NAME_LENGTH);

                                placeholder_amounts[component_index] += 1;
                                break;
                            }

                            count += 1;
                            p += 1;
                        }
                    }
                }

                if (strncmp("<x-", c, strlen("<x-")) == 0) {
                    tag_stack_count += 1;
                }

                if (strncmp("</x-", c, strlen("</x-")) == 0) {
                    tag_stack_count += 1;
                }
            }

            if (*c == '\n') {
                lexer.line_number += 1;
            }

            advance_cursor(&lexer);
        }
    }

    for (i = 0; i < components_count; i++) {
        components_soa.placeholders_soa[i] = memory_alloc(memory, sizeof(PlaceholderSOA));
        components_soa.placeholders_soa[i]->names = memory_alloc(memory, sizeof(Name) * placeholder_amounts[i]);
    }

    TagStack tag_stack = {0}; // used to keep track of open tags, ensuring proper closing order.
    tag_stack.data = memory_alloc(memory, sizeof(TagStackElement) * tag_stack_max);

    NodeArray node_array = {0}; // used to temporarily hold nodes that are later used to construct the component AST.
    node_array.data = memory_alloc(memory, sizeof(Node *) * tag_stack_max);

    component_index = 0;

    for (i = 0; i < assets_soa.count; i++) {
        if (!is_html_path(assets_soa.locations[i])) {
            continue;
        }

        Lexer lexer = lexer_init(assets_soa.locations[i], assets_soa.contents[i]);

        Token token = get_next_token(&lexer);
        while (token.kind != TOKEN_EOF) {
            if (token.kind == TOKEN_INVALID) {
                // ASSERT(0);
                goto proceed_to_next_token;
            }

            if (token.kind == TOKEN_PLACEHOLDER) {
                u32 count = components_soa.placeholders_soa[component_index]->count;
                memcpy(components_soa.placeholders_soa[component_index]->names[count], token.identifier.data, token.identifier.length);
                components_soa.placeholders_soa[component_index]->count += 1;

                goto proceed_to_next_token;
            }

            AttributeArray attribute_array = get_attributes(memory, &lexer);

            boolean has_name_attr = false;
            u32 name_attr_index = 0;

            size_t j;
            for (j = 0; j < attribute_array.count; j++) {
                if (strncmp("name", attribute_array.attributes[j].name.data, attribute_array.attributes[j].name.length) == 0) {
                    has_name_attr = true;
                    name_attr_index = j;

                    ASSERT(attribute_array.attributes[j].name.length < MAX_NAME_LENGTH);
                }
            }

            switch (token.kind) {
                case TOKEN_COMPONENT_DEFINITION: {
                    switch (token.tag.type) {
                        case TAG_SELFCLOSING: {
                            char error[] = "x-component-def can NOT be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                            ASSERT(0); // TODO: remove
                            return -1;
                        }
                        case TAG_OPENING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            for (j = 0; j < attribute_array.count; j++) {
                                if (j == name_attr_index)
                                    continue;

                                char error[] = "Invalid attribute for x-component-def tag";
                                print_no_attribute_allowed_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number, &attribute_array.attributes[j]);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            // components_soa.components[components_soa.count].data = token.tag.data + token.tag.length;

                            Node *root = create_node(memory, token, lexer.file_path, lexer.line_number, attribute_array, NULL);

                            tag_stack.data[tag_stack.count].kind = token.kind;
                            tag_stack.data[tag_stack.count].node = root;
                            tag_stack.count += 1;

                            node_array.data[node_array.count] = root;
                            node_array.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_CLOSING: {
                            if (attribute_array.count) {
                                print_tag_error(lexer.content, "Closing tag is not allowed to have attributes", lexer.file_path, token.identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            // components_soa.components[components_soa.count].length = token.tag.data - components_soa.components[components_soa.count].data;

                            ASTNode *ast_root = convert_to_ast(memory, node_array.data, node_array.count);
                            // print_ast_tree(ast_root, 0);

                            u32 error_count = 0;

                            u32 import_count = 0;
                            u32 slot_count = 0;

                            validate_ast_tag_hierarchy(lexer.content, lexer.file_path, ast_root, &import_count, &slot_count, &error_count);

                            if (error_count) {
                                printf("%d errors.\n", error_count);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            memcpy(components_soa.names[components_soa.count], ast_root->attribute_array.attributes[ast_root->attribute_array.name_attr_index].value.data, ast_root->attribute_array.attributes[ast_root->attribute_array.name_attr_index].value.length);
                            components_soa.file_paths[components_soa.count] = lexer.file_path;
                            components_soa.contents[components_soa.count] = lexer.content;

                            if (import_count) {
                                components_soa.imports_soa[components_soa.count] = memory_alloc(memory, sizeof(ImportSOA));
                                components_soa.imports_soa[components_soa.count]->names = memory_alloc(memory, sizeof(Name) * import_count);
                                components_soa.imports_soa[components_soa.count]->ast_nodes = memory_alloc(memory, sizeof(ASTNode *) * import_count);
                                components_soa.imports_soa[components_soa.count]->inserts_soa = memory_alloc(memory, sizeof(InsertSOA) * import_count);
                            }

                            if (slot_count) {
                                components_soa.slots_soa[components_soa.count] = memory_alloc(memory, sizeof(ImportSOA));
                                components_soa.slots_soa[components_soa.count]->names = memory_alloc(memory, sizeof(Name) * slot_count);
                                components_soa.slots_soa[components_soa.count]->ast_nodes = memory_alloc(memory, sizeof(ASTNode *) * slot_count);
                            }

                            ast_to_soa(memory, ast_root, &components_soa);

                            components_soa.count += 1;

                            tag_stack.count = 0;
                            memset(tag_stack.data, 0, sizeof(TagStackElement) * tag_stack_max);

                            node_array.count = 0;
                            memset(node_array.data, 0, sizeof(Node *) * tag_stack_max);

                            component_index += 1;

                            goto proceed_to_next_token;
                        }
                        default: {
                            ASSERT(0);
                            break;
                        }
                    }

                    ASSERT(0);
                    break;
                }
                case TOKEN_COMPONENT_IMPORT: {
                    switch (token.tag.type) {
                        case TAG_SELFCLOSING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            Node *parent = tag_stack.data[tag_stack.count - 1].node;
                            Node *node = create_node(memory, token, lexer.file_path, lexer.line_number, attribute_array, parent);

                            node_array.data[node_array.count] = node;
                            node_array.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_OPENING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            Node *parent = tag_stack.data[tag_stack.count - 1].node;
                            Node *node = create_node(memory, token, lexer.file_path, lexer.line_number, attribute_array, parent);

                            tag_stack.data[tag_stack.count].kind = token.kind;
                            tag_stack.data[tag_stack.count].node = node;
                            tag_stack.count += 1;

                            node_array.data[node_array.count] = node;
                            node_array.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_CLOSING: {
                            if (attribute_array.count) {
                                print_tag_error(lexer.content, "Closing tag is not allowed to have attributes", lexer.file_path, token.identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            TagStackElement last_tag_opened = tag_stack.data[tag_stack.count - 1];
                            if (token.kind != last_tag_opened.kind) {

                                // print something like: tag last_tag_opened.node is child of token.kind and needs to be closed before closing the parent token.kind

                                char error[] = "NOT SURE WHAT GOES HERE";
                                print_tag_error(lexer.content, error, lexer.file_path, last_tag_opened.node->token.identifier, last_tag_opened.node->line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            tag_stack.data[tag_stack.count - 1].kind = 0;
                            tag_stack.data[tag_stack.count - 1].node = NULL;
                            tag_stack.count -= 1;

                            goto proceed_to_next_token;
                        }
                        default: {
                            ASSERT(0);
                            break;
                        }
                    }

                    ASSERT(0);
                    break;
                }
                case TOKEN_SLOT: {
                    switch (token.tag.type) {
                        case TAG_SELFCLOSING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            for (j = 0; j < attribute_array.count; j++) {
                                if (j == name_attr_index)
                                    continue;

                                char error[] = "Invalid attribute for x-slot tag";
                                print_no_attribute_allowed_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number, &attribute_array.attributes[j]);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            Node *parent = tag_stack.data[tag_stack.count - 1].node;
                            Node *node = create_node(memory, token, lexer.file_path, lexer.line_number, attribute_array, parent);

                            node_array.data[node_array.count] = node;
                            node_array.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_OPENING: {
                            char error[] = "x-slot must be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                            ASSERT(0); // TODO: remove
                            return -1;
                        }
                        case TAG_CLOSING: {
                            char error[] = "x-slot must be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                            ASSERT(0); // TODO: remove
                            return -1;
                        }
                        default: {
                            ASSERT(0);
                            break;
                        }
                    }

                    ASSERT(0);
                    break;
                }
                case TOKEN_INSERT: {
                    switch (token.tag.type) {
                        case TAG_SELFCLOSING: {
                            char error[] = "x-insert can NOT be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                            ASSERT(0); // TODO: remove
                            return -1;
                        }
                        case TAG_OPENING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            for (j = 0; j < attribute_array.count; j++) {
                                if (j == name_attr_index)
                                    continue;

                                char error[] = "Invalid attribute for x-insert tag";
                                print_no_attribute_allowed_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number, &attribute_array.attributes[j]);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            Node *parent = tag_stack.data[tag_stack.count - 1].node;
                            Node *node = create_node(memory, token, lexer.file_path, lexer.line_number, attribute_array, parent);

                            tag_stack.data[tag_stack.count].kind = token.kind;
                            tag_stack.data[tag_stack.count].node = node;
                            tag_stack.count += 1;

                            node_array.data[node_array.count] = node;
                            node_array.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_CLOSING: {
                            if (attribute_array.count) {
                                print_tag_error(lexer.content, "Closing tag is not allowed to have attributes", lexer.file_path, token.identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            TagStackElement last_tag_opened = tag_stack.data[tag_stack.count - 1];
                            if (token.kind != last_tag_opened.kind) {

                                char error[] = "NOT SURE WHAT GOES HERE";
                                print_tag_error(lexer.content, error, lexer.file_path, last_tag_opened.node->token.identifier, last_tag_opened.node->line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            tag_stack.data[tag_stack.count - 1].kind = 0;
                            tag_stack.data[tag_stack.count - 1].node = NULL;
                            tag_stack.count -= 1;

                            goto proceed_to_next_token;
                        }
                        default: {
                            ASSERT(0);
                            break;
                        }
                    }

                    ASSERT(0);
                    break;
                }
                case TOKEN_VAL: {
                    switch (token.tag.type) {
                        case TAG_SELFCLOSING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            for (j = 0; j < attribute_array.count; j++) {
                                if (j == name_attr_index)
                                    continue;

                                char error[] = "Invalid attribute for x-val tag";
                                print_no_attribute_allowed_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number, &attribute_array.attributes[j]);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            Node *parent = tag_stack.data[tag_stack.count - 1].node;
                            Node *node = create_node(memory, token, lexer.file_path, lexer.line_number, attribute_array, parent);

                            node_array.data[node_array.count] = node;
                            node_array.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_OPENING: {
                            char error[] = "x-val must be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                            ASSERT(0); // TODO: remove
                            return -1;
                        }
                        case TAG_CLOSING: {
                            char error[] = "x-val must be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                            ASSERT(0); // TODO: remove
                            return -1;
                        }
                        default: {
                            ASSERT(0);
                            break;
                        }
                    }

                    ASSERT(0);
                    break;
                }
                case TOKEN_FOR: {
                    switch (token.tag.type) {
                        case TAG_SELFCLOSING: {
                            char error[] = "x-for can NOT be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                            ASSERT(0); // TODO: remove
                            return -1;
                        }
                        case TAG_OPENING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            for (j = 0; j < attribute_array.count; j++) {
                                if (j == name_attr_index)
                                    continue;

                                char error[] = "Invalid attribute for x-for tag";
                                print_no_attribute_allowed_error(lexer.content, error, lexer.file_path, token.identifier, lexer.line_number, &attribute_array.attributes[j]);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            Node *parent = tag_stack.data[tag_stack.count - 1].node;
                            Node *node = create_node(memory, token, lexer.file_path, lexer.line_number, attribute_array, parent);

                            tag_stack.data[tag_stack.count].kind = token.kind;
                            tag_stack.data[tag_stack.count].node = node;
                            tag_stack.count += 1;

                            node_array.data[node_array.count] = node;
                            node_array.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_CLOSING: {
                            if (attribute_array.count) {
                                print_tag_error(lexer.content, "Closing tag is not allowed to have attributes", lexer.file_path, token.identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            TagStackElement last_tag_opened = tag_stack.data[tag_stack.count - 1];
                            if (token.kind != last_tag_opened.kind) {

                                char error[] = "NOT SURE WHAT GOES HERE";
                                print_tag_error(lexer.content, error, lexer.file_path, last_tag_opened.node->token.identifier, last_tag_opened.node->line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            tag_stack.data[tag_stack.count - 1].kind = 0;
                            tag_stack.data[tag_stack.count - 1].node = NULL;
                            tag_stack.count -= 1;

                            goto proceed_to_next_token;
                        }
                        default: {
                            ASSERT(0);
                            break;
                        }
                    }

                    ASSERT(0);
                    break;
                }
                default: {
                    ASSERT(0);
                    break;
                }
            }

        proceed_to_next_token:;
            token = get_next_token(&lexer);
        }
    }

    //  - check that all component import attributes must exist inside the imported component as %replasables% - TODO this
    //  - warn user if it's using a component import with self-closing tag but component definition for the imported component does contain slots. Same for attribues.

    for (i = 0; i < components_soa.count; i++) {
        print_components_soa(&components_soa, i);
        char *component_name = components_soa.names[i];

        if (components_soa.imports_soa[i]) {
            u32 num_imports = components_soa.imports_soa[i]->count;

            u32 j;
            for (j = 0; j < num_imports; j++) {
                ASTNode *import_ast_node = components_soa.imports_soa[i]->ast_nodes[j];
                char *import_name = components_soa.imports_soa[i]->names[j];
                if (strncmp(component_name, import_name, strlen(import_name)) == 0) {
                    char error[] = "Recursive import. Component is importing itself";
                    printf("%s:\n", error);
                    printf("    file: %.*s\n", (int)components_soa.file_paths[i].length, components_soa.file_paths[i].data);
                    printf("    line: %d\n", components_soa.imports_soa[i]->ast_nodes[j]->line_number);
                    printf("\n");
                    print_tag_attr_error(components_soa.contents[i], components_soa.imports_soa[i]->ast_nodes[j], components_soa.imports_soa[i]->ast_nodes[j]->attribute_array.attributes[components_soa.imports_soa[i]->ast_nodes[j]->attribute_array.name_attr_index]);

                    ASSERT(0);
                }

                // check that imported components_soa actually exist
                u32 imported_component_index = 9999;

                u32 k;
                for (k = 0; k < components_soa.count; k++) {
                    if (strncmp(import_name, components_soa.names[k], strlen(import_name)) == 0) {
                        imported_component_index = k;

                        u32 h;
                        for (h = 0; h < import_ast_node->attribute_array.count; h++) {
                            if (h == import_ast_node->attribute_array.name_attr_index) {
                                continue;
                            }

                            boolean found = false;

                            String import_attr_name = import_ast_node->attribute_array.attributes[h].name;

                            u32 f;
                            for (f = 0; f < components_soa.placeholders_soa[k]->count; f++) {
                                // TODO: make all strncmp to also check for lengths equality like in line bellow
                                if (strncmp(components_soa.placeholders_soa[k]->names[f], import_attr_name.data, import_attr_name.length) == 0 && strlen(components_soa.placeholders_soa[k]->names[f]) == import_attr_name.length) {
                                    found = true;

                                    break;
                                }
                            }

                            // TODO: print error
                            ASSERT(found);
                        }

                        break;
                    }
                }

                if (imported_component_index == 9999) {
                    char error[] = "Component you are trying to import does not exist";
                    printf("%s:\n", error);
                    printf("    file: %.*s\n", (int)components_soa.file_paths[i].length, components_soa.file_paths[i].data);
                    printf("    line: %d\n", components_soa.imports_soa[i]->ast_nodes[j]->line_number);
                    printf("\n");
                    print_tag_attr_error(components_soa.contents[i], components_soa.imports_soa[i]->ast_nodes[j], components_soa.imports_soa[i]->ast_nodes[j]->attribute_array.attributes[components_soa.imports_soa[i]->ast_nodes[j]->attribute_array.name_attr_index]);

                    ASSERT(0);
                }

                // check that imported components_soa do contain inserts as slots
                u32 insert_count = components_soa.imports_soa[i]->inserts_soa[j].count;
                InsertSOA *inserts_soa = &components_soa.imports_soa[i]->inserts_soa[j];
                u32 h;
                for (h = 0; h < insert_count; h++) {
                    char *insert = inserts_soa->names[h];

                    u32 imported_component_slot_index = 9999;

                    if (components_soa.slots_soa[imported_component_index]) {
                        u32 f;
                        for (f = 0; f < components_soa.slots_soa[imported_component_index]->count; f++) {
                            if (strncmp(components_soa.slots_soa[imported_component_index]->names[f], insert, strlen(insert)) == 0) {
                                imported_component_slot_index = f;
                                break;
                            }
                        }
                    }

                    if (imported_component_slot_index == 9999) {
                        ASSERT(0);
                    }
                }
            }
        }
    }

    return 0;
}

size_t render_val(char *template, char *val_name, char *value) {
    size_t r = 0;
    return r;
}
size_t replace_val(char *template, char *val_name, char *value) {
    size_t r = 0;
    return r;
}