/* contrib/pg_grab_statement/pg_grab_statement--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_grab_statement" to load this file. \quit


CREATE SCHEMA grab;

CREATE UNLOGGED TABLE grab.statement(
  transaction_id  int,
  query_id        int,
  process_id      int,
  user_id         int,
  query_start     timestamp,
  total_execution float8,
  query_source    text,
  query_params    text
);

COMMENT ON COLUMN grab.statement.transaction_id IS 'Number of transaction';
COMMENT ON COLUMN grab.statement.query_id IS 'Number of query of the specific transaction';
COMMENT ON COLUMN grab.statement.process_id IS 'Backend PID';
COMMENT ON COLUMN grab.statement.user_id IS 'User ID';
COMMENT ON COLUMN grab.statement.query_start IS 'Timestamp of query execution';
COMMENT ON COLUMN grab.statement.total_execution IS 'Total time execution(in seconds)';
COMMENT ON COLUMN grab.statement.query_source IS 'Source of the query';
COMMENT ON COLUMN grab.statement.query_params IS 'Parameters of query';

