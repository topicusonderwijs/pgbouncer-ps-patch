package nl.topicus.integrations.pgbouncer;

import java.nio.file.Path;

import org.testcontainers.images.builder.ImageFromDockerfile;

public class PgBouncerImageBuilder {
	public static ImageFromDockerfile createImage() {
		return new ImageFromDockerfile("pgbouncer", false)
				.withFileFromPath("Dockerfile", Path.of("../Dockerfile"))
				.withFileFromPath("Makefile.diff", Path.of("../../Makefile.diff"))
				.withFileFromPath("configure.ac.diff", Path.of("../../configure.ac.diff"))
				.withFileFromPath("install-pgbouncer-ps-patch.sh", Path.of("../../install-pgbouncer-ps-patch.sh"))
				.withFileFromPath("etc", Path.of("../../etc"))
				.withFileFromPath("include", Path.of("../../include"))
				.withFileFromPath("src", Path.of("../../src"));
	}
}
