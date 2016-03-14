pg_grab_statement - PostgreSQL extension for recoding workload of specific database
========================================================================

pg_grab_statement is a PostgreSQL extension for recording detailed information of successfully committed transactions.

This extension overrides Executor{Start,End} PostgreSQL hooks and writes detailed information in an unlogged table without using the SQL level.


Authors
-------

 * Dmitry Vasilyev <d.vasilyev@postgrespro.ru>, Postgres Professional, Moscow, Russia


License
-------

Development version, available on github, released under the
GNU General Public License, version 2 (June 1991).

Downloads
---------
Stable version of pg_grab_statement is available at https://github.com/postgrespro/pg_grab_statement

Installation
------------

pg_grab_statement is a regular PostgreSQL extension.
To build and install it you should ensure the following:

 * The development package of PostgreSQL is installed or
   PostgreSQL is built from source.
 * Your PATH variable is configured so that pg_config command is available.

Typical installation procedure may look like this:

	$ git clone https://github.com/postgrespro/pg_grab_statement.git
	$ cd pg_grab_statement
	$ make USE_PGXS=1
	$ sudo make USE_PGXS=1 install
	$ make USE_PGXS=1 installcheck
	$ psql YourDatabaseName -c "CREATE EXTENSION pg_grab_statement;"


PostgreSQL 9.0 installation notes
----------------------
You need to create the logging table by hand:

	CREATE SCHEMA grab;
	CREATE TABLE grab.statement(
	    transaction_id        int,
	    query_id              int,
	    process_id            int,
	    user_id               int,
	    query_start           timestamptz,
	    total_execution       float8,
	    query_type_id         int,
	    query_source          text,
	    query_param_values    text[],
	    query_param_types     regtype[]
	);



Choose your right way to enable the library
---------------------------------------

Load library in current session:

	LOAD 'pg_grab_statement';

Or set shared_preload_libraries in postgresql.conf (works for version >= 9.0) to record all transactions on all databases:

	shared_preload_libraries = 'pg_grab_statement'

Or enable recording for all new sessions with a specific role (works for version >= 9.4, without restart):

	ALTER ROLE rolename SET session_preload_libraries = 'pg_grab_statement';

Or record all new sessions of all users in a specific database (works for version >= 9.4, without restart):

	ALTER DATABASE dbname SET session_preload_libraries = 'pg_grab_statement';


Overhead
--------

* No overhead if library is not loaded
* 10-15% with SELECT-only benchmark


Recorded data
--------------
```
=# \d+ grab.statement_log
                                                Unlogged table "grab.statement_log"
       Column       |           Type           | Modifiers | Storage  | Stats target |                 Description
--------------------+--------------------------+-----------+----------+--------------+---------------------------------------------
 transaction_id     | integer                  |           | plain    |              | Number of transaction
 query_id           | integer                  |           | plain    |              | Number of query of the specific transaction
 process_id         | integer                  |           | plain    |              | Backend PID
 user_id            | integer                  |           | plain    |              | User ID
 query_start        | timestamp with time zone |           | plain    |              | Timestamp of query execution
 total_execution    | double precision         |           | plain    |              | Total time execution (in seconds)
 query_type_id      | integer                  |           | plain    |              | Type of query operation id
 query_source       | text                     |           | extended |              | Source of the query
 query_param_values | text[]                   |           | extended |              | Parameter values of query
 query_param_types  | regtype[]                |           | extended |              | Parameter types of query
```
