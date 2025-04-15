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

#define STACK_CAPACITY 64

typedef enum { TAG_OPENING, TAG_CLOSING, TAG_SELFCLOSING } TagType;

typedef enum {
    TOKEN_EOF = 0,

    TOKEN_COMPONENT_DEFINITION = 1, // x-component-def
    TOKEN_COMPONENT_IMPORT = 2,     // x-component
    TOKEN_SLOT = 3,                 // x-slot
    TOKEN_INSERT = 4,               // x-insert
    TOKEN_VAL = 5,                  // x-val
    TOKEN_FOR = 6,                  // x-for

    TOKEN_PLACEHOLDER, // for example "%product_image%" <img src="%product_image%" /> or "%attributes%" in <button %attributes%>...</button>

    TOKEN_INVALID
} TokenType;

typedef struct {
    TokenType token_type;
    TagType tag_type;
    String tag_identifier;
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
    Attribute *attribute;
    u32 count;
} AttributeArray;

typedef struct ParentPointerNode ParentPointerNode;
struct ParentPointerNode {
    TokenType token_type;
    String tag_identifier;
    u32 line_number;
    AttributeArray attributes;
    ParentPointerNode *parent;
};

typedef struct ChildSiblingNode ChildSiblingNode;
struct ChildSiblingNode {
    TokenType token_type;
    String tag_identifier;
    u32 line_number;
    u32 name_attr_index;
    AttributeArray attributes;
    ChildSiblingNode *first_child;
    ChildSiblingNode *next_sibling;
};

typedef struct {
    ParentPointerNode *data[STACK_CAPACITY];
    u8 count;
} NodesStack;

typedef struct {
    ParentPointerNode *node;
    TokenType token_type;
} TagStackElement;

typedef struct {
    TagStackElement data[STACK_CAPACITY];
    u8 count;
} TagsStack;

#define MAX_COMPONENTS_COUNT 128

#define MAX_TAG_NAME_LENGTH 32
typedef char TagName[MAX_TAG_NAME_LENGTH]; // 2 of these fit nicely in a 64 bytes cache line

typedef struct {
    TagName *names;
    ChildSiblingNode **nodes;
    u32 count;
} LookupSlots;

typedef struct {
    TagName *names;
    ChildSiblingNode **nodes;
    u32 count;
} LookupInserts;

typedef struct {
    TagName *names;
    ChildSiblingNode **nodes;
    LookupInserts *inserts;
    u32 count;
} LookupImports;

typedef struct {
    TagName names[MAX_COMPONENTS_COUNT];
    LookupImports *imports[MAX_COMPONENTS_COUNT];
    LookupSlots *slots[MAX_COMPONENTS_COUNT];
    String file_paths[MAX_COMPONENTS_COUNT];
    String contents[MAX_COMPONENTS_COUNT];
    u32 count;
} LookupComponents;

char *peek(Lexer *lexer) { return &lexer->content.data[lexer->cursor]; }

TokenType get_token_type(String str) {
    char COMPONENT_DEFINITION[] = "x-component-def";
    char COMPONENT_IMPORT[] = "x-component";
    char SLOT[] = "x-slot";
    char INSERT[] = "x-insert";
    char VAL[] = "x-val";
    char FOR[] = "x-for";

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
                char *p = NULL;

                p = c;
                String token_string = {0};
                token_string.data = p;
                while (*p != '\0' && !isspace(*p) && *p != '>') {
                    token_string.length += 1;
                    p += 1;
                }

                // check if tag is self closing
                p = token_string.data;
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

                token.tag_identifier = token_string;
                token.token_type = get_token_type(token.tag_identifier);

                lexer->cursor = (token_string.data + token_string.length) - lexer->content.data;

                return token;
            }

            if (*(c - 1) == '/' && *(c - 2) == '<') { // closing tag
                char *p = NULL;

                p = c;
                String token_string = {0};
                token_string.data = p;
                while (*p != '\0' && !isspace(*p) && *p != '>') {
                    token_string.length += 1;
                    p += 1;
                }

                token.tag_type = TAG_CLOSING;
                token.token_type = get_token_type(token_string);

                lexer->cursor = (token_string.data + token_string.length) - lexer->content.data;

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
                        String token_string = {0};
                        token_string.data = c;
                        token_string.length = (p + 1) - token_string.data;

                        token.tag_identifier = token_string;
                        token.token_type = TOKEN_PLACEHOLDER;

                        lexer->cursor = (token_string.data + token_string.length) - lexer->content.data;

                        return token;
                    }

                    p += 1;
                }
            }
        }

        if (*c == '\n') {
            lexer->line_number += 1;
        }

        lexer->cursor += 1;
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

