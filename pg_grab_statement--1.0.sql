/* contrib/pg_grab_statement/pg_grab_statement--1.0.sql */

\echo Use "CREATE EXTENSION pg_grab_statement" to load this file. \quit

CREATE UNLOGGED TABLE grab.statement_log(
  transaction_id  int,
  query_id        int,
  process_id      int,
  user_id         int,
  query_start     timestamptz,
  total_execution float8,
  query_type_id   int,
  query_source    text,
  query_param_values    text[],
  query_param_types     regtype[]
);

COMMENT ON COLUMN grab.statement_log.transaction_id IS 'Number of transaction';
COMMENT ON COLUMN grab.statement_log.query_id IS 'Number of query of the specific transaction';
COMMENT ON COLUMN grab.statement_log.process_id IS 'Backend PID';
COMMENT ON COLUMN grab.statement_log.user_id IS 'User ID';
COMMENT ON COLUMN grab.statement_log.query_start IS 'Timestamp of query execution';
COMMENT ON COLUMN grab.statement_log.total_execution IS 'Total time execution (in seconds)';
COMMENT ON COLUMN grab.statement_log.query_type_id IS 'Type of query operation id';
COMMENT ON COLUMN grab.statement_log.query_source IS 'Source of the query';
COMMENT ON COLUMN grab.statement_log.query_param_values IS 'Parameter values of query';
COMMENT ON COLUMN grab.statement_log.query_param_types IS 'Parameter types of query';

CREATE FUNCTION grab.query_types(
    OUT id int,
    OUT modify boolean,
    OUT name text
)
RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE;

CREATE VIEW grab.statements AS
    SELECT
        l.transaction_id AS transaction,
        l.query_id AS query_number,
        l.process_id AS backend_pid,
        u.usename AS username,
        l.query_start,
        l.total_execution,
        t.name AS query_type,
        l.query_source,
        l.query_param_values,
        l.query_param_types
    FROM grab.statement_log AS l
        LEFT JOIN pg_catalog.pg_user AS u ON u.usesysid = l.user_id
        LEFT JOIN grab.query_types() AS t ON t.id = l.query_type_id;
