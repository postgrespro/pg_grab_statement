#include "postgres.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "access/xact.h"
#include "nodes/makefuncs.h"
#include "utils/rel.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"
#include "lib/stringinfo.h"
#include "executor/instrument.h"

#if (PG_VERSION_NUM >= 90400)
#include "access/htup_details.h"
#endif

PG_MODULE_MAGIC;

void		_PG_init(void);
void		_PG_fini(void);

static ExecutorStart_hook_type prev_ExecutorStart = NULL;
static ExecutorEnd_hook_type prev_ExecutorEnd = NULL;

static void grab_ExecutorStart(QueryDesc *queryDesc, int eflags);
static void grab_ExecutorEnd(QueryDesc *queryDesc);

/*	magic delimiter query_params */
const char *DELIMETER = "\n---\n";

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
grab_ExecutorStart(QueryDesc *queryDesc, int eflags)
{
	if (prev_ExecutorStart)
		prev_ExecutorStart(queryDesc, eflags);
	else
		standard_ExecutorStart(queryDesc, eflags);

	if (queryDesc->totaltime == NULL)
	{
		MemoryContext oldcxt;

		oldcxt = MemoryContextSwitchTo(queryDesc->estate->es_query_cxt);
		queryDesc->totaltime = InstrAlloc(1, INSTRUMENT_ALL);
		MemoryContextSwitchTo(oldcxt);
	}
}

static void
grab_ExecutorEnd(QueryDesc *queryDesc)
{
	Datum		values[8];
	bool		nulls[8] = {false, false, false, false, false, false, false, false};
	Relation	dump_heap;
	RangeVar   *dump_table_rv;
	HeapTuple	tuple;
	Oid			namespaceId;

	/* lookup schema */
	namespaceId = GetSysCacheOid1(NAMESPACENAME, CStringGetDatum("grab"));
	if (OidIsValid(namespaceId))
	{
		/* lookup table */
		if (OidIsValid(get_relname_relid("statement", namespaceId)))
		{
			/* get table heap */
			dump_table_rv = makeRangeVar("grab", "statement", -1);
			dump_heap = heap_openrv(dump_table_rv, RowExclusiveLock);

			/* fill data */
			values[0] = Int32GetDatum(GetCurrentTransactionId());
			values[1] = Int32GetDatum(GetCurrentCommandId(false));
			values[2] = Int32GetDatum(MyProcPid);
			values[3] = Int32GetDatum(GetUserId());
			/* data timing */
			if (queryDesc->totaltime != NULL)
			{
				InstrEndLoop(queryDesc->totaltime);
				values[5] = Float8GetDatum(queryDesc->totaltime->total);
				values[4] = TimestampGetDatum(
						   TimestampTzPlusMilliseconds(GetCurrentTimestamp(),
								   (queryDesc->totaltime->total * -1000.0)));
			}
			else
			{
				nulls[4] = true;
				nulls[6] = true;
			}
			values[6] = CStringGetDatum(
									 cstring_to_text(queryDesc->sourceText));

			/* fill query params */
			if (queryDesc->params != NULL)
			{
				Oid			out_func_oid,
							ptype;
				Datum		pvalue;
				bool		isvarlena;
				FmgrInfo   *out_functions;
				StringInfoData buf;

				/* init */
				out_functions = (FmgrInfo *) palloc(
						  (queryDesc->params->numParams) * sizeof(FmgrInfo));
				initStringInfo(&buf);

				for (int paramno = 0; paramno < queryDesc->params->numParams; paramno++)
				{
					pvalue = queryDesc->params->params[paramno].value;
					ptype = queryDesc->params->params[paramno].ptype;
					getTypeOutputInfo(ptype, &out_func_oid, &isvarlena);
					fmgr_info(out_func_oid, &out_functions[paramno]);
					/* append param to buf */
					if (paramno != (queryDesc->params->numParams - 1))
					{
						appendStringInfo(&buf, "%s%s", OutputFunctionCall(&out_functions[paramno], pvalue), DELIMETER);
					}
					else
					{
						/* is last param */
						appendStringInfo(&buf, "%s", OutputFunctionCall(&out_functions[paramno], pvalue));
					}
				}
				values[7] = PointerGetDatum(cstring_to_text((buf.data)));
			}
			else
			{
				/* query params is null */
				nulls[7] = true;
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
