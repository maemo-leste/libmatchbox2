#ifndef _MB_HAVE_UTIL_H
#define _MB_HAVE_UTIL_H

#include <matchbox/core/mb-wm.h>

/* See http://rlove.org/log/2005102601 */
#if __GNUC__ >= 3
#if ! USE_GLIB_MAINLOOP
# define inline __attribute__ ((always_inline))
#endif
# define __pure__attribute__ ((pure))
# define __const__attribute__ ((const))
# define __noreturn__attribute__ ((noreturn))
# define __malloc__attribute__ ((malloc))
# define __must_check__attribute__ ((warn_unused_result))
# define __deprecated__attribute__ ((deprecated))
# define __used__attribute__ ((used))
# define __unused__attribute__ ((unused))
# define __packed__attribute__ ((packed))
# define LIKELY(x)__builtin_expect (!!(x), 1)
# define UNLIKELY(x)__builtin_expect (!!(x), 0)
#else
# define inline/* no inline */
# define __pure/* no pure */
# define __const/* no const */
# define __noreturn/* no noreturn */
# define __malloc/* no malloc */
# define __must_check/* no warn_unused_result */
# define __deprecated/* no deprecated */
# define __used/* no used */
# define __unused/* no unused */
# define __packed/* no packed */
# define LIKELY(x)(x)
# define UNLIKELY(x)(x)
#endif

#define streq(a,b)      (strcmp(a,b) == 0)
#define strcaseeq(a,b)  (strcasecmp(a,b) == 0)
#define unless(x)       if (!(x))

#define MBWMChildMask (SubstructureRedirectMask|SubstructureNotifyMask)
#define MBWMButtonMask (ButtonPressMask|ButtonReleaseMask)
#define MBWMMouseMask (ButtonMask|PointerMotionMask)
#define MBWMKeyMask (KeyPressMask|KeyReleaseMask)

void*
mb_wm_util_malloc0(int size);

void
mb_wm_util_fatal_error(char *msg);

void
mb_wm_util_warn (const char *format, ...);

/*  Misc */

Bool  /* True if matching */
mb_geometry_compare (MBGeometry *g1, MBGeometry *g2);

Bool  /* True if overlaps */
mb_geometry_intersects (MBGeometry *g1, MBGeometry *g2);

/* XErrors */

void
mb_wm_util_trap_x_errors(void);

int
mb_wm_util_untrap_x_errors(void);


/* List */


#define mb_wm_util_list_next(list) (list)->next
#define mb_wm_util_list_prev(list) (list)->prev
#define mb_wm_util_list_data(data) (list)->data

MBWMList*
mb_wm_util_list_alloc_item(void);

MBWMList*
mb_wm_util_list_remove(MBWMList *list, void *data);

int
mb_wm_util_list_length(MBWMList *list);

MBWMList*
mb_wm_util_list_get_last(MBWMList *list);

MBWMList*
mb_wm_util_list_get_first(MBWMList *list);

void*
mb_wm_util_list_get_nth_data(MBWMList *list, int n);

MBWMList*
mb_wm_util_list_append(MBWMList *list, void *data);

MBWMList*
mb_wm_util_list_prepend(MBWMList *list, void *data);

void
mb_wm_util_list_foreach (const MBWMList *list, MBWMListForEachCB func, void *userdata);

void
mb_wm_util_list_free (MBWMList * list);

MBWMRgbaIcon *
mb_wm_rgba_icon_new ();

void
mb_wm_rgba_icon_free (MBWMRgbaIcon *icon);

int
mb_wm_util_pixels_to_points (MBWindowManager *wm, int pixels);

#endif
