struct Config {
    name: String,
    retries: u32,
    verbose: bool,
}

fn build(name: String) -> Config {
    Config {
        name,
        retries: 3,
        verbose: false,
    }
}

fn report(config: &Config) -> String {
    render_report_line(
        config.name.clone(),
        config.retries,
        config.verbose,
        String::from("a fairly long trailing label"),
    )
}

fn render_report_line(name: String, retries: u32, verbose: bool, label: String) -> String {
    name
}
