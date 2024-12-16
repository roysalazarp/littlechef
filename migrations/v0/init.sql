/**
 * Run a migration on this sql file with the default postgres user
 * ❯ sudo -u postgres psql -f init.sql
 */

DO $$ 
BEGIN
  IF NOT EXISTS (SELECT FROM pg_user WHERE usename = 'littlechef_app') THEN
    CREATE USER littlechef_app;
  END IF;
END $$;

 -- `CREATE DATABASE` cannot be executed inside a transaction block, so we use `\gexec` instead.
SELECT 'CREATE DATABASE littlechef OWNER littlechef_app' WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = 'littlechef')\gexec

\connect littlechef;
CREATE SCHEMA IF NOT EXISTS app AUTHORIZATION littlechef_app;

-- Let's make sure no one uses the public schema.
REVOKE ALL PRIVILEGES ON SCHEMA public FROM PUBLIC;

/**
 * After creating the littlechef_app user, need to:
 * 
 * 1. ❯ sudo -u postgres psql
 * 2. postgres=# ALTER USER littlechef_app WITH PASSWORD 'password';
 * 3. postgres=# \q
 * 4. ❯ sudo service postgresql restart
 * 5. ❯ psql -U littlechef_app -h localhost -d littlechef -W
 * 6. littlechef=> ...
 * 
 * We don't parameterize the andlifecate_app user password in a script
 * because command-line arguments can be visible in process lists or logs
 */