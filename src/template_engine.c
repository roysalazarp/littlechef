#include "headers.h"

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
    TagLocation opening_tag;
    TagLocation closing_tag;
} BlockLocation;

typedef struct {
    String block;
    String opening_tag;
} HTMLBlock;

char *string_skip_whitespaces(char *text);
StringArray get_files_list(Arena *arena, const char *base_path, const char *extension);
char *find_component_declaration(char *string);
Dict load_html_components(Arena *arena, const char *base_path);
String match_pattern(const char *pattern, const char *text, size_t text_length);
String find_attribute(String opening_tag, const char *attr_name);
String find_name_attribute_value(String opening_tag);
HTMLBlock find_html_block(char *text, size_t text_length, const char *tag_name);
Dict get_slots(Arena *arena, HTMLBlock component_import);
Dict get_tag_attributes(Arena *arena, String opening_tag);
size_t html_minify(char *buffer, char *html, size_t html_length);
BlockLocation find_block(char *template, char *block_name);
void clear_leftovers(char *ptr);

/**
 * Loads all public files (such as .js, .css, .json) excluding HTML files, from the specified `base_path`.
 */
Dict load_public_files(Arena *arena, const char *base_path) {
    StringArray files_list = get_files_list(scratch_arena, base_path, NULL);

    uint8_t files_list_length = get_string_array_length(files_list);

    char *public_files = NULL;
    char *ptr = NULL;
    ARENA_IN_USE(arena, public_files, ptr) {
        uint8_t i;
        for (i = 0; i < files_list_length; i++) {
            char *path = get_string_at(files_list, i);

            /** NOT interested in html files */
            if (strncmp(path + strlen(path) - strlen(".html"), ".html", strlen(".html")) == 0) {
                continue;
            }

            char *file_content = NULL;
            long file_size = 0;
            read_file(&file_content, &file_size, path);

            /** Minify js files */
            if (strncmp(path + strlen(path) - strlen(".js"), ".js", strlen(".js")) == 0) {
                char *dest = file_content;
                char *src = file_content;
                boolean inside_string = false;
                boolean last_was_space = false;

                while (*src != '\0') {
                    /* Check for string literals (inside " or ') */
                    if (*src == '\"' || *src == '\'') {
                        if (inside_string) {
                            if (*(src - 1) != '\\') {
                                inside_string = false; /* Exit string literal */
                            }
                        } else {
                            inside_string = true; /* Enter string literal */
                        }
                    }

                    if (inside_string) {
                        /* Copy everything inside a string as-is */
                        *dest++ = *src;
                    } else {
                        /* Outside of string literals, manage spaces */
                        if (isspace(*src)) {
                            if (!last_was_space) {
                                *dest++ = ' '; /* Replace sequence of spaces with one space */
                                last_was_space = true;
                            }
                        } else {
                            /* Non-space character resets the space tracking */
                            *dest++ = *src;
                            last_was_space = false;
                        }
                    }

                    src++;
                }

                /* Null-terminate the result */
                *dest = '\0';
            }

            Dict inserted_element = add_to_dictionary(ptr, 1, path, file_content);
            ptr = inserted_element.end_addr;

            free(file_content);
            file_content = NULL;
        }
    }

    Dict public_files_dict = {0};
    public_files_dict.start_addr = public_files;
    public_files_dict.end_addr = ptr - 1;

    arena_reset(scratch_arena, sizeof(Arena));

    return public_files_dict;
}

/**
 * Loads all HTML components from the specified `base_path`.
 */
