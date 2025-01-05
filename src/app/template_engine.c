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

#define COMPONENT_DEFINITION_OPENING_TAG__START "<x-component-def "
#define COMPONENT_DEFINITION_OPENING_TAG__END "\">"
#define COMPONENT_IMPORT_OPENING_TAG__START "<x-component "
#define OPENING_COMPONENT_IMPORT_TAG_SELF_CLOSING_END " />"
#define COMPONENT_IMPORT_OPENING_TAG__END "\">"
#define COMPONENT_DEFINITION_CLOSING_TAG "</x-component-def>"
#define COMPONENT_IMPORT_CLOSING_TAG "</x-component>"
#define INSERT_OPENING_TAG "<x-insert>"
#define INSERT_CLOSING_TAG "</x-insert>"
#define SLOT_MARK "%x-slot%"
#define SELF_CLOSING_TAG "/>"
#define CLOSING_BRACKET ">"

#define FOR_OPENING_TAG__START "<x-for name=\""
#define FOR_OPENING_TAG__END "\">"
#define FOR_CLOSING_TAG "</x-for>"

#define VAL_OPENING_TAG__START "<x-val name=\""
#define VAL_SELF_CLOSING_TAG__END "\" />"

#define NAME_ATTRIBUTE_PATTERN "\\s+name\\s*=\\s*\\\"[^\\\"]*\\\""
#define ATTRIBUTE_KEY_PATTERN "^[[:space:]]*([[:alnum:]_-]+)[[:space:]]*=" /** C regex compatible */
#define ATTRIBUTE_VALUE_PATTERN "\\\"[^\\\"]*\\\""
#define ATTRIBUTE_PATTERN "\\s\\w+(?:[-]\\w+)*\\s*= *\"[^\"]*\""
#define ATTRIBUTE_PATTERN_2 "[ \t]\\w+(-\\w+)*[ \t]*=[ \t]*\"[^\"]*\""
#define INJECT_ATTRIBUTE_PATTERN "\\sinject:\\w+(?:[-]\\w+)*\\s*= *\"[^\"]*\""
#define INHERIT_ATTRIBUTE_PATTERN "\\sinherit:\\w+(?:[-]\\w+)*"
#define SELF_CLOSING_TAG_PATTERN "/>"
#define COMPONENT_IMPORT_CLOSING_TAG_PATTERN "<\\/x-component\\s*>"
#define PLACEHOLDER_PATTERN "%[a-zA-Z0-9_-]+%"
#define COMPONENT_IMPORT_TAG_PATTERN "<x-component "
#define SLOT_TAG_PATTERN "<x-slot "
#define INSERT_TAG_PATTERN "<x-insert "

typedef CharsBlock TagLocation;

typedef struct {
    String block;
    String opening_tag;
} HTMLBlock;

void clear_leftovers(char *ptr) {
    if (*ptr == '\0') {
        ptr++;
    }

    while (*ptr) {
        size_t str_len = strlen(ptr);
        memset(ptr, 0, str_len);
        ptr += str_len + 1;
    }
}

char *string_skip_whitespaces(char *text) {
    while (isspace(*text)) {
        text++;
    }

    return text;
}

char *find_component_declaration(char *string) {
    char *ptr = strstr(string, COMPONENT_DEFINITION_OPENING_TAG__START);
    return ptr;
}

String match_pattern(const char *pattern, const char *text, size_t text_length) {
    regex_t regex;
    regmatch_t match[1];
    String result = {0};

    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        printf("Could not compile regex: %s\n", pattern);
        return result;
    }

    if (regexec(&regex, text, 1, match, 0) == 0) {
        if (match[0].rm_eo <= (regoff_t)text_length) {
            result.start_addr = (char *)(text + match[0].rm_so);
            result.length = match[0].rm_eo - match[0].rm_so;
        }
    }

    regfree(&regex);
    return result;
}

