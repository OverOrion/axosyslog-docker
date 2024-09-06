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
#include "filterx/expr-literal-generator.h"
#include "filterx/filterx-eval.h"
#include "object-extractor.h"
#include "object-string.h"
#include "filterx/object-json.h"
#include "filterx/object-list-interface.h"

typedef struct FilterXObjectWithCache
{
  FilterXObject *obj;
  const gchar *borrowed_str_value;
  gchar *owned_str_value;
  gssize str_len;
} FilterXObjectWithCache;

typedef struct _FilterXExprAffix
{
  FilterXFunction super;
  FilterXExpr *haystack_expr;
  FilterXExpr *needle_expr;
  gboolean ignore_case;
  guint64 num_of_needles;
  FilterXObjectWithCache *_cached_haystack;
  struct
  {
    FilterXObjectWithCache *single;
    struct
    {
      FilterXObject *needle_list;
      GPtrArray *needle_objects_with_cache;
      GArray *uncached_indexes;
    } multiple;
  } _cached_needle;
  gboolean (*process)(const gchar *haystack, gsize haystack_len, const gchar *needle, gsize needle_len);
} FilterXExprAffix;

typedef struct _FilterXFunctionStartsWith
{
  FilterXExprAffix super;
} FilterXFunctionStartsWith;

static gboolean
_cache_string_literal(FilterXExpr *expr, const gchar *expression_name, gboolean ignore_case, FilterXObject **obj_dest,
                      const gchar **borrowed_str_value, gssize *str_len, gchar **owned_str_value)
{
  *obj_dest = filterx_expr_eval(expr);
  if (!*obj_dest)
    {
      const gchar *err_msg = g_strdup_printf("failed to evaluate %s", expression_name);
      filterx_eval_push_error_info(err_msg, expr,
                                   g_strdup_printf("invalid expression"), TRUE);
      return FALSE;
    }
  if (!filterx_object_extract_string(*obj_dest, borrowed_str_value, str_len))
    {
      const gchar *err_msg = g_strdup_printf("failed to extract %s, it must be a string", expression_name);
      filterx_eval_push_error_info(err_msg, expr,
                                   g_strdup_printf("got %s instead", (*obj_dest)->type->name), TRUE);
      filterx_object_unref(*obj_dest);
      return FALSE;
    }
  if (ignore_case)
    {
      *owned_str_value = g_utf8_casefold(*borrowed_str_value, *str_len);
      *str_len = g_utf8_strlen(*owned_str_value, -1);
      filterx_object_unref(*obj_dest);
    }
  return TRUE;
}

static gboolean
_filterx_expr_affix_cache_haystack(FilterXExprAffix *self)
{
  if (!filterx_expr_is_literal(self->haystack_expr))
    return TRUE;
  return _cache_string_literal(self->haystack_expr, "haystack", self->ignore_case, &self->_cached_haystack->obj,
                               &self->_cached_haystack->borrowed_str_value, &self->_cached_haystack->str_len,
                               &self->_cached_haystack->owned_str_value);
}

static void
_filterx_object_with_cache_free(FilterXObjectWithCache *self)
{
  if (self->owned_str_value)
    g_free(self->owned_str_value);
  else
    {
      filterx_object_unref(self->obj);
    }
  g_free(self);
}

static gboolean
_cache_needle(gint64 index, FilterXExpr *value, gpointer user_data)
{
  gboolean *ignore_case = ((gpointer *)user_data)[0];
  GPtrArray *needle_objects_with_cache = ((gpointer *) user_data)[1];
  GArray *uncached_indexes = ((gpointer *) user_data)[2];
  if (filterx_expr_is_literal(value))
    {
      FilterXObjectWithCache *obj_with_cache = g_new0(FilterXObjectWithCache, 1);
      gboolean success = _cache_string_literal(value, "needle list", *ignore_case, &obj_with_cache->obj,
                                               &obj_with_cache->borrowed_str_value, &obj_with_cache->str_len, &obj_with_cache->owned_str_value);
      if (!success)
        {
          _filterx_object_with_cache_free(obj_with_cache);
          return FALSE;
        }
      g_ptr_array_add(needle_objects_with_cache, obj_with_cache);
    }
  else
    uncached_indexes = g_array_append_val(uncached_indexes, index);
  return TRUE;
}


