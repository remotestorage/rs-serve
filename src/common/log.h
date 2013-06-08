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

#ifndef RS_COMMON_LOG_H
#define RS_COMMON_LOG_H

void log_info(char *format, ...);
void log_warn(char *format, ...);
void log_error(char *format, ...);
void do_log_debug(const char *file, int line, char *format, ...);
void dont_log_debug(const char *file, int line, char *format, ...);
extern void (*current_log_debug)(const char *file, int line, char *format, ...);
#define log_debug(...) current_log_debug(__FILE__, __LINE__, __VA_ARGS__)

#endif /* !RS_COMMON_LOG_H */
