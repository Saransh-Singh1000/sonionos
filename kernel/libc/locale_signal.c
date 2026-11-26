#include <locale.h>
#include <signal.h>
#include <string.h>
#include <limits.h>

static struct lconv c_locale = {
    .decimal_point     = ".",
    .thousands_sep     = "",
    .grouping          = "",
    .int_curr_symbol   = "",
    .currency_symbol   = "",
    .mon_decimal_point = "",
    .mon_thousands_sep = "",
    .mon_grouping      = "",
    .positive_sign     = "",
    .negative_sign     = "",
    .int_frac_digits   = CHAR_MAX,
    .frac_digits       = CHAR_MAX,
    .p_cs_precedes     = CHAR_MAX,
    .p_sep_by_space    = CHAR_MAX,
    .n_cs_precedes     = CHAR_MAX,
    .n_sep_by_space    = CHAR_MAX,
    .p_sign_posn       = CHAR_MAX,
    .n_sign_posn       = CHAR_MAX
};

char *setlocale(int category, const char *locale) {
    (void)category;
    if (!locale || locale[0] == 0 || strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0) {
        return "C";
    }
    return 0;
}

struct lconv *localeconv(void) { return &c_locale; }

static __sighandler_t sig_handlers[32];

__sighandler_t signal(int sig, __sighandler_t func) {
    if (sig <= 0 || sig >= 32) return SIG_ERR;
    __sighandler_t prev = sig_handlers[sig];
    sig_handlers[sig] = func;
    return prev;
}

int raise(int sig) {
    if (sig <= 0 || sig >= 32) return -1;
    __sighandler_t h = sig_handlers[sig];
    if (h != SIG_DFL && h != SIG_IGN && h != SIG_ERR) {
        sig_handlers[sig] = SIG_DFL;
        h(sig);
    }
    return 0;
}
