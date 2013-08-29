{
  "targets": [
    {
      "target_name": "rs_serve_auth",
      "sources": [ "auth.cc", "../../src/common/auth.c" ],
      "include_dirs": [ "../../src" ],
      "cflags": [ "-std=c99", "-ggdb" ],
      "link_settings": {
        "libraries": [ "-ldb" ]
      }
    }
  ]
}
