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

#ifndef RS_PROCESS_H
#define RS_PROCESS_H

struct rs_process_info {
  // PID of the process. Filled by parent process.
  pid_t pid;
  // UID the process is associated with.
  // Only applicable for storage processes.
  uid_t uid;

  struct event_base *base;

  struct event *timeout_event;

  // socket pair to communicate between parent and child.
  int socket_in;
  int socket_out;
};

typedef int (*rs_process_main)(struct rs_process_info*);

/* PROCESS HANDLING */

int start_process(struct rs_process_info *info, rs_process_main process_main);
void send_fd_to_process(struct rs_process_info *process, int fd, char *buf, int buflen);
struct rs_process_info *process_find_uid(uid_t uid);
struct rs_process_info *process_remove(pid_t pid);
void process_kill_all();

/* STORAGE PROCESS */

int storage_main(struct rs_process_info *process_info);

#endif /* !RS_PROCESS_H */
