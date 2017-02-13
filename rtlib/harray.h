/*
 * harray.h
 * Ryan Mallon (2006)
 *
 * Header file for history arrays
 *
 */
#ifndef _RTLIB_HARRAY_H_
#define _RTLIB_HARRAY_H_

#ifdef __HCC__
# define __STATIC
# define __INLINE inline
# define __FUNC func
#else
# define __STATIC static
# define __INLINE
# define __FUNC
#endif

__FUNC void __hist_iw_init__(int *addr, int **ptr, int size);

__FUNC __INLINE void __hist_iw_store__(int *addr, int **ptr, int *current,
				       int index, int size, int depth,
				       int value);

__FUNC __INLINE int __hist_iw_load__(int *addr, int *ptr, int *current,
				     int index, int depth, int history,
				     int size);

__FUNC void __hist_daw_init__(int *addr, int **ptr, int *clist,
			      int **clist_ptr, int depth);

__FUNC __INLINE void __hist_naw_store__(int *addr, int **ptr, int *current,
			       int depth, int size, int index, int value);

__FUNC  void __hist_daw_store__(int *addr, int **ptr, int *current,
					int depth, int size, int *clist,
					int **clist_ptr, int index, int value);

__FUNC __INLINE int *__hist_aw_load__(int *addr, int *current, int *ptr,
				      int depth, int size, int index,
				      int history);

#endif /* _RTLIB_HARRAY_H_ */
