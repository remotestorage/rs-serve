/*
 * rs-serve - (c) 2013 Niklas E. Cathor
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <rs-serve.h>

int set_xattr(const char *path, const char *key, const char *value, size_t len) {
  if(setxattr(path, key, value, len, 0) != 0) {
    log_error("setxattr() failed: %s", strerror(errno));
    return -1;
  }
  return 0;
}

int set_meta_attr(const char *path, const char *key, const char *value, size_t len) {
  log_error("set_meta_attr() not implemented!");
  abort();
}

char *get_xattr(const char *path, const char *key, size_t maxlen) {
  int len = 32;
  char *value = NULL;
  for(value = malloc(len);len<=maxlen;value = realloc(value, len+=16)) {
    if(value == NULL) {
      log_error("malloc() / realloc() failed: %s", strerror(errno));
      return NULL;
    }
    int actual_len = getxattr(path, key, value, len);
    if(actual_len > 0) {
      value[actual_len] = 0;
      return value;
    } else {
      if(errno == ERANGE) {
        // buffer too small.
        continue;
      } else if(errno == ENOATTR) {
        // attribute not set
        free(value);
        return NULL;
      } else if(errno == ENOTSUP) {
        // xattr not supported
        log_error("File system doesn't support extended attributes! You may want to use another one.");
        free(value);
        return NULL;
      } else {
        log_error("Unexpected error while getting %s attribute: %s", key, strerror(errno));
        free(value);
        return NULL;
      }
    }
  }
  log_error("%s attribute seems to be longer than %d bytes. That is simply unreasonable.", key, maxlen);
  free(value);
  return NULL;
}

char *get_meta_attr(const char *path, const char *key, size_t maxlen) {
  log_error("get_meta_attr() not implemented!");
  abort();
}

char *content_type_from_xattr(const char *path) {
  char *mime_type = get_meta(path, "mime_type", 128);
  if(mime_type == NULL) {
    return NULL;
  }
  char *charset = get_meta(path, "charset", 64);
  if(charset == NULL) {
    return mime_type;
  }
  int mt_len = strlen(mime_type);
  char *content_type = realloc(mime_type, mt_len + strlen(charset) + 10 + 1);
  if(content_type == NULL) {
    log_error("realloc() failed: %s", strerror(errno));
    free(charset);
    return mime_type;
  }
  sprintf(content_type + mt_len, "; charset=%s", charset);
  free(charset);
  return content_type;
}

int content_type_to_xattr(const char *path, const char *content_type) {
  char *content_type_copy = strdup(content_type), *saveptr = NULL;
  if(content_type_copy == NULL) {
    log_error("strdup() failed: %s", strerror(errno));
    return -1;
  }
  char *mime_type = strtok_r(content_type_copy, ";", &saveptr);
  log_debug("extracted mime type: %s", mime_type);
  char *rest = strtok_r(NULL, "", &saveptr);
  char *charset_begin = NULL, *charset = NULL;
  if(rest) {
    charset_begin = strstr(rest, "charset=");
    if(charset_begin) {
      charset = charset_begin + 8;
      log_debug("extracted charset: %s", charset);
    }
  }
  if(charset == NULL) {
    // FIXME: should this rather be binary or us-ascii?
    charset = "UTF-8";
    log_debug("guessed charset: %s", charset);
  }
  set_meta(path, "mime_type", mime_type, strlen(mime_type) + 1);
  set_meta(path, "charset", charset, strlen(charset) + 1);
  free(content_type_copy);
  return 0;
}

char *get_etag(const char *disk_path) {
  size_t etag_len = SHA_DIGEST_LENGTH * 2;
  char *etag = get_meta(disk_path, "etag", etag_len + 1);
  if(etag == NULL) {
    log_debug("%s: etag not set, calculating SHA1 sum", disk_path);
    etag = malloc(etag_len + 1);
    if(etag == NULL) {
      log_error("malloc() failed: %s", strerror(errno));
      return NULL;
    }
    *etag = 0;
    SHA_CTX c;
    if(SHA1_Init(&c) != 1) {
      log_error("SHA1_Init() failed");
      free(etag);
      return NULL;
    }
    int fd = open(disk_path, O_RDONLY);
    if(fd == -1) {
      log_error("open() failed: %s", strerror(errno));
      free(etag);
      return NULL;
    }
    struct stat stat_buf;
    memset(&stat_buf, 0, sizeof(struct stat));
    if(stat(disk_path, &stat_buf) == -1) {
      log_error("stat() failed: %s", strerror(errno));
      free(etag);
      return NULL;
    }
    unsigned char buf[4096];
    if(S_ISDIR(stat_buf.st_mode)) {
      // DIRECTORY: calculate sum of child etags
      size_t path_len = strlen(disk_path);
      char name_buf[path_len + pathconf(disk_path, _PC_NAME_MAX) + 2];
      char *path_concat_format = disk_path[path_len - 1] == '/' ? "%s%s" : "%s/%s";
      DIR *dir = opendir(disk_path);
      if(dir == NULL) {
        log_error("opendir() failed: %s", strerror(errno));
        free(etag);
        return NULL;
      }
      size_t child_len = offsetof(struct dirent, d_name) + pathconf(disk_path, _PC_NAME_MAX) + 1;
      struct dirent *child = malloc(child_len), *result = NULL;
      if(child == NULL) {
        log_error("malloc() failed: %s", strerror(errno));
        free(etag);
        return NULL;
      }
      for(readdir_r(dir, child, &result); result != NULL;
          readdir_r(dir, child, &result)) {
        if(strcmp(child->d_name, ".") == 0 ||
           strcmp(child->d_name, "..") == 0)
          continue;
        sprintf(name_buf, path_concat_format, disk_path, child->d_name);
        SHA1_Update(&c, get_etag(name_buf), etag_len);
      }
      closedir(dir);
      free(child);
    } else {
      // FILE: calculate sum of contents
      ssize_t buf_bytes;
      for(buf_bytes = read(fd, buf, 4096); buf_bytes > 0;
          buf_bytes = read(fd, buf, 4096)) {
        if(SHA1_Update(&c, buf, buf_bytes) != 1) {
          log_error("SHA1_Update() failed");
          free(etag);
          return NULL;
        }
      }
      if(buf_bytes < 0) { // error during read()
        log_error("read() failed: %s", strerror(errno));
        free(etag);
        return NULL;
      }
    }
    if(SHA1_Final(buf, &c) != 1) {
      log_error("SHA1_Final() failed");
      free(etag);
      return NULL;
    }
    int i;
    for(i=0;i<SHA_DIGEST_LENGTH;i++) {
      sprintf(etag, "%s%02x", etag, buf[i]);
    }
    set_meta(disk_path, "etag", etag, etag_len);
    // TODO: set last update time of etag also to detect oob edits.
  }
  return etag;
}
