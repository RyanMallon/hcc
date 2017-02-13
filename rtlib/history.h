/*
 * History runtime library header
 */
#ifndef _RTLIB_HISTORY_H_
#define _RTLIB_HISTORY_H_

#ifdef __HCC__
# define __STATIC
# define __INLINE inline
# define __FUNC func
#else
# define __STATIC static
# define __INLINE
# define __FUNC
#endif

__FUNC __INLINE void __hist_p_store__(int *addr, int **ptr, int current,
				      int depth);
__FUNC __INLINE int __hist_p_load__(int *addr, int current,
				    int *ptr, int depth, int history);
__FUNC __INLINE void __hist_fp_store__(int *addr, int depth, int value);
__FUNC int __hist_fp_load__(int *addr, int depth);
__STATIC __FUNC void oldest_history(int *addr, int **ptr, int depth);

#endif /* _RTLIB_HISTORY_H_ */
