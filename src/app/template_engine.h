#ifndef TEMPLATE_ENGINE_H
#define TEMPLATE_ENGINE_H

KeyValueArray *build_html_components(Memory *memory, Memory *scratch_memory, KeyValueArray *asset_array);
size_t render_val(char *template, char *val_name, char *value);
size_t replace_val(char *template, char *val_name, char *value);
size_t render_for(char *template, char *block_name, ...);

#endif /* TEMPLATE_ENGINE_H */