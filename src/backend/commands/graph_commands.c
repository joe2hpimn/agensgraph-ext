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

#include "postgres.h"

#include "access/xact.h"
#include "commands/defrem.h"
#include "commands/schemacmds.h"
#include "commands/tablecmds.h"
#include "fmgr.h"
#include "nodes/makefuncs.h"
#include "nodes/nodes.h"
#include "nodes/parsenodes.h"
#include "nodes/pg_list.h"
#include "nodes/value.h"
#include "parser/parser.h"

#include "catalog/ag_graph.h"
#include "catalog/ag_label.h"
#include "utils/graphid.h"

static Oid create_schema_for_graph(const Name graph_name);
static void drop_schema_for_graph(char *graph_name_str, const bool cascade);
static void rename_graph(const Name graph_name, const Name new_name);

PG_FUNCTION_INFO_V1(create_graph);

Datum create_graph(PG_FUNCTION_ARGS)
{
    Name graph_name;
    Oid nsp_id;

    if (PG_ARGISNULL(0))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("graph name must not be NULL")));
    }
    graph_name = PG_GETARG_NAME(0);

    nsp_id = create_schema_for_graph(graph_name);

    insert_graph(graph_name, nsp_id);
    CommandCounterIncrement();

    ereport(NOTICE,
            (errmsg("graph \"%s\" has been created", NameStr(*graph_name))));

    PG_RETURN_VOID();
}

static Oid create_schema_for_graph(const Name graph_name)
{
    char *graph_name_str = NameStr(*graph_name);
    CreateSchemaStmt *schema_stmt;
    CreateSeqStmt *seq_stmt;
    TypeName *integer;
    DefElem *data_type;
    DefElem *maxvalue;
    DefElem *cycle;
    Oid nsp_id;

    /*
     * This is the same with running the following SQL statement.
     *
     * CREATE SCHEMA `graph_name`
     *   CREATE SEQUENCE `LABEL_ID_SEQ_NAME`
     *     AS integer
     *     MAXVALUE `LABEL_ID_MAX`
     *     CYCLE
     *
     * The sequence will be used to assign a unique id to a label in the graph.
     *
     * schemaname doesn't have to be graph_name but the same name is used so
     * that users can find the backed schema for a graph only by its name.
     *
     * ProcessUtilityContext of this command is PROCESS_UTILITY_SUBCOMMAND
     * so the event trigger will not be fired.
     */
    schema_stmt = makeNode(CreateSchemaStmt);
    schema_stmt->schemaname = get_graph_namespace_name(graph_name_str);
    schema_stmt->authrole = NULL;
    seq_stmt = makeNode(CreateSeqStmt);
    seq_stmt->sequence = makeRangeVar(graph_name_str, LABEL_ID_SEQ_NAME, -1);
    integer = SystemTypeName("int4");
    data_type = makeDefElem("as", (Node *)integer, -1);
    maxvalue = makeDefElem("maxvalue", (Node *)makeInteger(LABEL_ID_MAX), -1);
    cycle = makeDefElem("cycle", (Node *)makeInteger(true), -1);
    seq_stmt->options = list_make3(data_type, maxvalue, cycle);
    seq_stmt->ownerId = InvalidOid;
    seq_stmt->for_identity = false;
    seq_stmt->if_not_exists = false;
    schema_stmt->schemaElts = list_make1(seq_stmt);
    schema_stmt->if_not_exists = false;
    nsp_id = CreateSchemaCommand(schema_stmt,
                                 "(generated CREATE SCHEMA command)", -1, -1);
    // CommandCounterIncrement() is called in CreateSchemaCommand()

    return nsp_id;
}

PG_FUNCTION_INFO_V1(drop_graph);

