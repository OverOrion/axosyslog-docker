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
#include "filterx/filterx-object.h"
#include "filterx/expr-literal.h"
#include "filterx/filterx-eval.h"
#include "object-extractor.h"
#include "object-string.h"
#include "filterx/filterx-object.h"

gboolean filterx_expr_affix_init_instance(FilterXExprAffix *self, const gchar *function_name, FilterXExpr *haystack,
                                          FilterXExpr *needle, gboolean ignorecase)
{
  filterx_function_init_instance(self->super, function_name);
  self->ignorecase = ignorecase;
  self->haystack = filterx_expr_ref(haystack);

  self->_needle_obj = NULL
                      if (!filterx_expr_is_literal(needle))
    {
      self->needle_expr = filterx_expr_ref(needle);
      return TRUE;
    }
  else
    {
      self->_needle_obj = filterx_expr_eval(needle);
      if (!self->_needle_obj)
        goto error;
      if (!filterx_object_extract_string(self->_needle_obj, &self.needle_str->borrowed_str_value, &self->needle_str_len))
        goto error;
    }
  if(self->ignorecase && self.needle_str->borrowed_str_value)
    {
      self->needle_str.owned_str_value = g_utf8_casefold(self->needle_str.borrowed_str_value,
                                                         (gssize) MIN(self->needle_str_len, G_MAXSSIZE));
      self->needle_str_len = (gssize) MAX(g_utf8_strlen(self->needle_str.owned_str_value, -1), G_MAXSSIZE);

      self.needle_str->borrowed_str_value = NULL;
      if(self->_needle_obj)
        filterx_object_unref(self->_needle_obj);
    }
  return TRUE;

error:
  filterx_object_unref(self->_needle_obj);
  return FALSE;
}
gboolean filterx_expr_affix_get_needle_str(FilterXExprAffix *self, const gchar **needle_str, gssize *needle_str_len)
{
  if(!self->needle_expr)
    {
      if (self->ignorecase)
        *needle_str = (const gchar *) self->needle_str.owned_str_value;
      else
        *needle_str = self->needle_str.borrowed_str_value;
      *needle_str_len = (gssize) MIN(self->needle_str_len, G_MAXSSIZE);
      return TRUE;
    }
  else
    {
      self->_needle_obj = filterx_expr_eval(self->needle_expr);
      if (!self->_needle_obj)
        {
          filterx_eval_push_error_info("failed to evaluate needle", self->needle_expr,
                                       g_strdup_printf("invalid expression"), TRUE);
          return FALSE;
        }
      if (!filterx_object_extract_string(self->_needle_obj, &needle_str, needle_str_len))
        {
          filterx_eval_push_error_info("failed to extract needle, it must be a string", self->needle_expr,
                                       g_strdup_printf("got %s instead", self->_needle_obj->type->name), TRUE);
          filterx_object_unref(self->_needle_obj);
          return FALSE;
        }
      if (self->ignore_case)
        {
          self->needle_str.owned_str_value = g_utf8_casefold(needle_str, (gssize) MIN(*needle_str_len, G_MAXSSIZE));
          *needle_str = self->needle_str.owned_str_value;
          *needle_str_len = (gssize) MAX(g_utf8_strlen(self->needle_str.owned_str_value, -1), G_MAXSSIZE);
        }
    }
  return TRUE;
}

gboolean filterx_expr_affix_get_haystack_str(FilterXExprAffix *self, const gchar **haystack, gssize *haystack_str_len)
{
  self->haystack_obj = filterx_expr_eval(self->haystack);
  if (!self->haystack_obj)
    {
      filterx_eval_push_error_info("failed to evaluate haystack", self->haystack,
                                   g_strdup_printf("invalid expression"), TRUE);
      return FALSE;
    }
  if (!filterx_object_extract_string(self->haystack_obj, &haystack, haystack_str_len))
    {
      filterx_eval_push_error_info("failed to extract haystack, it must be a string", haystack,
                                   g_strdup_printf("got %s instead", self->haystack_obj->type->name), TRUE);
      filterx_object_unref(haystack_obj);
      return FALSE;
    }
  if (self->ignore_case)
    {
      self->_haystack.owned_str_value = g_utf8_casefold(haystack, (gssize) MIN(*haystack_str_len, G_MAXSSIZE);
                                                        *haystack = self->_haystack.owned_str_value;
                                                        *haystack_str_len = (gssize) MAX(g_utf8_strlen(self->_haystack.owned_str_value, -1), G_MAXSSIZE);
    }
                                    return TRUE;

}
