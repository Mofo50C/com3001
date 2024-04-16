#ifndef UTIL_H
#define UTIL_H 1
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _MNOOP do {} while(0)

#ifdef DEBUG
#define DEBUGLOG(fmt, ...) do { fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__, __VA_ARGS__); } while(0)
#define DEBUGPRINT(fmt, ...) do { fprintf(stderr, fmt "\n", __VA_ARGS__); } while(0)
#define DEBUGABORT() do { fprintf(stderr, "%s(): TX aborted\n", __func__); } while(0)
#else
#define DEBUGLOG(fmt, ...) _MNOOP
#define DEBUGPRINT(fmt, ...) _MNOOP
#define DEBUGABORT() _MNOOP
#endif

#define IS_EVEN(val) (((val) & 1) == 0)
#define IS_ODD(val) (((val) & 1) != 0)

int util_is_zeroed(const void *addr, size_t len);

/*
 * hash_64 - 64 bit Fowler/Noll/Vo-0 FNV-1a hash code
 *
 ***
 *
 * Fowler/Noll/Vo hash
 *
 * The basis of this hash algorithm was taken from an idea sent
 * as reviewer comments to the IEEE POSIX P1003.2 committee by:
 *
 *      Phong Vo (http://www.research.att.com/info/kpv/)
 *      Glenn Fowler (http://www.research.att.com/~gsf/)
 *
 * In a subsequent ballot round:
 *
 *      Landon Curt Noll (http://www.isthe.com/chongo/)
 *
 * improved on their algorithm.  Some people tried this hash
 * and found that it worked rather well.  In an EMail message
 * to Landon, they named it the ``Fowler/Noll/Vo'' or FNV hash.
 *
 * FNV hashes are designed to be fast while maintaining a low
 * collision rate. The FNV speed allows one to quickly hash lots
 * of data while maintaining a reasonable collision rate.  See:
 *
 *      http://www.isthe.com/chongo/tech/comp/fnv/index.html
 *
 * for more details as well as other forms of the FNV hash.
 *
 * Please do not copyright this code.  This code is in the public domain.
 *
 * LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
 * EVENT SHALL LANDON CURT NOLL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * By:
 *	chongo <Landon Curt Noll> /\oo/\
 *      http://www.isthe.com/chongo/
 *
 * Share and Enjoy!	:-)
 */

/*
 * 64 bit magic FNV-1a prime
 */
#define FNV_64_PRIME ((uint64_t)0x100000001b3ULL)
#define FNV1_64_INIT ((uint64_t)0xcbf29ce484222325ULL)

/* hashes a 64-bit integer input */
static inline uint64_t fnv64(uint64_t x)
{
	/* return our new hash value */
	return (FNV1_64_INIT ^ x) * FNV_64_PRIME;
}

#ifdef __cplusplus
}
#endif

#endif