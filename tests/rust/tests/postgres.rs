use std::env;

// use dotenv::dotenv;
use testcontainers::clients::Cli;
use testcontainers::images::postgres::Postgres;
use testcontainers::{Container, RunnableImage};

// pub fn setup(docker: &Cli) -> (Container<Postgres>, PgPool) {
//     dotenv().ok();
//     let pg_container = setup_database(docker);
//     let pool = create_connection_pool();
//     run_migrations(&pool);
//     (pg_container)
// }

pub fn setup_database(docker: &Cli) -> Container<Postgres> {
    let pg_container = docker.run(get_pg_image());
    let pg_port = pg_container.get_host_port_ipv4(5432);
    env::set_var(
        "DATABASE_URL",
        format!(
            "postgres://postgres:password@localhost:{}/satellites-db",
            pg_port
        ),
    );
    pg_container
}

fn get_pg_image() -> RunnableImage<Postgres> {
    RunnableImage::from(Postgres::default())
		.with_tag("15-alpine")
        .with_env_var(("POSTGRES_DB", "satellites-db"))
        .with_env_var(("POSTGRES_PASSWORD", "password"))
}


