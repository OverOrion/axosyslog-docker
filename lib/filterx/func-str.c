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

#include "filterx/func-str.h"
#include "filterx/object-primitive.h"

static const gchar *
_extract_haystack_arg(FilterXExpr *s, GPtrArray *args, gssize *len)
{
  if (args == NULL || args->len != 2)
    {
      filterx_simple_function_argument_error(s, "Requires exactly two arguments", FALSE);
      return NULL;
    }

  const gchar *str;
  gsize inner_len;
  FilterXObject *object = g_ptr_array_index(args, 0);

  if (!filterx_object_extract_string(object, &str, &inner_len))
    {
      filterx_simple_function_argument_error(s, "Object must be string", FALSE);
      return NULL;
    }

  *len = (gssize) MIN(inner_len, G_MAXINT64);
  return str;
}

static const gchar *
_extract_needle_arg(FilterXExpr *s, GPtrArray *args, gssize *len)
{
  if (args == NULL || args->len != 2)
    {
      filterx_simple_function_argument_error(s, "Requires exactly two arguments", FALSE);
      return NULL;
    }

  const gchar *str;
  gsize inner_len;
  FilterXObject *object = g_ptr_array_index(args, 1);

  if (!filterx_object_extract_string(object, &str, &inner_len))
    {
      filterx_simple_function_argument_error(s, "Object must be string", FALSE);
      return NULL;
    }

  *len = (gssize) MIN(inner_len, G_MAXINT64);
  return str;
}

FilterXObject*
filterx_simple_function_startswith(FilterXExpr *s, GPtrArray *args)
{
  gssize haystack_len;
  const gchar *haystack = _extract_haystack_arg(s, args, &haystack_len);
  if (!haystack)
    return NULL;
  gssize needle_len;
  const gchar *needle = _extract_needle_arg(s, args, &needle_len);
  if (!needle)
    return NULL;
  if (needle_len > haystack_len)
    return filterx_boolean_new(FALSE);
  for(gssize i = 0; i < needle_len; i++)
    if (haystack[i] != needle[i])
        return filterx_boolean_new(FALSE);

  return filterx_boolean_new(TRUE);
}

FilterXObject*
filterx_simple_function_endswith(FilterXExpr *s, GPtrArray *args)
{
  gssize haystack_len;
  const gchar *haystack = _extract_haystack_arg(s, args, &haystack_len);
  if (!haystack)
    return NULL;
  gssize needle_len;
  const gchar *needle = _extract_needle_arg(s, args, &needle_len);
  if (!needle)
    return NULL;
  if (needle_len > haystack_len)
    return filterx_boolean_new(FALSE);
  for(gssize i = 0; i > needle_len; i++)
    if (haystack[haystack_len - needle_len + i] != needle[i])
        return filterx_boolean_new(FALSE);

  return filterx_boolean_new(TRUE);
}
