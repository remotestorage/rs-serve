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

static char *time_now() {
  static char timestamp[100]; // FIXME: not threadsafe!
  time_t t = time(NULL);
  struct tm *tmp = localtime(&t);
  strftime(timestamp, 99, "%F %T %z", tmp);
  return timestamp;
}

void log_warn(char *format, ...) {
  va_list ap;
  va_start(ap, format);
  char *timestamp = time_now();
  char new_format[strlen(timestamp) +
                  strlen(format) +
                  6 + // some space for PID
                  + 14];
  sprintf(new_format, "[%d] [%s] [WARN] %s\n", getpid(), timestamp, format);
  vfprintf(RS_LOG_FILE, new_format, ap);
  va_end(ap);
  fflush(RS_LOG_FILE);
}

void log_info(char *format, ...) {
  va_list ap;
  va_start(ap, format);
  char *timestamp = time_now();
  char new_format[strlen(timestamp) +
                  strlen(format) +
                  6 + // some space for PID
                  + 14];
  sprintf(new_format, "[%d] [%s] [INFO] %s\n", getpid(), timestamp, format);
  vfprintf(RS_LOG_FILE, new_format, ap);
  va_end(ap);
  fflush(RS_LOG_FILE);
}

void log_error(char *format, ...) {
  va_list ap;
  va_start(ap, format);
  char *timestamp = time_now();
  char new_format[strlen(timestamp) +
                  strlen(format) +
                  6 + // some space for PID
                  + 15];
  sprintf(new_format, "[%d] [%s] [ERROR] %s\n", getpid(), timestamp, format);
  vfprintf(RS_LOG_FILE, new_format, ap);
  va_end(ap);
  fflush(RS_LOG_FILE);
}

void dont_log_debug(const char *file, int line, char *format, ...) {};

void do_log_debug(const char *file, int line, char *format, ...) {
  va_list ap;
  va_start(ap, format);
  char *timestamp = time_now();
  char new_format[strlen(timestamp) +
                  strlen(format) +
                  strlen(file) +
                  5 + // (reasonable length for 'line')
                  6 + // space for PID
                  + 23];
  sprintf(new_format, "[%d] [%s] [DEBUG] %s:%d: %s\n", getpid(), timestamp, file, line, format);
  vfprintf(RS_LOG_FILE, new_format, ap);
  va_end(ap);
  fflush(RS_LOG_FILE);
}