static gboolean
_filterx_expr_affix_cache_needle(FilterXExprAffix *self)
{
  if (filterx_expr_is_literal(self->needle_expr))
    return _cache_string_literal(self->needle_expr, "needle", self->ignore_case, &self->_cached_needle.single->obj,
                                 &self->_cached_needle.single->borrowed_str_value, &self->_cached_needle.single->str_len,
                                 &self->_cached_needle.single->owned_str_value);
  if (filterx_expr_is_literal_list_generator(self->needle_expr))
    {
      gpointer user_data[] = {&self->ignore_case, self->_cached_needle.multiple.needle_objects_with_cache, self->_cached_needle.multiple.uncached_indexes};
      if(!filterx_literal_list_generator_foreach(self->needle_expr, _cache_needle, user_data))
        return FALSE;
    }
  return TRUE;
}

gboolean
_filterx_expr_affix_cache(FilterXExprAffix *self)
{
  return _filterx_expr_affix_cache_haystack(self) && _filterx_expr_affix_cache_needle(self);
}

static void
_filterx_expr_affix_free(FilterXExpr *s)
{
  FilterXExprAffix *self = (FilterXExprAffix *) s;
  if (self->ignore_case)
    {
      if (self->num_of_needles == 1)
        g_free(self->_cached_needle.single->owned_str_value);
    }

  g_ptr_array_free(self->_cached_needle.multiple.needle_objects_with_cache, TRUE);
  g_array_free(self->_cached_needle.multiple.uncached_indexes, TRUE);
  _filterx_object_with_cache_free(self->_cached_needle.single);

  if (self->num_of_needles == 1)
    filterx_object_unref(self->_cached_needle.single->obj);
  else
    filterx_object_unref(self->_cached_needle.multiple.needle_list);

  filterx_expr_unref(self->needle_expr);

  _filterx_object_with_cache_free(self->_cached_haystack);
  filterx_expr_unref(self->haystack_expr);

  filterx_function_free_method(&self->super);
}

static FilterXObjectWithCache *
_filterx_expr_affix_extract_nth_cached_needle(FilterXExprAffix *self, guint64 index, const gchar **needle_str,
                                              gssize *needle_str_len)
{
  FilterXObjectWithCache *result = g_ptr_array_index(self->_cached_needle.multiple.needle_objects_with_cache, index);

  if (self->ignore_case)
    *needle_str = result->borrowed_str_value;
  else
    *needle_str = result->owned_str_value;
  *needle_str_len = result->str_len;

  return result;
}

gboolean
_eval_needle(FilterXObject *obj, gboolean ignore_case, const gchar **needle_str,
             gssize *needle_str_len)
{
  if (filterx_object_is_type(obj, &FILTERX_TYPE_NAME(string)) &&
      !filterx_object_extract_string(obj, needle_str, needle_str_len))
    goto error;
  if(ignore_case)
    {
      *needle_str = g_utf8_casefold(*needle_str, (gssize) MIN(*needle_str_len, G_MAXSSIZE));
      *needle_str_len = (gssize) MIN(g_utf8_strlen(*needle_str, -1), G_MAXSSIZE);
    }
  return TRUE;
error:
  filterx_object_unref(obj);
  return FALSE;
}

gboolean
filterx_expr_affix_get_haystack_str(FilterXExprAffix *self, const gchar **haystack, gssize *haystack_str_len)
{
  self->_cached_haystack->obj = filterx_expr_eval(self->haystack_expr);
  if (!self->_cached_haystack->obj)
    {
      filterx_eval_push_error_info("failed to evaluate haystack", self->haystack_expr,
                                   g_strdup_printf("invalid expression"), TRUE);
      return FALSE;
    }
  if (!filterx_object_extract_string(self->_cached_haystack->obj, haystack, haystack_str_len))
    {
      filterx_eval_push_error_info("failed to extract haystack, it must be a string", self->haystack_expr,
                                   g_strdup_printf("got %s instead", self->_cached_haystack->obj->type->name), TRUE);
      filterx_object_unref(self->_cached_haystack->obj);
      return FALSE;
    }
  if (self->ignore_case)
    {
      self->_cached_haystack->owned_str_value = g_utf8_casefold(*haystack, (gssize) MIN(*haystack_str_len, G_MAXSSIZE));
      *haystack = self->_cached_haystack->owned_str_value;
      *haystack_str_len = (gssize) MAX(g_utf8_strlen(self->_cached_haystack->owned_str_value, -1), G_MAXSSIZE);
    }
  return TRUE;
}

