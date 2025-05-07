#ifndef JSON_PROCESSOR_H
#define JSON_PROCESSOR_H

typedef struct JSONElement JSONElement;
JSONElement *json_parse(Memory *memory, String input_json);

#endif /* JSON_PROCESSOR_H */