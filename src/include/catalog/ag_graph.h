/*
 * Copyright 2020 Bitnine Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AG_AG_GRAPH_H
#define AG_AG_GRAPH_H

#include "postgres.h"

#include "catalog/ag_catalog.h"

#define Anum_ag_graph_name 1
#define Anum_ag_graph_namespace 2

#define Natts_ag_graph 2

#define ag_graph_relation_id() ag_relation_id("ag_graph", "table")
#define ag_graph_name_index_id() ag_relation_id("ag_graph_name_index", "index")

Oid insert_graph(const Name graph_name, const Oid nsp_id);
void delete_graph(const Name graph_name);
void update_graph_name(const Name graph_name, const Name new_name);

Oid get_graph_oid(const char *graph_name);
Oid get_graph_namespace(const char *graph_name);

/*
 * Schema name doesn't have to be graph name but the same name is used so
 * that users can find the backed schema for a graph only by its name.
 */
#define get_graph_namespace_name(graph_name) (graph_name)

#define graph_exists(graph_name) OidIsValid(get_graph_oid(graph_name))

#endif
