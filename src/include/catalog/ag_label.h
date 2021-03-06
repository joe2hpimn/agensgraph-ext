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

#ifndef AG_AG_LABEL_H
#define AG_AG_LABEL_H

#include "postgres.h"

#include "catalog/ag_catalog.h"

#define Anum_ag_label_name 1
#define Anum_ag_label_graph 2
#define Anum_ag_label_id 3
#define Anum_ag_label_kind 4
#define Anum_ag_label_relation 5

#define Natts_ag_label 5

#define ag_label_relation_id() ag_relation_id("ag_label", "table")
#define ag_label_oid_index_id() ag_relation_id("ag_label_oid_index", "index")
#define ag_label_name_graph_index_id() \
    ag_relation_id("ag_label_name_graph_index", "index")
#define ag_label_graph_id_index_id() \
    ag_relation_id("ag_label_graph_id_index", "index")

#define LABEL_ID_SEQ_NAME "_label_id_seq"

#define LABEL_KIND_VERTEX 'v'
#define LABEL_KIND_EDGE 'e'

Oid insert_label(const char *label_name, Oid label_graph, int32 label_id,
                 char label_kind, Oid label_relation);

Oid get_label_oid(const char *label_name, Oid label_graph);
int32 get_label_id(const char *label_name, Oid label_graph);

/*
 * Relation name doesn't have to be label name but the same name is used so
 * that users can find the backed relation for a label only by its name.
 */
#define get_label_relation_name(label_name) (label_name)

bool label_id_exists(Oid label_graph, int32 label_id);

#define label_exists(label_name, label_graph) \
    OidIsValid(get_label_oid(label_name, label_graph))

#endif
