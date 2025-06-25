#ifndef MARKUP_PARSER_H
#define MARKUP_PARSER_H

int build_html_components(Memory *memory, Memory *scratch_memory, AssetSOA asset_list);
size_t render_val(char *template, char *val_name, char *value);
size_t replace_val(char *template, char *val_name, char *value);

#endif /* MARKUP_PARSER_H */