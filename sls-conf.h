/* Set up defines needed for each system.
 * I have not personally tested all of these, but there are not too many
 * combinations to be tried. All systems will probably need one of 
 * NEED_DIRENT or NEED_SYS_DIR. Systems that have NEED_DIRENT probably
 * need USE_DIRENT.
 */

#ifndef SLS_CONF_H
#define SLS_CONF_H

/* Pick a template for your system if listed, or create a new one for it.
 */
#  define SUNOS
#undef    SOLARIS
#undef    HPUX
#undef    IRIX
#undef    LINUX
#undef    DECOSF

/* Below are the templates which set the system specific compile options.
 */
#ifdef SUNOS
#undef    NEED_DIRENT
#  define NEED_SYS_DIR
#undef    USE_DIRENT
#endif /* SUNOS */


#ifdef SOLARIS
#  define NEED_DIRENT
#undef    NEED_SYS_DIR
#  define USE_DIRENT
#endif /* SOLARIS */


#ifdef HPUX
#undef    NEED_DIRENT
#  define NEED_SYS_DIR
#undef    USE_DIRENT
#endif /* HPUX */


#ifdef IRIX
#  define NEED_DIRENT
#  define NEED_SYS_DIR
#  define USE_DIRENT
#endif /* IRIX */


#ifdef LINUX
#  define NEED_DIRENT
#  define NEED_SYS_DIR
#  define USE_DIRENT
#endif /* LINUX */


#ifdef DECOSF
#  define NEED_DIRENT
#  define NEED_SYS_DIR
#  define USE_DIRENT
#endif /* DECOSF */

#endif /* SLS_CONF_H */
