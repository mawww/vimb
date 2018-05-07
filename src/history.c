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

#include <fcntl.h>
#include <glib.h>
#include <sys/file.h>

#include "ascii.h"
#include "completion.h"
#include "config.h"
#include "history.h"
#include "main.h"
#include "util.h"

#define HIST_FILE(t) (vb.files[file_map[t]])
typedef struct {
    char *first;
    char *second;
} History;

static gboolean fill_completion(GtkListStore *store, HistoryType type);
static void free_history(History *item);
static History *line_to_history(const char *uri, const char *title);
static GList *load(const char *file);
static void write_to_file(GList *list, const char *file);

/* map history types to files */
static const int file_map[HISTORY_LAST] = {
    FILES_COMMAND,
    FILES_SEARCH,
    FILES_HISTORY
};
extern struct Vimb vb;

/**
 * Write a new history entry to the end of history file.
 */
void history_add(Client *c, HistoryType type, const char *value, const char *additional)
{
    const char *file;

    /* Don't write a history entry if the history max size is set to 0. */
    if (!vb.config.history_max) {
        return;
    }

    file = HIST_FILE(type);
    if (additional) {
        util_file_append(file, "%s\t%s\n", value, additional);
    } else {
        util_file_append(file, "%s\n", value);
    }
}

/**
 * Makes all history items unique and force them to fit the maximum history
 * size and writes all entries of the different history types to file.
 */
void history_cleanup(void)
{
    const char *file;
    GList *list;

    /* don't cleanup the history file if history max size is 0 */
    if (!vb.config.history_max) {
        return;
    }

    for (HistoryType i = HISTORY_FIRST; i < HISTORY_LAST; i++) {
        file = HIST_FILE(i);
        list = load(file);
        write_to_file(list, file);
        g_list_free_full(list, (GDestroyNotify)free_history);
    }
}

gboolean history_fill_url_completion(GtkListStore *store, gpointer data)
{
    return fill_completion(store, HISTORY_URL);
}

gboolean history_fill_search_completion(GtkListStore *store, gpointer data)
{
    return fill_completion(store, HISTORY_SEARCH);
}

static gboolean fill_completion(GtkListStore *store, HistoryType type)
{
    gboolean found = FALSE;
    GList *src = NULL;
    GtkTreeIter iter;

    src = load(HIST_FILE(type));
    src = g_list_reverse(src);
    /* without any tags return all items */
    for (GList *l = src; l; l = l->next) {
        History *item = l->data;

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(
            store, &iter,
            COMPLETION_STORE_FIRST, item->first,
#ifdef FEATURE_TITLE_IN_COMPLETION
            COMPLETION_STORE_SECOND, item->second,
#endif
            -1
        );
        found = TRUE;
    }
    g_list_free_full(src, (GDestroyNotify)free_history);

    return found;
}

gboolean history_completion_visible_func(GtkTreeModel *model, GtkTreeIter *iter, char **input)
{
    char *url, *tags;
    char **parts;
    unsigned int len;
    gboolean found = TRUE;

    gtk_tree_model_get(model, iter, COMPLETION_STORE_FIRST, &url, COMPLETION_STORE_SECOND, &tags, -1);
    parts = g_strsplit(*input, " ", 0);
    len   = g_strv_length(parts);

    if (len) {
        unsigned int i;
        /* iterate over all query parts */
        for (i = 0; i < len; i++) {
            if (!(util_strcasestr(url, parts[i])
                || (tags && util_strcasestr(tags, parts[i])))
            ) {
                found = FALSE;
                break;
            }
        }
    }

    g_strfreev(parts);
    g_free(tags);
    g_free(url);

    return found;
}

/**
 * Retrieves the list of matching history items.
 * The list must be freed.
 */
GList *history_get_list(VbInputType type, const char *query)
{
    GList *result = NULL, *src = NULL;

    switch (type) {
        case INPUT_COMMAND:
            src = load(HIST_FILE(HISTORY_COMMAND));
            break;

        case INPUT_SEARCH_FORWARD:
        case INPUT_SEARCH_BACKWARD:
            src = load(HIST_FILE(HISTORY_SEARCH));
            break;

        default:
            return NULL;
    }

    /* generate new history list with the matching items */
    for (GList *l = src; l; l = l->next) {
        History *item = l->data;
        if (g_str_has_prefix(item->first, query)) {
            result = g_list_prepend(result, g_strdup(item->first));
        }
    }
    g_list_free_full(src, (GDestroyNotify)free_history);

    /* Prepend the original query as own item like done in vim to have the
     * original input string in input box if we step before the first real
     * item. */
    result = g_list_prepend(result, g_strdup(query));

    return result;
}

static void free_history(History *item)
{
    g_free(item->first);
    g_free(item->second);
    g_slice_free(History, item);
}

static History *line_to_history(const char *uri, const char *title)
{
    History *item = g_slice_new0(History);

    item->first  = g_strdup(uri);
    item->second = g_strdup(title);

    return item;
}

/**
 * Loads history items form file but eliminate duplicates in FIFO order.
 *
 * Returned list must be freed with (GDestroyNotify)free_history.
 */
static GList *load(const char *file)
{
    return util_file_to_unique_list(
        file, (Util_Content_Func)line_to_history, vb.config.history_max
    );
}

/**
 * Loads the entries from file, make them unique and write them back to file.
 */
static void write_to_file(GList *list, const char *file)
{
    FILE *f;
    if ((f = fopen(file, "w"))) {
        flock(fileno(f), LOCK_EX);

        /* overwrite the history file with new unique history items */
        for (GList *link = list; link; link = link->next) {
            History *item = link->data;
            if (item->second) {
                fprintf(f, "%s\t%s\n", item->first, item->second);
            } else {
                fprintf(f, "%s\n", item->first);
            }
        }

        flock(fileno(f), LOCK_UN);
        fclose(f);
    }
}
