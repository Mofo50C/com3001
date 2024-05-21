#ifndef UTIL_H
#define UTIL_H 1
#define _GNU_SOURCE
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>

#define CFENCE  __asm__ volatile ("":::"memory")
#define CAS(ptr, expected, desired)\
__atomic_compare_exchange_n(ptr, expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)

#define _MNOOP do {} while(0)
#define _DLOG(fmt, ...) do { fprintf(stderr, "[%d] %s:%d:%s(): " fmt "\n", gettid(), __FILE__, __LINE__, __func__, ##__VA_ARGS__); } while(0)
#define _DPRINT(fmt, ...) do { fprintf(stderr, fmt "\n", ##__VA_ARGS__); } while(0)
#define _DABORT() do { fprintf(stderr, "[%d] TX ABORTED: %s()\n", gettid(), __func__); } while(0)

// #define DEBUG

#if defined(DEBUG)
#define DEBUGLOG(fmt, ...) _DLOG(fmt, ##__VA_ARGS__)
#define DEBUGABORT() _DABORT()
#define DEBUGPRINT(fmt, ...) _DPRINT(fmt, ##__VA_ARGS__)
#elif defined(DEBUGERR)
#define DEBUGLOG(fmt, ...) _DLOG(fmt, ##__VA_ARGS__)
#define DEBUGABORT() _DABORT()
#define DEBUGPRINT(fmt, ...) _MNOOP
#else
#define DEBUGLOG(fmt, ...) _MNOOP
#define DEBUGABORT() _MNOOP
#define DEBUGPRINT(fmt, ...) _MNOOP
#endif


#define IS_EVEN(val) (((val) & 1) == 0)
#define IS_ODD(val) (((val) & 1) != 0)

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
	uint64_t hval = FNV1_64_INIT;
	const unsigned char *px = (const unsigned char *)&x;
	for (int i = 0; i < 8; i++) {
		hval = ((uint64_t)*px++ ^ hval) * FNV_64_PRIME;
	}
	return hval;
}

#endif