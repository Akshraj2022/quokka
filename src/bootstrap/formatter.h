#ifndef QK_FORMATTER_H
#define QK_FORMATTER_H

#include <stdbool.h>

char *qk_format_code(const char *source);
int   qk_format_file(const char *filepath, bool write_in_place, bool check_only);

#endif /* QK_FORMATTER_H */
