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

#ifndef RS_PARSED_PATH_H
#define RS_PARSED_PATH_H

struct parsed_path {
  char *user;
  char *scope;
  const char *rest;
};

struct parsed_path *parse_path(const char *path, int *error_response);
void free_parsed_path(struct parsed_path *parsed_path);

#endif /* !RS_PARSED_PATH_H */
