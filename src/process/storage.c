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

static void receive_request_fd(evutil_socket_t fd, short events, void *_process_info) {
  struct rs_process_info *process_info = _process_info;
  struct msghdr msg = {0};
  struct cmsghdr *cmsg;
  int  buf[CMSG_SPACE(sizeof(int))];

  char *databuf = malloc(4096);
  if(databuf == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    return;
  }
  *databuf = 0;
  struct iovec iov;
  iov.iov_base = databuf;
  iov.iov_len = 4096;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = buf;
  msg.msg_controllen = sizeof(buf);
  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  if(recvmsg(fd, &msg, 0) < 0) {
    log_error("recvmsg() failed: %s", strerror(errno));
    return;
  }
  int request_fd = CMSG_DATA(cmsg)[0];
  log_debug("Received fd: %d", request_fd);
  // make sure that socket is non blocking:
  fcntl(request_fd, F_SETFL, O_NONBLOCK);
  process_request(process_info, request_fd, databuf, 4096);
}

static void storage_exit(struct rs_process_info *process_info) {
  log_info("stopping process: storage (uid: %ld)", process_info->uid);
  event_base_loopexit(process_info->base, NULL);
}

static void read_signal(evutil_socket_t sfd, short events, void *_process_info) {
  struct signalfd_siginfo siginfo;
  if(read(sfd, &siginfo, sizeof(siginfo)) == -1) {
    log_error("failed to read() from signal fd: %s", strerror(errno));
    return;
  }

  if(siginfo.ssi_signo == SIGHUP) {
    log_info("Received SIGHUP, will exit now.");
    storage_exit((struct rs_process_info*) _process_info);
  } else {
    log_error("Unhandled signal: %s", strsignal(siginfo.ssi_signo));
  }
}

int storage_main(struct rs_process_info *process_info,
                 int initial_fd, char *initial_buf, int initial_buf_len) {
  log_info("starting process: storage (uid: %ld)", process_info->uid);

  /** CHROOT TO HOME DIRECTORY **/

  struct passwd *user_entry = getpwuid(process_info->uid);
  char *chroot_directory;
  if(RS_HOME_SERVE_ROOT) {
    chroot_directory = malloc(strlen(user_entry->pw_dir) +
                              RS_HOME_SERVE_ROOT_LEN + 2);
    sprintf(chroot_directory, "%s/%s", user_entry->pw_dir, RS_HOME_SERVE_ROOT);
  } else {
    chroot_directory = user_entry->pw_dir;
  }
  if(chroot(chroot_directory) != 0) {
    log_error("Failed to chroot to directory \"%s\": %s",
              chroot_directory, strerror(errno));
    exit(EXIT_FAILURE);
  }

  log_debug("changed root to \"%s\"", chroot_directory);

  /** SET SOME OPTIONS **/

  // set process name
  char process_name[16];
  snprintf(process_name, 16, "rs-serve [user: %s]", user_entry->pw_name);
  if(prctl(PR_SET_NAME, process_name, 0, 0, 0) != 0) {
    log_error("Failed to set process name: %s", strerror(errno));
  }

  // tell kernel to send us SIGHUP when the parent process dies
  // for whatever reason.
  if(prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0) != 0) {
    log_error("Failed to specify parent-death-signal: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  /** DROP PRIVILEGES **/

  if(setuid(process_info->uid) != 0) {
    log_error("failed to drop privileges: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  log_debug("dropped privileges");

  /** SETUP EVENT BASE **/

  process_info->base = event_base_new();

  /** SETUP SIGNALS **/

  sigset_t sigmask;
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGHUP);
  if(sigprocmask(SIG_BLOCK, &sigmask, NULL) != 0) {
    log_error("sigprocmask() failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
  int sfd = signalfd(-1, &sigmask, SFD_NONBLOCK);
  if(sfd == -1) {
    log_error("signalfd() failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  struct event *signal_event = event_new(process_info->base, sfd,
                                         EV_READ | EV_PERSIST,
                                         read_signal, process_info);
  event_add(signal_event, NULL);

  /** SETUP SOCKET THAT INFORMS ABOUT REQUESTS **/

  // socket pair sends request fds to child process
  struct event *socket_event = event_new(process_info->base,
                                         process_info->socket_out,
                                         EV_READ | EV_PERSIST,
                                         receive_request_fd,
                                         process_info);
  event_add(socket_event, NULL);

  if(initial_fd > 0) {
    // got initial fd passed. -> process it immediately!
    process_request(process_info, initial_fd, initial_buf, initial_buf_len);
  }

  /** RUN EVENT LOOP **/

  // run loop
  event_base_dispatch(process_info->base);
  // loop done (probably storage_exit has been called)
  event_base_free(process_info->base);
  exit(EXIT_SUCCESS);
}
