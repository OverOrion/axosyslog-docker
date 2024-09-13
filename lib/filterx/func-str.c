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
#include "filterx/object-json.h"
#include "filterx/object-list-interface.h"

static void _filterx_expr_affix_free(FilterXExprAffix *self)
{
  if (self->ignore_case)
    {
      g_free(self->_haystack.owned_str_value);
      g_free(self->needle_str.owned_str_value);
    }
  filterx_object_unref(self->_needle_obj);
  filterx_expr_unref(self->needle_expr);

  filterx_object_unref(self->_haystack.haystack_obj);
  filterx_expr_unref(self->haystack_expr);

  filterx_function_free_method(&self->super);
}

gboolean
_filterx_expr_affix_init_needle_list(FilterXExprAffix *self)
{
  guint64 size;
  if (!filterx_object_len(self->_needle_obj, &size))
    return FALSE;
  self->num_of_needles = size;
  self->_needle_list = self->_needle_obj;
  self->_needle_obj = NULL;

  return TRUE;
}

gboolean
_filterx_expr_affix_get_nth_needle(FilterXExprAffix *self, guint64 index, const gchar **needle_str,
                                   gssize *needle_str_len)
{

  filterx_object_unref(self->_needle_obj);
  self->_needle_obj = filterx_list_get_subscript(self->_needle_list, index);
  if (!self->_needle_obj)
    goto error;
  if (filterx_object_is_type(self->_needle_obj, &FILTERX_TYPE_NAME(string)) &&
      !filterx_object_extract_string(self->_needle_obj, &self->needle_str.borrowed_str_value, &self->needle_str_len))
    goto error;
  *needle_str = (const gchar *) self->needle_str.owned_str_value;
  *needle_str_len = (gssize) MIN(self->needle_str_len, G_MAXSSIZE);
  if(self->ignore_case)
    {
      g_free(self->needle_str.owned_str_value);
      self->needle_str.owned_str_value = g_utf8_casefold(self->needle_str.borrowed_str_value,
                                                         (gssize) MIN(self->needle_str_len, G_MAXSSIZE));
      self->needle_str_len = (gssize) MIN(g_utf8_strlen(self->needle_str.owned_str_value, -1), G_MAXSSIZE);

      self->needle_str.borrowed_str_value = NULL;

      *needle_str = self->needle_str.owned_str_value;
      *needle_str_len = (gssize) MIN(g_utf8_strlen(self->needle_str.owned_str_value, -1), G_MAXSSIZE);
    }
    return TRUE;
error:
  filterx_object_unref(self->_needle_obj);
  return FALSE;
}

gboolean
filterx_expr_affix_init_instance(FilterXExprAffix *self, const gchar *function_name, FilterXExpr *haystack,
                                 FilterXExpr *needle, gboolean ignorecase)
{
  filterx_function_init_instance(&self->super, function_name);
  self->num_of_needles = 0;
  self->ignore_case = ignorecase;
  self->haystack_expr = haystack;

  self->_needle_obj = NULL;
  if (!filterx_expr_is_literal(needle))
    {
      self->needle_expr = needle;
      return TRUE;
    }
  else
    {
      self->_needle_obj = filterx_expr_eval(needle);
      if (!self->_needle_obj)
        goto error;
      if (filterx_object_is_type(self->_needle_obj, &FILTERX_TYPE_NAME(string)) &&
          !filterx_object_extract_string(self->_needle_obj, &self->needle_str.borrowed_str_value, &self->needle_str_len))
        goto error;
    }
  if(self->ignore_case)
    {
      self->needle_str.owned_str_value = g_utf8_casefold(self->needle_str.borrowed_str_value,
                                                         (gssize) MIN(self->needle_str_len, G_MAXSSIZE));
      self->needle_str_len = (gssize) MIN(g_utf8_strlen(self->needle_str.owned_str_value, -1), G_MAXSSIZE);

      self->needle_str.borrowed_str_value = NULL;
    }
  return TRUE;

error:
  filterx_object_unref(self->_needle_obj);
  return FALSE;
}

gboolean
filterx_expr_affix_get_needle_str(FilterXExprAffix *self, const gchar **needle_str, gssize *needle_str_len)
{
  if(!self->needle_expr)
    {
      if (self->ignore_case)
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
      else if (filterx_object_is_type(self->_needle_obj, &FILTERX_TYPE_NAME(json_array)))
        return _filterx_expr_affix_init_needle_list(self);
        if (!filterx_object_extract_string(self->_needle_obj, needle_str, needle_str_len))
        {
          filterx_eval_push_error_info("failed to extract needle, it must be a string", self->needle_expr,
                                       g_strdup_printf("got %s instead", self->_needle_obj->type->name), TRUE);
            filterx_object_unref(self->_needle_obj);
            return FALSE;
          }
      if (self->ignore_case)
      {
        self->needle_str.owned_str_value = g_utf8_casefold(*needle_str, (gssize) MIN(*needle_str_len, G_MAXSSIZE));
          *needle_str = self->needle_str.owned_str_value;
          *needle_str_len = (gssize) MIN(g_utf8_strlen(self->needle_str.owned_str_value, -1), G_MAXSSIZE);
        }
    }
  return TRUE;
}

