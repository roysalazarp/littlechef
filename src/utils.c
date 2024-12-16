#include "headers.h"

Dict load_env_variables(const char *filepath) {
    char *file_content = NULL;
    long file_size = 0;
    read_file(&file_content, &file_size, filepath);

    assert(file_size != 0);

    char *envs = (char *)arena->current;
    char *p_dict_buffer = envs;

    char *line = file_content;
    char *end_of_file = file_content + file_size;

    while (line < end_of_file) { /** Basic .env file parsing. */
        char *key = NULL;
        boolean processed_key = false;

        char *value = NULL;
        boolean processed_value = false;

        char *c = line;

        /** Skip empty lines */
        if (*c == '\n') {
            goto end_of_line;
        }

        /** Skip comment line */
        if (*c == '#') {
            while (*c != '\n') {
                if (c == end_of_file) {
                    goto end_of_line;
                }

                c++;
            }

            goto end_of_line;
        }

        /** Skip whitespace characters at the beginning of the line */
        while (isspace(*c)) {
            if (c == end_of_file) {
                goto end_of_line;
            }

            c++;
        }

        /** Start processing key */
        while (!(isspace(*c)) && *c != '=') {
            if (c == end_of_file) {
                /**
                 * If we've reached the end_of_file of the file while processing
                 * the key, such variable does not have an associated value.
                 */
                assert(0);
            }

            /** Copy key into memory buffer */
            *p_dict_buffer = *c;
            if (!key) {
                key = p_dict_buffer;
            }
            p_dict_buffer++;

            c++;
        }

        *p_dict_buffer = '\0';
        p_dict_buffer++;

        processed_key = true;

        /** Skip whitespace characters after key */
        while (isspace(*c)) {
            if (c == end_of_file) {
                goto end_of_line;
            }

            c++;
        }

        /**
         * The first non-whitespace character we should find after
         * the key is the '=' after which comes the value.
         */
        if (*c != '=') {
            printf("Env variable '%s' does not have a corresponding value.\n", key);
            assert(0);
        } else {
            /** Skip '=' character */
            c++;
        }

        /** Skip whitespace characters after '=' */
        while (isspace(*c)) {
            if (c == end_of_file) {
                goto end_of_line;
            }

            c++;
        }

        /** From here we start processing value */
        while (!(isspace(*c))) {
            if (c == end_of_file) {
                if (value) {
                    printf("WARNING: EOF reached while processing value '%s' for key '%s'. Ensure value '%s' is as intended.\n", value, key, value);
                    processed_value = true;
                }

                goto end_of_line;
            }

            /** Copy value into memory buffer */
            *p_dict_buffer = *c;
            if (!value) {
                value = p_dict_buffer;
            }
            p_dict_buffer++;

            c++;
        }

        *p_dict_buffer = '\0';
        p_dict_buffer++;

        processed_value = true;

        /** Skip all character after the value and proceed to next line */
        while (*c != '\n') {
            if (c == end_of_file) {
                goto end_of_line;
            }

            c++;
        }

    end_of_line:
        line = c + 1;

        if ((processed_key == false) != (processed_value == false)) {
            /**
             * Key and value must be processed together.
             * One should not be processed without the other.
             */
            assert(0);
        }

        processed_key = false;
        processed_value = false;
    }

    arena->current = p_dict_buffer + 1;

    free(file_content);
    file_content = NULL;

    Dict envs_dict = {0};
    envs_dict.start_addr = envs;
    envs_dict.end_addr = p_dict_buffer;

    return envs_dict;
}

/**
 * Reads a file into a dynamically allocated buffer; caller must free the buffer.
 */
void read_file(char **buffer, long *file_size, const char *absolute_file_path) {
    FILE *file = fopen(absolute_file_path, "r");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) != -1);
    *file_size = ftell(file);
    assert(*file_size != -1);
    rewind(file);

    *buffer = (char *)malloc(*file_size * sizeof(char) + 1);
    assert(*buffer != NULL);

    size_t read_size = fread(*buffer, sizeof(char), *file_size, file);
    assert(read_size == (size_t)*file_size);

    (*buffer)[*file_size] = '\0';

    fclose(file);
}

/**
 * Recursively searches a directory and its subdirectories to locate files with a specified
 * `extension`. If no `extension` is provided, it retrieves all files regardless of their type.
 */
char *locate_files(char *buffer, const char *base_path, const char *extension, uint8_t level, uint8_t *total_files, size_t *all_paths_length) {
    DIR *dir = opendir(base_path);
    assert(dir != NULL);

    struct dirent *entry;
    struct stat statbuf;
    size_t entry_name_length;
    char path[MAX_PATH_LENGTH];
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            sprintf(path, "%s/%s", base_path, entry->d_name);

            if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
                buffer = locate_files(buffer, path, extension, level + 1, total_files, all_paths_length);
            } else {
                entry_name_length = strlen(entry->d_name);
                if ((extension == NULL) || ((entry_name_length > strlen(extension)) && (strcmp(entry->d_name + entry_name_length - strlen(extension), extension) == 0))) {
                    assert(*total_files < MAX_FILES);
                    size_t path_len = strlen(path);
                    strcpy(buffer, path);
                    buffer[path_len] = '\0';
                    buffer += (path_len + 1);
                    (*all_paths_length) = (*all_paths_length) + (path_len + 1);
                    (*total_files)++;
                }
            }
        }
    }

    closedir(dir);

    return buffer;
}