gboolean
filterx_expr_affix_get_needle_str(FilterXExprAffix *self, const gchar **needle_str, gssize *needle_str_len)
{
  if(self->_cached_needle.single->obj || self->_cached_needle.single->owned_str_value)
    {
      if (self->ignore_case)
        *needle_str = (const gchar *) self->_cached_needle.single->owned_str_value;
      else
        *needle_str = self->_cached_needle.single->borrowed_str_value;
      *needle_str_len = self->_cached_needle.single->str_len;
      return TRUE;
    }
  else
    {
      self->_cached_needle.single->obj = filterx_expr_eval(self->needle_expr);
      if (!self->_cached_needle.single->obj)
        {
          filterx_eval_push_error_info("failed to evaluate needle", self->needle_expr,
                                       g_strdup_printf("invalid expression"), TRUE);
          return FALSE;
        }
      if (!filterx_object_extract_string(self->_cached_needle.single->obj, needle_str, needle_str_len))
        {
          filterx_eval_push_error_info("failed to extract needle, it must be a string", self->needle_expr,
                                       g_strdup_printf("got %s instead", self->_cached_needle.single->obj->type->name), TRUE);
          filterx_object_unref(self->_cached_needle.single->obj);
          return FALSE;
        }
      if (self->ignore_case)
        {
          *needle_str = g_utf8_casefold(*needle_str, (gssize) MIN(*needle_str_len, G_MAXSSIZE));
          *needle_str_len = (gssize) MIN(g_utf8_strlen(*needle_str, -1), G_MAXSSIZE);
        }
    }
  return TRUE;
}

static gboolean
_filterx_expr_affix_eval_optimized(FilterXExprAffix *self, const gchar *haystack_str, gssize haystack_len,
                                   gboolean *startswith)
{
  const gchar *needle_str;
  gssize needle_len;
  if (self->num_of_needles == 1)
    {
      if(!filterx_expr_affix_get_needle_str(self, &needle_str, &needle_len))
        return FALSE;

      *startswith = self->process(haystack_str, haystack_len, needle_str, needle_len);
      return TRUE;
    }

  for(guint64 i = 0; i < self->_cached_needle.multiple.needle_objects_with_cache->len && !*startswith; i++)
    {
      FilterXObjectWithCache *obj_with_cache = _filterx_expr_affix_extract_nth_cached_needle(self, i, &needle_str,
                                               &needle_len);
      *startswith = self->process(haystack_str, haystack_len, needle_str, needle_len);
    }
  if (*startswith)
    return TRUE;

  if(self->_cached_needle.multiple.uncached_indexes->len != 0)
    {
      FilterXObject *current_needle;
      for(guint64 i = 0; i < self->_cached_needle.multiple.uncached_indexes->len && !*startswith; i++)
        {
          gint64 uncached_needle_index = g_array_index(self->_cached_needle.multiple.uncached_indexes, gint64, i);
          current_needle = filterx_list_get_subscript(self->_cached_needle.multiple.needle_list, uncached_needle_index);
          gboolean success = _eval_needle(current_needle, self->ignore_case, &needle_str, &needle_len);
          if (!success)
            {
              filterx_object_unref(current_needle);
              return FALSE;
            }
          *startswith = self->process(haystack_str, haystack_len, needle_str, needle_len);
          filterx_object_unref(current_needle);
        }
    }
  return TRUE;
}

