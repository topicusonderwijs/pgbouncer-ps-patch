use testcontainers::clients::Cli;
use testcontainers::core::WaitFor;
use testcontainers::images::generic::GenericImage;
use testcontainers::Container;

// pub struct BouncyTestConfig<'a> {
//     pub pgbouncer_container: Container<'a, GenericImage>, // Needed to keep container running
//     // pub db_config: db::DbConfig,
// }

// impl<'a> DatabaseTestConfig<'a> {
//     pub async fn new(cli: &'a Cli) -> BouncyTestConfig {

// 	}
// }
