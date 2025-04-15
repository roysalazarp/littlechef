#ifndef TEMPLATE_ENGINE_H
#define TEMPLATE_ENGINE_H

int build_html_components(Memory *memory, Memory *scratch_memory, AssetList asset_list);
size_t render_val(char *template, char *val_name, char *value);
size_t replace_val(char *template, char *val_name, char *value);

#endif /* TEMPLATE_ENGINE_H */