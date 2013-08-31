
#include <node.h>
#include <v8.h>

#include "../../src/common/auth.h"

#include <stdlib.h>
#include <stdio.h>

#define EXPECT_ARGS(n)                          \
  if(args.Length() < n) {                                               \
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments"))); \
    return scope.Close(Undefined());                                    \
  }


using namespace v8;

Handle<Value> Add(const Arguments& args) {
  HandleScope scope;
  EXPECT_ARGS(3);
  String::AsciiValue username(args[0]->ToString());
  String::AsciiValue token(args[1]->ToString());

  struct rs_authorization auth;
  auth.username = *username;
  auth.token = *token;

  Local<Object> scopes(args[2]->ToObject());
  Local<Array> scope_keys(scopes->GetPropertyNames());

  uint32_t n = scope_keys->Length(), i;

  auth.scopes.count = n;
  auth.scopes.ptr = (struct rs_scope**) malloc(sizeof(struct rs_scope*) * n);
  if(auth.scopes.ptr == NULL) {
    ThrowException(Exception::Error(String::New("Failed to allocate memory")));
    return scope.Close(Undefined());
  }

  for(i=0;i<n;i++) {
    Local<Value> _key(scope_keys->Get(i));
    String::AsciiValue key(_key);
    String::AsciiValue mode(scopes->Get(_key));
    struct rs_scope *auth_scope = (struct rs_scope*)malloc(sizeof(struct rs_scope));
    if(auth_scope == NULL) {
      ThrowException(Exception::Error(String::New("Failed to allocate memory")));
      return scope.Close(Undefined());
    }
    if(strcmp(*key, "root") == 0) {
      auth_scope->name = strdup("");
    } else {
      auth_scope->name = strdup(*key);
    }
    auth_scope->write = (strcmp(*mode, "rw") == 0) ? 1 : 0;
    auth.scopes.ptr[i] = auth_scope;
  }
  open_authorizations("w");
  add_authorization(&auth);
  close_authorizations();

  for(i=0;i<n;i++) {
    free(auth.scopes.ptr[i]->name);
    free(auth.scopes.ptr[i]);
  }

  free(auth.scopes.ptr);

  return Undefined();
}

Handle<Value> Remove(const Arguments& args) {
  HandleScope scope;
  EXPECT_ARGS(2);
  String::AsciiValue username(args[0]->ToString());
  String::AsciiValue token(args[1]->ToString());
  struct rs_authorization auth = {
    .username = *username,
    .token = *token
  };
  open_authorizations("w");
  remove_authorization(&auth);
  close_authorizations();
  return Undefined();
}

inline Handle<Value> auth_to_object(struct rs_authorization *auth) {
  Local<Object> obj = Object::New();
  obj->Set(String::NewSymbol("username"), String::New(auth->username));
  obj->Set(String::NewSymbol("token"), String::New(auth->token));
  int n = auth->scopes.count, i;
  Local<Object> scopes = Object::New();
  for(i=0;i<n;i++) {
    scopes->Set(String::NewSymbol(auth->scopes.ptr[i]->name),
                String::NewSymbol(auth->scopes.ptr[i]->write ? "rw" : "r"));
  }
  obj->Set(String::NewSymbol("scopes"), scopes);
  return obj;
}

Handle<Value> Lookup(const Arguments& args) {
  HandleScope scope;
  EXPECT_ARGS(2);
  String::AsciiValue username(args[0]->ToString());
  String::AsciiValue token(args[1]->ToString());
  open_authorizations("r");
  struct rs_authorization *auth = lookup_authorization(*username, *token);
  if(auth) {
    return scope.Close(auth_to_object(auth));
  }
  return scope.Close(Undefined());
}

void add_auth_to_list(struct rs_authorization *auth, void *ctx) {
  Local<Array> auth_list = *reinterpret_cast<Local<Array>*>(ctx);
  auth_list->Set(Number::New(auth_list->Length()), auth_to_object(auth));
}

Handle<Value> List(const Arguments& args) {
  HandleScope scope;
  char *username = NULL;
  if(args.Length() > 0) {
    String::AsciiValue ascii_username(args[0]->ToString());
    username = strdup(*ascii_username);
  }
  open_authorizations("r");
  Local<Array> _array = Array::New();
  Local<Array> *array = &_array;
  list_authorizations(username, add_auth_to_list, array);
  close_authorizations();
  if(username) free(username);
  return scope.Close(*array);
}

void init(Handle<Object> exports) {
  exports->Set(String::NewSymbol("add"), FunctionTemplate::New(Add)->GetFunction());
  exports->Set(String::NewSymbol("remove"), FunctionTemplate::New(Remove)->GetFunction());
  exports->Set(String::NewSymbol("lookup"), FunctionTemplate::New(Lookup)->GetFunction());
  exports->Set(String::NewSymbol("list"), FunctionTemplate::New(List)->GetFunction());
}

NODE_MODULE(rs_serve_auth, init);
