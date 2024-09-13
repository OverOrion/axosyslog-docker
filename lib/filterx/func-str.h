/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 Szilard Parrag
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#ifndef FILTERX_FUNC_STR_H_INCLUDED
#define FILTERX_FUNC_STR_H_INCLUDED

#include "filterx/expr-function.h"

typedef struct _FilterXExprAffix
{
  FilterXFunction super;
  FilterXExpr *haystack_expr;
  struct
  {
    FilterXObject *haystack_obj;
    gchar *owned_str_value;
  } _haystack;
  FilterXExpr *needle_expr;
  FilterXObject *_needle_obj;
  struct
  {
    const gchar *borrowed_str_value;
    gchar *owned_str_value;
  } needle_str;
  FilterXObject *_needle_list;
  gssize needle_str_len;
  gboolean ignore_case;
  guint64 num_of_needles;
  gboolean (*process)(const gchar *haystack, gsize haystack_len, const gchar *needle, gsize needle_len);
} FilterXExprAffix;

void filterx_expr_affix_init(FilterXExprAffix *self, const gchar *function_name, FilterXExpr *haystack,
                             FilterXExpr *needle, gboolean ignorecase);
gboolean filterx_expr_affix_get_needle_str(FilterXExprAffix *self, const gchar **needle_str, gssize *needle_str_len);
gboolean filterx_expr_affix_get_haystack_str(FilterXExprAffix *self, const gchar **haystack, gssize *haystack_str_len);


typedef struct _FilterXFunctionStartsWith
{
  FilterXExprAffix super;
} FilterXFunctionStartsWith;
FilterXExpr *filterx_function_startswith_new(const gchar *function_name, FilterXFunctionArgs *args, GError **error);

#endif