Dict load_html_components(Arena *arena, const char *base_path) {
    StringArray files_list = get_files_list(scratch_arena, base_path, ".html");

    uint8_t files_list_length = get_string_array_length(files_list);

    /* A Component is an HTML snippet that may include references to other HTML snippets, i.e., it is composable */
    char *components = NULL;
    char *ptr = NULL;
    ARENA_IN_USE(arena, components, ptr) {
        uint8_t i;
        for (i = 0; i < files_list_length; i++) {
            char *path = get_string_at(files_list, i);

            char *file_content = NULL;
            long file_size = 0;
            read_file(&file_content, &file_size, path);

            /** A .html file may contain multiple Components */
            char *tmp_file_content = file_content;
            while ((tmp_file_content = find_component_declaration(tmp_file_content)) != NULL) { /** Process Components inside .html file. */
                /** Found component in file content */
                HTMLBlock component = find_html_block(tmp_file_content, strlen(tmp_file_content), "x-component-def");
                String component_name = find_attribute(component.opening_tag, "name");

                strncpy(ptr, component_name.start_addr, component_name.length);
                ptr += strlen(ptr) + 1;

                char *component_content = component.opening_tag.start_addr + component.opening_tag.length;
                char *component_content_end = (component.block.start_addr + component.block.length) - strlen(COMPONENT_DEFINITION_CLOSING_TAG);
                size_t component_content_length = component_content_end - component_content;

                size_t minified_html_length = html_minify(ptr, component_content, component_content_length);
                ptr += minified_html_length;

                tmp_file_content++;
            }

            free(file_content);
            file_content = NULL;
        }
    }

    Dict components_dict = {0};
    components_dict.start_addr = components;
    components_dict.end_addr = ptr - 1;

    arena_reset(scratch_arena, sizeof(Arena));

    return components_dict;
}

/**
 * Loads and resolves all HTML components along with their imports from the specified `base_path`.
 */
Dict load_templates(Arena *arena, const char *base_path) {
    Dict html_raw_components = load_html_components(arena, base_path);

    uint8_t components_count = get_dictionary_size(html_raw_components);

    /* A template is essentially a Component that has been compiled with all its imports. */
    char *templates = NULL;
    char *ptr_1 = NULL;
    ARENA_IN_USE(arena, templates, ptr_1) {
        uint8_t i;
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
                Dict import__tag_attributes = get_tag_attributes(scratch_arena, component_import_block.opening_tag);

                /** Does import block contain any 'slots'? */
                Dict import__slots = get_slots(scratch_arena, component_import_block);

                /** Let's bring in component the import statement is intending to import */
                char *name_of_component_to_be_imported = find_value("name", import__tag_attributes);
                char *component_to_be_imported = find_value(name_of_component_to_be_imported, html_raw_components);

                char *component_to_be_imported_cpy = NULL;
                char *tmp = NULL;
                ARENA_IN_USE(scratch_arena, component_to_be_imported_cpy, tmp) {
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

                    /** ARENA_IN_USE will use tmp to at arena_out_of_use */
                    tmp = component_to_be_imported_cpy + strlen(component_to_be_imported_cpy) + 1;
                }

                char *after = component_import_block.block.start_addr + component_import_block.block.length;
                memmove(component_import_block.block.start_addr + strlen(component_to_be_imported_cpy), after, strlen(after) + 1);
                memcpy(component_import_block.block.start_addr, component_to_be_imported_cpy, strlen(component_to_be_imported_cpy));
                component_import_block.block.length = strlen(component_import_block.block.start_addr);

                clear_leftovers(component_import_block.block.start_addr + component_import_block.block.length);

                arena_reset(scratch_arena, sizeof(Arena));

                goto repeat;
            }

            ptr_1 += strlen(ptr_1) + 1;
        }
    }

    Dict templates_dict = {0};
    templates_dict.start_addr = templates;
    templates_dict.end_addr = ptr_1 - 1;

    return templates_dict;
}

/**
 * TODO: ADD FUNCTION DOCUMENTATION
 */
