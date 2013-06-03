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

#include "rs-serve.h"

/**
 * This file implements one-shot session management.
 * A session is only valid for a single request. It's only used to implement CSRF
 * protection.
 *
 * Session management is *not* thread safe at the moment.
 */

// number of session slots to pre-allocate
#define RS_SESSION_ALLOC_STEP 10
// maximum number of session slots to allocate
#define RS_SESSION_MAX_SLOTS 100
// maximum age of sessions in seconds
// (this is the time the user has to decide about a authorization request)
#define RS_SESSION_MAX_AGE 120

struct session_store {
  char **ids;
  struct session_data **datas;
  int count;
  int prealloc;
};

struct session_store session_store = {
  .ids = NULL,
  .datas = NULL,
  .count = 0,
  .prealloc = 0
};

void reset_session_store(void) {
  int i;
  for(i=0;i<session_store.count;i++) {
    free(session_store.ids[i]);
    free_session_data(session_store.datas[i]);
  }

  if(session_store.ids) {
    free(session_store.ids);
  }
  if(session_store.datas) {
    free(session_store.datas);
  }
  memset(&session_store, 0, sizeof(struct session_store));
}

static int resize_session_store(int inc) {
  char **ids = session_store.ids;
  struct session_data **datas = session_store.datas;
  int prealloc = session_store.prealloc;
  prealloc += inc;

  if(prealloc > RS_SESSION_MAX_SLOTS) {
    // don't allocate any more session slots to prevent DoS on /auth.
    // Instead reset the session store (possibly invalidating existing sessions).
    reset_session_store();
    // now re-try resizing session store (effectively allocating slots for just
    // `inc' number of sessions)
    return resize_session_store(inc);
  }

  ids = realloc(ids, prealloc * sizeof(char *));
  datas = realloc(datas, prealloc * sizeof(struct session_data*));

  if(ids == NULL || datas == NULL) {
    perror("failed to allocate session slots");
    return 1;
  }

  session_store.ids = ids;
  session_store.datas = datas;
  session_store.prealloc = prealloc;
  return 0;
}

void expire_session(evutil_socket_t ign1, short ign2, void *_session_id) {
  char *session_id = (char *) _session_id;
  log_debug("Session expired: %s", session_id);
  struct session_data *data = pop_session(session_id);
  free_session_data(data);
}

int push_session(char *session_id, struct session_data *data) {
  if(session_store.count == session_store.prealloc) {
    if(resize_session_store(RS_SESSION_ALLOC_STEP) != 0) {
      return 1;
    }
  }

  data->timer = evtimer_new(RS_EVENT_BASE, expire_session, session_id);
  struct timeval timeout = {
    .tv_sec = RS_SESSION_MAX_AGE,
    .tv_usec = 0
  };
  evtimer_add(data->timer, &timeout);

  int index = session_store.count;
  session_store.ids[index] = session_id;
  session_store.datas[index] = data;
  session_store.count++;
  return 0;
}

struct session_data *pop_session(const char *session_id) {
  struct session_data *session_data = NULL;

  int i;
  for(i=0;i<session_store.count;i++) {
    if(strcmp(session_store.ids[i], session_id) == 0) {
      session_data = session_store.datas[i];
      free(session_store.ids[i]);
      memmove(session_store.ids + i, session_store.ids + i + 1,
              (session_store.count - i) * sizeof(char*));
      memmove(session_store.datas + i, session_store.datas + i + 1,
              (session_store.count - i) * sizeof(struct session_data*));
      session_store.count--;
      break;
    }
  }

  if(session_data && session_data->timer != NULL) {
    evtimer_del(session_data->timer);
  }

  if(session_store.count < (session_store.prealloc - RS_SESSION_ALLOC_STEP)) {
    // shrink the session store again
    if(resize_session_store(- RS_SESSION_ALLOC_STEP) != 0) {
      fprintf(stderr, "BUG: failed to shrink session store. This should not be possible!\n");
    }
  }

  return session_data;
}

struct session_data *make_session_data(char *csrf_token) {
  struct session_data *data = malloc(sizeof(struct session_data));
  if(data == NULL) {
    perror("failed to allocate session data");
    return NULL;
  }
  data->csrf_token = csrf_token;
  data->created = time(NULL);
  data->timer = NULL;
  return data;
}

void free_session_data(struct session_data *session_data) {
  if(session_data->csrf_token) {
    free(session_data->csrf_token);
  }
  free(session_data);
}

void print_session_info(FILE *fp) {
  fprintf(fp, "Session info: MAX_SLOTS: %d, ALLOC_STEP: %d, MAX_AGE: %d\n",
          RS_SESSION_MAX_SLOTS, RS_SESSION_ALLOC_STEP, RS_SESSION_MAX_AGE);
  fprintf(fp, " (currently allocated: %d)\n", session_store.prealloc);
  fprintf(fp, "Active sessions (%d):\n", session_store.count);
  int i;
  time_t now = time(NULL);
  for(i=0;i<session_store.count;i++) {
    fprintf(fp, " - %s (age: %d)\n", session_store.ids[i], (int) (now - session_store.datas[i]->created));
  }
}
