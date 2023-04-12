package nl.topicus.integrations.pgbouncer;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.time.Duration;
import java.time.temporal.ChronoUnit;
import java.util.Collections;
import java.util.concurrent.Future;

import org.ini4j.Ini;
import org.ini4j.Profile.Section;
import org.testcontainers.containers.JdbcDatabaseContainer;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.containers.wait.strategy.LogMessageWaitStrategy;
import org.testcontainers.utility.MountableFile;

public class PgBouncerContainer<SELF extends PgBouncerContainer<SELF>> extends JdbcDatabaseContainer<SELF> {

	public static final Integer PGBOUNCER_PORT = 6432;

	private PostgreSQLContainer<?> postgres;

	public PgBouncerContainer(final Future<String> image) {
		super(image);

		this.waitStrategy = new LogMessageWaitStrategy()
				.withRegEx(".*process up.*\\s")
				.withTimes(2)
				.withStartupTimeout(Duration.of(60, ChronoUnit.SECONDS));
		this.setCommand("pgbouncer", "-vv", "/etc/pgbouncer/pgbouncer.ini");
		this.setExposedPorts(Collections.singletonList(PGBOUNCER_PORT));
	}

	@Override
	protected void configure() {
		// Disable Postgres driver use of java.util.logging to reduce noise at startup
		// time
		withUrlParam("loggerLevel", "OFF");

		try {
			File config = createConfigFile();
			withCopyFileToContainer(MountableFile.forHostPath(config.getAbsolutePath()),
					"/etc/pgbouncer/pgbouncer.ini");

			File userlist = createUserlistFile();
			withCopyFileToContainer(MountableFile.forHostPath(userlist.getAbsolutePath()),
					"/etc/pgbouncer/userlist.txt");
		} catch (IOException e) {
			throw new RuntimeException(e);
		}

		// addEnv("POSTGRES_DB", databaseName);
		// addEnv("POSTGRES_USER", username);
		// addEnv("POSTGRES_PASSWORD", password);

	}

	@Override
	public String getDriverClassName() {
		return "org.postgresql.Driver";
	}

	@Override
	public String getJdbcUrl() {
		String additionalUrlParams = constructUrlParameters("?", "&");
		return ("jdbc:postgresql://" +
				getHost() +
				":" +
				getMappedPort(PGBOUNCER_PORT) +
				"/" +
				postgres.getDatabaseName() +
				additionalUrlParams);
	}

	@Override
	public String getUsername() {
		return postgres.getUsername();
	}

	@Override
	public String getPassword() {
		return postgres.getPassword();
	}

	@Override
	protected String getTestQueryString() {
		return postgres.getTestQueryString();
	}

	public SELF withDatabase(final PostgreSQLContainer<?> postgres) {
		this.postgres = postgres;
		return self();
	}

	public Ini buildConfig() {
		Ini ini = new Ini();
		ini.getConfig().setEscape(false);

		Section database = ini.add("databases");
		database.add(postgres.getDatabaseName(), String.format("host=%s port=%d",
				postgres.getNetworkAliases().stream().findFirst().get(), PostgreSQLContainer.POSTGRESQL_PORT));

		Section pgbouncer = ini.add("pgbouncer");
		pgbouncer.add("listen_addr", "*");
		pgbouncer.add("listen_port", PGBOUNCER_PORT);
		pgbouncer.add("auth_type", "md5");
		pgbouncer.add("auth_file", "/etc/pgbouncer/userlist.txt");
		pgbouncer.add("ignore_startup_parameters", "extra_float_digits");
		pgbouncer.add("pool_mode", "transaction");
		pgbouncer.add("disable_prepared_statement_support", "0");
		pgbouncer.add("prepared_statement_cache_queries", "10"); // TODO: configurable

		return ini;
	}

	private File createConfigFile() throws IOException {
		Ini ini = buildConfig();

		File config = File.createTempFile("pgbouncer", ".ini");
		ini.store(config);

		return config;
	}

	private File createUserlistFile() throws IOException {
		String userlistContent = String.format("\"%s\" \"%s\"", postgres.getUsername(), postgres.getPassword());
		File userlist = File.createTempFile("userlist", ".txt");

		BufferedWriter bw = new BufferedWriter(new FileWriter(userlist));
		bw.write(userlistContent);
		bw.close();

		return userlist;
	}
}