/**
 * Finds the value associated with a given key in a dictionary.
 */
char *find_value(const char key[], Dict dict) {
    char *ptr = dict.start_addr;
    while (ptr < dict.end_addr) {
        /** Include null-terminator (+ 1) because key is a null-terminated string */
        if (strncmp(ptr, key, strlen(key) + 1) == 0) {
            ptr += strlen(ptr) + 1; /* Advance past key */
            return (ptr);
        }

        ptr += strlen(ptr) + 1; /* Advance past key */
        ptr += strlen(ptr) + 1; /* Advance past value */
    }

    return NULL;
}

Dict add_key_value(const char *buffer, int key_value_pairs, ...) {
    va_list args;

    va_start(args, key_value_pairs);

    char *current = (char *)buffer;

    int i;
    for (i = 0; i < key_value_pairs * 2; i++) {
        const char *str = va_arg(args, const char *);
        size_t len = strlen(str) + 1;
        memcpy(current, str, len);
        current += len;
    }

    va_end(args);

    Dict dict = {0};
    dict.start_addr = (char *)buffer;
    dict.end_addr = current;

    return dict;
}

uint8_t get_dictionary_size(Dict dict) {
    size_t size = 0;

    char *ptr = dict.start_addr;
    while (ptr < dict.end_addr) {
        ptr += strlen(ptr) + 1; /* Advance past key */
        ptr += strlen(ptr) + 1; /* Advance past value */
        size++;
    }

    return size;
}

int generate_salt(uint8_t *salt, size_t salt_size) {
    FILE *dev_urandom = fopen("/dev/urandom", "rb");
    if (dev_urandom == NULL) {
        fprintf(stderr, "Error opening /dev/urandom\nError code: %d\n", errno);
        return -1;
    }

    if (fread(salt, 1, salt_size, dev_urandom) != salt_size) {
        fprintf(stderr, "Error reading from /dev/urandom\nError code: %d\n", errno);
        fclose(dev_urandom);
        return -1;
    }

    fclose(dev_urandom);

    return 0;
}

void replace_slashes(char *str) {
    while (*str != '\0') {
        if (*str == '/') {
            *str = '\\'; /* Replace '/' with '\' */
        }
        str++;
    }
}

void dump_dict(Dict dict, char dir_name[]) {
    char cwd[KB(1)];
    memset(cwd, 0, KB(1));

    assert(getcwd(cwd, sizeof(cwd)) != NULL);

    char memory_dir[] = "/memory";

    assert((strlen(cwd) + strlen(memory_dir)) < KB(1));

    memcpy(&(cwd[strlen(cwd)]), memory_dir, strlen(memory_dir));

    /* Check if the directory exists */
    if (access(cwd, F_OK) == -1) {
        /* Directory doesn't exist, so create it */
        assert(mkdir(cwd, 0755) == 0);
    }

    char slash[] = "/";
    assert((strlen(cwd) + strlen(slash) + strlen(dir_name)) < KB(1));

    memcpy(&(cwd[strlen(cwd)]), slash, strlen(slash));
    memcpy(&(cwd[strlen(cwd)]), dir_name, strlen(dir_name));

    char command[KB(2)];
    sprintf(command, "rm -rf %s", cwd);
    assert(system(command) == 0);
    assert(mkdir(cwd, 0755) == 0);

    char *ptr = dict.start_addr;
    while (ptr < dict.end_addr) {
        char *key = ptr;

        char file_name[KB(2)];
        memset(file_name, 0, KB(2));
        sprintf(file_name, "%s/%s", cwd, key);

        char *name = file_name + strlen(cwd) + strlen(slash);
        replace_slashes(name);

        FILE *file = fopen(file_name, "w");
        assert(file);

        ptr += strlen(ptr) + 1;
        char *value = ptr;

        fprintf(file, "%s", value);
        ptr += strlen(ptr) + 1;

        fclose(file);
    }
}

KV get_key_value(Dict dict, uint8_t pos) {
    KV kv = {0};

    uint8_t size = get_dictionary_size(dict);

    if (pos > size) {
        printf("You requested to get key-value at pos %d, but dict only contains %d elements", pos, size);
        assert(0);
    }

    char *ptr = dict.start_addr;

    uint8_t i;
    for (i = 0; i < size; i++) {
        if (i == pos) {
            char *key = ptr;
            ptr += strlen(ptr) + 1; /* pass key */

            char *value = ptr;

            kv.k = key;
            kv.v = value;

            break;
        }

        ptr += strlen(ptr) + 1; /* pass key */
        ptr += strlen(ptr) + 1; /* pass value */
    }

    return kv;
}

char *copy_string(Arena *scratch_arena, String str) {
    char *buffer = arena_alloc(scratch_arena, str.length);
    memcpy(buffer, str.start_addr, str.length);

    return buffer;
}