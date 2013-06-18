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

#ifndef RS_COMMON_USER_H
#define RS_COMMON_USER_H

uid_t user_get_uid(const char *username);
struct passwd *user_get_entry(const char *username, char **bufptr);

#endif /* !RS_COMMON_USER_H */
