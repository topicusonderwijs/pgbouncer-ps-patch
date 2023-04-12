package nl.topicus.integrations.pgbouncer.quarkus;

import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.RepeatedTest;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.TestInstance;
import org.junit.jupiter.api.TestInstance.Lifecycle;

import io.agroal.api.AgroalDataSource;
import io.quarkus.test.common.QuarkusTestResource;
import io.quarkus.test.junit.QuarkusTest;
import nl.topicus.integrations.pgbouncer.quarkus.model.Gift;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import javax.transaction.Transactional;

import static org.junit.Assert.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;

import java.sql.*;

@QuarkusTest
@QuarkusTestResource(PgBouncerResource.class)
@TestInstance(Lifecycle.PER_CLASS)
public class QuarkusServerSidePreparedStatementTest {
	@Inject
	AgroalDataSource defaultDataSource;

	@Inject
    EntityManager em;

	@BeforeAll
	@Transactional
	void initAll() {

		Gift dog = new Gift();
		dog.setName("string_value");
		em.persist(dog);
		em.flush();
    }

	@RepeatedTest(10)
	@Transactional
	public void testGETHelloEndpoint() {
		for (int i=1; i<=5; i++) {
			Gift gift = em.find(Gift.class, 1L);
			assertNotNull(gift);
			assertEquals("string_value",gift.getName());
			em.clear();
		}
	}
}