BlockLocation find_block(char *template, char *block_name) {
    BlockLocation block = {0};

    char *ptr = NULL;

    while ((ptr = strstr(template, FOR_OPENING_TAG__START)) != NULL) {
        char *before = ptr;

        ptr += strlen(FOR_OPENING_TAG__START);

        if (strncmp(ptr, block_name, strlen(block_name)) == 0) {
            char *after = ptr + strlen(block_name) + strlen(FOR_OPENING_TAG__END);

            block.opening_tag.start_addr = before;
            block.opening_tag.end_addr = after;

            uint8_t inside = 0;
            while (*ptr != '\0') {
                if (strncmp(ptr, FOR_OPENING_TAG__START, strlen(FOR_OPENING_TAG__START)) == 0) {
                    inside++;
                }

                if (strncmp(ptr, FOR_CLOSING_TAG, strlen(FOR_CLOSING_TAG)) == 0) {
                    if (inside > 0) {
                        inside--;
                    } else {
                        block.closing_tag.start_addr = ptr;
                        block.closing_tag.end_addr = ptr + strlen(FOR_CLOSING_TAG);

                        return block;
                    }
                }

                ptr++;
            }
        }
    }

    return block;
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
                assert(0);
            }
        }

        if (strncmp(ptr, VAL_OPENING_TAG__START, strlen(VAL_OPENING_TAG__START)) == 0) {
            if (inside == 0) {
                size_t value_name_length = 0;
                char *value_name = ptr + strlen(VAL_OPENING_TAG__START);
                char *tmp = value_name;

                boolean single_quote = false;
                while (*tmp != '\0') {
                    if (*tmp != '"') {
                        break;
                    }

                    if (*tmp != '\'') {
                        single_quote = true;
                        break;
                    }

                    value_name_length++;
                    tmp++;
                }

                TagLocation val_tag = {0};
                val_tag.start_addr = ptr;
                val_tag.end_addr = ptr + strlen(VAL_OPENING_TAG__START) + value_name_length + strlen(VAL_SELF_CLOSING_TAG__END);

                char buff[255];
                memset(buff, 0, 255);
                if (single_quote) {
                    sprintf(buff, "%s\'", val_name);
                } else {
                    sprintf(buff, "%s\"", val_name);
                }

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

                    return strlen(template);
                }
            }
        }

        ptr++;
    }

    assert(0);
}

/**
 * TODO: ADD FUNCTION DOCUMENTATION
 */
size_t render_for(char *template, char *block_name, int times, ...) {
    va_list args;
    CharsBlock key_value = {0};

    BlockLocation block = find_block(template, block_name);

    if (!block.opening_tag.start_addr || !block.opening_tag.end_addr || !block.closing_tag.start_addr || !block.closing_tag.end_addr) {
        /** Didn't find block */
        return strlen(template);
    }

    size_t block_length = block.closing_tag.start_addr - block.opening_tag.end_addr;

    char *block_copy = (char *)malloc((block_length + 1) * sizeof(char));
    memcpy(block_copy, block.opening_tag.end_addr, block_length);
    block_copy[block_length] = '\0';
    char *block_copy_end = block_copy + block_length;

    char *start = block.opening_tag.start_addr;

    size_t after_copy_lenght = strlen(block.closing_tag.end_addr);
    char *after_copy = (char *)malloc((after_copy_lenght + 1) * sizeof(char));
    memcpy(after_copy, block.closing_tag.end_addr, after_copy_lenght);
    after_copy[after_copy_lenght] = '\0';

    va_start(args, times);

    int i;
    for (i = 0; i < times; i++) {
        key_value = va_arg(args, CharsBlock);

        char *ptr = block_copy;

        if (!key_value.start_addr && !key_value.end_addr) {
            while (ptr < block_copy_end) {
                *start = *ptr;

                start++;
                ptr++;
            }

            continue;
        } else {
            uint8_t inside = 0;
            while (ptr < block_copy_end) {
                if (strncmp(ptr, FOR_OPENING_TAG__START, strlen(FOR_OPENING_TAG__START)) == 0) {
                    inside++;
                }

                if (strncmp(ptr, FOR_CLOSING_TAG, strlen(FOR_CLOSING_TAG)) == 0) {
                    if (inside > 0) {
                        inside--;
                    } else {
                        assert(0);
                    }
                }

                if (strncmp(ptr, VAL_OPENING_TAG__START, strlen(VAL_OPENING_TAG__START)) == 0) {
                    if (inside == 0) {
                        size_t val_name_length = 0;
                        char *val_name = ptr + strlen(VAL_OPENING_TAG__START);
                        char *tmp = val_name;

                        while (*tmp != '"') {
                            val_name_length++;
                            tmp++;
                        }

                        TagLocation val_tag = {0};
                        val_tag.start_addr = ptr;
                        val_tag.end_addr = ptr + strlen(VAL_OPENING_TAG__START) + val_name_length + strlen(VAL_SELF_CLOSING_TAG__END);

                        char buff[255];
                        memset(buff, 0, 255);
                        memcpy(buff, val_name, val_name_length);

                        char *value = find_value(buff, key_value);
                        assert(value);

                        size_t val_length = strlen(value);
                        memcpy(start, value, val_length);

                        ptr = val_tag.end_addr;
                        start += val_length;

                        continue;
                    }
                }

                *start = *ptr;

                start++;
                ptr++;
            }
        }
    }

    memcpy(start, after_copy, after_copy_lenght);
    start[after_copy_lenght] = '\0';

    free(block_copy);
    free(after_copy);

    /** Clean up memory */
    char *p = start + after_copy_lenght + 1;
    while (*p != '\0') {
        *p = '\0';
        p++;
    }

    va_end(args);

    return strlen(template);
}

