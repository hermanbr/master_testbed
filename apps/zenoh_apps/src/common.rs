// Shared helpers for the testbed apps (included with `#[path = "../common.rs"] mod common;`)
use std::time::{SystemTime, UNIX_EPOCH};

/// All values given for `flag` on the command line, e.g. `-l a -l b` -> ["a", "b"]
pub fn args(flag: &str) -> Vec<String> {
    let args: Vec<String> = std::env::args().collect();
    args.windows(2).filter(|w| w[0] == flag).map(|w| w[1].clone()).collect()
}

/// Last value given for `flag`, or `default`
pub fn arg(flag: &str, default: &str) -> String {
    args(flag).pop().unwrap_or_else(|| default.to_string())
}

/// Zenoh config from the launcher's arguments: `-l <endpoint>` (repeatable), `-e <endpoint>` (repeatable).
/// `-m <mode>`. Everything else stays at Zenoh defaults (peer mode, multicast scouting).
pub fn config_from_args() -> zenoh::Config {
    let mut config = zenoh::Config::default();
    // Retry connecting forever (client default is 0 = one try, then exit): nodes may start before the router
    config.insert_json5("connect/timeout_ms", "-1").unwrap();
    if let Some(mode) = args("-m").pop() {
        config.insert_json5("mode", &serde_json::to_string(&mode).unwrap()).unwrap();
    }
    let listen = args("-l");
    if !listen.is_empty() {
        config.insert_json5("listen/endpoints", &serde_json::to_string(&listen).unwrap()).unwrap();
    }
    let connect = args("-e");
    if !connect.is_empty() {
        config.insert_json5("connect/endpoints", &serde_json::to_string(&connect).unwrap()).unwrap();
    }
    config
}

/// Unix time in nanoseconds (same host clock as the pcap and events.csv)
pub fn now_ns() -> u128 {
    SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_nanos()
}
