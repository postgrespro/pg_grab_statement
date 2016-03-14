#include "postgres.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "access/xact.h"
#include "nodes/makefuncs.h"
#include "catalog/pg_type.h"
#include "utils/rel.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"
#include "lib/stringinfo.h"
#include "executor/instrument.h"

#if (PG_VERSION_NUM >= 90400)
#include "access/htup_details.h"
#endif

#define EXTENSION_SCHEMA	"grab"
#define EXTENSION_LOG_TABLE	"statement_log"

PG_MODULE_MAGIC;

void            _PG_init(void);
void            _PG_fini(void);

static ExecutorStart_hook_type prev_ExecutorStart = NULL;
static ExecutorEnd_hook_type prev_ExecutorEnd = NULL;

static void     grab_ExecutorStart(QueryDesc * queryDesc, int eflags);
static void     grab_ExecutorEnd(QueryDesc * queryDesc);

void
_PG_init(void)
{
	prev_ExecutorStart = ExecutorStart_hook;
	ExecutorStart_hook = grab_ExecutorStart;
	prev_ExecutorEnd = ExecutorEnd_hook;
	ExecutorEnd_hook = grab_ExecutorEnd;
}

void
_PG_fini(void)
{
	ExecutorStart_hook = prev_ExecutorStart;
	ExecutorEnd_hook = prev_ExecutorEnd;
}

static void
grab_ExecutorStart(QueryDesc * queryDesc, int eflags)
{
	if (prev_ExecutorStart)
		prev_ExecutorStart(queryDesc, eflags);
	else
		standard_ExecutorStart(queryDesc, eflags);

	if (queryDesc->totaltime == NULL) {
		MemoryContext   oldcxt;

		oldcxt = MemoryContextSwitchTo(queryDesc->estate->es_query_cxt);
		queryDesc->totaltime = InstrAlloc(1, INSTRUMENT_ALL);
		MemoryContextSwitchTo(oldcxt);
	}
}

PG_FUNCTION_INFO_V1(query_types);
Datum
query_types(PG_FUNCTION_ARGS)
{
	FuncCallContext *funcctx;
	CmdType         MaxCmdElement = CMD_NOTHING;

	if (SRF_IS_FIRSTCALL()) {
		MemoryContext   oldcontext;
		TupleDesc       tupdesc;

		funcctx = SRF_FIRSTCALL_INIT();

		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		tupdesc = CreateTemplateTupleDesc(3, false);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "id",
				   INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "modify",
				   BOOLOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "name",
				   TEXTOID, -1, 0);
		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		MemoryContextSwitchTo(oldcontext);
	}
	funcctx = SRF_PERCALL_SETUP();

	if (funcctx->call_cntr < MaxCmdElement + 1) {
		Datum           values[3];
		bool            nulls[3];
		HeapTuple       tuple;
		CmdType         current;

		MemSet(values, 0, sizeof(values));
		MemSet(nulls, false, sizeof(nulls));

		current = (CmdType) funcctx->call_cntr;
		values[0] = Int32GetDatum(funcctx->call_cntr);
		if (CMD_SELECT == current) {
			values[1] = BoolGetDatum(false);
		} else {
			values[1] = BoolGetDatum(true);
		}

		switch (current) {
		case CMD_UNKNOWN:
			values[2] = CStringGetDatum(cstring_to_text("UNKNOWN"));
			break;
		case CMD_SELECT:
			values[2] = CStringGetDatum(cstring_to_text("SELECT"));
			break;
		case CMD_UPDATE:
			values[2] = CStringGetDatum(cstring_to_text("UPDATE"));
			break;
		case CMD_INSERT:
			values[2] = CStringGetDatum(cstring_to_text("INSERT"));
			break;
		case CMD_DELETE:
			values[2] = CStringGetDatum(cstring_to_text("DELETE"));
			break;
		case CMD_UTILITY:
			values[2] = CStringGetDatum(cstring_to_text("UTILITY"));
			break;
		case CMD_NOTHING:
			values[2] = CStringGetDatum(cstring_to_text("NOTHING"));
			break;
		default:
			values[2] = CStringGetDatum(cstring_to_text("UNKNOWN"));
			break;
		}

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
		funcctx->call_cntr++;
	} else {
		SRF_RETURN_DONE(funcctx);
	}
}