ParentPointerNode *create_parent_pointer_node(Memory *memory, TokenType token_type, String tag_identifier, String file_path, u32 line_number, AttributeArray attributes, ParentPointerNode *parent) {
    ParentPointerNode *new_node = memory_alloc(memory, sizeof(ParentPointerNode));
    if (new_node == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }
    new_node->token_type = token_type;
    new_node->tag_identifier = tag_identifier;
    new_node->line_number = line_number;
    new_node->attributes = attributes;
    new_node->parent = parent;
    return new_node;
}

ChildSiblingNode *create_child_sibling_node(Memory *memory, TokenType token_type, String tag_identifier, u32 line_number, AttributeArray attributes) {
    ChildSiblingNode *new_node = memory_alloc(memory, sizeof(ChildSiblingNode));
    if (new_node == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }
    new_node->token_type = token_type;
    new_node->tag_identifier = tag_identifier;
    new_node->line_number = line_number;
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

    u32 i;

    // Create corresponding ChildSiblingNodes
    for (i = 0; i < count; i++) {
        cs_nodes[i] = create_child_sibling_node(memory, nodes[i]->token_type, nodes[i]->tag_identifier, nodes[i]->line_number, nodes[i]->attributes);
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
            return "INVALID";
    }
}

void print_child_sibling_tree(ChildSiblingNode *node, int depth) {
    if (node == NULL) {
        return;
    }

    // Print node with indentation
    for (int i = 0; i < depth; i++) {
        printf("    ");
    }

    printf("%s ", token_to_string(node->token_type));
    u32 i;
    for (i = 0; i < node->attributes.count; i++) {
        printf("%.*s=\"%.*s\" ", (int)(node->attributes.attribute[i].name.length), node->attributes.attribute[i].name.data, (int)(node->attributes.attribute[i].value.length), node->attributes.attribute[i].value.data);
    }
    printf("\n");

    // Print first child and siblings
    print_child_sibling_tree(node->first_child, depth + 1);
    print_child_sibling_tree(node->next_sibling, depth);
}