String find_attribute(String opening_tag, const char *attr_name) {
    char *p = NULL;

    char *end = opening_tag.start_addr + opening_tag.length;
    while (true) {
        String attribute = match_pattern(ATTRIBUTE_PATTERN_2, opening_tag.start_addr, opening_tag.length);
        if (!attribute.start_addr) {
            String value = {0};
            return value;
        }

        String key_match = match_pattern(ATTRIBUTE_KEY_PATTERN, attribute.start_addr, attribute.length);
        char *key_start = string_skip_whitespaces(key_match.start_addr);

        p = key_start;
        while (!isspace(*p) && *p != '=') {
            p++;
        }

        char *key_end = p;

        size_t key_length = key_end - key_start;

        String key = {0};
        if (strncmp(key_start, attr_name, strlen(attr_name)) == 0 && key_length == strlen(attr_name)) {
            String value_match = match_pattern(ATTRIBUTE_VALUE_PATTERN, attribute.start_addr, attribute.length);
            char *value_start = value_match.start_addr + 1 /** " */;

            p = value_start;
            while (*p != '"') {
                p++;
            }

            char *value_end = p;

            size_t value_length = value_end - value_start;

            String value = {0};
            value.start_addr = value_start;
            value.length = value_length;

            return value;
        }

        opening_tag.start_addr = attribute.start_addr + attribute.length;
        opening_tag.length = end - opening_tag.start_addr;
    }

    ASSERT(0);
}

HTMLBlock find_html_block(char *text, size_t text_length, const char *tag_name) {
    char opening_tag[50];
    memset(opening_tag, 0, sizeof(opening_tag));
    sprintf(opening_tag, "<%s ", tag_name);

    char closing_tag[50];
    memset(closing_tag, 0, sizeof(closing_tag));
    sprintf(closing_tag, "</%s>", tag_name);

    HTMLBlock html_block = {0};

    html_block.block = match_pattern(opening_tag, text, text_length);
    html_block.opening_tag.start_addr = html_block.block.start_addr;

    if (!html_block.block.start_addr) {
        return html_block;
    }

    char *ptr = html_block.block.start_addr;
    ptr++;

    uint8_t inner = 0;

    uint8_t nested = 0;
    while (*ptr != '\0') {
        if (*ptr == '"') {
            ptr++;
            while (*ptr != '"') {
                ptr++;
            }
        }

        if (strncmp(ptr, SELF_CLOSING_TAG, strlen(SELF_CLOSING_TAG)) == 0) {
            ptr += strlen(SELF_CLOSING_TAG);
            html_block.block.length = ptr - html_block.block.start_addr;
            html_block.opening_tag.length = html_block.block.length;

            return html_block;
        }

        if (strncmp(ptr, CLOSING_BRACKET, strlen(CLOSING_BRACKET)) == 0) {
            while (*ptr != '\0') {
                if (strncmp(ptr, opening_tag, strlen(opening_tag)) == 0) {
                    ptr += strlen(opening_tag);
                    while (*ptr != '\0') {
                        if (strncmp(ptr, SELF_CLOSING_TAG, strlen(SELF_CLOSING_TAG)) == 0) {
                            break;
                        }

                        if (strncmp(ptr, CLOSING_BRACKET, strlen(CLOSING_BRACKET)) == 0) {
                            nested++;
                            break;
                        }

                        ptr++;
                    }
                }

                if (strncmp(ptr, closing_tag, strlen(closing_tag)) == 0) {
                    ptr += strlen(closing_tag);
                    if (nested == 0) {
                        html_block.block.length = ptr - html_block.block.start_addr;

                        String closing_bracket = match_pattern(CLOSING_BRACKET, html_block.block.start_addr, html_block.block.length);
                        char *passed_bracket = closing_bracket.start_addr + strlen(CLOSING_BRACKET);

                        html_block.opening_tag.length = passed_bracket - html_block.block.start_addr;

                        goto exit;
                    } else {
                        nested--;
                    }
                }

                ptr++;
            }
        }

        ptr++;
    }

exit:
    return html_block;
}

