/*
 * Copyright (C) 2024 Alexey Sakovets <alexeysakovets@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#define _GNU_SOURCE /* asprintf */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include <glib.h>

#include <archive.h>
#include <archive_entry.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#define FICTIONBOOK_NS "http://www.gribuser.ru/xml/fictionbook/2.0"

// Note: do not bother freeing resources as thumbnailers are short-lived creatures.

static int
get_zipped_contents (const char * path, char ** buffer, size_t * out_size)
{
    struct archive *a;
    struct archive_entry *entry;
    int r;

    a = archive_read_new ();
    archive_read_support_format_zip (a);
    r = archive_read_open_filename (a, path, 10240);
    if (r != ARCHIVE_OK) {
        g_warning ("archive_read_open_filename failed\n");
        return -1;
    }

    r = archive_read_next_header (a, &entry);
    if (r != ARCHIVE_OK) {
        g_warning ("archive_read_next_header failed\n");
        return -1;
    }

    size_t size = archive_entry_size (entry);

    char * p = malloc (size);

    if (p == 0) return -1;

    if (archive_read_data (a, p, size) <= 0) return -1;

    *buffer = p;
    *out_size = size;
    return 0;
}

static int
get_raw_contents (const char * path, char ** buffer, size_t * out_size)
{
    FILE *f;
    int r = -1;

    f = fopen (path, "r");
    if (f == 0) return -1;

    fseek (f, 0, SEEK_END);
    long s = ftell (f);
    rewind (f);

    char * p = malloc (s);
    if (p != 0) {
        fread (p, 1, s, f);
        *buffer = p;
        *out_size = s;
        r = 0;
    }

    fclose (f);
    return r;
}

static int
get_contents (const char * path, char ** buffer, size_t * out_size)
{
    int r;

    r = get_zipped_contents (path, buffer, out_size);

    if (r) {
        r = get_raw_contents (path, buffer, out_size);
    }

    return r;
}


char * file_to_data (const char  *path,
                     gsize       *ret_length,
                     GError     **error)
{
    xmlDocPtr doc;
    xmlNodePtr root;
    xmlXPathContextPtr xpath_ctx;
    xmlXPathObjectPtr xpath_obj;
    xmlNodeSetPtr nodeset;

    char * buff;
    size_t buffsize;

    if (get_contents (path, &buff, &buffsize)) return 0;

    doc = xmlParseMemory (buff, buffsize);
    if (doc == 0) {
        g_warning ("document not parsed successfully\n");
        return 0;
    }

    root = xmlDocGetRootElement(doc);

    if (root == 0) {
        g_warning ("empty document\n");
        return 0;
    }

    xpath_ctx = xmlXPathNewContext (doc);
    if (xpath_ctx == 0) {
        g_warning ("failed to create XPath context\n");
        return 0;
    }

    if (xmlXPathRegisterNs (xpath_ctx, BAD_CAST "ns", BAD_CAST FICTIONBOOK_NS)) {
        g_warning ("failed to register XPath namespace \"%s\"\n", FICTIONBOOK_NS);
        return 0;
    }

    xpath_obj = xmlXPathEval (BAD_CAST "//ns:FictionBook/ns:description/ns:title-info/ns:coverpage/ns:image", xpath_ctx);

    if (xpath_obj == 0) {
        g_warning ("failed to evaluate XPath expression \"%s\"\n",
                  "//FictionBook/description/title-info/coverpage/image");
        return 0;
    }

    nodeset = xpath_obj->nodesetval;

    if (nodeset == 0) {
        g_warning ("cover not found");
        return 0;
    }

    xmlChar * coverlink = xmlGetProp(*nodeset->nodeTab, BAD_CAST "href");

    if (coverlink == 0) {
        g_warning ("href attribute missing\n");
        return 0;
    }

    if (*coverlink != '#') {
        g_warning ("non-inline cover images are not supported\n");
        return 0;
    }

    char * query;

    asprintf (&query, "//ns:FictionBook/ns:binary[@id='%s']", coverlink + 1);

    xpath_obj = xmlXPathEval (BAD_CAST query, xpath_ctx);

    if (xpath_obj == 0) {
        g_warning ("failed to evaluate XPath expression \"%s\"\n",
                   query);
        return 0;
    }

    nodeset = xpath_obj->nodesetval;
    if (nodeset == 0) {
        return 0;
    }

    return (char *) g_base64_decode ((gchar *) xmlNodeGetContent (*nodeset->nodeTab), ret_length);
}
