/**
 * vimb - a webkit based vim like browser.
 *
 * Copyright (C) 2012-2018 Daniel Carl
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/.
 */

#ifndef _COMPLETION_H
#define _COMPLETION_H

#include <glib.h>
#include "main.h"

typedef void (*CompletionSelectFunc) (char *match, gpointer data);
typedef gboolean (*CompletionFillFunc) (GtkListStore *store, gpointer data);
typedef struct completion Completion;

enum {
    COMPLETION_STORE_FIRST,
#ifdef FEATURE_TITLE_IN_COMPLETION
    COMPLETION_STORE_SECOND,
#endif
    COMPLETION_ADDITIONAL,
    COMPLETION_STORE_NUM
};


Completion *completion_new(void);
void completion_free(Completion *comp);
gboolean completion_start(Completion *comp, GtkTreeModel *model,
        CompletionSelectFunc selfunc, gpointer data, GtkWidget *widget,
        gboolean back);
gboolean completion_next(Completion *comp, gboolean back);
gboolean completion_is_active(Completion *comp);
void completion_stop(Completion *comp);

#endif /* end of include guard: _COMPLETION_H */