Datum drop_graph(PG_FUNCTION_ARGS)
{
    Name graph_name;
    char *graph_name_str;
    bool cascade;

    if (PG_ARGISNULL(0))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("graph name must not be NULL")));
    }
    graph_name = PG_GETARG_NAME(0);
    cascade = PG_GETARG_BOOL(1);

    graph_name_str = NameStr(*graph_name);
    if (!graph_exists(graph_name_str))
    {
        ereport(ERROR,
                (errcode(ERRCODE_UNDEFINED_SCHEMA),
                 errmsg("graph \"%s\" does not exist", graph_name_str)));
    }

    drop_schema_for_graph(graph_name_str, cascade);

    delete_graph(graph_name);
    CommandCounterIncrement();

    ereport(NOTICE,
            (errmsg("graph \"%s\" has been dropped", NameStr(*graph_name))));

    PG_RETURN_VOID();
}

static void drop_schema_for_graph(char *graph_name_str, const bool cascade)
{
    DropStmt *drop_stmt;
    Value *schema_name;
    List *label_id_seq_name;

    /*
     * ProcessUtilityContext of commands below is PROCESS_UTILITY_SUBCOMMAND
     * so the event triggers will not be fired.
     */

    // DROP SEQUENCE `graph_name_str`.`LABEL_ID_SEQ_NAME`
    drop_stmt = makeNode(DropStmt);
    schema_name = makeString(get_graph_namespace_name(graph_name_str));
    label_id_seq_name = list_make2(schema_name, makeString(LABEL_ID_SEQ_NAME));
    drop_stmt->objects = list_make1(label_id_seq_name);
    drop_stmt->removeType = OBJECT_SEQUENCE;
    drop_stmt->behavior = DROP_RESTRICT;
    drop_stmt->missing_ok = false;
    drop_stmt->concurrent = false;

    RemoveRelations(drop_stmt);
    // CommandCounterIncrement() is called in RemoveRelations()

    // DROP SCHEMA `graph_name_str` [ CASCADE ]
    drop_stmt = makeNode(DropStmt);
    drop_stmt->objects = list_make1(schema_name);
    drop_stmt->removeType = OBJECT_SCHEMA;
    drop_stmt->behavior = cascade ? DROP_CASCADE : DROP_RESTRICT;
    drop_stmt->missing_ok = false;
    drop_stmt->concurrent = false;

    RemoveObjects(drop_stmt);
    // CommandCounterIncrement() is called in RemoveObjects()
}

PG_FUNCTION_INFO_V1(alter_graph);

/*
 * Function alter_graph, invoked by the sql function -
 * alter_graph(graph_name name, operation cstring, new_value name)
 * NOTE: Currently only RENAME is supported.
 *       graph_name and new_value are case sensitive.
 *       operation is case insensitive.
 */
Datum alter_graph(PG_FUNCTION_ARGS)
{
    Name graph_name;
    Name new_value;
    char *operation;

    if (PG_ARGISNULL(0))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("graph_name must not be NULL")));
    }
    if (PG_ARGISNULL(1))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("operation must not be NULL")));
    }
    if (PG_ARGISNULL(2))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("new_value must not be NULL")));
    }

    graph_name = PG_GETARG_NAME(0);
    operation = PG_GETARG_CSTRING(1);
    new_value = PG_GETARG_NAME(2);

    if (strcasecmp("RENAME", operation) == 0)
    {
        rename_graph(graph_name, new_value);
    }
    else
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                        errmsg("invalid operation \"%s\"", operation),
                        errhint("valid operations: RENAME")));
    }

    PG_RETURN_VOID();
}

/*
 * Function to rename a graph by renaming the schema (which is also the
 * namespace) and updating the name in ag_graph
 */
static void rename_graph(const Name graph_name, const Name new_name)
{
    char *oldname = NameStr(*graph_name);
    char *newname = NameStr(*new_name);

    /*
     * ProcessUtilityContext of this command is PROCESS_UTILITY_SUBCOMMAND
     * so the event trigger will not be fired.
     *
     * CommandCounterIncrement() does not have to be called after this.
     */
    RenameSchema(get_graph_namespace_name(oldname),
                 get_graph_namespace_name(newname));

    update_graph_name(graph_name, new_name);
    CommandCounterIncrement();

    ereport(NOTICE,
            (errmsg("graph \"%s\" renamed to \"%s\"", oldname, newname)));
}