Dict get_tag_attributes(Memory *memory, String opening_tag) {
    char *tmp = NULL;

    char *end = opening_tag.start_addr + opening_tag.length;

    Dict attributes = {0};
    char *p = NULL;

    attributes.start_addr = p = (char *)memory_in_use(memory);
    while (true) {
        String attribute = match_pattern(ATTRIBUTE_PATTERN_2, opening_tag.start_addr, opening_tag.length);
        if (!attribute.start_addr) {
            break;
        }

        String key_match = match_pattern(ATTRIBUTE_KEY_PATTERN, attribute.start_addr, attribute.length);
        char *key_start = string_skip_whitespaces(key_match.start_addr);

        tmp = key_start;
        while (!isspace(*tmp) && *tmp != '=') {
            tmp++;
        }

        char *key_end = tmp;

        size_t key_length = key_end - key_start;

        String key = {0};
        if (strncmp(key_start, "name", strlen("name")) == 0 && key_length == strlen("name")) {
            memcpy(p, key_start, key_length);
            p += strlen(p) + 1;
        } else {
            p[0] = '%';
            memcpy(&p[1], key_start, key_length);
            p[strlen(p)] = '%';
            p += strlen(p) + 1;
        }

        String value_match = match_pattern(ATTRIBUTE_VALUE_PATTERN, attribute.start_addr, attribute.length);
        char *value_start = value_match.start_addr + 1 /** " */;

        tmp = value_start;
        while (*tmp != '"') {
            tmp++;
        }

        char *value_end = tmp;

        size_t value_length = value_end - value_start;

        memcpy(p, value_start, value_length);
        p += strlen(p) + 1;

        opening_tag.start_addr = attribute.start_addr + attribute.length;
        opening_tag.length = end - opening_tag.start_addr;
    }

    attributes.end_addr = p - 1;
    memory_out_of_use(memory, p);

    return attributes;
}

Dict get_slots(Memory *memory, HTMLBlock component_import) {
    char *end = component_import.block.start_addr + component_import.block.length;

    Dict slots = {0};
    char *p = NULL;

    slots.start_addr = p = (char *)memory_in_use(memory);
    while (true) {
        HTMLBlock insert = find_html_block(component_import.block.start_addr, component_import.block.length, "x-insert");
        if (!insert.block.start_addr) {
            break;
        }

        String key = find_attribute(insert.opening_tag, "name");
        memcpy(p, key.start_addr, key.length);
        p += strlen(p) + 1;

        String value = {0};
        value.start_addr = insert.opening_tag.start_addr + insert.opening_tag.length;
        char *value_end = (insert.block.start_addr + insert.block.length) - strlen("</x-insert>");
        value.length = value_end - value.start_addr;
        memcpy(p, value.start_addr, value.length);
        p += strlen(p) + 1;

        component_import.block.start_addr = insert.block.start_addr + insert.block.length;
        component_import.block.length = end - component_import.block.start_addr;
    }

    slots.end_addr = p - 1;
    memory_out_of_use(memory, p);

    return slots;
}

String find_name_attribute_value(String opening_tag) {
    String name_attr = match_pattern(NAME_ATTRIBUTE_PATTERN, opening_tag.start_addr, opening_tag.length);
    ASSERT(name_attr.start_addr);

    String name = match_pattern(ATTRIBUTE_VALUE_PATTERN, name_attr.start_addr, name_attr.length);
    ASSERT(name.start_addr);

    /** exclude " at the beginning and end */
    name.start_addr += 1;
    name.length -= 2;

    return name;
}

