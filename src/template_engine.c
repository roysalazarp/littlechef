#include "headers.h"

/**
 * Loads all public files (such as .js, .css, .json) excluding HTML files, from the specified `base_path`.
 */
Dict load_public_files(const char *base_path) {
    char *public_files_paths = (char *)global_arena->current;
    size_t all_paths_length = 0;
    locate_files(public_files_paths, base_path, NULL, 0, &all_paths_length);
    char *public_files_paths_end = public_files_paths + all_paths_length;
    global_arena->current = public_files_paths_end + 1;

    char *public_files_dict = (char *)global_arena->current;
    char *tmp_public_files_dict = public_files_dict;
    char *tmp_public_files_paths = public_files_paths;
    char extension[] = ".html";
    while (tmp_public_files_paths < public_files_paths_end) {
        /** NOT interested in html files */
        if (strncmp(tmp_public_files_paths + strlen(tmp_public_files_paths) - strlen(extension), extension, strlen(extension)) == 0) {
            tmp_public_files_paths += strlen(tmp_public_files_paths) + 1;
            continue;
        }

        char *file_content = NULL;
        long file_size = 0;
        read_file(&file_content, &file_size, tmp_public_files_paths);

        /** File path key */
        strncpy(tmp_public_files_dict, tmp_public_files_paths, strlen(tmp_public_files_paths) + 1);
        tmp_public_files_dict[strlen(tmp_public_files_paths)] = '\0';
        tmp_public_files_dict += strlen(tmp_public_files_paths) + 1;

        /** File content value */
        strncpy(tmp_public_files_dict, file_content, file_size + 1);
        tmp_public_files_dict[file_size] = '\0';
        tmp_public_files_dict += file_size + 1;

        free(file_content);
        file_content = NULL;

        tmp_public_files_paths += strlen(tmp_public_files_paths) + 1;
    }

    size_t public_files_dict_length = tmp_public_files_dict - public_files_dict;

    /** `public_files_paths` is no longer needed since file paths are now stored as keys in
     * `public_files_dict`. Shift `public_files_dict` to occupy its memory space to prevent waste. */
    char *start = public_files_paths;
    memcpy(start, public_files_dict, public_files_dict_length);
    global_arena_data->public_files_dict.start_addr = start;
    global_arena_data->public_files_dict.end_addr = start + public_files_dict_length;

    global_arena->current = global_arena_data->public_files_dict.end_addr + 1;

    return global_arena_data->public_files_dict;
}

String find_tag_name(char *import_statement) {
    char *ptr = NULL;
    String component_name = {0};

    ptr = strstr(import_statement, "name=\"");
    assert(ptr);

    component_name.start_addr = ptr + strlen("name=\"");

    ptr = component_name.start_addr;

    uint8_t length = 0;
    while (!isspace(*ptr) && *ptr != '\0') {
        if (*ptr == '\"') {
            component_name.length = length;
            return component_name;
        }

        length++;
        ptr++;
    }

    assert(0);
}

char *find_component_declaration(char *string) {
    char *ptr = strstr(string, COMPONENT_DEFINITION_OPENING_TAG__START);
    return ptr;
}

/**
 * Loads all HTML components from the specified `base_path`.
 */
Dict load_html_components(const char *base_path) {
    uint8_t i;

    /** Find the paths of all html files */
    StringArray html_files_paths = {0};
    html_files_paths.start_addr = (char *)global_arena->current;
    size_t all_paths_length = 0;
    locate_files(html_files_paths.start_addr, base_path, ".html", 0, &all_paths_length);
    html_files_paths.end_addr = html_files_paths.start_addr + all_paths_length;
    global_arena->current = html_files_paths.end_addr + 1;

    /* A Component is an HTML snippet that may include references to other HTML snippets, i.e., it is composable */
    char *components_dict = (char *)global_arena->current;
    char *buffer = components_dict;

    uint8_t html_files_paths_count = get_string_array_length(html_files_paths);
    for (i = 0; i < html_files_paths_count; i++) {
        char *filepath = get_string_at(html_files_paths, i);

        char *file_content = NULL;
        long file_size = 0;
        read_file(&file_content, &file_size, filepath);

        /** A .html file may contain multiple Components */
        char *tmp_file_content = file_content;
        while ((tmp_file_content = find_component_declaration(tmp_file_content)) != NULL) { /** Process Components inside .html file. */
            /** Found component in file content */
            /** Add component name to the dictionary as a key */
            String component_name = find_tag_name(tmp_file_content);
            strncpy(buffer, component_name.start_addr, component_name.length);
            buffer[component_name.length] = '\0';
            buffer += component_name.length + 1;

            char *component_markup = strchr(tmp_file_content, '>') + 1 /* pass '>' char */;

            char *closing_tag = strstr(component_markup, COMPONENT_DEFINITION_CLOSING_TAG);
            assert(closing_tag);

            size_t component_markup_length = closing_tag - component_markup;

            size_t minified_html_length = html_minify(buffer, component_markup, component_markup_length);

            buffer += minified_html_length;
            tmp_file_content++;
        }

        free(file_content);
        file_content = NULL;
    }

    size_t components_dict_length = buffer - components_dict;

    /** `html_files_paths` is no longer needed since file paths are now stored as keys in
     * `components_dict`. Shift `components_dict` to occupy its memory space to prevent waste. */
    char *start = html_files_paths.start_addr;
    memcpy(start, components_dict, components_dict_length);
    Dict html_raw_components_dict = {0};
    html_raw_components_dict.start_addr = start;
    html_raw_components_dict.end_addr = start + components_dict_length;

    /** Clean up excess from shift */
    size_t excess = buffer - html_raw_components_dict.end_addr;
    memset(html_raw_components_dict.end_addr, 0, excess);

    global_arena->current = html_raw_components_dict.end_addr + 1;

    return html_raw_components_dict;
}

