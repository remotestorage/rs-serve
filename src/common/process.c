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

void *tree_root = NULL;

int compare_proc_pid(const void *a, const void *b) {
  pid_t pid_a = ((struct rs_process_info*)a)->pid;
  pid_t pid_b = ((struct rs_process_info*)b)->pid;
  return pid_a > pid_b ? 1 : (pid_a < pid_b ? -1 : 0);
}

int compare_proc_uid(const void *a, const void *b) {
  uid_t uid_a = ((struct rs_process_info*)a)->uid;
  uid_t uid_b = ((struct rs_process_info*)b)->uid;
  return uid_a > uid_b ? 1 : (uid_a < uid_b ? -1 : 0);
}

void process_add(struct rs_process_info *process_info) {
  log_debug("PROCESS_ADD({ pid:%ld, uid:%ld })",
            process_info->pid, process_info->uid);
  tsearch(process_info, &tree_root, compare_proc_pid);
}

struct rs_process_info *process_remove(pid_t pid) {
  log_debug("PROCESS_REMOVE({ pid:%ld })", pid);
  struct rs_process_info process_info;
  process_info.pid = pid;
  void *result = tdelete(&process_info, &tree_root,
                         compare_proc_pid);
  if(result) {
    return *((struct rs_process_info **)result);
  } else {
    return NULL;
  }
}

struct rs_process_info *process_find_uid(uid_t uid) {
  log_debug("PROCESS_FIND({ uid:%ld })", uid);
  struct rs_process_info process_info;
  memset(&process_info, 0, sizeof(struct rs_process_info));
  process_info.uid = uid;
  void *result = tfind(&process_info, &tree_root, compare_proc_uid);
  if(result) {
    return *((struct rs_process_info**)result);
  } else {
    return NULL;
  }
}

static void process_kill_one(const void *nodep, const VISIT which, const int depth) {
  if(which == leaf) {
    struct rs_process_info *process_info = *(struct rs_process_info**)nodep;
    log_debug("Sending HUP signal to child: %ld", process_info->pid);
    kill(process_info->pid, SIGHUP);
  }
}

void process_kill_all() {
  twalk(tree_root, process_kill_one);
}

void stop_process(evutil_socket_t fd, short events, void *_process_info) {
  struct rs_process_info *process_info = _process_info;
  kill(process_info->pid, SIGHUP);
}

int start_process(struct rs_process_info *info, rs_process_main process_main,
                  int initial_fd, char *initial_buf, int initial_buf_len) {
  // setup socket pair
  int sock_fds[2] = { 0, 0 };
  if(socketpair(AF_UNIX, SOCK_DGRAM, 0, sock_fds) != 0) {
    perror("Failed to create socketpair() for new process");
    return -1;
  }

  // set non-blocking
  if(fcntl(sock_fds[0], F_SETFL, O_NONBLOCK) != 0 ||
     fcntl(sock_fds[1], F_SETFL, O_NONBLOCK) != 0) {
    perror("Failed to set O_NONBLOCK on a socket");
    return -1;
  }

  info->socket_out = sock_fds[0];
  info->socket_in = sock_fds[1];

  // fork
  pid_t child_pid = fork();
  if(child_pid == 0) {
    close(info->socket_in);
    // run processes main()
    exit(process_main(info, initial_fd, initial_buf, initial_buf_len));
  } else if(child_pid > 0) {
    close(info->socket_out);
    info->pid = child_pid;
    /* info->timeout_event = evtimer_new(RS_EVENT_BASE, */
    /*                                   stop_process, */
    /*                                   info); */
    /* struct timeval tv = { */
    /*   .tv_sec = 5, */
    /*   .tv_usec = 0 */
    /* }; */
    /* evtimer_add(info->timeout_event, &tv); */
    process_add(info);
    return 0;
  } else {
    perror("Failed to fork() child process");
    return -1;
  }
}

void send_fd_to_process(struct rs_process_info *process, int fd, char *databuf, int databuflen) {
  struct msghdr msg = {0};
  struct cmsghdr *cmsg;
  int fds[1] = {fd};
  char buf[CMSG_SPACE(sizeof(fds))];

  struct iovec iov;
  iov.iov_base = databuf;
  iov.iov_len = databuflen;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = buf;
  msg.msg_controllen = sizeof(buf);
  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  memcpy(CMSG_DATA(cmsg), &fds, sizeof(int));
  msg.msg_controllen = cmsg->cmsg_len;
  log_debug("sending fd %d via socket %d", fd, process->socket_in);
  if(sendmsg(process->socket_in, &msg, 0) < 0) {
    log_error("sendmsg() failed: %s", strerror(errno));
  }
}
