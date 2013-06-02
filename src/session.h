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

#ifndef RS_SESSION_H
#define RS_SESSION_H

struct session_data {
  char *csrf_token;
};

struct session_data *make_session_data(char *csrf_token);
void free_session_data(struct session_data *session_data);
int push_session(char *session_id, struct session_data *data);
struct session_data *pop_session(const char *session_id);
void reset_session_store(void);

#endif