static void
grab_ExecutorEnd(QueryDesc * queryDesc)
{
	Datum           values[10];
	bool            nulls[10] = {false, false, false, false, false, false, false, false, false, false};
	Relation        dump_heap;
	RangeVar       *dump_table_rv;
	HeapTuple       tuple;
	Oid             namespaceId;

	/* lookup schema */
	namespaceId = GetSysCacheOid1(NAMESPACENAME, CStringGetDatum(EXTENSION_SCHEMA));
	if (OidIsValid(namespaceId)) {
		/* lookup table */
		if (OidIsValid(get_relname_relid(EXTENSION_LOG_TABLE, namespaceId))) {

			/* get table heap */
			dump_table_rv = makeRangeVar(EXTENSION_SCHEMA, EXTENSION_LOG_TABLE, -1);
			dump_heap = heap_openrv(dump_table_rv, RowExclusiveLock);

			/* transaction info */
			values[0] = Int32GetDatum(GetCurrentTransactionId());
			values[1] = Int32GetDatum(GetCurrentCommandId(false));
			values[2] = Int32GetDatum(MyProcPid);
			values[3] = Int32GetDatum(GetUserId());

			/* query timing */
			if (queryDesc->totaltime != NULL) {
				InstrEndLoop(queryDesc->totaltime);
				values[4] = TimestampGetDatum(
							      TimestampTzPlusMilliseconds(GetCurrentTimestamp(),
				  (queryDesc->totaltime->total * -1000.0)));
				values[5] = Float8GetDatum(queryDesc->totaltime->total);
			} else {
				nulls[4] = true;
				nulls[5] = true;
			}

			/* query command type */
			values[6] = Int32GetDatum(queryDesc->operation);

			/* query text */
			values[7] = CStringGetDatum(
				    cstring_to_text(queryDesc->sourceText));

			/* query params */
			if (queryDesc->params != NULL) {
				int             numParams = queryDesc->params->numParams;
				Oid             out_func_oid, ptype;
				Datum           pvalue;
				bool            isvarlena;
				FmgrInfo       *out_functions;

				bool            arr_nulls[numParams];
				size_t          arr_nelems = (size_t) numParams;
				Datum          *arr_val_elems = palloc(sizeof(Datum) * arr_nelems);
				Datum          *arr_typ_elems = palloc(sizeof(Datum) * arr_nelems);
				char            elem_val_byval, elem_val_align, elem_typ_byval,
				                elem_typ_align;
				int16           elem_val_len, elem_typ_len;
				int             elem_dims[1], elem_lbs[1];

				/* init */
				out_functions = (FmgrInfo *) palloc(
					    (numParams) * sizeof(FmgrInfo));
				get_typlenbyvalalign(TEXTOID, &elem_val_len, &elem_val_byval, &elem_val_align);
				get_typlenbyvalalign(REGTYPEOID, &elem_typ_len, &elem_typ_byval, &elem_typ_align);
				elem_dims[0] = arr_nelems;
				elem_lbs[0] = 1;

				for (int paramno = 0; paramno < numParams; paramno++) {
					pvalue = queryDesc->params->params[paramno].value;
					ptype = queryDesc->params->params[paramno].ptype;
					getTypeOutputInfo(ptype, &out_func_oid, &isvarlena);
					fmgr_info(out_func_oid, &out_functions[paramno]);

					arr_typ_elems[paramno] = ptype;

					arr_nulls[paramno] = true;
					if (!queryDesc->params->params[paramno].isnull) {
						arr_nulls[paramno] = false;
						arr_val_elems[paramno] = PointerGetDatum(
							    cstring_to_text(
									    OutputFunctionCall(&out_functions[paramno], pvalue)));
					}
				}
				values[8] = PointerGetDatum(
							 construct_md_array(
							      arr_val_elems,
								  arr_nulls,
									  1,
								  elem_dims,
								   elem_lbs,
								    TEXTOID,
							       elem_val_len,
							     elem_val_byval,
							   elem_val_align));
				values[9] = PointerGetDatum(
							    construct_array(
							      arr_typ_elems,
								 arr_nelems,
								 REGTYPEOID,
							       elem_typ_len,
							     elem_typ_byval,
							   elem_typ_align));

				pfree(out_functions);
				pfree(arr_val_elems);

			} else {
				nulls[8] = true;
				nulls[9] = true;
			}

			/* insert */
			tuple = heap_form_tuple(dump_heap->rd_att, values, nulls);
			simple_heap_insert(dump_heap, tuple);
			heap_close(dump_heap, RowExclusiveLock);
		}
	}
	if (prev_ExecutorEnd)
		prev_ExecutorEnd(queryDesc);
	else
		standard_ExecutorEnd(queryDesc);
}
