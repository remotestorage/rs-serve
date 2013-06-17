
#ifndef RS_JSON_H
#define RS_JSON_H

struct json;
typedef int(*json_write_f)(char *buf, size_t count, void *arg);

struct json *new_json(json_write_f writer, void *arg);
void free_json(struct json *json);
void json_start_object(struct json *json);
void json_end_object(struct json *json);
void json_write_string(struct json *json, char *string);
void json_write_key(struct json *json, char *key);
void json_write_key_val_string(struct json *json, char *key, char *val);

#endif