Dict build_html_components(Memory *memory, Memory *scratch_memory, Dict assets) {
    uint8_t size = get_dictionary_size(assets);
    uint8_t i;

    /* A Component is an HTML snippet that may include references to other HTML snippets, i.e., it is composable */
    Dict html_raw_components = {0};
    char *p = NULL;

    html_raw_components.start_addr = p = (char *)memory_in_use(memory);
    for (i = 0; i < size; i++) {
        KV asset = get_key_value(assets, i);

        /* ONLY interested in html files */
        if (strncmp(asset.k + strlen(asset.k) - strlen(".html"), ".html", strlen(".html")) != 0) {
            continue;
        }

        /** A .html file may contain multiple Components */
        char *content = asset.v;
        while ((content = find_component_declaration(content)) != NULL) { /** Process Components inside .html file. */
            /** Found component in file content */
            HTMLBlock component = find_html_block(content, strlen(content), "x-component-def");
            String component_name = find_attribute(component.opening_tag, "name");

            strncpy(p, component_name.start_addr, component_name.length);
            p += strlen(p) + 1;

            char *component_content = component.opening_tag.start_addr + component.opening_tag.length;
            char *component_content_end = (component.block.start_addr + component.block.length) - strlen(COMPONENT_DEFINITION_CLOSING_TAG);
            size_t component_content_length = component_content_end - component_content;

            size_t minified_html_length = html_minify(p, component_content, component_content_length);
            p += minified_html_length;

            content++;
        }
    }

    html_raw_components.end_addr = p - 1;
    memory_out_of_use(memory, p);

    uint8_t components_count = get_dictionary_size(html_raw_components);

    /* A template is essentially a Component that has been compiled with all its imports. */
    Dict templates = {0};
    char *ptr_1 = NULL;

    templates.start_addr = ptr_1 = (char *)memory_in_use(memory);

    for (i = 0; i < components_count; i++) { /** Compile Components. */
        KV raw_component = get_key_value(html_raw_components, i);

        strncpy(ptr_1, raw_component.k, strlen(raw_component.k) + 1);
        ptr_1 += strlen(raw_component.k) + 1;

        strncpy(ptr_1, raw_component.v, strlen(raw_component.v) + 1);

    repeat:;
        char *ptr_2 = ptr_1;

        HTMLBlock component_import_block = find_html_block(ptr_2, strlen(ptr_2), "x-component");

        /** Was an import statement found in the component markup? */
        if (component_import_block.block.start_addr) {
            uint8_t j;

            /** Does import statement contain any 'inline slots' html attributes? */
            Dict import__tag_attributes = get_tag_attributes(scratch_memory, component_import_block.opening_tag);

            /** Does import block contain any 'slots'? */
            Dict import__slots = get_slots(scratch_memory, component_import_block);

            /** Let's bring in component the import statement is intending to import */
            char *name_of_component_to_be_imported = find_value("name", import__tag_attributes);
            char *component_to_be_imported = find_value(name_of_component_to_be_imported, html_raw_components);

            char *component_to_be_imported_cpy = NULL;
            char *tmp = NULL;

            component_to_be_imported_cpy = tmp = (char *)memory_in_use(scratch_memory);

            memcpy(component_to_be_imported_cpy, component_to_be_imported, strlen(component_to_be_imported));

            /** Compile 'inline slots' if any */
            uint8_t inline_slot_count = get_dictionary_size(import__tag_attributes);
            for (j = 0; j < inline_slot_count; j++) {
                KV kv = get_key_value(import__tag_attributes, j);
                char *replacement_name = kv.k;
                char *replacement = kv.v;

                if (strncmp(replacement_name, "name", strlen("name")) == 0 && strlen(replacement_name) == strlen("name")) {
                    /** Tag name attribute is NOT a 'inline slots' so no need to do anything */
                    continue;
                }

                char *ptr_3 = component_to_be_imported_cpy;
                while (true) {
                    String Location = match_pattern(replacement_name, ptr_3, strlen(ptr_3));
                    if (!Location.start_addr) {
                        break;
                    }

                    char *after = Location.start_addr + Location.length;
                    memmove(Location.start_addr + strlen(replacement), after, strlen(after) + 1);
                    memcpy(Location.start_addr, replacement, strlen(replacement));

                    clear_leftovers(component_to_be_imported_cpy + strlen(component_to_be_imported_cpy));
                }
            }

            /** Compile 'slots' if any */
            uint8_t slot_count = get_dictionary_size(import__slots);
            for (j = 0; j < slot_count; j++) {
                KV kv = get_key_value(import__slots, j);
                char *replacement_name = kv.k;
                char *replacement = kv.v;

                char *ptr_3 = component_to_be_imported_cpy;
                while (true) {
                    HTMLBlock slot_reference = find_html_block(ptr_3, strlen(ptr_3), "x-slot");
                    if (!slot_reference.block.start_addr) {
                        break;
                    }

                    String name_attribute_value = find_name_attribute_value(slot_reference.opening_tag);
                    if (strncmp(replacement_name, name_attribute_value.start_addr, name_attribute_value.length) == 0 && strlen(replacement_name) == name_attribute_value.length) {
                        char *after = slot_reference.block.start_addr + slot_reference.block.length;
                        memmove(slot_reference.block.start_addr + strlen(replacement), after, strlen(after) + 1);
                        memcpy(slot_reference.block.start_addr, replacement, strlen(replacement));

                        clear_leftovers(component_to_be_imported_cpy + strlen(component_to_be_imported_cpy));

                        break;
                    }
                }
            }

            tmp = component_to_be_imported_cpy + strlen(component_to_be_imported_cpy) + 1;
            memory_out_of_use(scratch_memory, tmp);

            char *after = component_import_block.block.start_addr + component_import_block.block.length;
            memmove(component_import_block.block.start_addr + strlen(component_to_be_imported_cpy), after, strlen(after) + 1);
            memcpy(component_import_block.block.start_addr, component_to_be_imported_cpy, strlen(component_to_be_imported_cpy));
            component_import_block.block.length = strlen(component_import_block.block.start_addr);

            clear_leftovers(component_import_block.block.start_addr + component_import_block.block.length);

            memory_reset(scratch_memory, (uint8_t *)scratch_memory->start + sizeof(Memory));

            goto repeat;
        }

        ptr_1 += strlen(ptr_1) + 1;
    }

    templates.end_addr = ptr_1 - 1;
    memory_out_of_use(memory, ptr_1);

    return templates;
}