/**
 * TODO: ADD FUNCTION DOCUMENTATION
 */
size_t replace_val(char *template, char *val_name, char *value) {
    char *ptr = template;

    char key[100];

    size_t key_length = strlen(val_name) + strlen("%%");
    assert(key_length < 100);

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

/**
 * A simple HTML minifier that compresses the given HTML content and stores the
 * minified result in the provided buffer. It returns the size of the minified HTML.
 */
size_t html_minify(char *buffer, char *html, size_t html_length) {
    char *start = buffer;

    char *html_end = html + html_length;

    uint8_t skip_whitespace = 0;
    while (html < html_end) {
        if (strlen(start) == 0 && isspace(*html)) {
            skip_whitespace = 1;
            html++;
            continue;
        }

        if (*html == '>') {
            char *temp = html - 1;
            if (isspace(*temp) && !skip_whitespace) {
                uint8_t i = 0;
                while (*temp) {
                    if (!isspace(*temp)) {
                        skip_whitespace = 1;
                        buffer -= i - 1;
                        break;
                    }

                    temp -= 1;
                    i++;
                }

                continue;
            }

            skip_whitespace = 1;
            goto copy_char;
        }

        if (*html == '<') {
            char *temp = html - 1;
            if (isspace(*temp) && !skip_whitespace) {
                uint8_t i = 0;
                while (*temp) {
                    if (!isspace(*temp)) {
                        skip_whitespace = 1;
                        buffer -= i - 1;
                        break;
                    }

                    temp -= 1;
                    i++;
                }

                continue;
            }

            skip_whitespace = 0;
            goto copy_char;
        }

        if (!skip_whitespace && *html == '\n') {
            html++;
            continue;
        }

        if (skip_whitespace && isspace(*html)) {
            html++;
            continue;
        }

        if (skip_whitespace && !isspace(*html)) {
            skip_whitespace = 0;
            goto copy_char;
        }

    copy_char:
        *buffer = *html;
        buffer++;

        html++;
    }

    buffer[0] = '\0';
    buffer++;

    size_t length = buffer - start;

    return length;
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

                        return html_block;
                    } else {
                        nested--;
                    }
                }

                ptr++;
            }
        }

        ptr++;
    }

    assert(0);
}

