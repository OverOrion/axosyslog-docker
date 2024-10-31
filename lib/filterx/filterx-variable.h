/*
 * Copyright (c) 2024 Axoflow
 * Copyright (c) 2024 László Várady
 * Copyright (c) 2023 Balazs Scheidler <balazs.scheidler@axoflow.com>
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

#ifndef FILTERX_VARIABLE_H
#define FILTERX_VARIABLE_H

#include "syslog-ng.h"
#include "filterx-object.h"

#define FILTERX_SCOPE_MAX_GENERATION ((1UL << 20) - 1)

typedef guint32 FilterXVariableHandle;

typedef struct _FilterXVariable
{
  /* the MSB indicates that the variable is a floating one */
  FilterXVariableHandle handle;
  /*
   * assigned -- Indicates that the variable was assigned to a new value
   *
   * declared -- this variable is declared (e.g. retained for the entire input pipeline)
   */
  guint32 assigned:1,
          declared:1,
          generation:20;
  FilterXObject *value;
} FilterXVariable;

typedef enum
{
  FX_VAR_MESSAGE,
  FX_VAR_FLOATING,
  FX_VAR_DECLARED,
} FilterXVariableType;


void filterx_variable_init_instance(FilterXVariable *v, FilterXVariableHandle handle,
                                    FilterXObject *initial_value, guint32 generation);
void filterx_variable_free_method(FilterXVariable *v);

gboolean filterx_variable_is_floating(FilterXVariable *v);
gboolean filterx_variable_handle_is_floating(FilterXVariableHandle handle);
const gchar *filterx_variable_get_name(FilterXVariable *v, gssize *len);
NVHandle filterx_variable_get_nv_handle(FilterXVariable *v);
FilterXObject *filterx_variable_get_value(FilterXVariable *v);
void filterx_variable_set_value(FilterXVariable *v, FilterXObject *new_value);
void filterx_variable_unset_value(FilterXVariable *v);
gboolean filterx_variable_is_set(FilterXVariable *v);

static inline gboolean
filterx_variable_is_declared(FilterXVariable *v)
{
  return v->declared;
}

static inline gboolean
filterx_variable_is_assigned(FilterXVariable *v)
{
  return v->assigned;
}

static inline gboolean
filterx_variable_is_same_generation(FilterXVariable *v, guint32 generation)
{
  return v->generation == generation;
}

static inline void
filterx_variable_set_generation(FilterXVariable *v, guint32 generation)
{
  v->generation = generation;
}

static inline void
filterx_variable_unassign(FilterXVariable *v)
{
  v->assigned = FALSE;
}

static inline void
filterx_variable_set_declared(FilterXVariable *v, gboolean declared)
{
  v->declared = declared;
}

FilterXVariableHandle filterx_map_varname_to_handle(const gchar *name, FilterXVariableType type);

#endif