gboolean
filterx_expr_affix_get_haystack_str(FilterXExprAffix *self, const gchar **haystack, gssize *haystack_str_len)
{
  self->_haystack.haystack_obj = filterx_expr_eval(self->haystack_expr);
  if (!self->_haystack.haystack_obj)
    {
      filterx_eval_push_error_info("failed to evaluate haystack", self->haystack_expr,
                                   g_strdup_printf("invalid expression"), TRUE);
      return FALSE;
    }
  if (!filterx_object_extract_string(self->_haystack.haystack_obj, haystack, haystack_str_len))
    {
      filterx_eval_push_error_info("failed to extract haystack, it must be a string", self->haystack_expr,
                                   g_strdup_printf("got %s instead", self->_haystack.haystack_obj->type->name), TRUE);
      filterx_object_unref(self->_haystack.haystack_obj);
      return FALSE;
    }
  if (self->ignore_case)
    {
      self->_haystack.owned_str_value = g_utf8_casefold(*haystack, (gssize) MIN(*haystack_str_len, G_MAXSSIZE));
      *haystack = self->_haystack.owned_str_value;
      *haystack_str_len = (gssize) MAX(g_utf8_strlen(self->_haystack.owned_str_value, -1), G_MAXSSIZE);
    }
  return TRUE;
}

static FilterXExpr *
_extract_haystack_arg(FilterXFunctionArgs *args, GError **error, const gchar *function_usage)
{
  gsize len = filterx_function_args_len(args);
  if (len != 2)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. %s", function_usage);
      return NULL;
    }
  FilterXExpr *haystack = filterx_function_args_get_expr(args, 0);
  if (!haystack)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "haystack must be set. %s", function_usage);
      return NULL;
    }
  return haystack;
}

static FilterXExpr *
_extract_needle_arg(FilterXFunctionArgs *args, GError **error, const gchar *function_usage)
{
  gsize len = filterx_function_args_len(args);
  if (len != 2)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. %s", function_usage);
      return NULL;
    }
  FilterXExpr *needle = filterx_function_args_get_expr(args, 1);
  if (!needle)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "needle must be set. %s", function_usage);
      return NULL;
    }
  return needle;
}

static gboolean
_extract_optional_args(gboolean *ignore_case, FilterXFunctionArgs *args, GError **error, const gchar *function_usage)
{
  gboolean exists, eval_error;
  gboolean value = filterx_function_args_get_named_literal_boolean(args, "ignorecase", &exists, &eval_error);
  if (!exists)
    return TRUE;

  if (eval_error)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "ignorecase argument must be boolean literal. %s", function_usage);
      return FALSE;
    }

  *ignore_case = value;
  return TRUE;
}

gboolean
_startswith_process(const gchar *haystack, gsize haystack_len, const gchar *needle, gsize needle_len)
{
  if (needle_len > haystack_len)
    return FALSE;
  return memcmp(haystack, needle, needle_len) == 0;
}

static FilterXObject *
_startswith_eval(FilterXExpr *s)
{
  FilterXFunctionStartsWith *self = (FilterXFunctionStartsWith *)s;

  const gchar *haystack_str;
  gssize haystack_len;
  if (!filterx_expr_affix_get_haystack_str(&self->super, &haystack_str, &haystack_len))
    return NULL;

  const gchar *needle_str;
  gssize needle_len;

  gboolean is_needle_ready = filterx_expr_affix_get_needle_str(&self->super, &needle_str, &needle_len);
  if (!is_needle_ready)
    return NULL;


  gboolean startswith = FALSE;
  if (self->super.num_of_needles == 0 )
    {
      startswith = self->super.process(haystack_str, haystack_len, needle_str, needle_len);
      return filterx_boolean_new(startswith);
    }

  guint64 num_of_needles = self->super.num_of_needles;
  for(guint64 i = 0; i < num_of_needles && !startswith; i++)
    {
      _filterx_expr_affix_get_nth_needle(&self->super, i, &needle_str, &needle_len);
      startswith = self->super.process(haystack_str, haystack_len, needle_str, needle_len);
    }
  return filterx_boolean_new(startswith);


}

static void
_startswith_free(FilterXExpr *s)
{
  FilterXFunctionStartsWith *self = (FilterXFunctionStartsWith *) s;

  _filterx_expr_affix_free(&self->super);
}

FilterXExpr *
filterx_function_startswith_new(const gchar *function_name, FilterXFunctionArgs *args, GError **error)
{
  FilterXFunctionStartsWith *self = g_new0(FilterXFunctionStartsWith, 1);

  gboolean ignore_case = FALSE;
  if(!_extract_optional_args(&ignore_case, args, error, "asd"))
    goto error;
  FilterXExpr *haystack_expr = _extract_haystack_arg(args, error, "asd");
  if (!haystack_expr)
    goto error;
  FilterXExpr *needle_expr = _extract_needle_arg(args, error, "asd");
  if (!needle_expr)
    goto error;

  filterx_expr_affix_init_instance(&self->super, function_name, haystack_expr, needle_expr, ignore_case);
  self->super.process = _startswith_process;

  self->super.super.super.eval = _startswith_eval;
  self->super.super.super.free_fn = _startswith_free;

  filterx_function_args_free(args);
  return &self->super.super.super;

error:
  filterx_function_args_free(args);
  filterx_expr_unref(&self->super.super.super);
  return NULL;
}
