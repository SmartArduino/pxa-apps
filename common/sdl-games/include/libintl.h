/* English fallback for games without packaged message catalogs.
 * GPL-3.0-or-later. Upstream bitmap font builds use ASCII message text. */
#pragma once
static inline const char *gettext(const char *message) { return message; }
static inline const char *ngettext(const char *one,const char *many,unsigned long n) { return n==1?one:many; }
static inline const char *dgettext(const char *domain,const char *message) { (void)domain;return message; }
static inline const char *bindtextdomain(const char *domain,const char *path) { (void)domain;return path; }
static inline const char *textdomain(const char *domain) { return domain; }
static inline const char *bind_textdomain_codeset(const char *domain,const char *encoding) { (void)domain;return encoding; }