static FilterXObject *
_filterx_expr_affix_eval_unoptimized(FilterXExprAffix *self, const gchar *haystack_str, gssize haystack_len)
{
  FilterXObject *needle_list = filterx_expr_eval(self->needle_expr);
  if (!needle_list)
    return NULL;

  gssize list_length;
  if(!filterx_object_len(needle_list, &list_length))
    return NULL;

  const gchar *needle_str;
  gchar *owned_needle_str;
  gssize needle_len;
  FilterXObject *current_needle;
  gboolean startswith = FALSE;
  for(guint64 i = 0; i < list_length && !startswith; i++)
    {
      current_needle = filterx_list_get_subscript(needle_list, i);

      if (!filterx_object_extract_string(current_needle, (const gchar **) &needle_str, &needle_len))
        {
          filterx_eval_push_error_info("failed to extract needle, it must be a string", self->needle_expr,
                                       g_strdup_printf("got %s instead", current_needle->type->name), TRUE);
          filterx_object_unref(current_needle);
          return FALSE;
        }
      if (self->ignore_case)
        {
          owned_needle_str = g_utf8_casefold(needle_str, (gssize) MIN(needle_len, G_MAXSSIZE));
          needle_len = (gssize) MIN(g_utf8_strlen(needle_str, -1), G_MAXSSIZE);
        }
      startswith = self->process(haystack_str, haystack_len, needle_str, needle_len);
      filterx_object_unref(current_needle);
      if (self->ignore_case)
        g_free(owned_needle_str);
    }
  return filterx_boolean_new(startswith);
}

static FilterXObject *
_filterx_expr_affix_eval(FilterXExpr *s)
{
  FilterXExprAffix *self = (FilterXExprAffix *)s;
  gboolean startswith = FALSE;

  const gchar *haystack_str;
  gssize haystack_len;
  if (!filterx_expr_affix_get_haystack_str(self, &haystack_str, &haystack_len))
    return NULL;

  gboolean success = _filterx_expr_affix_eval_optimized(self, haystack_str, haystack_len, &startswith);
  if(!success)
    return NULL;
  if(startswith)
    return filterx_boolean_new(TRUE);

  return _filterx_expr_affix_eval_unoptimized(self, haystack_str, haystack_len);

}

gboolean
filterx_expr_affix_init_instance(FilterXExprAffix *self, const gchar *function_name, FilterXExpr *haystack,
                                 FilterXExpr *needle, gboolean ignorecase)
{
  filterx_function_init_instance(&self->super, function_name);
  self->num_of_needles = 0;
  self->ignore_case = ignorecase;
  self->haystack_expr = haystack;
  self->needle_expr = needle;
  self->super.super.eval = _filterx_expr_affix_eval;
  self->super.super.free_fn = _filterx_expr_affix_free;

  self->_cached_haystack = g_new0(FilterXObjectWithCache, 1);

  self->_cached_needle.single = g_new0(FilterXObjectWithCache, 1);
  self->_cached_needle.multiple.needle_objects_with_cache = g_ptr_array_new_with_free_func((
                                                              GDestroyNotify) _filterx_object_with_cache_free);
  self->_cached_needle.multiple.uncached_indexes = g_array_new(FALSE, FALSE, sizeof(gint64));

  return _filterx_expr_affix_cache(self);
}

static FilterXExpr *
_extract_haystack_arg(FilterXFunctionArgs *args, GError **error, const gchar *function_usage)
{
  gsize len = filterx_function_args_len(args);
  if (len < 1)
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

static FilterXExpr *
_extract_needle_list_arg(FilterXFunctionArgs *args, GError **error, const gchar *function_usage)
{
  gsize len = filterx_function_args_len(args);
  if (len != 1)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "invalid number of arguments. %s", function_usage);
      return NULL;
    }
  FilterXExpr *needle_list = filterx_function_args_get_named_expr(args, "values");
  return needle_list;
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
    needle_expr = _extract_needle_list_arg(args, error, "asd");
  if (!needle_expr)
    {
      g_set_error(error, FILTERX_FUNCTION_ERROR, FILTERX_FUNCTION_ERROR_CTOR_FAIL,
                  "needle must be set. %s", "asd");
      goto error;
    }

  filterx_expr_affix_init_instance(&self->super, function_name, haystack_expr, needle_expr, ignore_case);
  self->super.process = _startswith_process;

  filterx_function_args_free(args);
  return &self->super.super.super;

error:
  filterx_function_args_free(args);
  filterx_expr_unref(&self->super.super.super);
  return NULL;
}