char *string_skip_whitespaces(char *text) {
    while (isspace(*text)) {
        text++;
    }

    return text;
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

typedef struct {
    String block;
    String opening_tag;
    boolean is_self_closing;
} HTMLBlock;

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

String find_component_import_name(String opening_tag) {
    String name_attr = match_pattern(NAME_ATTRIBUTE_PATTERN, opening_tag.start_addr, opening_tag.length);
    assert(name_attr.start_addr);

    String name = match_pattern(ATTRIBUTE_VALUE_PATTERN, name_attr.start_addr, name_attr.length);
    assert(name.start_addr);

    /** exclude " at the beginning and end */
    name.start_addr += 1;
    name.length -= 2;

    return name;
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
        /*
        if (strncmp(ptr, SELF_CLOSING_TAG, strlen(SELF_CLOSING_TAG)) == 0) {
            ptr += strlen(SELF_CLOSING_TAG);
            html_block.is_self_closing = true;
            html_block.block.length = ptr - html_block.block.start_addr;
            html_block.opening_tag.length = html_block.block.length;

            return html_block;
        }
        */

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
                html_block.is_self_closing = false;
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

    assert(0);
}

typedef enum {
    _COMPONENT_DEFINITION_OPENING_TAG,
    _COMPONENT_DEFINITION_CLOSING_TAG,

    _COMPONENT_IMPORT_SELF_CLOSING_TAG,

    _COMPONENT_IMPORT_OPENING_TAG,
    _COMPONENT_IMPORT_CLOSING_TAG,

    _SLOT_SELF_CLOSING_TAG,

    _INSERT_OPENING_TAG,
    _INSERT_CLOSING_TAG,

    _ATTRIBUTE,
    _INJECT_ATTRIBUTE,
    _INHERIT_ATTRIBUTE,

    _PLACEHOLDER,

    _ANY,

    _END
} TokenKind;

typedef struct {
    TokenKind kind;
    String text;
} Token;

Dict get_slots(Arena *arena, HTMLBlock component_import) {
    Dict dict = {0};

    char *p = NULL;

    char *slots_dict = (char *)arena->current;
    dict.start_addr = slots_dict;
    char *tmp_slots_dict = slots_dict;

    char *end = component_import.block.start_addr + component_import.block.length;
    while (true) {
        HTMLBlock insert = find_html_block(component_import.block.start_addr, component_import.block.length, "x-insert");
        if (!insert.block.start_addr) {
            dict.end_addr = tmp_slots_dict - 1;
            arena->current = tmp_slots_dict;

            return dict;
        }

        String key = find_attribute(insert.opening_tag, "name");
        memcpy(tmp_slots_dict, key.start_addr, key.length);
        tmp_slots_dict += strlen(tmp_slots_dict) + 1;

        String value = {0};
        value.start_addr = insert.opening_tag.start_addr + insert.opening_tag.length;
        char *value_end = (insert.block.start_addr + insert.block.length) - strlen("</x-insert>");
        value.length = value_end - value.start_addr;
        memcpy(tmp_slots_dict, value.start_addr, value.length);
        tmp_slots_dict += strlen(tmp_slots_dict) + 1;

        component_import.block.start_addr = insert.block.start_addr + insert.block.length;
        component_import.block.length = end - component_import.block.start_addr;
    }
}

Dict get_tag_attributes(Arena *arena, String opening_tag) {
    Dict dict = {0};

    char *p = NULL;

    char *attr_dict = (char *)arena->current;
    dict.start_addr = attr_dict;
    char *tmp_attr_dict = attr_dict;

    char *end = opening_tag.start_addr + opening_tag.length;
    while (true) {
        String attribute = match_pattern(ATTRIBUTE_PATTERN_2, opening_tag.start_addr, opening_tag.length);
        if (!attribute.start_addr) {
            dict.end_addr = tmp_attr_dict - 1;
            arena->current = tmp_attr_dict;

            return dict;
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
            memcpy(tmp_attr_dict, key_start, key_length);
            tmp_attr_dict += strlen(tmp_attr_dict) + 1;
        } else {
            tmp_attr_dict[0] = '%';
            memcpy(&tmp_attr_dict[1], key_start, key_length);
            tmp_attr_dict[strlen(tmp_attr_dict)] = '%';
            tmp_attr_dict += strlen(tmp_attr_dict) + 1;
        }

        String value_match = match_pattern(ATTRIBUTE_VALUE_PATTERN, attribute.start_addr, attribute.length);
        char *value_start = value_match.start_addr + 1 /** " */;

        p = value_start;
        while (*p != '"') {
            p++;
        }

        char *value_end = p;

        size_t value_length = value_end - value_start;

        memcpy(tmp_attr_dict, value_start, value_length);
        tmp_attr_dict += strlen(tmp_attr_dict) + 1;

        opening_tag.start_addr = attribute.start_addr + attribute.length;
        opening_tag.length = end - opening_tag.start_addr;
    }

    assert(0);
}

/**
 * Loads and resolves all HTML components along with their imports from the specified `base_path`.
 */
Dict load_templates(const char *base_path) {
    Dict html_raw_components = load_html_components(base_path);

    uint8_t i;

    /* A template is essentially a Component that has been compiled with all its imports. */
    char *templates_dict = (char *)global_arena->current;
    char *buffer = templates_dict;

    uint8_t components_count = get_dictionary_size(html_raw_components);

    for (i = 0; i < components_count; i++) { /** Compile Components. */
        KV raw_component = get_key_value(html_raw_components, i);

        strncpy(buffer, raw_component.k, strlen(raw_component.k) + 1);
        buffer += strlen(raw_component.k) + 1;

        strncpy(buffer, raw_component.v, strlen(raw_component.v) + 1);

        char *component_start = buffer;

        char *ptr = buffer;
        HTMLBlock component_import = find_html_block(ptr, strlen(ptr), "x-component");
        printf("%.*s\n", (int)component_import.block.length, component_import.block.start_addr);

        Dict attributes = get_tag_attributes(scratch_arena, component_import.opening_tag);
        dump_dict(attributes, "attributes");
        char *component_name = find_value("name", attributes);

        Dict slots = get_slots(scratch_arena, component_import);
        dump_dict(slots, "slots");

        printf("\n");

        /*
    look_for_import:
        printf("foo\n");

        ComponentImport component_import = find_component_import(buffer);
        if (!component_import.import_statement.start_addr) {
            buffer += strlen(buffer) + 1;
            continue;
        }

        char import_name[MAX_COMPONENT_NAME_LENGTH];
        memset(import_name, 0, MAX_COMPONENT_NAME_LENGTH);
        memcpy(import_name, component_import.import_name.start_addr, component_import.import_name.length);

        char *_raw_component_v = find_value(import_name, html_raw_components);

        If the import does not contain slots, simply replace
        the import statement with the appropriate component markup
        if (component_import.contains_slots == false) {
            size_t component_markdown_length = strlen(_raw_component_v);
            size_t len = strlen(component_import.import_statement.start_addr + component_import.import_statement.length);

            memmove(component_import.import_statement.start_addr + component_markdown_length, component_import.import_statement.start_addr + component_import.import_statement.length, len);

            char *ptr = component_import.import_statement.start_addr + component_markdown_length + len;
            ptr[0] = '\0';
            ptr++;
            while (*ptr) {
                size_t str_len = strlen(ptr);
                memset(ptr, 0, str_len);
                ptr += str_len + 1;
            }

            memcpy(component_import.import_statement.start_addr, _raw_component_v, component_markdown_length);

            buffer = component_start;
            goto look_for_import;
        } else {
            Dict slots = get_slots(component_import.import_statement);
        }

        buffer += strlen(buffer) + 1;

        printf("%.*s\n", (int)component_import.import_statement.length, component_import.import_statement.start_addr);
        */

        /** Does import contain slots or inline slots */
        /*
        Dict slots = get_slots();
        */

        /**
         * NEXT: Put in a dictionary all the attributes found in the import tag. The key is the string to substitute.
         * For example:
         * <x-component name="circle_container" tw-bg-color="bg-[#CDCDD5]" tw-text-color="text-[#141E20]" inject:id="my_id">
         * { %tw-bg-color%, bg-[#CDCDD5], %tw-text-color%, text-[#141E20], inherit:id, id="my_id" }
         */
    }

    Dict output = {0};
    return output;
}

void resolve_slots(char *component_markdown, char *import_statement, char **templates) {
    char *ptr;

    char *tmp_templates = *templates;
    while (*tmp_templates) {
        if (strncmp(tmp_templates, COMPONENT_IMPORT_CLOSING_TAG, strlen(COMPONENT_IMPORT_CLOSING_TAG)) == 0) {
            char *component_import_closing_tag = strstr(import_statement, COMPONENT_IMPORT_CLOSING_TAG);
            char *passed_component_import_closing_tag = component_import_closing_tag + strlen(COMPONENT_IMPORT_CLOSING_TAG);

            size_t component_markdown_length = strlen(component_markdown);
            memmove((*templates) + component_markdown_length, passed_component_import_closing_tag, strlen(passed_component_import_closing_tag));
            ptr = (*templates) + component_markdown_length + strlen(passed_component_import_closing_tag);
            ptr[0] = '\0';
            ptr++;
            while (*ptr) {
                size_t str_len = strlen(ptr);
                memset(ptr, 0, str_len);
                ptr += str_len + 1;
            }

            memcpy((*templates), component_markdown, component_markdown_length);

            char *start = *templates;
            char *end = (*templates) + component_markdown_length;
            while ((start + strlen(SLOT_MARK)) < end) {
                if (strncmp(start, SLOT_MARK, strlen(SLOT_MARK)) == 0) {
                    char *passed_slot_tag = start + strlen(SLOT_MARK);
                    memmove(start, passed_slot_tag, strlen(passed_slot_tag));
                    ptr = start + strlen(passed_slot_tag);
                    ptr[0] = '\0';
                    ptr++;
                    while (*ptr) {
                        size_t str_len = strlen(ptr);
                        memset(ptr, 0, str_len);
                        ptr += str_len + 1;
                    }
                }

                start++;
            }

            return;
        }

        if (strncmp(tmp_templates, INSERT_OPENING_TAG, strlen(INSERT_OPENING_TAG)) == 0) {
            break;
        }

        tmp_templates++;
    }

    char *slot_tag_inside_component_markdown = strstr(component_markdown, SLOT_MARK);

    char *template_opening_tag = strstr(import_statement, INSERT_OPENING_TAG);
    char *passed_template_opening_tag = template_opening_tag + strlen(INSERT_OPENING_TAG);

    size_t portion = slot_tag_inside_component_markdown - component_markdown;

    memmove((*templates) + portion, passed_template_opening_tag, strlen(passed_template_opening_tag));
    ptr = (*templates) + portion + strlen(passed_template_opening_tag);
    ptr[0] = '\0';
    ptr++;
    while (*ptr) {
        size_t str_len = strlen(ptr);
        memset(ptr, 0, str_len);
        ptr += str_len + 1;
    }

    memcpy((*templates), component_markdown, portion);

    (*templates) += portion;
    component_markdown += portion;
    component_markdown += strlen(SLOT_MARK);

    char *template_closing_tag = (*templates);
    uint8_t skip = 0;
    while (*template_closing_tag) {
        if (strncmp(template_closing_tag, INSERT_OPENING_TAG, strlen(INSERT_OPENING_TAG)) == 0) {
            skip++;
        }

        if (strncmp(template_closing_tag, INSERT_CLOSING_TAG, strlen(INSERT_CLOSING_TAG)) == 0) {
            if (skip == 0) {
                break;
            }

            skip--;
        }

        template_closing_tag++;
    }

    char *passed_template_closing_tag = template_closing_tag + strlen(INSERT_CLOSING_TAG);
    memmove(template_closing_tag, passed_template_closing_tag, strlen(passed_template_closing_tag));
    ptr = template_closing_tag + strlen(passed_template_closing_tag);
    ptr[0] = '\0';
    ptr++;
    while (*ptr) {
        size_t str_len = strlen(ptr);
        memset(ptr, 0, str_len);
        ptr += str_len + 1;
    }

    (*templates) = template_closing_tag;
    resolve_slots(component_markdown, template_closing_tag, templates);
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