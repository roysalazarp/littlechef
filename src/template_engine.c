#include "headers.h"

/**
 * Loads all public files (such as .js, .css, .json) excluding HTML files, from the specified `base_path`.
 */
Dict load_public_files(const char *base_path) {
    char *public_files_paths = (char *)arena->current;
    uint8_t public_files_count = 0;
    size_t all_paths_length = 0;
    locate_files(public_files_paths, base_path, NULL, 0, &public_files_count, &all_paths_length);
    char *public_files_paths_end = public_files_paths + all_paths_length;
    arena->current = public_files_paths_end + 1;

    char *public_files_dict = (char *)arena->current;
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
    arena_data->public_files_dict.start_addr = start;
    arena_data->public_files_dict.end_addr = start + public_files_dict_length;

    arena->current = arena_data->public_files_dict.end_addr + 1;

    return arena_data->public_files_dict;
}

/**
 * Loads all HTML components from the specified `base_path`.
 */
Dict load_html_components(const char *base_path) {
    /** Find the paths of all html files */
    char *html_files_paths = (char *)arena->current;
    char extension[] = ".html";
    uint8_t html_files_count = 0;
    size_t all_paths_length = 0;
    locate_files(html_files_paths, base_path, extension, 0, &html_files_count, &all_paths_length);
    char *html_files_paths_end = html_files_paths + all_paths_length;
    arena->current = html_files_paths_end + 1;

    /* A Component is an HTML snippet that may include references to other HTML snippets, i.e., it is composable */
    char *components_dict = (char *)arena->current;
    char *tmp_components_dict = components_dict;
    char *tmp_filepath = html_files_paths;

    while (tmp_filepath < html_files_paths_end) {
        char *file_content = NULL;
        long file_size = 0;
        read_file(&file_content, &file_size, tmp_filepath);

        /** A .html file may contain multiple Components */
        char *tmp_file_content = file_content;
        while ((tmp_file_content = strstr(tmp_file_content, COMPONENT_DEFINITION_OPENING_TAG__START)) != NULL) { /** Process Components inside .html file. */
            /** Start processing key (component name) */
            char *component_name_start = tmp_file_content + strlen(COMPONENT_DEFINITION_OPENING_TAG__START);
            char *component_name_end = NULL;

            uint8_t component_name_length = 0;

            if ((component_name_end = strchr(component_name_start, '\"')) != NULL) {
                component_name_length = component_name_end - component_name_start;
                strncpy(tmp_components_dict, component_name_start, component_name_length);
                tmp_components_dict[component_name_length] = '\0';
                tmp_components_dict += component_name_length + 1;
            } else {
                assert(0);
            }

            /** Start processing value (component html) */
            char *html = tmp_file_content + strlen(COMPONENT_DEFINITION_OPENING_TAG__START) + (size_t)component_name_length + strlen(COMPONENT_DEFINITION_OPENING_TAG__END);

            size_t html_length = 0;
            char *ptr = html;
            while (*ptr) {
                if (strncmp(ptr, COMPONENT_DEFINITION_CLOSING_TAG, strlen(COMPONENT_DEFINITION_CLOSING_TAG)) == 0) {
                    break;
                }

                html_length++;
                ptr++;
            }

            size_t minified_html_length = html_minify(tmp_components_dict, html, html_length);

            tmp_components_dict += minified_html_length;
            tmp_file_content++;
        }

        free(file_content);
        file_content = NULL;

        tmp_filepath += strlen(tmp_filepath) + 1;
    }

    size_t components_dict_length = tmp_components_dict - components_dict;

    /** `html_files_paths` is no longer needed since file paths are now stored as keys in
     * `components_dict`. Shift `components_dict` to occupy its memory space to prevent waste. */
    char *start = html_files_paths;
    memcpy(start, components_dict, components_dict_length);
    Dict html_raw_components_dict = {0};
    html_raw_components_dict.start_addr = start;
    html_raw_components_dict.end_addr = start + components_dict_length;

    arena->current = html_raw_components_dict.end_addr + 1;

    return html_raw_components_dict;
}

