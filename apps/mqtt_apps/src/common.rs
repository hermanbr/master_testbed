// Shared helpers for the MQTT testbed apps (included with `#[path = "../common.rs"] mod common;`).
// Same as apps/zenoh_apps/src/common.rs minus the Zenoh config.
#![allow(dead_code)]
use std::hash::{BuildHasher, Hasher};
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

/// Unix time in nanoseconds (same host clock as the pcap and events.csv)
pub fn now_ns() -> u128 {
    SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_nanos()
}

/// Random 16-hex-digit id (MQTT has no zid; used as client id and topic suffix)
pub fn random_id() -> String {
    format!("{:016x}", std::collections::hash_map::RandomState::new().build_hasher().finish())
}

/// Max MQTT packet size we send/accept (rumqttc's default of 10 KiB is smaller than a video chunk)
pub const MAX_PACKET: u32 = 1 << 20;
