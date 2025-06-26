#ifndef JSON_PARSER_H
#define JSON_PARSER_H

typedef struct JSONElement JSONElement;
JSONElement *json_parse(Memory *memory, String input_json);

#endif /* JSON_PARSER_H */