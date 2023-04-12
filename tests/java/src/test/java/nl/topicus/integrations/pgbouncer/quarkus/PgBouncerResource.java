package nl.topicus.integrations.pgbouncer.quarkus;

import java.util.Map;

import org.testcontainers.containers.Network;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.images.builder.ImageFromDockerfile;

import io.quarkus.test.common.QuarkusTestResourceLifecycleManager;
import nl.topicus.integrations.pgbouncer.*;

public class PgBouncerResource implements QuarkusTestResourceLifecycleManager {
	static ImageFromDockerfile pgbouncerImg = PgBouncerImageBuilder.createImage();

	static PostgreSQLContainer<?> postgres;

	static PgBouncerContainer<?> pgbouncer;

	@Override
	public Map<String, String> start() {
		Network network = Network.newNetwork();

		postgres = new PostgreSQLContainer<>("postgres:15")
				.withNetwork(network)
				.withNetworkAliases("pg")
				.withDatabaseName("test")
				.withUsername("quarkus")
				.withPassword("s3cr3t");

		pgbouncer = new PgBouncerContainer<>(pgbouncerImg)
				.withNetwork(network)
				.withNetworkAliases("pgbouncer")
				.withDatabase(postgres)
				.withUrlParam("prepareThreshold", "2")
				.withUrlParam("preparedStatementCacheQueries", "5");

		postgres.start();
		pgbouncer.start();

		return Map.of(
				"quarkus.datasource.db-kind", "postgresql",
				"quarkus.datasource.jdbc.url", pgbouncer.getJdbcUrl(),
				"quarkus.datasource.username", pgbouncer.getUsername(),
				"quarkus.datasource.password", pgbouncer.getPassword(),
				"quarkus.hibernate-orm.database.generation", "drop-and-create");
	}

	@Override
	public void stop() {
		System.out.println(pgbouncer.getLogs());
		
		pgbouncer.close();
		postgres.close();
	}
}