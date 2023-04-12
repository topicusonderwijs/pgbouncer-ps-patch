package nl.topicus.integrations.pgbouncer;

import org.ini4j.Ini;
import org.ini4j.Profile.Section;
import org.testcontainers.containers.PostgreSQLContainer;

public class PgBouncerConfigBuilder {
  private String databaseName;

  private String postgresHost = "pg";

  private Integer postgresPort = PostgreSQLContainer.POSTGRESQL_PORT;

  public static PgBouncerConfigBuilder newInstance() {
    return new PgBouncerConfigBuilder();
  }
  
  private PgBouncerConfigBuilder() {}

  public PgBouncerConfigBuilder databaseName(String databaseName) {
    this.databaseName = databaseName;
    return this;
  }

  public PgBouncerConfigBuilder postgresHost(String host) {
    this.postgresHost = host;
    return this;
  }

  public PgBouncerConfigBuilder postgresPort(Integer port) {
    this.postgresPort = port;
    return this;
  }

  public Ini build() {
    Ini ini = new Ini();
    ini.getConfig().setEscape(false);
    
    Section database = ini.add("databases");
    database.add(databaseName, String.format("host=%s port=%d", postgresHost, postgresPort));

    Section pgbouncer = ini.add("pgbouncer");
    pgbouncer.add("listen_addr", "*");
    pgbouncer.add("listen_port", "6432");
    pgbouncer.add("auth_type", "md5");
    pgbouncer.add("auth_file", "/etc/pgbouncer/userlist.txt");
    pgbouncer.add("ignore_startup_parameters", "extra_float_digits");
    pgbouncer.add("pool_mode", "transaction");
    pgbouncer.add("disable_prepared_statement_support", "0");
    pgbouncer.add("prepared_statement_cache_queries", "10");

    return ini;
  }
}
