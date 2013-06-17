
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include "common/json.h"

// this is by no means a complete json implementation.
// but it works for all our needs.

struct json {
  json_write_f write;
  void *arg;
  char cs      : 1; // comma separated
  char padding : 7;
};

static char *json_make_string(const char *string, int *len) {
  int max_len = strlen(string) * 2, i = 0;
  char *escaped = malloc(max_len + 3);
  if(escaped == NULL) {
    perror("malloc() failed");
    return NULL;
  }
  const char *str_p;
  escaped[i++] = '"';
  for(str_p = string; *str_p != 0; str_p++) {
    if(*str_p == '"' || *str_p == '\\') {
      escaped[i++] = '\\';
    }
    escaped[i++] = *str_p;
  }
  escaped[i++] = '"';
  escaped[i++] = 0;
  escaped = realloc(escaped, i);
  *len = i - 1;
  return escaped;
}

struct json *new_json(json_write_f writer, void *arg) {
  struct json *json = malloc(sizeof(struct json));
  if(json == NULL) {
    return NULL;
  }
  json->write = writer;
  json->arg = arg;
  return json;
}

void free_json(struct json *json) {
  free(json);
}

void json_start_object(struct json *json) {
  json->write("{", 1, json->arg);
  json->cs = 0;
}

void json_end_object(struct json *json) {
  json->write("}", 1, json->arg);
  json->cs = 0;
}

void json_write_string(struct json *json, char *string) {
  int len;
  char *token = json_make_string(string, &len);
  json->write(token, len, json->arg);
  free(token);
}

void json_write_key(struct json *json, char *key) {
  if(json->cs) {
    json->write(",", 1, json->arg);
  }
  json_write_string(json, key);
  json->write(":", 1, json->arg);
  json->cs = 1;
}

void json_write_key_val_string(struct json *json, char *key, char *val) {
  json_write_key(json, key);
  json_write_string(json, val);
}
