use testcontainers::{core::WaitFor, *};
// use postgres::DatabaseTestConfig;

mod postgres;
mod pgbouncer;

#[derive(Debug, Default)]
pub struct HelloWorld;

impl Image for HelloWorld {
    type Args = ();

    fn name(&self) -> String {
        "hello-world".to_owned()
    }

    fn tag(&self) -> String {
        "latest".to_owned()
    }

    fn ready_conditions(&self) -> Vec<WaitFor> {
        vec![WaitFor::message_on_stdout("Hello from Docker!")]
    }
}

#[tokio::test(flavor = "multi_thread")]
async fn cli_can_run_hello_world() {
    let _ = pretty_env_logger::try_init();

    let docker = clients::Cli::default();

    let _container = docker.run(HelloWorld);
	// let test_config = DatabaseTestConfig::new(&docker).await;
	// let test_config = BouncyTestConfig::new(&docker).await;
	postgres::setup_database(&docker);
}