Dict get_slots(Arena *arena, HTMLBlock component_import) {
    char *end = component_import.block.start_addr + component_import.block.length;

    char *slots = NULL;
    char *ptr = NULL;
    ARENA_IN_USE(arena, slots, ptr) {
        while (true) {
            HTMLBlock insert = find_html_block(component_import.block.start_addr, component_import.block.length, "x-insert");
            if (!insert.block.start_addr) {
                break;
            }

            String key = find_attribute(insert.opening_tag, "name");
            memcpy(ptr, key.start_addr, key.length);
            ptr += strlen(ptr) + 1;

            String value = {0};
            value.start_addr = insert.opening_tag.start_addr + insert.opening_tag.length;
            char *value_end = (insert.block.start_addr + insert.block.length) - strlen("</x-insert>");
            value.length = value_end - value.start_addr;
            memcpy(ptr, value.start_addr, value.length);
            ptr += strlen(ptr) + 1;

            component_import.block.start_addr = insert.block.start_addr + insert.block.length;
            component_import.block.length = end - component_import.block.start_addr;
        }
    }

    Dict slots_dict = {0};
    slots_dict.start_addr = slots;
    slots_dict.end_addr = ptr - 1;

    return slots_dict;
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

Dict get_tag_attributes(Arena *arena, String opening_tag) {
    char *p = NULL;

    char *end = opening_tag.start_addr + opening_tag.length;

    char *attributes = NULL;
    char *ptr = NULL;
    ARENA_IN_USE(arena, attributes, ptr) {
        while (true) {
            String attribute = match_pattern(ATTRIBUTE_PATTERN_2, opening_tag.start_addr, opening_tag.length);
            if (!attribute.start_addr) {
                break;
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
            if (strncmp(key_start, "name", strlen("name")) == 0 && key_length == strlen("name")) {
                memcpy(ptr, key_start, key_length);
                ptr += strlen(ptr) + 1;
            } else {
                ptr[0] = '%';
                memcpy(&ptr[1], key_start, key_length);
                ptr[strlen(ptr)] = '%';
                ptr += strlen(ptr) + 1;
            }

            String value_match = match_pattern(ATTRIBUTE_VALUE_PATTERN, attribute.start_addr, attribute.length);
            char *value_start = value_match.start_addr + 1 /** " */;

            p = value_start;
            while (*p != '"') {
                p++;
            }

            char *value_end = p;

            size_t value_length = value_end - value_start;

            memcpy(ptr, value_start, value_length);
            ptr += strlen(ptr) + 1;

            opening_tag.start_addr = attribute.start_addr + attribute.length;
            opening_tag.length = end - opening_tag.start_addr;
        }
    }

    Dict attributes_dict = {0};
    attributes_dict.start_addr = attributes;
    attributes_dict.end_addr = ptr - 1;

    return attributes_dict;
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

    assert(0);
}

String find_name_attribute_value(String opening_tag) {
    String name_attr = match_pattern(NAME_ATTRIBUTE_PATTERN, opening_tag.start_addr, opening_tag.length);
    assert(name_attr.start_addr);

    String name = match_pattern(ATTRIBUTE_VALUE_PATTERN, name_attr.start_addr, name_attr.length);
    assert(name.start_addr);

    /** exclude " at the beginning and end */
    name.start_addr += 1;
    name.length -= 2;

    return name;
}

StringArray get_files_list(Arena *arena, const char *base_path, const char *extension) {
    char *files_list = NULL;
    char *ptr = NULL;
    ARENA_IN_USE(arena, files_list, ptr) {
        size_t all_paths_length = 0;
        locate_files(files_list, base_path, extension, 0, &all_paths_length);
        ptr = files_list + all_paths_length;
    }

    StringArray files_list_arr = {0};
    files_list_arr.start_addr = files_list;
    files_list_arr.end_addr = ptr - 1;

    return files_list_arr;
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