/**
 * Loads and resolves all HTML components along with their imports from the specified `base_path`.
 */
Dict load_templates(const char *base_path) {
    Dict html_raw_components = load_html_components(base_path);

    uint8_t i;

    /* A template is essentially a Component that has been compiled with all its imports. */
    char *templates_dict = (char *)arena->current;
    char *buffer = templates_dict;

    uint8_t components_count = get_dictionary_size(html_raw_components);

    for (i = 0; i < components_count; i++) { /** Compile Components. */
        KV raw_component = get_key_value(html_raw_components, i);

        strncpy(buffer, raw_component.k, strlen(raw_component.k) + 1);
        buffer += strlen(raw_component.k) + 1;

        strncpy(buffer, raw_component.v, strlen(raw_component.v) + 1);

        char *template_start = buffer;

        char *component_import_opening_tag = buffer;
        while ((component_import_opening_tag = strstr(component_import_opening_tag, COMPONENT_IMPORT_OPENING_TAG__START)) != NULL) { /** Resolve Component imports. */
            buffer += (component_import_opening_tag - buffer);

            char *import_name_start = component_import_opening_tag + strlen(COMPONENT_IMPORT_OPENING_TAG__START);
            const char *import_name_end = strchr(import_name_start, '"');
            uint8_t imported_name_length = (uint8_t)(import_name_end - import_name_start);

            char import_name[MAX_COMPONENT_NAME_LENGTH];
            memset(import_name, 0, MAX_COMPONENT_NAME_LENGTH);
            memcpy(import_name, import_name_start, imported_name_length);

            char *_raw_component_v = find_value(import_name, html_raw_components);

            if (strncmp(import_name_end, OPENING_COMPONENT_IMPORT_TAG_SELF_CLOSING_END, strlen(OPENING_COMPONENT_IMPORT_TAG_SELF_CLOSING_END)) == 0) { /** Import doesn't contain "slots" */
                uint8_t import_statement_length = (uint8_t)strlen(COMPONENT_IMPORT_OPENING_TAG__START) + imported_name_length + (uint8_t)strlen(OPENING_COMPONENT_IMPORT_TAG_SELF_CLOSING_END);
                size_t component_markdown_length = strlen(_raw_component_v);

                size_t len = strlen(buffer + import_statement_length);
                memmove(buffer + component_markdown_length, buffer + import_statement_length, len);
                char *ptr = buffer + component_markdown_length + len;
                ptr[0] = '\0';
                ptr++;
                while (*ptr) {
                    size_t str_len = strlen(ptr);
                    memset(ptr, 0, str_len);
                    ptr += str_len + 1;
                }

                memcpy(buffer, _raw_component_v, component_markdown_length);

                /**
                 * The component we imported may contain additional imports. Reset the pointer to
                 * the start of the HTML template to check for imports from the beginning again.
                 */
                buffer = template_start;
            }

            if (strncmp(import_name_end, COMPONENT_IMPORT_OPENING_TAG__END, strlen(COMPONENT_IMPORT_OPENING_TAG__END)) == 0) { /** Import contain "slots" */
                resolve_slots(_raw_component_v, component_import_opening_tag, &buffer);
            }
        }

        buffer += strlen(buffer) + 1;
    }

    size_t templates_dict_length = buffer - templates_dict;

    /** `html_raw_components` is no longer needed since they have been compiled
     * into the `templates_dict`. Shift `templates_dict` to occupy its memory
     * space to prevent waste. */
    char *start = html_raw_components.start_addr;
    memcpy(start, templates_dict, templates_dict_length);
    arena_data->templates.start_addr = start;
    arena_data->templates.end_addr = start + templates_dict_length;

    arena->current = arena_data->templates.end_addr + 1;

    return arena_data->templates;
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