void print_tag_error(String content, char error[], String file_path, String tag_identifier, u32 line_number) {
    char *content_end = content.data + content.length;

    printf("%s:\n", error);
    printf("    file: %.*s\n", (int)file_path.length, file_path.data);
    printf("    line: %d\n", line_number);
    printf("\n");
    char *bol = find_bol(tag_identifier.data, content.data);
    if (line_number > 1) {
        char *prev_bol = find_bol(bol - 2, content.data);
        u32 prev_line_length = (bol - 1) - prev_bol;
        printf("    %d|  %.*s\n", line_number - 1, prev_line_length, prev_bol);
    }
    char *eol = find_eol(bol, content_end);

    printf("    %d|  ", line_number);
    char *p = bol;
    while (p < eol) {
        if (p >= tag_identifier.data && p < tag_identifier.data + tag_identifier.length) {
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

void print_tag_attr_error(String content, ChildSiblingNode *node, Attribute attribute) {
    char *content_end = content.data + content.length;

    char *bol = find_bol(node->tag_identifier.data, content.data);
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

void tree_traverse_error_checking(String content, String file_path, ChildSiblingNode *node, u32 *import_count, u32 *slot_count, u32 *error_count) {
    switch (node->token_type) {
        case TOKEN_COMPONENT_DEFINITION: {
            ChildSiblingNode *child = node->first_child;
            while (child) {
                if (child->token_type == TOKEN_INSERT) {
                    char error[] = "x-insert can not be direct child of x-component-def, it can only be direct child of x-component";

                    print_tag_error(content, error, file_path, child->tag_identifier, child->line_number);
                    *error_count += 1;
                }

                child = child->next_sibling;
            }

            break;
        }
        case TOKEN_COMPONENT_IMPORT: {
            ChildSiblingNode *child = node->first_child;
            while (child) {
                if (child->token_type != TOKEN_INSERT) {
                    char error[] = "x-component can only have direct child x-insert";

                    print_tag_error(content, error, file_path, child->tag_identifier, child->line_number);
                    *error_count += 1;
                }

                child = child->next_sibling;
            }

            *import_count += 1;

            break;
        }
        case TOKEN_SLOT: {
            ChildSiblingNode *child = node->first_child;
            if (child) {
                char error[] = "x-slot can't have any children";

                print_tag_error(content, error, file_path, node->tag_identifier, node->line_number);
                *error_count += 1;
            }

            *slot_count += 1;

            break;
        }
        case TOKEN_INSERT: {
            ChildSiblingNode *child = node->first_child;
            while (child) {
                if (child->token_type == TOKEN_INSERT) {
                    char error[] = "x-insert can not be direct child of x-insert, it can only be direct child of x-component";

                    print_tag_error(content, error, file_path, child->tag_identifier, child->line_number);
                    *error_count += 1;
                }

                child = child->next_sibling;
            }

            break;
        }
        case TOKEN_VAL: {
            ChildSiblingNode *child = node->first_child;
            if (child) {
                char error[] = "x-val can't have any children";

                print_tag_error(content, error, file_path, node->tag_identifier, node->line_number);
                *error_count += 1;
            }

            break;
        }
        case TOKEN_FOR: {
            ChildSiblingNode *child = node->first_child;
            while (child) {
                if (child->token_type == TOKEN_INSERT) {
                    char error[] = "x-insert can not be direct child of x-for, it can only be direct child of x-component";

                    print_tag_error(content, error, file_path, child->tag_identifier, child->line_number);
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
        tree_traverse_error_checking(content, file_path, node->next_sibling, import_count, slot_count, error_count);
    }

    if (node->first_child) {
        tree_traverse_error_checking(content, file_path, node->first_child, import_count, slot_count, error_count);
    }
}

void tree_traverse_make_lookup(Memory *memory, ChildSiblingNode *node, LookupComponents *lookup_components) {
    u32 current_component_index = lookup_components->count;

    if (node->token_type == TOKEN_SLOT) {
        LookupSlots *slots = lookup_components->slots[current_component_index];

        memcpy(slots->names[slots->count], node->attributes.attribute[node->name_attr_index].value.data, node->attributes.attribute[node->name_attr_index].value.length);
        slots->nodes[slots->count] = node;

        slots->count += 1;
    }

    if (node->token_type == TOKEN_COMPONENT_IMPORT) {
        LookupImports *imports = lookup_components->imports[current_component_index];

        memcpy(imports->names[imports->count], node->attributes.attribute[node->name_attr_index].value.data, node->attributes.attribute[node->name_attr_index].value.length);
        imports->nodes[imports->count] = node;

        u32 child_count = 0;
        ChildSiblingNode *child = node->first_child;
        while (child) {
            child_count += 1;
            child = child->next_sibling;
        }

        LookupInserts *inserts = &(imports->inserts[imports->count]);

        if (child_count) {
            inserts->names = memory_alloc(memory, sizeof(TagName) * child_count);
            inserts->nodes = memory_alloc(memory, sizeof(ChildSiblingNode *) * child_count);

            child = node->first_child;
            while (child) {

                memcpy(inserts->names[inserts->count], child->attributes.attribute[child->name_attr_index].value.data, child->attributes.attribute[child->name_attr_index].value.length);
                inserts->nodes[inserts->count] = child;
                inserts->count += 1;

                child = child->next_sibling;
            }
        }

        imports->count += 1;
    }

    if (node->next_sibling) {
        tree_traverse_make_lookup(memory, node->next_sibling, lookup_components);
    }

    if (node->first_child) {
        tree_traverse_make_lookup(memory, node->first_child, lookup_components);
    }
}

void print_component_lookup(LookupComponents *lookup_components, u32 i) {
    printf("Component %s\n", lookup_components->names[i]);

    if (lookup_components->slots[i]) {
        LookupSlots *slots = lookup_components->slots[i];
        printf("    has %d slots: ", slots->count);
        u32 j;
        for (j = 0; j < slots->count; j++) {
            printf("%s", slots->names[j]);

            if ((j + 1) != slots->count) {
                printf(", ");
            }
        }
        printf("\n");
    }

    if (lookup_components->imports[i]) {
        LookupImports *imports = lookup_components->imports[i];
        printf("    has %d imports: ", imports->count);
        u32 j;
        for (j = 0; j < imports->count; j++) {
            printf("%s", imports->names[j]);
            if (imports->inserts[j].count) {
                LookupInserts *inserts = &(imports->inserts[j]);
                printf("(");
                u32 k;
                for (k = 0; k < inserts->count; k++) {
                    printf("%s", inserts->names[k]);

                    if ((k + 1) != inserts->count) {
                        printf(", ");
                    }
                }
                printf(")");
            }

            if ((j + 1) != imports->count) {
                printf(", ");
            }
        }
        printf("\n");
    }

    printf("\n");
}

AttributeArray get_attributes(Memory *memory, Lexer *lexer) {
    AttributeArray attributes = {0};

    Attribute *attribute = memory_alloc(memory, sizeof(Attribute)); //
    *attribute = get_next_attribute(lexer);
    attributes.attribute = attribute;
    while (attribute->name.data != NULL) {
        attributes.count += 1;

        attribute = memory_alloc(memory, sizeof(Attribute));
        *attribute = get_next_attribute(lexer);
    }

    return attributes;
}

void print_no_attribute_allowed_error(String content, char *error, String file_path, String tag_identifier, u32 line_number, Attribute *attribute) {
    char *content_end = content.data + content.length;
    printf("Invalid attribute for %.*s tag, only x-component tag can have attributes other than the name attribute:\n", (int)tag_identifier.length, tag_identifier.data);
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

int build_html_components(Memory *memory, Memory *scratch_memory, AssetList asset_list) {
    LookupComponents lookup_components = {0};

    u32 i;
    for (i = 0; i < asset_list.count; i++) {
        if (!is_html_path(asset_list.asset_list[i])) {
            continue;
        }

        Lexer lexer = {0};

        // init lexer
        lexer.file_path = asset_list.asset_list[i];
        lexer.content = asset_list.asset_list_content[i];
        lexer.line_number = 1;

        TagsStack tags_stack = {0};
        NodesStack nodes_stack = {0};

        boolean processing_component = false;

        ParentPointerNode *root = NULL;

        Token token = get_next_token(&lexer);
        while (token.token_type != TOKEN_EOF) {
            if (token.token_type == TOKEN_PLACEHOLDER) {

                // ASSERT(0);
                goto proceed_to_next_token;
            }

            if (token.token_type == TOKEN_INVALID) {
                // ASSERT(0);
                goto proceed_to_next_token;
            }

            AttributeArray attributes = get_attributes(memory, &lexer);

            boolean has_name_attr = false;
            u32 name_attr_index = 0;

            size_t j;
            for (j = 0; j < attributes.count; j++) {
                if (strncmp("name", attributes.attribute[j].name.data, attributes.attribute[j].name.length) == 0) {
                    has_name_attr = true;
                    name_attr_index = j;

                    ASSERT(attributes.attribute[j].name.length < MAX_TAG_NAME_LENGTH);
                }
            }

            switch (token.token_type) {
                case TOKEN_COMPONENT_DEFINITION: {
                    switch (token.tag_type) {
                        case TAG_SELFCLOSING: {
                            char error[] = "x-component-def can NOT be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                            ASSERT(0); // TODO: remove
                            return -1;
                        }
                        case TAG_OPENING: {
                            if (root) {
                                char error[] = "x-component-def can't ever be a child tag, it must always be placed at the top level. "
                                               "If you think you already placed x-component-def at the top level, check that you did't "
                                               "leave open a x-component-def earlier in the file";

                                print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            for (j = 0; j < attributes.count; j++) {
                                if (j == name_attr_index)
                                    continue;

                                char error[] = "Invalid attribute for x-component-def tag";
                                print_no_attribute_allowed_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number, &attributes.attribute[j]);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            root = create_parent_pointer_node(memory, token.token_type, token.tag_identifier, lexer.file_path, lexer.line_number, attributes, NULL);

                            tags_stack.data[tags_stack.count].token_type = token.token_type;
                            tags_stack.data[tags_stack.count].node = root;
                            tags_stack.count += 1;

                            nodes_stack.data[nodes_stack.count] = root;
                            nodes_stack.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_CLOSING: {
                            if (attributes.count) {
                                print_tag_error(lexer.content, "Closing tag is not allowed to have attributes", lexer.file_path, token.tag_identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            if (nodes_stack.count) {
                                ChildSiblingNode *cs_root = convert_to_child_sibling(memory, nodes_stack.data, nodes_stack.count);
                                // print_child_sibling_tree(cs_root, 0);

                                u32 error_count = 0;

                                u32 import_count = 0;
                                u32 slot_count = 0;

                                tree_traverse_error_checking(lexer.content, lexer.file_path, cs_root, &import_count, &slot_count, &error_count);

                                if (error_count) {
                                    printf("%d errors.\n", error_count);

                                    ASSERT(0); // TODO: remove
                                    return -1;
                                }

                                if (lookup_components.count > MAX_COMPONENTS_COUNT) {
                                    printf("no more space for components");
                                    ASSERT(0);
                                }

                                memcpy(lookup_components.names[lookup_components.count], cs_root->attributes.attribute[cs_root->name_attr_index].value.data, cs_root->attributes.attribute[cs_root->name_attr_index].value.length);
                                lookup_components.file_paths[lookup_components.count] = lexer.file_path;
                                lookup_components.contents[lookup_components.count] = lexer.content;

                                if (import_count) {
                                    lookup_components.imports[lookup_components.count] = memory_alloc(memory, sizeof(LookupImports));
                                    lookup_components.imports[lookup_components.count]->names = memory_alloc(memory, sizeof(TagName) * import_count);
                                    lookup_components.imports[lookup_components.count]->nodes = memory_alloc(memory, sizeof(ChildSiblingNode *) * import_count);
                                    lookup_components.imports[lookup_components.count]->inserts = memory_alloc(memory, sizeof(LookupInserts) * import_count);
                                }

                                if (slot_count) {
                                    lookup_components.slots[lookup_components.count] = memory_alloc(memory, sizeof(LookupImports));
                                    lookup_components.slots[lookup_components.count]->names = memory_alloc(memory, sizeof(TagName) * slot_count);
                                    lookup_components.slots[lookup_components.count]->nodes = memory_alloc(memory, sizeof(ChildSiblingNode *) * slot_count);
                                }

                                tree_traverse_make_lookup(memory, cs_root, &lookup_components);

                                lookup_components.count += 1;
                            }

                            root = NULL;
                            memset(&tags_stack, 0, sizeof(TagsStack));
                            memset(&nodes_stack, 0, sizeof(NodesStack));

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
                    switch (token.tag_type) {
                        case TAG_SELFCLOSING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            ParentPointerNode *parent = tags_stack.data[tags_stack.count - 1].node;
                            ParentPointerNode *node = create_parent_pointer_node(memory, token.token_type, token.tag_identifier, lexer.file_path, lexer.line_number, attributes, parent);

                            nodes_stack.data[nodes_stack.count] = node;
                            nodes_stack.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_OPENING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            ParentPointerNode *parent = tags_stack.data[tags_stack.count - 1].node;
                            ParentPointerNode *node = create_parent_pointer_node(memory, token.token_type, token.tag_identifier, lexer.file_path, lexer.line_number, attributes, parent);

                            tags_stack.data[tags_stack.count].token_type = token.token_type;
                            tags_stack.data[tags_stack.count].node = node;
                            tags_stack.count += 1;

                            nodes_stack.data[nodes_stack.count] = node;
                            nodes_stack.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_CLOSING: {
                            if (attributes.count) {
                                print_tag_error(lexer.content, "Closing tag is not allowed to have attributes", lexer.file_path, token.tag_identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            TagStackElement last_tag_opened = tags_stack.data[tags_stack.count - 1];
                            if (token.token_type != last_tag_opened.token_type) {

                                // print something like: tag last_tag_opened.node is child of token.token_type and needs to be closed before closing the parent token.token_type

                                char error[] = "NOT SURE WHAT GOES HERE";
                                print_tag_error(lexer.content, error, lexer.file_path, last_tag_opened.node->tag_identifier, last_tag_opened.node->line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            tags_stack.data[tags_stack.count - 1].token_type = 0;
                            tags_stack.data[tags_stack.count - 1].node = NULL;
                            tags_stack.count -= 1;

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
                    switch (token.tag_type) {
                        case TAG_SELFCLOSING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            for (j = 0; j < attributes.count; j++) {
                                if (j == name_attr_index)
                                    continue;

                                char error[] = "Invalid attribute for x-slot tag";
                                print_no_attribute_allowed_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number, &attributes.attribute[j]);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            ParentPointerNode *parent = tags_stack.data[tags_stack.count - 1].node;
                            ParentPointerNode *node = create_parent_pointer_node(memory, token.token_type, token.tag_identifier, lexer.file_path, lexer.line_number, attributes, parent);

                            nodes_stack.data[nodes_stack.count] = node;
                            nodes_stack.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_OPENING: {
                            char error[] = "x-slot must be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                            ASSERT(0); // TODO: remove
                            return -1;
                        }
                        case TAG_CLOSING: {
                            char error[] = "x-slot must be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

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
                    switch (token.tag_type) {
                        case TAG_SELFCLOSING: {
                            char error[] = "x-insert can NOT be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                            ASSERT(0); // TODO: remove
                            return -1;
                        }
                        case TAG_OPENING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            for (j = 0; j < attributes.count; j++) {
                                if (j == name_attr_index)
                                    continue;

                                char error[] = "Invalid attribute for x-insert tag";
                                print_no_attribute_allowed_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number, &attributes.attribute[j]);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            ParentPointerNode *parent = tags_stack.data[tags_stack.count - 1].node;
                            ParentPointerNode *node = create_parent_pointer_node(memory, token.token_type, token.tag_identifier, lexer.file_path, lexer.line_number, attributes, parent);

                            tags_stack.data[tags_stack.count].token_type = token.token_type;
                            tags_stack.data[tags_stack.count].node = node;
                            tags_stack.count += 1;

                            nodes_stack.data[nodes_stack.count] = node;
                            nodes_stack.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_CLOSING: {
                            if (attributes.count) {
                                print_tag_error(lexer.content, "Closing tag is not allowed to have attributes", lexer.file_path, token.tag_identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            TagStackElement last_tag_opened = tags_stack.data[tags_stack.count - 1];
                            if (token.token_type != last_tag_opened.token_type) {

                                char error[] = "NOT SURE WHAT GOES HERE";
                                print_tag_error(lexer.content, error, lexer.file_path, last_tag_opened.node->tag_identifier, last_tag_opened.node->line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            tags_stack.data[tags_stack.count - 1].token_type = 0;
                            tags_stack.data[tags_stack.count - 1].node = NULL;
                            tags_stack.count -= 1;

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
                    switch (token.tag_type) {
                        case TAG_SELFCLOSING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            for (j = 0; j < attributes.count; j++) {
                                if (j == name_attr_index)
                                    continue;

                                char error[] = "Invalid attribute for x-val tag";
                                print_no_attribute_allowed_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number, &attributes.attribute[j]);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            ParentPointerNode *parent = tags_stack.data[tags_stack.count - 1].node;
                            ParentPointerNode *node = create_parent_pointer_node(memory, token.token_type, token.tag_identifier, lexer.file_path, lexer.line_number, attributes, parent);

                            nodes_stack.data[nodes_stack.count] = node;
                            nodes_stack.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_OPENING: {
                            char error[] = "x-val must be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                            ASSERT(0); // TODO: remove
                            return -1;
                        }
                        case TAG_CLOSING: {
                            char error[] = "x-val must be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

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
                    switch (token.tag_type) {
                        case TAG_SELFCLOSING: {
                            char error[] = "x-for can NOT be self-closing";
                            print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                            ASSERT(0); // TODO: remove
                            return -1;
                        }
                        case TAG_OPENING: {
                            if (!has_name_attr) {
                                char error[] = "All tags must contain a name attribute";
                                print_tag_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            for (j = 0; j < attributes.count; j++) {
                                if (j == name_attr_index)
                                    continue;

                                char error[] = "Invalid attribute for x-for tag";
                                print_no_attribute_allowed_error(lexer.content, error, lexer.file_path, token.tag_identifier, lexer.line_number, &attributes.attribute[j]);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            ParentPointerNode *parent = tags_stack.data[tags_stack.count - 1].node;
                            ParentPointerNode *node = create_parent_pointer_node(memory, token.token_type, token.tag_identifier, lexer.file_path, lexer.line_number, attributes, parent);

                            tags_stack.data[tags_stack.count].token_type = token.token_type;
                            tags_stack.data[tags_stack.count].node = node;
                            tags_stack.count += 1;

                            nodes_stack.data[nodes_stack.count] = node;
                            nodes_stack.count += 1;

                            goto proceed_to_next_token;
                        }
                        case TAG_CLOSING: {
                            if (attributes.count) {
                                print_tag_error(lexer.content, "Closing tag is not allowed to have attributes", lexer.file_path, token.tag_identifier, lexer.line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            TagStackElement last_tag_opened = tags_stack.data[tags_stack.count - 1];
                            if (token.token_type != last_tag_opened.token_type) {

                                char error[] = "NOT SURE WHAT GOES HERE";
                                print_tag_error(lexer.content, error, lexer.file_path, last_tag_opened.node->tag_identifier, last_tag_opened.node->line_number);

                                ASSERT(0); // TODO: remove
                                return -1;
                            }

                            tags_stack.data[tags_stack.count - 1].token_type = 0;
                            tags_stack.data[tags_stack.count - 1].node = NULL;
                            tags_stack.count -= 1;

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

        if (root) {
            char error[] = "Forgot to close x-component-def";

            printf("%s:\n", error);
            printf("    file: %.*s\n", (int)lexer.file_path.length, lexer.file_path.data);
            printf("    line: %d\n", lexer.line_number);
            printf("\n");

            ASSERT(0); // TODO: remove
            return -1;
        }
    }

    //  - all component import attributes must exist inside the imported component as %replasables%
    //  - warn user if it's using a component import with self-closing tag but component definition for the imported component does contain slots. Same for attribues.

    for (i = 0; i < lookup_components.count; i++) {
        print_component_lookup(&lookup_components, i);
        char *component_name = lookup_components.names[i];

        if (lookup_components.imports[i]) {
            u32 num_imports = lookup_components.imports[i]->count;

            u32 j;
            for (j = 0; j < num_imports; j++) {
                char *import = lookup_components.imports[i]->names[j];
                if (strncmp(component_name, import, strlen(import)) == 0) {
                    char error[] = "Recursive import. Component is importing itself";
                    printf("%s:\n", error);
                    printf("    file: %.*s\n", (int)lookup_components.file_paths[i].length, lookup_components.file_paths[i].data);
                    printf("    line: %d\n", lookup_components.imports[i]->nodes[j]->line_number);
                    printf("\n");
                    print_tag_attr_error(lookup_components.contents[i], lookup_components.imports[i]->nodes[j], lookup_components.imports[i]->nodes[j]->attributes.attribute[lookup_components.imports[i]->nodes[j]->name_attr_index]);

                    ASSERT(0);
                }

                // check that imported components actually exist
                u32 imported_component_index = 9999;

                u32 k;
                for (k = 0; k < lookup_components.count; k++) {
                    if (strncmp(lookup_components.names[k], import, strlen(import)) == 0) {
                        imported_component_index = k;
                        break;
                    }
                }

                if (imported_component_index == 9999) {
                    char error[] = "Component you are trying to import does not exist";
                    printf("%s:\n", error);
                    printf("    file: %.*s\n", (int)lookup_components.file_paths[i].length, lookup_components.file_paths[i].data);
                    printf("    line: %d\n", lookup_components.imports[i]->nodes[j]->line_number);
                    printf("\n");
                    print_tag_attr_error(lookup_components.contents[i], lookup_components.imports[i]->nodes[j], lookup_components.imports[i]->nodes[j]->attributes.attribute[lookup_components.imports[i]->nodes[j]->name_attr_index]);

                    ASSERT(0);
                }

                // check that imported components do contain inserts as slots
                u32 insert_count = lookup_components.imports[i]->inserts[j].count;
                LookupInserts *inserts = &lookup_components.imports[i]->inserts[j];
                u32 h;
                for (h = 0; h < insert_count; h++) {
                    char *insert = inserts->names[h];

                    u32 imported_component_slot_index = 9999;

                    if (lookup_components.slots[imported_component_index]) {
                        u32 f;
                        for (f = 0; f < lookup_components.slots[imported_component_index]->count; f++) {
                            if (strncmp(lookup_components.slots[imported_component_index]->names[f], insert, strlen(insert)) == 0) {
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