size_t render_val(char *template, char *val_name, char *value) {
    char *ptr = template;
    uint8_t inside = 0;
    while (*ptr != '\0') {
        if (strncmp(ptr, FOR_OPENING_TAG__START, strlen(FOR_OPENING_TAG__START)) == 0) {
            inside++;
        }

        if (strncmp(ptr, FOR_CLOSING_TAG, strlen(FOR_CLOSING_TAG)) == 0) {
            if (inside > 0) {
                inside--;
            } else {
                ASSERT(0);
            }
        }

        if (strncmp(ptr, VAL_OPENING_TAG__START, strlen(VAL_OPENING_TAG__START)) == 0) {
            if (inside == 0) {
                size_t value_name_length = 0;
                char *value_name = ptr + strlen(VAL_OPENING_TAG__START);
                char *tmp = value_name;

                while (*tmp != '"') {
                    value_name_length++;
                    tmp++;
                }

                TagLocation val_tag = {0};
                val_tag.start_addr = ptr;
                val_tag.end_addr = ptr + strlen(VAL_OPENING_TAG__START) + value_name_length + strlen(VAL_SELF_CLOSING_TAG__END);

                char buff[255];
                memset(buff, 0, 255);
                sprintf(buff, "%s\"", val_name);

                if (strncmp(buff, value_name, strlen(buff)) == 0) {
                    size_t val_length = strlen(value);

                    memmove(ptr + val_length, val_tag.end_addr, strlen(val_tag.end_addr) + 1);
                    memcpy(ptr, value, val_length);

                    ptr += strlen(ptr) + 1;

                    /** Clean up memory */
                    while (*ptr != '\0') {
                        *ptr = '\0';
                        ptr++;
                    }

                    break;
                }
            }
        }

        ptr++;
    }

    return strlen(template);
}

/**
 * TODO: ADD FUNCTION DOCUMENTATION
 */
size_t replace_val(char *template, char *val_name, char *value) {
    char *ptr = template;

    char key[100];

    size_t key_length = strlen(val_name) + strlen("%%");
    ASSERT(key_length < 100);

    sprintf(key, "%c%s%c", '%', val_name, '%');
    key[key_length] = '\0';

    while (*ptr != '\0') {
        if (strncmp(ptr, key, key_length) == 0) {
            size_t val_length = strlen(value);

            char *after = ptr + strlen(key);

            memmove(ptr + val_length, after, strlen(after) + 1);
            memcpy(ptr, value, val_length);

            ptr += strlen(ptr) + 1;

            /** Clean up memory */
            while (*ptr != '\0') {
                *ptr = '\0';
                ptr++;
            }

            return strlen(template);
        }

        ptr++;
    }

    return strlen(template);
}
