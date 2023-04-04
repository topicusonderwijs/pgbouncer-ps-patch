package nl.topicus;

import java.nio.file.Path;

import org.testcontainers.images.builder.ImageFromDockerfile;

public class PgBouncerImageBuilder {
  public static ImageFromDockerfile createImage() {
    return new ImageFromDockerfile("pgbouncer", false)
    .withFileFromPath("Dockerfile", Path.of("test/Dockerfile"))
    .withFileFromPath("Makefile.diff", Path.of("Makefile.diff"))
    .withFileFromPath("install-pgbouncer-ps-patch.sh", Path.of("install-pgbouncer-ps-patch.sh"))
    .withFileFromPath("etc", Path.of("etc"))
    .withFileFromPath("include", Path.of("include"))
    .withFileFromPath("src", Path.of("src"));
  }
}
