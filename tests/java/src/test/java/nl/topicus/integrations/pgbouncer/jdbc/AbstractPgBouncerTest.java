package nl.topicus.integrations.pgbouncer.jdbc;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.util.logging.Level;
import java.util.logging.LogManager;

import org.ini4j.Ini;
import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.BeforeEach;
import org.testcontainers.containers.BindMode;
import org.testcontainers.containers.GenericContainer;
import org.testcontainers.containers.Network;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.images.builder.ImageFromDockerfile;
import org.testcontainers.utility.MountableFile;

import nl.topicus.integrations.pgbouncer.*;

abstract class AbstractPgBouncerTest {
  static {
    // Postgres JDBC driver uses JUL; disable it to avoid annoying, irrelevant, stderr logs during connection testing
    LogManager.getLogManager().getLogger("").setLevel(Level.OFF);
  }

  public static final String POSTGRESQL_NETWORK_ALIAS = "pg";

  private static ImageFromDockerfile pgbouncerImg;

  private static PostgreSQLContainer<?> postgres;

  private static GenericContainer<?> pgbouncer;

  @BeforeAll
  static void setup() throws IOException {
    pgbouncerImg = PgBouncerImageBuilder.createImage();

    Network network = Network.newNetwork();

    postgres = new PostgreSQLContainer<>("postgres:15")
      .withNetwork(network)
      .withNetworkAliases(POSTGRESQL_NETWORK_ALIAS);
    postgres.start();

    PgBouncerConfigBuilder pgbouncerConfigBuilder = PgBouncerConfigBuilder.newInstance();
    pgbouncerConfigBuilder.databaseName(postgres.getDatabaseName());
    Ini ini = pgbouncerConfigBuilder.build();

    File pgbouncerIni = File.createTempFile("pgbouncer", ".ini");
    ini.store(pgbouncerIni);

    File userlist = createUserlist();

    pgbouncer = new GenericContainer<>(pgbouncerImg).withExposedPorts(6432)
      .withCopyFileToContainer(MountableFile.forHostPath(pgbouncerIni.getAbsolutePath()), "/etc/pgbouncer/pgbouncer.ini")
      .withCopyFileToContainer(MountableFile.forHostPath(userlist.getAbsolutePath()), "/etc/pgbouncer/userlist.txt")
      .withNetwork(network)
      .withCommand("pgbouncer", "/etc/pgbouncer/pgbouncer.ini");
   }
    
  @AfterAll
  static void log() {
    postgres.stop();

    // System.out.println(postgres.getLogs());
    // System.out.println(pgbouncer.getLogs());
  }
    
  @BeforeEach
  void startPgBouncer() {
    
    pgbouncer.start();
  }

  @AfterEach
  void stopPgBouncer() {
    System.out.println(pgbouncer.getLogs());
    pgbouncer.stop();
  }

  // TODO: allow setting jdbc options
  protected Connection getPgBouncerConnection() throws SQLException {
    String uri = String.format("jdbc:postgresql://%s:%s/%s?prepareThreshold=2&preparedStatementCacheQueries=5", pgbouncer.getHost(), pgbouncer.getFirstMappedPort(), postgres.getDatabaseName());
    Connection conn = DriverManager.getConnection(uri, postgres.getUsername(), postgres.getPassword());
    conn.setAutoCommit(false);
    return conn;
  }

  private static File createUserlist() throws IOException {
    String userlistContent = String.format("\"%s\" \"%s\"", postgres.getUsername(), postgres.getPassword());
    File userlist = File.createTempFile("userlist", ".txt");

    BufferedWriter bw = new BufferedWriter(new FileWriter(userlist));
    bw.write(userlistContent);
    bw.close();

    return userlist;
  }
}
